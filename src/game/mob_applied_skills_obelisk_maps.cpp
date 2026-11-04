#include "eternity/game/mob_applied_skills_obelisk_maps.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
struct RefreshMaps {
    ObeliskRuntimeMaps& owner;std::size_t id;
    ~RefreshMaps(){static_cast<void>(owner.runtime_map(id));}
};
}

// Concrete lifecycle guard, not another backend interface. Native coordinators
// can call again after arbitrary callbacks; every actor operation rereads the
// primary route before invoking the existing actual pool/Init/zone adapter.
struct MobAppliedSkillsObeliskMaps::Binding final:ObeliskRuntimeWorld {
    MobAppliedSkillsObeliskMaps& root;std::size_t id;ZoneMobs& zone;
    MobAppliedSkillsObeliskServices ports;std::unique_ptr<MobAppliedSkillsObelisk> actors;
    std::unique_ptr<ObeliskRuntimeZoneRemoval> removal;std::unique_ptr<ObeliskRuntimeDeathOwnership> death;
    Binding(MobAppliedSkillsObeliskMaps& r,std::size_t i,ZoneMobs& z):root(r),id(i),zone(z){
        ports.mob_name=[this](const MobRuntime& m){
            if(!m.definition || root.templates_.find(m.definition->id)!=m.definition || !root.catalog_.mob(m.definition->id))throw std::logic_error("Obelisk name requires coherent retained definition owners");
            const auto bytes=root.catalog_.mob_record(m.definition->id);const auto* text=reinterpret_cast<const char*>(bytes.data()+2);std::size_t n=0;while(n<32 && text[n])++n;return std::string_view(text,n);
        };
        ports.remove_by_id=[this](std::uint32_t actor){live();removal->remove_by_id(actor);};
        ports.send_change=[this](std::size_t record){root.owner_.publish_change(id,record);};
        ports.add_kill_count=[this](auto c,auto n){root.services_.add_kill_count(c,n);};
        ports.subtract_kill_count=[this](auto c,auto n){root.services_.subtract_kill_count(c,n);};
        ports.kill_count_status14=[this](auto c){return root.services_.kill_count_status14(c);};
        ports.broadcast=[this](const net::Packet& p){root.owner_.broadcast(p);};
        ports.game_log=[this](const net::Packet& p){root.services_.game_log(p);};
        ports.database=[this](const net::Packet& p){root.owner_.database(p);};
        ports.update_portal_links=[this]{root.owner_.update_portals();};
        actors=std::make_unique<MobAppliedSkillsObelisk>(root.pool_,zone,root.init_,ports);
        removal=std::make_unique<ObeliskRuntimeZoneRemoval>(root.owner_.runtime_map(id),root.owner_.records(id),zone,root.pool_,*this,root.kill_count_add_,root.services_.linked);
        death=std::make_unique<ObeliskRuntimeDeathOwnership>(root.owner_.runtime_map(id),root.owner_.records(id),zone,*this,root.kill_count_add_,*removal);
    }
    void live()const{if(root.routes_.primary_zone(root.configured(id))!=&zone)throw std::logic_error("Primary zone changed during retained obelisk operation");}
    MobRuntime* allocate(bool manager)override{live();return actors->allocate(manager);}
    void prepare_companion(MobRuntime& m)override{live();actors->prepare_companion(m);}
    void initialize(MobRuntime& m,const MobRuntimeTemplate& d,MobRespawn& s)override{live();actors->initialize(m,d,s);}
    void enter(MobRuntime& m)override{live();actors->enter(m);}
    std::int32_t random()override{live();return actors->random();}
    std::string_view mob_name(const MobRuntime& m)override{return actors->mob_name(m);}
    void remove_by_id(std::uint32_t actor)override{actors->remove_by_id(actor);}
    void send_change(std::size_t record)override{actors->send_change(record);}
    void add_kill_count(std::int32_t c,std::int32_t n)override{actors->add_kill_count(c,n);}
    bool kill_count_status14(std::int32_t c)override{return actors->kill_count_status14(c);}
    void subtract_kill_count(std::int32_t c,std::int32_t n)override{actors->subtract_kill_count(c,n);}
    void broadcast(const net::Packet& p)override{actors->broadcast(p);}
    void game_log(const net::Packet& p)override{actors->game_log(p);}
    void database(const net::Packet& p)override{actors->database(p);}
    void update_portal_links()override{actors->update_portal_links();}
};
MobAppliedSkillsObeliskMaps::MobAppliedSkillsObeliskMaps(ServerMaps& maps,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog,ObeliskRuntimeMaps& owner,ObeliskRuntimeMapsServices& routes,MobPool& pool,MobInitPorts& init,const std::array<std::int32_t,2>& kc,MobAppliedSkillsObeliskMapsServices& services)
    :maps_(maps),templates_(templates),catalog_(catalog),owner_(owner),routes_(routes),pool_(pool),init_(init),kill_count_add_(kc),services_(services),bindings_(maps.size()){
    if(!pool.has_applied_skills_runtime() || !init.tick || !init.compiled_ai || !init.lua_ai || !services.add_kill_count || !services.subtract_kill_count || !services.kill_count_status14 || !services.game_log)throw std::invalid_argument("Obelisk maps require bound actual owners and complete backend services");
    for(std::size_t i=0;i<maps.size();++i){const auto& m=configured(i);const auto& view=owner.runtime_map(i);if(view.size!=m.definition.size || view.blocked_tiles.data()!=m.definition.tiles.data())throw std::invalid_argument("Obelisk configuration is not bound to these ServerMaps");}
}
MobAppliedSkillsObeliskMaps::~MobAppliedSkillsObeliskMaps()=default;
const ConfiguredMap& MobAppliedSkillsObeliskMaps::configured(std::size_t id)const{if(id>=maps_.size())throw std::out_of_range("Obelisk map outside retained ServerMaps");return maps_.all()[id];}
MobAppliedSkillsObeliskMaps::Binding* MobAppliedSkillsObeliskMaps::binding(std::size_t id){
    auto* zone=routes_.primary_zone(configured(id));if(!zone)return nullptr;
    auto& value=bindings_.at(id);if(value && &value->zone!=zone)throw std::logic_error("Detach old primary binding before zone replacement");
    if(!value)value=std::make_unique<Binding>(*this,id,*zone);
    return value.get();
}
bool MobAppliedSkillsObeliskMaps::startup(std::size_t id){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)return false;initialize_obelisks(owner_.runtime_map(id),owner_.records(id),*b);return true;}
bool MobAppliedSkillsObeliskMaps::tick(std::size_t id,std::uint32_t now){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)return false;update_obelisk_respawns(owner_.runtime_map(id),owner_.records(id),now,*b);return true;}
bool MobAppliedSkillsObeliskMaps::reset(std::size_t id,const std::function<std::uint32_t()>& clock){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)return false;owner_.reset(id,clock,*b);return true;}
bool MobAppliedSkillsObeliskMaps::remove(std::size_t id,std::uint32_t actor){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);return b && b->removal->remove_by_id(actor);}
std::optional<std::int32_t> MobAppliedSkillsObeliskMaps::death_country(std::size_t id,MobRuntime& m,std::int32_t type){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)throw std::logic_error("Death requires retained primary zone");return b->death->country(m,type);}
void MobAppliedSkillsObeliskMaps::destroyed(std::size_t id,MobRuntime& m,std::int32_t country,std::int32_t type){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)throw std::logic_error("Death requires retained primary zone");b->death->destroyed(m,country,type);}
void MobAppliedSkillsObeliskMaps::remove_member(std::size_t id,MobRuntime& m,void* source){configured(id);RefreshMaps refresh{owner_,id};auto* b=binding(id);if(!b)throw std::logic_error("Member removal requires retained primary zone");b->death->remove_member(m,source);}
void MobAppliedSkillsObeliskMaps::detach_primary(std::size_t id){configured(id);auto& b=bindings_.at(id);if(!b)return;b->live();if(b->zone.size()!=0)throw std::logic_error("Drain primary zone before detaching obelisk bindings");for(const auto& o:owner_.records(id)){if(o.principal)throw std::logic_error("Principal still references primary zone");for(const auto& s:o.spawns)for(auto* m:s.members)if(m)throw std::logic_error("Member still references primary zone");}b.reset();}
}
