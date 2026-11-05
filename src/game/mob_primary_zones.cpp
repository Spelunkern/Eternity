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
MobInstanceZoneLease::MobInstanceZoneLease(std::shared_ptr<MobPrimaryZones> owner,RuntimeZone* zone,std::uint64_t generation)
    :owner_(std::move(owner)),zone_(zone),generation_(generation){}
MobInstanceZoneLease::~MobInstanceZoneLease(){reset();}
MobInstanceZoneLease::MobInstanceZoneLease(MobInstanceZoneLease&& other)noexcept
    :owner_(std::move(other.owner_)),zone_(other.zone_),generation_(other.generation_){other.zone_=nullptr;}
MobInstanceZoneLease& MobInstanceZoneLease::operator=(MobInstanceZoneLease&& other)noexcept {
    if(this!=&other){reset();owner_=std::move(other.owner_);zone_=other.zone_;generation_=other.generation_;other.zone_=nullptr;}return *this;
}
void MobInstanceZoneLease::reset()noexcept {if(owner_){auto retained=std::move(owner_);auto* zone=zone_;zone_=nullptr;retained->release_instance(zone,generation_);}}
void MobInstanceZoneLease::visit(const std::function<void(RuntimeZone&)>& fn)const {
    if(!owner_)throw std::logic_error("Empty instance zone lease");auto retained=owner_;retained->visit_instance(zone_,generation_,fn);
}
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
    if(ready_ && !instances_.empty())std::terminate();
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
MobPrimaryZoneLease MobPrimaryZones::lease_primary(RuntimeZone& zone) {
    std::lock_guard lock(mutex_);
    if(!ready_)throw std::logic_error("Primary zone authority is not initialized");
    if(!zone.map)throw std::logic_error("Active primary has no map");
    const auto id=index(*zone.map);
    if(id<life_.size() && life_[id].primary && actual(id)==&zone)return lease(id);
    throw std::logic_error("Active primary is not owned by this authority");
}
MobInstanceZoneLease MobPrimaryZones::lease_instance(RuntimeZone& zone) {
    auto retained=shared_from_this();std::lock_guard lock(mutex_);
    if(stopping_)return {};
    auto it=instances_.find(&zone);
    if(it==instances_.end() || retiring_instance_==&zone)throw std::logic_error("Active instance is not owned by this authority");
    ++it->second.leases;return {std::move(retained),&zone,it->second.generation};
}
MobInstanceZoneLease MobPrimaryZones::select_instance(std::uint16_t id,const ZoneUserAccess& user) {
    auto retained=shared_from_this();std::lock_guard lock(mutex_);
    if(!ready_ || stopping_)throw std::logic_error("Instance selection requires a running zone authority");
    const auto* map=maps_->find(id);if(!map || map->settings.create_type==0)return {};
    if(retiring_instance_)throw std::logic_error("Instance retirement must finish before selection");
    if(instance_generation_==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Instance generation exhausted");
    // Allocate tracking storage before creation/enqueue so an allocation
    // failure cannot strand a published instance without lifetime tracking.
    std::map<RuntimeZone*,InstanceLife> prepared;prepared.emplace(nullptr,InstanceLife{});
    auto node=prepared.extract(prepared.begin());
    Dispatch frame(dispatch_);auto* zone=registry_->select(id,user);if(!zone)return {};
    auto it=instances_.find(zone);
    if(it==instances_.end()) {
        node.key()=zone;node.mapped().generation=++instance_generation_;
        it=instances_.insert(std::move(node)).position;
    }
    ++it->second.leases;return {std::move(retained),zone,it->second.generation};
}
void MobPrimaryZones::release_instance(RuntimeZone* zone,std::uint64_t generation)noexcept {
    std::lock_guard lock(mutex_);auto it=instances_.find(zone);
    if(it==instances_.end() || it->second.generation!=generation || !it->second.leases)std::terminate();
    --it->second.leases;
}
void MobPrimaryZones::visit_instance(RuntimeZone* zone,std::uint64_t generation,const std::function<void(RuntimeZone&)>& fn) {
    std::lock_guard lock(mutex_);auto it=instances_.find(zone);
    if(!fn || it==instances_.end() || it->second.generation!=generation || !it->second.leases || retiring_instance_==zone)
        throw std::logic_error("Invalid instance zone lease visit");
    struct Visit {std::size_t& count;explicit Visit(std::size_t& n):count(n){++count;}~Visit(){--count;}} visit(it->second.visits);
    Dispatch frame(dispatch_);fn(*zone);
}
MobInstanceExpiry MobPrimaryZones::expire_instance(RuntimeZone& zone,WorldAdmission& admission,std::uint32_t now,ZoneActiveListRemoval& active) {
    auto retained=shared_from_this();std::lock_guard lock(mutex_);auto it=instances_.find(&zone);
    if(it==instances_.end())throw std::logic_error("Instance is not retained by this authority");
    if(retiring_instance_ && retiring_instance_!=&zone)throw std::logic_error("Another instance retirement requires retry");
    if(it->second.leases || it->second.visits)return MobInstanceExpiry::deferred;
    struct Removal final:ZoneActiveListRemoval {
        ZoneActiveListRemoval& real;RuntimeZone*& retiring;
        Removal(ZoneActiveListRemoval& list,RuntimeZone*& value):real(list),retiring(value){}
        void remove_active(RuntimeZone& current)noexcept override {real.remove_active(current);retiring=&current;}
    } removal(active,retiring_instance_);
    Dispatch frame(dispatch_);const auto result=registry_->expire_instance(zone,admission,now,removal);
    switch(result) {
        case InstanceExpiryResult::occupied:return MobInstanceExpiry::occupied;
        case InstanceExpiryResult::waiting:return MobInstanceExpiry::waiting;
        case InstanceExpiryResult::retired:instances_.erase(it);retiring_instance_=nullptr;return MobInstanceExpiry::retired;
    }
    std::terminate();
}
MobInstanceExpiry MobPrimaryZones::retry_instance_retirement(WorldAdmission& admission,std::uint32_t now,ZoneActiveListRemoval& active) {
    std::lock_guard lock(mutex_);if(!retiring_instance_)throw std::logic_error("No instance retirement requires retry");
    return expire_instance(*retiring_instance_,admission,now,active);
}
void MobPrimaryZones::visit_retiring_instance(const std::function<void(RuntimeZone&)>& fn) {
    auto retained=shared_from_this();std::lock_guard lock(mutex_);
    if(!retiring_instance_ || !fn)throw std::logic_error("No retiring instance or visitor");
    auto it=instances_.find(retiring_instance_);if(it==instances_.end())throw std::logic_error("Retiring instance lost its owner");
    struct Visit {std::size_t& count;explicit Visit(std::size_t& n):count(n){++count;}~Visit(){--count;}} visit(it->second.visits);
    Dispatch frame(dispatch_);fn(*retiring_instance_);
}
bool MobPrimaryZones::has_retiring_instance()const {std::lock_guard lock(mutex_);return retiring_instance_!=nullptr;}
std::size_t MobPrimaryZones::instance_count()const {std::lock_guard lock(mutex_);return instances_.size();}
void MobPrimaryZones::visit_logout_zones(const std::function<void(RuntimeZone&)>& visit) {
    if(!visit) throw std::invalid_argument("Logout zone visitor is required");
    auto retained=shared_from_this();std::lock_guard lock(mutex_);
    if(!ready_ || stopping_ || retiring_instance_) throw std::logic_error("Logout traversal requires live zone owners");
    struct Pinned {MobPrimaryZoneLease primary;MobInstanceZoneLease instance;};
    std::vector<Pinned> pinned;
    for(auto* zone:registry_->logout_snapshot()) {
        if(!zone || !zone->map) throw std::logic_error("Logout registry lost a zone owner");
        if(zone->map->settings.create_type==0) {
            auto lease=lease_primary(*zone);if(!lease) throw std::logic_error("Logout primary is closing");
            pinned.push_back({std::move(lease),{}});
        } else {
            auto lease=lease_instance(*zone);if(!lease) throw std::logic_error("Logout instance is closing");
            pinned.push_back({{},std::move(lease)});
        }
    }
    for(auto& pin:pinned) {
        if(pin.primary) pin.primary.visit(visit);else pin.instance.visit(visit);
    }
}
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
bool MobPrimaryZones::drain_all(const std::function<std::uint32_t()>& tick){auto retained=shared_from_this();{std::lock_guard lock(mutex_);stopping_=true;if(!instances_.empty())return false;}bool done=true;for(std::size_t i=0;i<maps_->size();++i)if(drain(i,tick)==MobPrimaryZoneClose::deferred)done=false;return done;}
bool MobPrimaryZones::reopen(std::size_t id){
    auto retained=shared_from_this();
    std::lock_guard lock(mutex_);auto* zone=actual(id);if(!zone)return false;SyncMaps sync{records_.get(),id};auto& v=life_[id];if(v.stage!=3 || v.leases || dispatch_)throw std::logic_error("Reopen requires completed primary drain and quiescence");if(v.generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Primary generation exhausted");
    v.stage=2;Dispatch frame(dispatch_);if(!services_->initialize_objects(*zone))return false;if(zone->map!=&maps_->all()[id] || !zone->mobs)throw std::logic_error("Backend reopen omitted actual primary owner");++v.generation;v.stage=0;v.closing=false;if(records_)static_cast<void>(records_->runtime_map(id));
    return true;
}
void MobPrimaryZones::serialized(const std::function<void()>& fn){auto retained=shared_from_this();std::lock_guard lock(mutex_);if(!fn)throw std::invalid_argument("Missing serialized operation");Dispatch frame(dispatch_);fn();}
std::uint32_t MobPrimaryZones::serial()const{std::lock_guard lock(mutex_);return registry_->serial();}
std::size_t MobPrimaryZones::map_index(const ConfiguredMap& map)const{std::lock_guard lock(mutex_);const auto id=index(map);if(id==maps_->size())throw std::logic_error("Map is not owned by this authority");return id;}
void MobPrimaryZones::database(const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);Dispatch frame(dispatch_);transport_.database(p);}
void MobPrimaryZones::world_broadcast(const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);Dispatch frame(dispatch_);transport_.world(p);}
void MobPrimaryZones::map_broadcast(const ConfiguredMap& m,ZoneMobs& zone,const net::Packet& p){auto retained=shared_from_this();std::lock_guard lock(mutex_);if(primary_zone(m)!=&zone)throw std::logic_error("Map publication is not current primary");Dispatch frame(dispatch_);transport_.map(m,zone,p);}
}
