#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "mob_damage_fixture.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>
using namespace eternity::game;
namespace {
void check(bool b,std::source_location at=std::source_location::current()){if(!b){std::cerr<<"Primary authority mismatch at "<<at.line()<<'\n';throw std::runtime_error("Primary authority mismatch at "+std::to_string(at.line()));}}
void put(std::vector<std::uint8_t>& out,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i) out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void real(std::vector<std::uint8_t>& out,float f) {put(out,std::bit_cast<std::uint32_t>(f));}
void write_maps(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::vector<std::uint8_t> bytes;put(bytes,81);bytes.resize(bytes.size()+(81*81+7)/8);
    put(bytes,0);put(bytes,0);put(bytes,1);for(float n:{10.F,0.F,10.F,11.F,0.F,11.F})real(bytes,n);put(bytes,1);put(bytes,1);put(bytes,1);put(bytes,0);put(bytes,3);
    for(unsigned j=0;j<3;++j) {real(bytes,20.25F+static_cast<float>(j)*10);real(bytes,99);real(bytes,30.5F+static_cast<float>(j)*10);put(bytes,j==2?1002:1001);put(bytes,0,2);put(bytes,60,2);put(bytes,0);for(unsigned i=0;i<3;++i) real(bytes,0);}
    put(bytes,0);put(bytes,0);
    for(unsigned i=0;i<2;++i) {std::ofstream out(root/(std::to_string(i)+".svmap"),std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
}
MobRuntimeTemplates templates() {
    eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};
    for(unsigned i=1;i<4;++i) rows.rows.push_back({std::to_string(i),i%7==0?"0":"1","0","1","0","0","0","0","0","0","0","0","0","0","0"});return MobRuntimeTemplates::load(rows);
}
MobTargetingBindingCatalog catalog() {
    eternity::storage::ResultSet rows,skills;rows.columns.resize(69);skills.columns.resize(82);
    for(unsigned i=1;i<4;++i) {eternity::storage::Row row(69,std::string("0"));row[0]=std::to_string(i);row[1]="Fixture";row[2]=i%7==0?"0":"1";row[8]="1";rows.rows.push_back(std::move(row));}
    eternity::storage::Row skill(82,std::string("0"));skill[0]="1";skill[2]="S";skill[16]="3";skill[17]="99";skills.rows.push_back(skill);
    return MobTargetingBindingCatalog::load(3,1,rows,skills);
}

struct SkillFactory:MobAppliedSkillPoolServices{std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct Stats{unsigned zones=0,destroyed=0,objects=0,released=0,ai=0,binds=0,db=0,world=0,map=0,entries=0;};
struct Zone final:RuntimeZone{std::shared_ptr<Stats> stats;std::function<void(RuntimeZone&)> cleanup;Zone(std::shared_ptr<Stats> s,std::function<void(RuntimeZone&)> fn):stats(std::move(s)),cleanup(std::move(fn)){++stats->zones;}~Zone(){cleanup(*this);++stats->destroyed;}};
struct Frame;
struct Backend:ZoneServices,MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,ZoneMobServices,MobRemovalOwners{
    Frame& frame;explicit Backend(Frame& f):frame(f){}
    bool omit_mobs=false,fail_objects=false,fail_release=false;std::function<void()> on_entry;
    std::unique_ptr<RuntimeZone> allocate()override;
    bool initialize_map(RuntimeZone&,const ConfiguredMap&)override;
    bool initialize_objects(RuntimeZone&)override;
    void release_objects(RuntimeZone&)override;
    void enqueue_wait(RuntimeZone&)override{throw std::logic_error("Unexpected dynamic selection");}
    ZoneLocalTime local_time()override{throw std::logic_error("Unexpected local calendar");}
    std::unique_ptr<MobRuntime> construct()override;
    std::uint32_t unix_time()override{return 123456;}
    MobDamagePool& damage_pool()override;
    void release_compiled_ai(MobRuntime&)override;
    void release_stack_ai(MobRuntime&)override{throw std::logic_error("Unexpected VM stack");}
    void close_lua(void*)override{throw std::logic_error("Unexpected VM close");}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override;
    std::uint32_t tick()noexcept override{return 77;}
    void log(const MobAppliedSkill&,std::string_view,std::uint8_t)noexcept override{std::terminate();}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
    std::uint32_t tick_ms()override{return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&)override;
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::logic_error("Unexpected buffer");}
    void lua_init(MobRuntime&)override{throw std::logic_error("Unexpected Lua entry");}
    void destroy_obelisk(MobRuntime&,std::int32_t)override{throw std::logic_error("Unexpected linked delegation");}
    void remove_obelisk_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked delegation");}
    void destroy_linked_group(std::int32_t)override{throw std::logic_error("Unexpected linked group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked member");}
};
struct Frame{
    std::shared_ptr<ServerMaps> maps;MobRuntimeTemplates runtime=templates();MobTargetingBindingCatalog full=catalog();
    std::shared_ptr<Stats> stats=std::make_shared<Stats>();MobDamageFixture damage;SkillFactory skills;MobAppliedSkillPool heap{skills};
    std::atomic<std::uint32_t> skill_ids{0};unsigned mob_ids=100;Backend backend{*this};MobAppliedSkillsRuntime binding{full,heap,skill_ids,backend};
    MicrosoftRandom random{17};MobInitPorts init{random,[]{return 77U;},[](MobRuntime&,std::uint8_t){throw std::logic_error("Unexpected compiled backend");return false;},[](MobRuntime&){throw std::logic_error("Unexpected Lua backend");}};
    ObeliskRuntimeConfigurationState state;MobAppliedSkillsObeliskMapsServices world{[](int,int){throw std::logic_error("Unexpected KC");},[](int,int){throw std::logic_error("Unexpected KC");},[](int)->bool{throw std::logic_error("Unexpected KC");},[](const eternity::net::Packet& p){check(p.size()>=48);},backend};
    ObeliskRuntimeMaps* records=nullptr;MobAppliedSkillsObeliskMaps* router=nullptr;MobPool* pool_view=nullptr;
    std::vector<std::unique_ptr<MobPopulation>> retired_population;
    std::vector<std::unique_ptr<LinkedMobPopulation>> retired_linked;
    std::unique_ptr<MobPool> pool; // Destroy actors BEFORE retained source/services.
    explicit Frame(std::shared_ptr<ServerMaps> m):maps(std::move(m)){pool=std::make_unique<MobPool>(mob_ids,backend,binding);pool_view=pool.get();}
};
std::unique_ptr<RuntimeZone> Backend::allocate(){return std::make_unique<Zone>(frame.stats,[this](RuntimeZone& z){if(z.mobs)release_objects(z);});}
bool Backend::initialize_map(RuntimeZone& z,const ConfiguredMap& map){check(z.map==&map);z.environment=ZoneEnvironment::prepare(map);return true;}
bool Backend::initialize_objects(RuntimeZone& z){++frame.stats->objects;check(z.map);if(!z.environment)z.environment=ZoneEnvironment::prepare(*z.map);if(!omit_mobs)z.mobs=std::make_unique<ZoneMobs>(z.map->definition.geometry(),*this);z.mob_population=std::make_unique<MobPopulation>(z.map->definition,frame.runtime);z.linked_mobs=std::make_unique<LinkedMobPopulation>(std::span<const LinkedMobGroup>{});if(z.mobs)z.mob_population->initialize(*z.mobs,*frame.pool_view,frame.init,[&]{return z.linked_mobs->initialize(z.map->definition,*z.mobs,*frame.pool_view,frame.init,[&]{return unix_time();});});return !fail_objects;}
void Backend::release_objects(RuntimeZone& z){++frame.stats->released;if(fail_release){fail_release=false;throw std::runtime_error("Observed backend release failure");}if(z.mobs && z.mobs->size())z.release_mob_population(*frame.pool_view);
    if(z.mob_population)frame.retired_population.push_back(std::move(z.mob_population));if(z.linked_mobs)frame.retired_linked.push_back(std::move(z.linked_mobs));z.mobs.reset();z.environment.reset();}
std::unique_ptr<MobRuntime> Backend::construct(){auto m=std::make_unique<MobRuntime>();m->drop_random=ServerRandom(unix_time());return m;}
MobDamagePool& Backend::damage_pool(){return frame.damage.pool;}
void Backend::release_compiled_ai(MobRuntime& m){check(m.damage && m.has_applied_skills());if(frame.records)check(frame.records->records(0).size()==1);if(m.respawn)check(m.respawn->count>0);++frame.stats->ai;}
MobAppliedSkillsCatalogPorts& Backend::ports(MobRuntime&){++frame.stats->binds;return *this;}
void Backend::send_cell_view(CellCoordinates,const eternity::net::Packet& p){check(p.size()>=6 && p[1]==6);if(p[0]==1){++frame.stats->entries;if(on_entry)on_entry();}else check(p[0]==2);}
std::shared_ptr<MobPrimaryZones> root_for(const std::shared_ptr<Frame>& f){
    const auto weak=std::weak_ptr<Frame>(f);MobPrimaryZoneTransport transport;
    transport.database=[weak](const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2);++f->stats->db;};
    transport.world=[weak](const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2);++f->stats->world;};
    transport.map=[weak](const ConfiguredMap& m,ZoneMobs&,const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2 && m.settings.create_type==0);++f->stats->map;};
    auto* address=f->pool.get();auto result=MobPrimaryZones::create(f->maps,std::shared_ptr<ZoneServices>(f,&f->backend),std::move(f->pool),std::move(transport));check(&result->mob_pool()==address);return result;
}
void attach(const std::shared_ptr<Frame>& f,const std::shared_ptr<MobPrimaryZones>& root){
    std::string ini="[zone_0]\n[Obelisk_1]=(20,0,20),(1,2,3),1\n[mob]=(30,0,30),1,(1,2,3),1\n[zone_1]\n[Obelisk_1]=(20,0,20),(1,2,3),2\n[mob]=(30,0,30),1,(1,2,3),1\n";
    auto records=bind_obelisk_runtime_maps(*f->maps,f->runtime,f->full,ini,f->state,*root);
    auto router=std::make_unique<MobAppliedSkillsObeliskMaps>(*f->maps,f->runtime,f->full,*records,*root,root->mob_pool(),f->init,f->state.kill_count_add,f->world);
    f->records=records.get();f->router=router.get();root->attach_obelisks(std::move(records),std::move(router));
}
}
#include "eternity/game/vertical_world.hpp"
struct LinkedFixture:LinkedMobWorld {void remove_by_id(std::uint32_t) override {throw std::logic_error("unexpected removal");} void set_portal_country(const MapPortal&,std::int32_t) override {throw std::logic_error("unexpected portal");}};
int main(int argc,char** argv) {try {
    check(argc==2);write_maps(argv[1]);
    auto config=MapConfiguration::decode("[BASE]\nMapCount=2\n");
    auto maps=std::make_shared<ServerMaps>(ServerMaps::load(config,argv[1]));
    auto f=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root;unsigned frames=0;
    MobPrimaryZoneTransport transport{[&](const eternity::net::Packet&){++f->stats->db;},[&](const eternity::net::Packet&){++f->stats->world;},[&](const ConfiguredMap&,ZoneMobs&,const eternity::net::Packet&){++f->stats->map;}};
    LinkedFixture linked;
    VerticalWorldPorts ports{[&](MobPrimaryZones&){attach(f,root);},[&](RuntimeZone& z,std::uint32_t now){
        check(z.mobs && now==5077);std::size_t id=z.map==&maps->all()[0]?0:1;
        if(id==0) check(f->records->records(0)[0].spawns[0].members[0]==nullptr);
        z.update_mob_population(root->mob_pool(),f->init,now,
            [&](std::uint32_t t){check(root->tick(id,t));},
            [&](std::uint32_t t){z.linked_mobs->update(z.map->definition,*z.mobs,root->mob_pool(),f->init,t,linked);});
        ++frames;
    },[]{return 100U;}};
    auto world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f,&f->backend),std::move(f->pool),transport,ports);root=world->zones();check(world->start());root->serialized([&]{auto* mob=f->records->records(0)[0].spawns[0].members[0];check(mob && f->router->remove(0,mob->id));});world->frame(5077);check(frames==2 && f->records->records(0)[0].spawns[0].members[0]);
    auto held=root->lease(0);check(!world->shutdown());held.reset();check(world->shutdown());check(world->shutdown());
    bool rejected=false;try{world->frame(77);}catch(const std::logic_error&){rejected=true;}check(rejected);
    auto f2=std::make_shared<Frame>(maps);
    VerticalWorldPorts fail{[](MobPrimaryZones&){throw std::runtime_error("attach failed");},[](RuntimeZone&,std::uint32_t){throw std::logic_error("unexpected frame");},[]{return 100U;}};
    auto failed=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f2,&f2->backend),std::move(f2->pool),transport,fail);auto root2=failed->zones();rejected=false;try{failed->start();}catch(const std::runtime_error&){rejected=true;}check(rejected && failed->shutdown() && root2->mob_pool().available()==root2->mob_pool().size());
    std::cout<<"World startup, frame, deferred shutdown and startup cleanup passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
