#include "eternity/game/mob_primary_zones.hpp"
#include <limits>
#include <stdexcept>
#include <utility>
namespace eternity::game {
namespace {
struct Dispatch {std::size_t& depth;explicit Dispatch(std::size_t& n):depth(n){++depth;}~Dispatch(){--depth;}};
struct SyncMaps {ObeliskRuntimeMaps* records;std::size_t id;~SyncMaps(){if(records)static_cast<void>(records->runtime_map(id));}};
}
MobPrimaryZoneLease::MobPrimaryZoneLease(std::shared_ptr<MobPrimaryZones> p,std::size_t id,std::uint64_t generation):owner_(std::move(p)),map_(id),generation_(generation){}
MobPrimaryZoneLease::~MobPrimaryZoneLease(){reset();}
MobPrimaryZoneLease::MobPrimaryZoneLease(MobPrimaryZoneLease&& other)noexcept:owner_(std::move(other.owner_)),map_(other.map_),generation_(other.generation_){}
MobPrimaryZoneLease& MobPrimaryZoneLease::operator=(MobPrimaryZoneLease&& other)noexcept{if(this!=&other){reset();owner_=std::move(other.owner_);map_=other.map_;generation_=other.generation_;}return *this;}
void MobPrimaryZoneLease::reset()noexcept{if(owner_){auto retained=std::move(owner_);retained->release(map_);}}
void MobPrimaryZoneLease::visit(const std::function<void(RuntimeZone&)>& fn)const{if(!owner_)throw std::logic_error("Empty primary zone lease");auto retained=owner_;retained->visit(map_,generation_,fn);}
std::shared_ptr<MobPrimaryZones> MobPrimaryZones::create(std::shared_ptr<ServerMaps> m,std::shared_ptr<ZoneServices> s,std::unique_ptr<MobPool> p,MobPrimaryZoneTransport t){return std::shared_ptr<MobPrimaryZones>(new MobPrimaryZones(std::move(m),std::move(s),std::move(p),std::move(t)));}
MobPrimaryZones::MobPrimaryZones(std::shared_ptr<ServerMaps> maps,std::shared_ptr<ZoneServices> services,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport)
    :maps_(std::move(maps)),services_(std::move(services)),transport_(std::move(transport)),pool_(std::move(pool)){
    if(!maps_ || !services_ || !pool_ || !pool_->has_applied_skills_runtime() || !transport_.database || !transport_.world || !transport_.map)throw std::invalid_argument("Primary zones require owning real maps/services/bound pool/transports");
    if(pool_->available()!=pool_->size())throw std::invalid_argument("Construct primary authority before active allocations");
    life_.resize(maps_->size());registry_=std::make_unique<ZoneRegistry>(*maps_,*services_);
}
MobPrimaryZones::~MobPrimaryZones(){
    // Destructors must not invent gameplay reset publications or swallow backend
    // failures. Explicit drain() is a required shutdown precondition.
    if(ready_)for(const auto& v:life_)if(v.primary && v.stage!=3)std::terminate();
    if(!ready_)registry_.reset(); // Backend concrete RuntimeZone RAII handles partial Init.
    obelisks_.reset();pool_.reset();registry_.reset();
    retired_population_.clear();retired_linked_.clear();records_.reset();
    transport_={};services_.reset();maps_.reset();
}
bool MobPrimaryZones::initialize(){
    auto retained=shared_from_this();
    std::lock_guard lock(mutex_);if(started_)throw std::logic_error("Primary zones initialization already attempted");started_=true;Dispatch frame(dispatch_);
    if(!registry_->initialize())return false;
    for(std::size_t i=0;i<life_.size();++i)if(maps_->all()[i].settings.create_type==0){auto* z=registry_->select(static_cast<std::uint16_t>(i),{});if(!z || z->map!=&maps_->all()[i] || !z->mobs)throw std::logic_error("Zone backend omitted actual primary mob owner");life_[i].primary=true;}
    ready_=true;return true;
}
void MobPrimaryZones::attach_obelisks(std::unique_ptr<ObeliskRuntimeMaps> records,std::unique_ptr<MobAppliedSkillsObeliskMaps> router){
    std::lock_guard lock(mutex_);if(!ready_ || records_ || !records || !router)throw std::logic_error("Attach retained obelisk owners once after InitZones");
    for(std::size_t i=0;i<maps_->size();++i)if(records->runtime_map(i).blocked_tiles.data()!=maps_->all()[i].definition.tiles.data())throw std::invalid_argument("Obelisks belong to different ServerMaps");
    records_=std::move(records);obelisks_=std::move(router);
}
void MobPrimaryZones::attached()const{if(!ready_ || !obelisks_ || !records_)throw std::logic_error("Primary zone gameplay requires actual retained obelisk assembly");}
RuntimeZone* MobPrimaryZones::actual(std::size_t id)const{if(id>=life_.size())throw std::out_of_range("Primary map outside ServerMaps");if(!ready_ || !life_[id].primary)return nullptr;return registry_->select(static_cast<std::uint16_t>(id),{});}
std::size_t MobPrimaryZones::index(const ConfiguredMap& map)const{for(std::size_t i=0;i<maps_->size();++i)if(&maps_->all()[i]==&map)return i;return maps_->size();}
ZoneMobs* MobPrimaryZones::primary_zone(const ConfiguredMap& map)const noexcept{
    std::lock_guard lock(mutex_);const auto id=index(map);if(id==maps_->size())return nullptr;if(!ready_ || !life_[id].primary || life_[id].stage>=2)return nullptr;auto* z=actual(id);return z?z->mobs.get():nullptr;
}
MobPrimaryZoneLease MobPrimaryZones::lease(std::size_t id){std::lock_guard lock(mutex_);auto* z=actual(id);if(!z || life_[id].closing || life_[id].stage>=2)return {};if(!z->mobs)throw std::logic_error("Live primary lost mob owner");++life_[id].leases;return {shared_from_this(),id,life_[id].generation};}
void MobPrimaryZones::release(std::size_t id)noexcept{std::lock_guard lock(mutex_);if(life_[id].leases==0)std::terminate();--life_[id].leases;}
void MobPrimaryZones::visit(std::size_t id,std::uint64_t generation,const std::function<void(RuntimeZone&)>& fn){
    std::lock_guard lock(mutex_);if(!fn || life_.at(id).generation!=generation || !life_[id].leases || life_[id].stage>=2)throw std::logic_error("Invalid primary zone lease visit");auto* z=actual(id);if(!z || !z->mobs)throw std::logic_error("Primary lease lost actual owner");Dispatch frame(dispatch_);fn(*z);
}
bool MobPrimaryZones::startup(std::size_t id){auto held=lease(id);if(!held)return false;bool result=false;held.visit([&](RuntimeZone&){attached();result=obelisks_->startup(id);});return result;}
bool MobPrimaryZones::tick(std::size_t id,std::uint32_t now){auto held=lease(id);if(!held)return false;bool result=false;held.visit([&](RuntimeZone&){attached();result=obelisks_->tick(id,now);});return result;}
bool MobPrimaryZones::reset(std::size_t id,const std::function<std::uint32_t()>& tick){auto held=lease(id);if(!held)return false;bool result=false;held.visit([&](RuntimeZone&){attached();result=obelisks_->reset(id,tick);});return result;}
MobPrimaryZoneClose MobPrimaryZones::drain(std::size_t id,const std::function<std::uint32_t()>& tick){
    auto retained=shared_from_this();
    std::lock_guard lock(mutex_);auto* zone=actual(id);if(!zone)return MobPrimaryZoneClose::absent;SyncMaps sync{records_.get(),id};auto& v=life_[id];if(v.stage==3)return MobPrimaryZoneClose::closed;v.closing=true;
    if(v.leases || dispatch_)return MobPrimaryZoneClose::deferred;
    Dispatch frame(dispatch_);
    if(v.stage==0){if(obelisks_ && !obelisks_->reset(id,tick))throw std::logic_error("Primary drain lost live obelisk route");zone->release_mob_population(*pool_);v.stage=1;}
    if(v.stage==1){if(obelisks_)obelisks_->detach_primary(id);v.stage=2;}
    if(v.stage==2){
        // Recycled mobs retain raw respawn pointers. Keep the original source
        // objects (not copies) beyond every pool actor's final destruction.
        if(zone->mob_population)retired_population_.push_back(std::move(zone->mob_population));
        if(zone->linked_mobs)retired_linked_.push_back(std::move(zone->linked_mobs));
        services_->release_objects(*zone);if(zone->mobs && zone->mobs->size()!=0)throw std::logic_error("Backend release left registered primary mobs");zone->mobs.reset();v.stage=3;}
    if(records_)static_cast<void>(records_->runtime_map(id));
    return MobPrimaryZoneClose::closed;
}
bool MobPrimaryZones::drain_all(const std::function<std::uint32_t()>& tick){auto retained=shared_from_this();bool done=true;for(std::size_t i=0;i<maps_->size();++i)if(drain(i,tick)==MobPrimaryZoneClose::deferred)done=false;return done;}
bool MobPrimaryZones::reopen(std::size_t id){
    auto retained=shared_from_this();
    std::lock_guard lock(mutex_);auto* zone=actual(id);if(!zone)return false;SyncMaps sync{records_.get(),id};auto& v=life_[id];if(v.stage!=3 || v.leases || dispatch_)throw std::logic_error("Reopen requires completed primary drain and quiescence");if(v.generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Primary generation exhausted");
    v.stage=2;Dispatch frame(dispatch_);if(!services_->initialize_objects(*zone))return false;if(zone->map!=&maps_->all()[id] || !zone->mobs)throw std::logic_error("Backend reopen omitted actual primary owner");++v.generation;v.stage=0;v.closing=false;if(records_)static_cast<void>(records_->runtime_map(id));
    return true;
}
void MobPrimaryZones::serialized(const std::function<void()>& fn){auto retained=shared_from_this();std::lock_guard lock(mutex_);if(!fn)throw std::invalid_argument("Missing serialized operation");Dispatch frame(dispatch_);fn();}
std::uint32_t MobPrimaryZones::serial()const{std::lock_guard lock(mutex_);return registry_->serial();}
void MobPrimaryZones::database(const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);Dispatch frame(dispatch_);transport_.database(p);}
void MobPrimaryZones::world_broadcast(const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);Dispatch frame(dispatch_);transport_.world(p);}
void MobPrimaryZones::map_broadcast(const ConfiguredMap& m,ZoneMobs& zone,const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);if(primary_zone(m)!=&zone)throw std::logic_error("Map publication is not current primary");Dispatch frame(dispatch_);transport_.map(m,zone,p);}
}
