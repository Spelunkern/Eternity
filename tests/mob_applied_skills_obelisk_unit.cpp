#include "eternity/game/mob_applied_skills_obelisk.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/linked_mobs.hpp"
#include "mob_damage_fixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <source_location>
using namespace eternity::game;
namespace {
void check(bool b,std::source_location where=std::source_location::current()){if(!b)throw std::runtime_error("Concrete obelisk adapter mismatch at line "+std::to_string(where.line()));}
struct Random:RandomSource {MicrosoftRandom source;unsigned calls=0;explicit Random(unsigned seed):source(seed){}std::int32_t next()override{++calls;return source.next();}};
struct Skills:MobAppliedSkillPoolServices{std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct Services:MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,ZoneMobServices {
    MobDamageFixture damage;ZoneMobs* zone=nullptr;ObeliskRuntime* obelisk=nullptr;const MobTargetingBindingCatalog& catalog;
    unsigned times=0,binds=0,releases=0,enters=0,removes=0,skill_logs=0;bool mutate=false,fail_entry=false;
    explicit Services(const MobTargetingBindingCatalog& c):catalog(c){}
    std::unique_ptr<MobRuntime> construct()override{auto m=std::make_unique<MobRuntime>();m->drop_random=ServerRandom(unix_time());return m;}
    std::uint32_t unix_time()override{++times;return 123456;}
    MobDamagePool& damage_pool()override{return damage.pool;}
    void release_compiled_ai(MobRuntime& m)override{check(m.has_applied_skills() && m.damage);++releases;}
    void release_stack_ai(MobRuntime&)override{throw std::runtime_error("Unexpected AI stack");}
    void close_lua(void*)override{throw std::runtime_error("Unexpected VM");}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{++binds;return *this;}
    std::uint32_t tick()noexcept override{return 77;}
    void log(const MobAppliedSkill& s,std::string_view name,std::uint8_t action)noexcept override{check(s.definition==catalog.skill_definition(1) && name=="S" && (action==5 || action==6));++skill_logs;}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
    std::uint32_t tick_ms()override{return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p)override{
        check(p.size()>=6 && p[1]==6);unsigned id=0;for(unsigned i=0;i<4;++i)id|=unsigned(p[2+i])<<(8*i);
        if(p[0]==1){auto* m=zone->find(id);check(m && m->type==m->definition->id && m->respawn && m->has_applied_skills());++enters;if(fail_entry)throw std::runtime_error("Observed entry transport failure");
            if(m==obelisk->principal)check(m->respawn==&obelisk->respawn && m->position.x==obelisk->center.x && m->position.z==obelisk->center.z);
            else{auto& spawn=obelisk->spawns[0];check(m->respawn==&spawn.respawn && std::find(spawn.members.begin(),spawn.members.end(),m)!=spawn.members.end());}
            if(mutate && m==obelisk->principal)obelisk->country=(obelisk->country+1)%3;
        }else{check(p[0]==2);++removes;}
    }
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::runtime_error("Unexpected broadcast allocation");}
    void lua_init(MobRuntime&)override{throw std::runtime_error("Unexpected Lua entry");}
};
struct Removal:MobRemovalOwners {
    ObeliskRuntimeMap& map;ObeliskRuntime& o;MobAppliedSkillsObelisk* adapter=nullptr;
    Removal(ObeliskRuntimeMap& m,ObeliskRuntime& r):map(m),o(r){}
    void destroy_obelisk(MobRuntime& m,std::int32_t)override{eternity::game::destroy_obelisk(map,o,0,m,2,true,{0,0},*adapter);}
    void remove_obelisk_member(MobRuntime& m,void* source)override{static_cast<ObeliskRuntimeSpawn*>(source)->remove(&m);}
    void destroy_linked_group(std::int32_t)override{throw std::runtime_error("Unexpected linked group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::runtime_error("Unexpected linked member");}
};
struct Death:MobDeathWorld {
    unsigned clear=0,logged=0,sent=0;
    void send_death(MobRuntime& m,const eternity::net::Packet&,std::uint8_t)override{check(!m.applied_skills().owner().records().empty());++sent;}
    void log_death(MobRuntime& m,MobDeathUser*,bool fair)override{check(!fair && !m.damage->records().empty() && !m.applied_skills().owner().records().empty());++logged;}
    void clear_applied_skills(MobRuntime& m)override{check(m.damage->records().empty());++clear;m.applied_skills().clear();}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t)override{throw std::runtime_error("Unexpected reward");}
    MobDeathParty* party(MobDeathUser&)override{throw std::runtime_error("Unexpected reward");}
    void lock_party(MobDeathParty&)override{throw std::runtime_error("Unexpected reward");}
    void unlock_party(MobDeathParty&)noexcept override{std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t)override{throw std::runtime_error("Unexpected reward");}
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t)override{throw std::runtime_error("Unexpected reward");}
    void drop_items(MobRuntime&,MobDeathUser*,MobDeathParty*)override{throw std::runtime_error("Unexpected reward");}
    void quest_kill(MobDeathUser&,MobRuntime&)override{throw std::runtime_error("Unexpected reward");}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t)override{throw std::runtime_error("Unexpected group");}
    void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t)override{throw std::runtime_error("Unexpected group");}
    void destroy_linked_group(MobRuntime&,std::int32_t)override{throw std::runtime_error("Unexpected group");}
    void remove_obelisk_member(MobRuntime& m,void* source)override{static_cast<ObeliskRuntimeSpawn*>(source)->remove(&m);}
    void remove_linked_member(MobRuntime&,void*)override{throw std::runtime_error("Unexpected group");}
};
}
int main(){try{for(unsigned n=0;n<48;++n){
    auto catalog=MobTargetingBindingCatalog::load(0,1,{},{});auto record=catalog.mutable_skill_record(1);record[0]=std::byte{1};record[2]=std::byte{100};record[3]=std::byte{'S'};record[49]=std::byte{3};record[50]=std::byte{99};catalog.publish_skill_record(1);
    Skills factory;MobAppliedSkillPool skills(factory);Services services(catalog);services.mutate=(n&1)!=0;std::atomic<std::uint32_t> skill_ids{0};unsigned ids=n&2?0xfffffffd:100;
    MobAppliedSkillsRuntime binding(catalog,skills,skill_ids,services);
    MapDefinition geometry;geometry.size=80;ZoneMobs zone(geometry.geometry(),services);services.zone=&zone;
    std::array<MobRuntimeTemplate,3> definitions{};for(unsigned i=0;i<3;++i){definitions[i].id=static_cast<std::uint16_t>(i+1);definitions[i].hp=1000+i;definitions[i].level=1;definitions[i].respawn_time=5000;}
    const unsigned count=1+n%3,total=1+count;ObeliskRuntime o;o.country=static_cast<int>((n/3)%3);o.center={20,0,20};o.respawn.obelisk=true;o.respawn.count=1;
    for(unsigned i=0;i<3;++i)o.definitions[i]=&definitions[i];o.spawns.resize(1);auto& spawn=o.spawns[0];spawn.count=static_cast<int>(count);spawn.members.resize(count);spawn.center={30,0,30};spawn.radius=1;spawn.radius_twice=2;spawn.respawn.count=static_cast<int>(count);spawn.respawn.obelisk_source=&spawn;spawn.definitions=o.definitions;services.obelisk=&o;
    std::vector<std::uint8_t> bitmap(800,0);ObeliskRuntimeMap map{true,false,1,80,bitmap};Random random(n+1);unsigned ticks=0;
    MobInitPorts init{random,[&]{++ticks;return 77U;},[](MobRuntime&,std::uint8_t){throw std::runtime_error("Unexpected compiled Init");return false;},[](MobRuntime&){throw std::runtime_error("Unexpected Lua Init");}};
    {
    MobPool pool(ids,services,binding);unsigned changes=0,spawn_logs=0,destroy_logs=0,databases=0,portals=0,expected_databases=0;Removal removal(map,o);MobAppliedSkillsObeliskServices world;
    world.mob_name=[&](const MobRuntime& m){check(pool.is_prepared(m) && m.definition);if(m.zone){check(zone.find(m.id)==&m);if(n&4)o.country=(o.country+1)%3;}else check(!m.cell && !zone.find(m.id));return std::string_view("Mob");};
    world.remove_by_id=[&](unsigned id){check(zone.remove_by_id(id,pool,removal));};world.send_change=[&](std::size_t i){check(i==0);++changes;};
    world.add_kill_count=[](int,int){throw std::runtime_error("Unexpected KC");};world.subtract_kill_count=world.add_kill_count;
    world.kill_count_status14=[](int)->bool{throw std::runtime_error("Unexpected KC");};
    world.broadcast=[](const eternity::net::Packet&){throw std::runtime_error("Unexpected transport");};world.database=[&](const eternity::net::Packet& p){check(p.size()==11 && p[0]==7 && p[1]==2);++databases;};
    world.game_log=[&](const eternity::net::Packet& p){check(p[1]==10 && p.back()==o.country);if(p[0]==2){check(o.principal && zone.find(o.principal->id)==o.principal);++spawn_logs;}else{check(p[0]==3 && !o.principal && zone.size()==0);++destroy_logs;}};
    world.update_portal_links=[&]{check(databases==portals+1);++portals;};
    MobAppliedSkillsObelisk adapter(pool,zone,init,world);removal.adapter=&adapter;
    struct Cleanup{ZoneMobs& zone;MobPool& pool;~Cleanup(){for(auto* m:zone.all()){zone.leave(*m);pool.release(*m);}}} cleanup{zone,pool};
    // Constructor must reject absent providers or a legacy unbound pool before
    // allocating an actor or consuming an ID/time/random sample.
    auto missing=world;missing.database={};bool rejected=false;try{MobAppliedSkillsObelisk invalid(pool,zone,init,missing);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    auto absent_init=init;absent_init.lua_ai={};rejected=false;try{MobAppliedSkillsObelisk invalid(pool,zone,absent_init,world);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    MobPool legacy(ids,services);rejected=false;try{MobAppliedSkillsObelisk invalid(legacy,zone,init,world);}catch(const std::invalid_argument&){rejected=true;}check(rejected && services.times==0 && pool.size()==0 && random.calls==0);
    const auto initial=ids;const auto startup_country=o.country;initialize_obelisks(map,std::span<ObeliskRuntime>(&o,1),adapter);
    check(ids==initial+total && services.times==2*total && services.binds==total && random.calls==1+3*count && ticks==2*total && zone.size()==total && spawn_logs==0);
    check(o.principal->definition==&definitions[static_cast<unsigned>(startup_country)]);
    for(auto* m:spawn.members)check(m && m->definition==&definitions[static_cast<unsigned>(o.country)] && m->drop_random.seed()==(123456U^m->id));
    auto* victim=spawn.members[0];auto* retained=&victim->applied_skills();victim->lua_ai=1;retained->apply(1,1,42);victim->damage->add(1,42,10,1,0);victim->damage->fair_combat=false;
    Death death;mob_death(*victim,1,42,{},death);check(death.clear==1 && death.logged==1 && retained->owner().records().empty() && spawn.members[0]==nullptr && victim->damage->records().empty());
    victim->status=2;victim->death_time=0;LinkedMobPopulation linked(std::span<const LinkedMobGroup>{});zone.update_deaths(pool,random,100,linked);
    check(zone.size()==count && pool.available()==1 && spawn.respawn.current==static_cast<int>(count)-1);
    const auto periodic_id=ids;update_obelisk_respawns(map,std::span<ObeliskRuntime>(&o,1),5100,adapter);
    check(ids==periodic_id+1U && services.times==2*total+1 && services.binds==total && random.calls==4+3*count && zone.size()==total && spawn.members[0]==victim && &victim->applied_skills()==retained);
    victim->lua_ai=1;retained->apply(1,1,42);auto* active_skill=retained->owner().records().at(0);check(skill_ids==1);
    if(o.country!=2)++expected_databases;adapter.remove_by_id(o.principal->id);check(zone.size()==0 && pool.available()==total && !o.principal && changes==1 && retained->owner().records().at(0)==active_skill);
    const auto respawn_country=o.country;const auto before=ids;respawn_obelisk(map,o,adapter);
    check(ids==before+total && services.times==3*total+1 && services.binds==total && random.calls==5+6*count && spawn_logs==1 && zone.size()==total && o.principal->definition==&definitions[static_cast<unsigned>(respawn_country)]);
    for(auto* m:spawn.members)check(m && m->definition==&definitions[static_cast<unsigned>(o.country)]);
    check(retained->owner().records().at(0)==active_skill && skills.owned_count()==1);
    if(o.country!=2)++expected_databases;adapter.remove_by_id(o.principal->id);check(zone.size()==0 && changes==2);
    auto* raw=adapter.allocate(false);const auto prior=raw->definition;rejected=false;try{adapter.initialize(*raw,definitions[0],o.respawn);}catch(const std::invalid_argument&){rejected=true;}check(rejected && raw->definition==prior);
    adapter.prepare_companion(*raw);adapter.initialize(*raw,definitions[0],o.respawn);raw->position={-100,0,-100};rejected=false;try{adapter.enter(*raw);}catch(const std::runtime_error&){rejected=true;}check(rejected && zone.size()==0 && pool.is_prepared(*raw));
    raw->position={40,0,40};services.fail_entry=true;rejected=false;try{adapter.enter(*raw);}catch(const std::runtime_error&){rejected=true;}services.fail_entry=false;check(rejected && zone.find(raw->id)==raw && pool.is_prepared(*raw));check(zone.leave(*raw));pool.release(*raw);
    rejected=false;try{adapter.enter(*raw);}catch(const std::invalid_argument&){rejected=true;}check(rejected && zone.size()==0 && services.skill_logs==3 && skills.owned_count()==1 && destroy_logs==2 && databases==expected_databases && portals==databases);
    }
    check(skills.owned_count()==0 && services.releases==3*total+2);
}std::cout<<"48 concrete adapter startup/periodic/death/removal/respawn/callback mutation/failure compositions matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
