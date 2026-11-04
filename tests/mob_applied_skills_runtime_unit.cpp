#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include "eternity/game/mob_death.hpp"
#include "mob_damage_fixture.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool b){if(!b)throw std::runtime_error("Retained mob skill runtime mismatch");}
void put(std::span<std::byte> b,unsigned at,unsigned v,unsigned n){for(unsigned i=0;i<n;++i)b[at+i]=static_cast<std::byte>((v>>(8*i))&255);}
std::string hex(std::span<const std::uint8_t> b){constexpr char d[]="0123456789abcdef";std::string out;for(auto n:b){out+=d[n>>4];out+=d[n&15];}return out;}
std::string core(const MobRuntime& m){
    std::vector<std::uint8_t> b;for(auto n:std::array<std::uint32_t,20>{m.hp,m.sp,m.mp,unsigned(m.dexterity),unsigned(m.wisdom),unsigned(m.luck),unsigned(m.attribute),unsigned(m.stun),unsigned(m.stop),unsigned(m.sleep),unsigned(m.move_speed),unsigned(m.attack_speed),unsigned(m.current_attack),m.attack_times[0],m.attack_times[1],m.attack_times[2],unsigned(m.status),m.vision_fixed_user,unsigned(m.target_kind),m.target_id})for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));return hex(b);
}
struct Skills:MobAppliedSkillPoolServices {
    std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}
};
struct Services:MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,MobAppliedSkillsRuntimeResetServices {
    MobDamageFixture damage_heap;const MobRuntimeTemplate& definition;const MobTargetingBindingCatalog& catalog;
    unsigned constructs=0,bindings=0,compiled=0,stacks=0,closes=0,logs=0;bool oracle=false,resetting=false;
    std::vector<void*> closed;std::string events;MobRuntime* reset_subject=nullptr;
    Services(const MobRuntimeTemplate& d,const MobTargetingBindingCatalog& c):definition(d),catalog(c){}
    std::unique_ptr<MobRuntime> construct()override{++constructs;auto p=std::make_unique<MobRuntime>();p->definition=&definition;return p;}
    std::uint32_t unix_time()override{return 123;}
    MobDamagePool& damage_pool()override{return damage_heap.pool;}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{++bindings;return *this;}
    void release_compiled_ai(MobRuntime& m)override{check(m.damage && m.has_applied_skills());++compiled;m.use_pais=false;}
    void release_stack_ai(MobRuntime& m)override{check(m.state_stack && m.damage && m.has_applied_skills());++stacks;}
    void close_lua(void* p)override{check(std::find(closed.begin(),closed.end(),p)==closed.end());closed.push_back(p);++closes;}
    std::uint32_t tick()noexcept override{return 100;}
    void log(const MobAppliedSkill& s,std::string_view name,std::uint8_t action)noexcept override{
        check(action==5 || action==6);auto* slot=catalog.from_definition(s.definition);check(slot && name==slot->name());if(resetting && reset_subject && action==6){check(reset_subject->hp==definition.hp && reset_subject->stun==0 && reset_subject->stop==0 && reset_subject->sleep==0 && reset_subject->wisdom==definition.wisdom);auto active=reset_subject->applied_skills().owner().records();check(std::find(active.begin(),active.end(),&s)!=active.end());}++logs;
    }
    void rebuild_attack(MobRuntime& m)noexcept override{rebuild_mob_attack(m);}
    void append(const std::string& tag,MobRuntime& m){if(!events.empty())events+=';';events+=tag+':'+core(m);}
    void reset_state(MobRuntime& m){check(m.applied_skills().owner().records().empty() && m.hp==definition.hp && m.stun==0 && m.stop==0 && m.sleep==0 && m.wisdom==definition.wisdom);if(oracle){for(auto a:m.attack)check(a.accuracy==0 && a.evasion==0 && a.attack==0 && a.defense==0 && a.critical_attack==0 && a.critical_defense==0);for(auto a:m.additions)check(a.accuracy==0 && a.attack==0 && a.evasion==0 && a.defense==0);for(auto n:m.attack_disabled)check(n==0);}}
    void send_hp(MobRuntime& m,const std::array<std::uint8_t,10>& p)override{check(m.zone && p[0]==5 && p[1]==3);reset_state(m);if(oracle)append("V:"+hex(p),m);}
    void send_speed(MobRuntime& m)noexcept override{if(resetting){reset_state(m);if(oracle)append("S",m);}}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
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
    void remove_obelisk_member(MobRuntime&,void*)override{throw std::runtime_error("Unexpected group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::runtime_error("Unexpected group");}
};
void load_skill(MobTargetingBindingCatalog& c,unsigned id,unsigned detail){auto b=c.mutable_skill_record(id);put(b,0,id,2);put(b,2,100,1);put(b,49,3,1);put(b,50,detail,2);put(b,92,10,4);b[3]=std::byte{'S'};put(b,124,7,1);put(b,126,5,2);c.publish_skill_record(id);}
}
int main(int argc,char** argv){try{
    check(argc==2);std::ifstream fixture(argv[1]);check(fixture.good());unsigned original_cases=0;
    for(std::string line;std::getline(fixture,line);){if(line.empty() || line[0]=='#')continue;
        unsigned n;std::string expected,final;std::istringstream row(line);row>>n>>expected>>final;check(!row.fail());
        auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});MobRuntimeTemplate d;d.hp=0xffffffff-n;d.sp=static_cast<std::uint16_t>(65535-n);d.mp=static_cast<std::uint16_t>(32768+n);d.dexterity=static_cast<std::uint16_t>(60000+n);d.wisdom=static_cast<std::uint16_t>(50000+n);d.luck=static_cast<std::uint16_t>(40000+n);d.attribute=static_cast<std::uint8_t>(n%9);
        Skills factory;MobAppliedSkillPool shared(factory);Services services(d,catalog);std::atomic<std::uint32_t> skills{0};MobAppliedSkillsRuntime runtime(catalog,shared,skills,services);unsigned ids=99+n;MobPool pool(ids,services,runtime);auto* m=pool.allocate();
        m->status=std::bit_cast<int>(0xa5a5a5a5U);m->vision_fixed_user=0xa5a5a5a5;m->target_kind=std::bit_cast<int>(0xa5a5a5a5U);m->target_id=0xa5a5a5a5;static unsigned zone;m->zone=n&1?reinterpret_cast<ZoneMobs*>(&zone):nullptr;
        const auto dirty=std::bit_cast<int>(0xa5a5a5a5U);m->attack_disabled.fill(dirty);m->attack.fill({dirty,dirty,dirty,dirty,dirty,dirty});m->additions.fill({dirty,dirty,dirty,dirty});m->attack_times.fill(0xa5a5a5a5);m->current_attack=dirty;m->stun=dirty;m->stop=dirty;m->sleep=dirty;
        services.oracle=true;services.resetting=true;reset_mob_applied_skills_runtime(*m,services);services.append("A",*m);check(services.events==expected && core(*m)==final);++original_cases;
    }check(original_cases==64);
    for(unsigned n=0;n<64;++n){
        auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});load_skill(catalog,1,0x40);load_skill(catalog,2,0x43);
        MobRuntimeTemplate d;d.hp=2000+n;d.sp=100;d.mp=200;d.wisdom=100;d.level=20;d.attribute=3;
        Skills factory;MobAppliedSkillPool shared(factory);Services services(d,catalog);std::atomic<std::uint32_t> skill_ids{0};MobAppliedSkillsRuntime runtime(catalog,shared,skill_ids,services);unsigned mob_ids=0;unsigned logs_before=0;
        {
            MobPool pool(mob_ids,services,runtime);auto* a=pool.allocate();auto* b=pool.allocate();check(a->has_applied_skills() && b->has_applied_skills());auto* bridge=&a->applied_skills();
            bool move_rejected=false;try{MobRuntime moved(std::move(*a));}catch(const std::invalid_argument&){move_rejected=true;}check(move_rejected && &a->applied_skills()==bridge);
            move_rejected=false;try{*a=MobRuntime{};}catch(const std::invalid_argument&){move_rejected=true;}check(move_rejected && &a->applied_skills()==bridge);
            MicrosoftRandom random;MobInitPorts init{random,[]{return 100U;},[](MobRuntime&,std::uint8_t){throw std::runtime_error("Unexpected compiled Init");return false;},[](MobRuntime&){throw std::runtime_error("Unexpected Lua Init");}};
            initialize_mob(*a,1,nullptr,init);initialize_mob(*b,1,nullptr,init);a->lua_ai=1;
            a->applied_skills().apply(1,1,42);a->applied_skills().apply(2,1,42);b->applied_skills().apply(1,1,43);check(skill_ids==3 && a->stun==1 && a->wisdom==95);
            auto records=a->applied_skills().owner().records();auto* identity=records[1]->definition;
            initialize_mob(*a,1,nullptr,init);reset_mob_attack(*a);check(a->applied_skills().owner().records()==records && a->stun==0 && a->wisdom==100);
            auto raw=catalog.mutable_skill_record(2);put(raw,126,9,2);put(raw,92,20,4);catalog.publish_skill_record(2);check(identity==catalog.skill_definition(2) && identity->abilities[0].value==9);
            a->lua_ai=1;a->hp=1;a->stun=7;a->sleep=9;static unsigned zone;a->zone=reinterpret_cast<ZoneMobs*>(&zone);services.reset_subject=a;services.resetting=true;auto before_reset=services.logs;reset_mob_applied_skills_runtime(*a,services);services.resetting=false;check(services.logs==before_reset+2);
            check(a->applied_skills().owner().records().empty() && shared.available().size()==2 && b->applied_skills().owner().records().size()==1);
            a->applied_skills().apply(1,1,42);a->damage->add(1,42,10,1,0);a->damage->fair_combat=false;Death death;mob_death(*a,1,42,{},death);check(death.clear==1 && death.logged==1 && death.sent==1 && a->applied_skills().owner().records().empty());
            a->state_stack=std::vector<std::int32_t>{1};a->lua_state=reinterpret_cast<void*>(std::uintptr_t(0x1000+n*16));a->lua_ai=1;pool.release(*a);check(a->lua_state==nullptr && !a->state_stack && a->lua_ai==0 && services.closes==1);
            auto* reused=pool.allocate();check(reused==a && &a->applied_skills()==bridge && services.bindings==2 && services.constructs==2);
            initialize_mob(*a,1,nullptr,init);a->applied_skills().apply(2,1,42);check(a->wisdom==91 && skill_ids==3);
            a->state_stack=std::vector<std::int32_t>{};if(n&1)a->lua_state=reinterpret_cast<void*>(std::uintptr_t(0x1001+n*16));
            b->state_stack=std::vector<std::int32_t>{1};b->lua_state=reinterpret_cast<void*>(std::uintptr_t(0x1002+n*16));
            if(n&2){auto retained=a->applied_skills().owner().records();pool.release(*a);check(a->applied_skills().owner().records()==retained);if(n&4){check(pool.allocate()==a && a->applied_skills().owner().records()==retained && &a->applied_skills()==bridge);}}
            logs_before=services.logs;
        }
        check(services.bindings==2 && services.compiled==(n&2?4U:3U) && services.stacks==3 && services.closes==(n&1?3U:2U));
        check(services.logs==logs_before && shared.owned_count()==1 && shared.available().size()==1 && skill_ids==3);
    }
    MobRuntime unbound;bool rejected=false;try{unbound.applied_skills();}catch(const std::logic_error&){rejected=true;}check(rejected);
    std::cout<<original_cases<<" original ResetAttack cases and 64 real retained pool/Init/death/reuse/destruction compositions matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
