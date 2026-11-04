// Reuse existing original-format maps/catalog/bound-pool fixtures unchanged.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main mob_primary_zones_previous_suite
#include "mob_primary_zones_unit.cpp"
#undef main
#pragma GCC diagnostic pop
#include "eternity/game/mob_zone_services.hpp"
namespace {
struct BoundMobs:ZoneMobServices {
    std::shared_ptr<Frame> frame;RuntimeZone& zone;
    BoundMobs(std::shared_ptr<Frame> f,RuntimeZone& z):frame(std::move(f)),zone(z){}
    std::uint32_t tick_ms()override{return frame->backend.tick_ms();}
    void send_cell_view(CellCoordinates cell,const eternity::net::Packet& p)override{check(zone.map && zone.id && zone.mobs);frame->backend.send_cell_view(cell,p);}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet& p,std::optional<std::uint8_t> priority)override{return frame->backend.allocate_broadcast(p,priority);}
    void lua_init(MobRuntime& m)override{frame->backend.lua_init(m);}
};
struct OtherServices:ZoneNpcServices,ZoneItemServices {
    NpcPool npc_pool;CorpsePool corpse_pool;WorldCorpses corpses;unsigned npc_ids=0;
    Npc* allocate()override{return npc_pool.allocate();}
    std::uint32_t next_id()override{return ++npc_ids;}
    std::uint32_t tick_ms()const noexcept override{return 77;}
    std::uint32_t tick_ms()override{return 77;}
    void invalid_guard_position(const MapNpc&)override{throw std::logic_error("Unexpected guard");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&)override{throw std::logic_error("Unexpected nonmob broadcast");}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::logic_error("Unexpected NPC buffer");}
    void release(GroundItem&)override{throw std::logic_error("Unexpected item release");}
};
// Corpse and NPC both use allocate() with incompatible results: separate port.
struct CorpseServices:ZoneCorpseServices {
    CorpsePool pool;unsigned packets=0;
    Corpse* allocate()override{return pool.allocate();}
    void release(Corpse& c)override{pool.release(c);}
    void diagnostic(CorpseIssue,std::uint32_t)override{throw std::logic_error("Unexpected corpse diagnostic");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p)override{check(p.size()==6 && p[0]==6 && p[1]==4);++packets;}
};
}
int main(int argc,char** argv){try{
    check(argc==2);write_maps(argv[1]);
    for(unsigned failure=0;failure<6;++failure){
        auto maps=std::make_shared<ServerMaps>(ServerMaps::load(MapConfiguration::decode("[BASE]\nMapCount=2\n"),argv[1]));
        auto frame=std::make_shared<Frame>(maps);
        if(failure==3 || failure==4){eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};rows.rows.push_back({"1","1",failure==3?"10":"100","1","0","0","0","0","0","0","0","0","0","0","0"});frame->runtime=MobRuntimeTemplates::load(rows);}
        auto other=std::make_shared<OtherServices>();auto corpses=std::make_shared<CorpseServices>();NpcDefinitions npcs;
        LinkedMobGroup group;group.leader.center={15,0,15};group.leader.count=1;group.leader.definitions[0]=frame->runtime.find(1);group.leader.respawn.count=1;group.leader.respawn.both=true;
        group.spawn_count=1;group.spawns[0].count=1;group.spawns[0].definitions[0]=frame->runtime.find(2);group.spawns[0].center={16,0,16};group.spawns[0].radius_twice=failure==1?0:1;group.spawns[0].respawn.count=1;group.spawns[0].respawn.both=true;
        unsigned links=0,unix_calls=0;
        auto inputs=MobZoneServicesInputs{frame,*frame->pool,frame->runtime,npcs,frame->init,other->corpses,
            [frame,other,corpses,failure](RuntimeZone& z){check(z.map && z.id);if(failure==5)return MobZoneServicesPorts{};return MobZoneServicesPorts{std::make_shared<BoundMobs>(frame,z),other,corpses,other};},
            [&](const ConfiguredMap&)->std::span<const LinkedMobGroup>{++links;if(failure==2)throw std::runtime_error("Missing linked configuration provider");return {&group,1};},
            [&]{++unix_calls;return 123456U;},[other](Npc& n){other->npc_pool.release(n);},[](RuntimeZone&){throw std::logic_error("Unexpected instance queue");},[]{return ZoneLocalTime{1,2,3,4};}};
        auto services=MobZoneServices::create(inputs);auto root=MobPrimaryZones::create(maps,services,std::move(frame->pool),MobPrimaryZoneTransport{[&](const eternity::net::Packet& p){check(p.size()>=2);++frame->stats->db;},[&](const eternity::net::Packet& p){check(p.size()>=2);++frame->stats->world;},[&](const ConfiguredMap&,ZoneMobs&,const eternity::net::Packet& p){check(p.size()>=2);++frame->stats->map;}});
        if(failure){bool rejected=false;try{root->initialize();}catch(const std::exception&){rejected=true;}check(rejected && root->mob_pool().available()==root->mob_pool().size());root.reset();check(links==(failure==5?0U:1U));continue;}
        check(root->initialize() && links==2 && unix_calls==2);auto l=root->lease(0);l.visit([&](RuntimeZone& z){check(z.environment && z.users && z.npcs && z.items && z.corpses && z.mobs->size()==3);check(z.linked_mobs->groups()[0].leader.members[0]->respawn==&z.linked_mobs->groups()[0].leader.respawn);});l.reset();
        attach(frame,root);check(root->startup(0) && root->primary_zone(maps->all()[0])->size()==5);
        check(root->tick(0,5077));RuntimeZone* live=nullptr;Corpse* body=corpses->allocate();body->id=42;body->position={10,0,10};auto held=root->lease(0);held.visit([&](RuntimeZone& z){live=&z;check(z.corpses->enter(*body));});held.reset();
        bool busy=false;try{root->drain(0,[]{return 99U;});}catch(const std::logic_error&){busy=true;}check(busy && !root->lease(0));root->serialized([&]{check(live->corpses->leave(*body));});check(corpses->packets==1 && other->corpses.size()==0);check(root->drain(0,[]{return 99U;})==MobPrimaryZoneClose::closed);check(root->reopen(0) && links==3 && unix_calls==3 && root->primary_zone(maps->all()[0])->size()==3);check(root->startup(0));check(root->drain_all([]{return 100U;}));check(root->mob_pool().available()==root->mob_pool().size());
        bool rejected=false;inputs.unix_time={};try{MobZoneServices::create(inputs);}catch(const std::invalid_argument&){rejected=true;}check(rejected);root.reset();
    }
    std::cout<<"Concrete factory regular/linked/obelisk lifecycle and partial failure compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
