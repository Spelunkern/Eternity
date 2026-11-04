#include "eternity/game/mob_applied_skills_obelisk_maps.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/linked_mobs.hpp"
#include "mob_damage_fixture.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <source_location>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool b,std::source_location at=std::source_location::current()){if(!b)throw std::runtime_error("Retained map composition mismatch at "+std::to_string(at.line()));}
void put(std::vector<std::uint8_t>& out,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i) out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void real(std::vector<std::uint8_t>& out,float f) {put(out,std::bit_cast<std::uint32_t>(f));}
void write_maps(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::vector<std::uint8_t> bytes;put(bytes,81);bytes.resize(bytes.size()+(81*81+7)/8);
    for(unsigned i=0;i<4;++i) put(bytes,0);put(bytes,3);
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
struct Services:ObeliskRuntimeMapsServices,MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,ZoneMobServices,MobRemovalOwners {
    ServerMaps& maps;MobDamageFixture damage;std::vector<std::unique_ptr<ZoneMobs>> zones;std::array<bool,2> enabled{true,false};ObeliskRuntimeMaps* records=nullptr;
    unsigned times=0,binds=0,releases=0,db=0,world=0,map_packets=0,entries=0;bool mutate=false,drop_entry=false,drop_db=false;
    explicit Services(ServerMaps& m):maps(m),zones(m.size()){for(std::size_t i=0;i<zones.size();++i)zones[i]=std::make_unique<ZoneMobs>(maps.all()[i].definition.geometry(),*this);}
    std::size_t index(const ConfiguredMap& m)const{for(std::size_t i=0;i<maps.size();++i)if(&maps.all()[i]==&m)return i;throw std::logic_error("Foreign configured map");}
    ZoneMobs* primary_zone(const ConfiguredMap& m)const noexcept override{for(std::size_t i=0;i<maps.size();++i)if(&maps.all()[i]==&m)return enabled[i]?zones[i].get():nullptr;std::terminate();}
    void database(const eternity::net::Packet& p)override{check(p.size()>=2);++db;if(drop_db){drop_db=false;enabled[0]=false;}}
    void world_broadcast(const eternity::net::Packet& p)override{check(p.size()>=2);++world;}
    void map_broadcast(const ConfiguredMap& m,ZoneMobs& z,const eternity::net::Packet& p)override{check(primary_zone(m)==&z && p.size()>=2);++map_packets;}
    std::unique_ptr<MobRuntime> construct()override{auto p=std::make_unique<MobRuntime>();p->drop_random=ServerRandom(unix_time());return p;}
    std::uint32_t unix_time()override{++times;return 123456;}
    MobDamagePool& damage_pool()override{return damage.pool;}
    void release_compiled_ai(MobRuntime& m)override{check(m.has_applied_skills() && m.damage && records);check(records->records(0).size()==1);++releases;}
    void release_stack_ai(MobRuntime&)override{throw std::logic_error("Unexpected stack backend");}
    void close_lua(void*)override{throw std::logic_error("Unexpected VM backend");}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{++binds;return *this;}
    std::uint32_t tick()noexcept override{return 77;}
    void log(const MobAppliedSkill&,std::string_view name,std::uint8_t action)noexcept override{check(name=="S" && (action==5 || action==6));}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
    std::uint32_t tick_ms()override{return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p)override{
        check(p.size()>=6 && p[1]==6);if(p[0]!=1){check(p[0]==2);return;}++entries;
        std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t(p[2+i])<<(8*i);
        if(zones[0]->find(id)){if(mutate){mutate=false;auto& later=records->records(1)[0];later.country=(later.country+1)%3;}if(drop_entry){drop_entry=false;enabled[0]=false;}}
    }
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::logic_error("Unexpected broadcast allocation");}
    void lua_init(MobRuntime&)override{throw std::logic_error("Unexpected Lua entry");}
    void destroy_obelisk(MobRuntime&,std::int32_t)override{throw std::logic_error("Unexpected linked delegation");}
    void remove_obelisk_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked delegation");}
    void destroy_linked_group(std::int32_t)override{throw std::logic_error("Unexpected linked group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked member");}
};
struct Clock:ObeliskRuntimeOwnershipClock{std::uint32_t packed_now()override{throw std::logic_error("Unexpected calendar sample");}std::uint32_t seconds_between(std::uint32_t,std::uint32_t)override{throw std::logic_error("Unexpected calendar difference");}};
struct Death:MobDeathWorld {
    MobAppliedSkillsObeliskMaps& root;unsigned map;unsigned cleared=0,drops=0;
    Death(MobAppliedSkillsObeliskMaps& r,unsigned m):root(r),map(m){}
    void send_death(MobRuntime&,const eternity::net::Packet&,std::uint8_t p)override{check(p==6);}
    void log_death(MobRuntime&,MobDeathUser*,bool)override{throw std::logic_error("Unexpected death log");}
    void clear_applied_skills(MobRuntime& m)override{check(m.damage->records().empty());++cleared;m.applied_skills().clear();}
    std::optional<std::int32_t> obelisk_country(MobRuntime& m,std::int32_t t)override{return root.death_country(map,m,t);}
    void destroy_obelisk(MobRuntime& m,std::int32_t c,std::int32_t t)override{root.destroyed(map,m,c,t);}
    void remove_obelisk_member(MobRuntime& m,void* s)override{root.remove_member(map,m,s);}
    void drop_items(MobRuntime&,MobDeathUser* u,MobDeathParty* p)override{check(!u && !p);++drops;}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t)override{throw std::logic_error("Unexpected reward");}
    MobDeathParty* party(MobDeathUser&)override{throw std::logic_error("Unexpected reward");}
    void lock_party(MobDeathParty&)override{throw std::logic_error("Unexpected reward");}
    void unlock_party(MobDeathParty&)noexcept override{std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t)override{throw std::logic_error("Unexpected reward");}
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t)override{throw std::logic_error("Unexpected reward");}
    void quest_kill(MobDeathUser&,MobRuntime&)override{throw std::logic_error("Unexpected reward");}
    void destroy_linked_group(MobRuntime&,std::int32_t)override{throw std::logic_error("Unexpected linked group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked group");}
};
}
int main(int argc,char** argv){try{check(argc==2);write_maps(argv[1]);for(unsigned n=0;n<16;++n){
    auto maps=ServerMaps::load(MapConfiguration::decode("[BASE]\nMapCount=2\n"),argv[1]);auto runtime=templates();auto full=catalog();Services services(maps);SkillFactory factory;MobAppliedSkillPool heap(factory);std::atomic<std::uint32_t> skill_ids{0};ObeliskRuntimeConfigurationState state;state.next_id=n&1?0xfffffffe:100;state.maximum_kill_count={100,200};
    const unsigned count=1+n%2,total=1+count;std::ostringstream ini;for(unsigned i=0;i<2;++i)ini<<"[zone_"<<i<<"]\n[Obelisk_1]=(20,0,20),(1,2,3),"<<1+(n+i)%3<<"\n[mob]=(30,0,30),1,(1,2,3),"<<count<<"\n";ini<<"[Portal_10,1,1001]\n[light]=0,Obelisk_1\n[fury]=1,Obelisk_1\n";
    auto owner=bind_obelisk_runtime_maps(maps,runtime,full,ini.str(),state,services);services.records=owner.get();check(state.total==2 && owner->portals().size()==1);check(owner->portals()[0].country==maps.mutable_portal_country(1,0));
    auto* records=owner->records(0).data();const auto config_id=state.next_id;for(unsigned id=1;id<4;++id)check(full.mob_record(id)[0x7c]==std::byte{1});
    MobAppliedSkillsRuntime binding(full,heap,skill_ids,services);unsigned mob_ids=n&2?0xfffffffc:100;
    {
        MobPool pool(mob_ids,services,binding);MicrosoftRandom random(n+1);unsigned clocks=0;MobInitPorts init{random,[&]{++clocks;return 77U;},[](MobRuntime&,std::uint8_t){throw std::logic_error("Unexpected compiled backend");return false;},[](MobRuntime&){throw std::logic_error("Unexpected Lua backend");}};
        MobAppliedSkillsObeliskMapsServices backend{[&](int c,int amount){check(amount==state.kill_count_add[static_cast<unsigned>(c)]);},[&](int c,int amount){check(amount==state.kill_count_add[static_cast<unsigned>(c)]);},[](int){return false;},[](const eternity::net::Packet& p){check(p.size()>=48 && p[1]==10 && p[16]=='F');},services};
        MobAppliedSkillsObeliskMaps root(maps,runtime,full,*owner,services,pool,init,state.kill_count_add,backend);
        check(!root.startup(1) && mob_ids==(n&2?0xfffffffcU:100U) && services.times==0);
        unsigned resets=0;const auto reset_clock=[&]{++resets;return 100U;};
        if(n&4){auto& live_view=owner->runtime_map(0);services.drop_entry=true;bool rejected=false;try{root.startup(0);}catch(const std::logic_error&){rejected=true;}check(rejected && !services.enabled[0] && !live_view.zone_present && services.zones[0]->size()==1 && owner->records(0)[0].principal);services.enabled[0]=true;check(root.reset(0,reset_clock));}
        services.mutate=(n&1)!=0;const auto later=owner->records(1)[0].country;const auto before_mobs=mob_ids,before_time=services.times,before_ticks=clocks;const auto before_size=pool.size();auto expected_random=random;for(unsigned step=0;step<2+6*count;++step)expected_random.next();check(root.startup(0));services.enabled[1]=true;check(root.startup(1));check(owner->records(1)[0].country==((n&1)?(later+1)%3:later));
        check(mob_ids==before_mobs+2*total && services.times==before_time+4*total-before_size && clocks==before_ticks+4*total && random.state()==expected_random.state());
        check(owner->records(1)[0].principal->definition==owner->records(1)[0].definitions[static_cast<std::size_t>(owner->records(1)[0].country)]);
        check(services.binds==2*total && pool.size()==2*total && services.zones[0]->size()==total && services.zones[1]->size()==total && state.next_id==config_id && owner->records(0).data()==records);
        auto* principal=owner->records(0)[0].principal;auto* skill_owner=&principal->applied_skills();skill_owner->apply(1,1,42);check(skill_ids==1);
        bool foreign=false;try{root.death_country(0,*owner->records(1)[0].principal,0);}catch(const std::invalid_argument&){foreign=true;}check(foreign);
        Death death(root,0);mob_death(*principal,1,42,{},death);check(death.drops==1 && death.cleared==1 && skill_owner->owner().records().empty() && owner->records(0)[0].respawn.current==0 && owner->records(0)[0].respawn.next==UINT32_MAX);
        principal->status=2;principal->death_time=0;LinkedMobPopulation linked(std::span<const LinkedMobGroup>{});services.zones[0]->update_deaths(pool,random,100,linked);check(services.zones[0]->size()==0);const auto before_tick=mob_ids,clock_before_tick=services.times;check(root.tick(0,5100) && services.zones[0]->size()==total && services.binds==2*total && mob_ids==before_tick+total && services.times==clock_before_tick+total);for(auto* m:services.zones[0]->all())check(m->drop_random.seed()==(123456U^m->id));
        bool busy=false;try{root.detach_primary(1);}catch(const std::logic_error&){busy=true;}check(busy);
        services.drop_db=(n&8)!=0;check(root.reset(0,reset_clock));if(!services.enabled[0]){const auto before=resets;check(!root.reset(0,reset_clock) && !root.tick(0,999999) && resets==before);services.enabled[0]=true;}
        auto previous_zone=std::move(services.zones[0]);services.zones[0]=std::make_unique<ZoneMobs>(maps.all()[0].definition.geometry(),services);const auto before_replace=mob_ids;bool replaced=false;try{root.startup(0);}catch(const std::logic_error&){replaced=true;}check(replaced && mob_ids==before_replace);services.zones[0]=std::move(previous_zone);
        root.detach_primary(0);services.zones[0].reset();check(!root.startup(0));services.zones[0]=std::make_unique<ZoneMobs>(maps.all()[0].definition.geometry(),services);check(root.startup(0));check(services.binds==2*total);check(root.reset(0,reset_clock));root.detach_primary(0);services.zones[0].reset();
        check(root.reset(1,reset_clock));root.detach_primary(1);services.zones[1].reset();check(pool.available()==pool.size());owner->persist();check(services.db>0 && services.map_packets>0);
        auto* retained=pool.allocate();retained->applied_skills().apply(1,1,42);pool.release(*retained);check(heap.owned_count()==1 && skill_ids==1 && records==owner->records(0).data());
    }
    check(heap.owned_count()==0 && services.releases>0);
}std::cout<<"16 retained ServerMaps/fullcatalog/two-primary startup/death/timer/reset/rebind/destructor compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
