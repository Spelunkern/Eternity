#include "eternity/game/mob_zone_services.hpp"
#include <stdexcept>
namespace eternity::game {
struct MobZoneServices::Zone final:RuntimeZone {
    std::shared_ptr<MobZoneServices> owner;
    bool prepared=false,active=false;
    std::optional<MapGeometry> geometry;
    MobZoneServicesPorts ports;
    explicit Zone(std::shared_ptr<MobZoneServices> p):owner(std::move(p)){}
    ~Zone()override{owner->destroy(*this);} // Partial Init uses the SAME cleanup.
};
std::shared_ptr<MobZoneServices> MobZoneServices::create(MobZoneServicesInputs in){return std::shared_ptr<MobZoneServices>(new MobZoneServices(std::move(in)));}
MobZoneServices::MobZoneServices(MobZoneServicesInputs in):in_(std::move(in)){
    if(!in_.lifetime || !in_.pool.has_applied_skills_runtime() || !in_.init.tick || !in_.init.compiled_ai || !in_.init.lua_ai || !in_.bind || !in_.linked || !in_.unix_time || !in_.release_npc || !in_.enqueue_wait || !in_.local_time)throw std::invalid_argument("Zone factory requires retained bound owners and real providers");
}
MobZoneServices::Zone& MobZoneServices::owned(RuntimeZone& z){auto* p=dynamic_cast<Zone*>(&z);if(!p || p->owner.get()!=this)throw std::invalid_argument("Foreign runtime zone");return *p;}
std::unique_ptr<RuntimeZone> MobZoneServices::allocate(){return std::make_unique<Zone>(shared_from_this());}
bool MobZoneServices::initialize_map(RuntimeZone& raw,const ConfiguredMap& map){
    auto& z=owned(raw);if(z.prepared || z.map!=&map)throw std::logic_error("Zone map initialization identity/order");
    z.ports=in_.bind(z);
    if(!z.ports.mobs || !z.ports.npcs || !z.ports.corpses || !z.ports.items)throw std::invalid_argument("Missing actual per-zone service binding");
    z.geometry=map.definition.geometry();const auto& geometry=*z.geometry;
    z.environment=ZoneEnvironment::prepare(map);
    z.mob_population=std::make_unique<MobPopulation>(map.definition,in_.templates);
    z.linked_mobs=std::make_unique<LinkedMobPopulation>(in_.linked(map));
    z.users=std::make_unique<ZoneUsers>(geometry,map.settings.war_type);
    z.npcs=std::make_unique<ZoneNpcs>(geometry,*z.ports.npcs);
    z.corpses=std::make_unique<ZoneCorpses>(geometry,in_.world_corpses,*z.ports.corpses);
    z.items=std::make_unique<ZoneItems>(geometry,*z.ports.items);
    z.prepared=true;
    z.npcs->initialize(map.definition,in_.npc_definitions);
    return true;
}
void MobZoneServices::retain(RuntimeZone& z){if(z.mob_population)regular_.push_back(std::move(z.mob_population));if(z.linked_mobs)linked_.push_back(std::move(z.linked_mobs));}
bool MobZoneServices::initialize_objects(RuntimeZone& raw){
    auto& z=owned(raw);if(!z.prepared || z.active || z.mobs)throw std::logic_error("Zone object initialization order");
    z.active=true; // Mark before allocations/callbacks so partial failures clean up.
    z.mobs=std::make_unique<ZoneMobs>(z.map->definition.geometry(),*z.ports.mobs);
    if(!z.mob_population)z.mob_population=std::make_unique<MobPopulation>(z.map->definition,in_.templates);
    if(!z.linked_mobs)z.linked_mobs=std::make_unique<LinkedMobPopulation>(in_.linked(*z.map));
    // Init callbacks precede EnterMob. On backend failure the subject is not
    // registered yet; recycle it explicitly, then zone RAII handles prior actors.
    MobInitPorts ports{in_.init.random,in_.init.tick,
        [&](MobRuntime& m,std::uint8_t ai){try{return in_.init.compiled_ai(m,ai);}catch(...){in_.pool.release(m);throw;}},
        [&](MobRuntime& m){try{in_.init.lua_ai(m);}catch(...){in_.pool.release(m);throw;}}};
    return z.mob_population->initialize(*z.mobs,in_.pool,ports,[&]{return z.linked_mobs->initialize(z.map->definition,*z.mobs,in_.pool,ports,in_.unix_time);});
}
void MobZoneServices::release_objects(RuntimeZone& raw){
    auto& z=owned(raw);
    if((z.users && z.users->size()) || (z.corpses && z.corpses->size()) || (z.items && z.items->size()))throw std::logic_error("Drain real users/corpses/items before zone release");
    if(z.mobs && z.mobs->size())z.release_mob_population(in_.pool);
    z.instance_portals.fill({});
    retain(z);z.mobs.reset();z.active=false;
}
void MobZoneServices::destroy(Zone& z){
    release_objects(z);
    if(z.npcs){while(!z.npcs->all().empty()){auto* npc=z.npcs->all().front();if(!z.npcs->leave(*npc))throw std::logic_error("NPC cleanup lost membership");in_.release_npc(*npc);}}
    z.items.reset();z.corpses.reset();z.npcs.reset();z.users.reset();
}
void MobZoneServices::enqueue_wait(RuntimeZone& z){owned(z);in_.enqueue_wait(z);}
ZoneLocalTime MobZoneServices::local_time(){return in_.local_time();}
}
