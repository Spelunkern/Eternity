#include "eternity/game/user_skill_runtime_attack.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
struct Scene final:ActorScene {
    mutable unsigned calls=0;std::function<void(const net::Packet&)> on_send,on_view;
    void send_self(const net::Packet& p) noexcept override{++calls;if(on_send)on_send(p);}
    void send_nearby(const net::Packet& p) override{++calls;if(on_view)on_view(p);}
    void send_cell_view(const net::Packet&) override{++calls;}
    std::optional<bool> party_kind() const noexcept override{++calls;return {};}
    void send_party(const net::Packet&) noexcept override{++calls;}
    void reset_damage() override{++calls;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{++calls;return 1;}
    void release_effect_id(std::uint32_t) noexcept override{++calls;}
    std::uint32_t tick_ms() const noexcept override{++calls;return 123;}
    void stop_target_mob(std::uint32_t) override{++calls;}
    void taunt(std::uint16_t,float) override{++calls;}
    ZoneUsers* user_zone() override{++calls;return nullptr;}
    UserShapeSocial user_shape_social() override{++calls;return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{++calls;}
};
std::string hex(std::span<const std::uint8_t> p){constexpr char d[]="0123456789abcdef";std::string out;for(auto b:p){out+=d[b>>4];out+=d[b&15];}return out;}
struct Manager:UserSkillRuntimeServices {
    SkillDefinition effect,other_effect;UserSkillDefinition info{effect,"Original"},alternate{other_effect,"Other"};std::vector<UserSkillHandle> stranded;
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{auto p=std::make_shared<UserSkillRuntimeSkill>();p->instance_id=501;return p;}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("unexpected definition failure");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected disconnect");}
};
std::string address(UserSkillAction action){switch(action){
    case UserSkillAction::attack:return "00451d90";case UserSkillAction::heal:return "00452b50";case UserSkillAction::debuff:return "00452bf0";case UserSkillAction::buff:return "00453470";case UserSkillAction::field:return "004534e0";case UserSkillAction::once:return "00453610";
    case UserSkillAction::attack_select:return "00450e20";case UserSkillAction::attack_range:return "00450f70";case UserSkillAction::debuff_select:return "0044fd90";case UserSkillAction::debuff_range:return "00451570";case UserSkillAction::debuff_around:return "00451980";
}throw std::runtime_error("unknown action");}
struct Services:UserSkillAttackServices {
    UserSkillRuntime& owner;Manager& manager;std::uint32_t now;unsigned mutation;std::string events;
    Services(UserSkillRuntime& o,Manager& m,std::uint32_t n,unsigned bits):owner(o),manager(m),now(n),mutation(bits){}
    std::string snapshot()const{auto p=owner.slot(0);return std::to_string(p->reset_tick)+','+std::to_string(std::bit_cast<std::uint32_t>(owner.actor().status().casting))+','+std::to_string(p->definition==&manager.alternate)+','+std::to_string(manager.info.sp_cost)+','+std::to_string(manager.info.mp_cost)+','+std::to_string(manager.info.disable_seconds);}
    void event(std::string tag){if(!events.empty())events+='|';events+=tag+':'+snapshot();}
    void effect(UserSkillAction action,const UserSkillDefinition& info){check(&info==&manager.info,"current runtime definition replaced captured definition");event("E"+address(action));if(mutation&1){auto p=owner.slot(0);p->reset_tick=77;p->definition=&manager.alternate;manager.info.sp_cost=3;manager.info.mp_cost=4;manager.info.disable_seconds=2;owner.actor().status().skills.cool_time=-1;}}
    void user_effect(UserSkillAction a,Actor&,Actor*,const UserSkillDefinition& i)override{effect(a,i);}
    void mob_effect(UserSkillAction a,Actor&,MobRuntime* target,const UserSkillDefinition& i)override{if(a==UserSkillAction::debuff_around)check(!target,"around effect received invented mob target");effect(a,i);}
    void spend(Actor&,std::uint16_t sp,std::uint16_t mp)override{event("C"+std::to_string(sp)+','+std::to_string(mp));if(mutation&2)manager.info.disable_seconds=3;}
    std::uint32_t tick_ms()override{event("T");if(mutation&4){manager.info.disable_seconds=0;owner.actor().status().casting=123;}return now+7;}
    void view(const net::Packet& p){event("V"+hex(p));if(mutation&8){owner.actor().status().casting=0;owner.slot(0)->reset_tick=88;}}
};
void seed(UserSkillRuntime& owner){net::Packet wire{4,5,42,0,0,0,1,11,0,2,0,0,0};check(owner.load(wire,0)==UserSkillLoadResult::loaded,"load failed");owner.slot(0)->reset_tick=999;}
struct SchedulerWorld:UserSkillRuntimeAttackWorld {
    Actor& peer;MobRuntime target;
    SchedulerWorld(UserSkillRuntime& o,Services& s,Actor& p):UserSkillRuntimeAttackWorld(o,s),peer(p){}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected scheduler provider");}
    MobRuntime* enable_mob(Actor&,std::uint32_t)override{return &target;}
    bool is_both_mob(MobRuntime&)override{return false;}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t)override{return &peer;}
    void use_runtime_mob(Actor&,MobRuntime*,UserSkillHandle,std::uint32_t)override{unused();}
    void use_runtime_user(Actor&,Actor&,UserSkillHandle,std::uint32_t)override{unused();}
    bool in_range(Actor&,MobRuntime&)override{unused();}
    void attack_mob(Actor&,MobRuntime&)override{unused();}
    Actor* find_user(Actor&,std::uint32_t)override{unused();}
    Actor* find_attack_user(Actor&,std::uint32_t)override{unused();}
    std::int32_t map_team_mode(const Actor&)const override{unused();}
    bool in_user_range(Actor&,Actor&)override{unused();}
    void attack_user(Actor&,Actor&)override{unused();}
    void send_move_cancel(Actor&,const net::Packet&)override{unused();}
};
void compositions(const ActorEquipmentRules& rules){for(bool mob:{false,true})for(unsigned seconds:{7U,501U})for(int cool:{0,1}){
    Scene scene;EffectStatus status;status.skills.cool_time=cool;status.skills.equipment.attack_speed=4;status.current_target_type=mob?2:1;status.current_target=77;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Actor peer(77,{}, {},{},scene,rules);Manager manager;manager.effect.effect=1;manager.info.target_kind=3;manager.info.reset_seconds=static_cast<std::uint16_t>(seconds);UserSkillRuntime owner(actor,manager);seed(owner);Services services(owner,manager,100,0);SchedulerWorld world(owner,services,peer);actor->attack_state().mode=3;actor->attack_state().next_tick=2;MobCombatSchedulerBindings binding;
    check(mob_combat_scheduler(*actor,binding,100,world),"casting scheduler failed");check(owner.slot(0)->reset_tick==100+((cool>0&&seconds<=500)?0U:seconds*1000U)&&actor->attack_state().resume_tick==2350&&actor->attack_state().motion==1&&actor->attack_state().mode==0&&!services.events.empty(),"actual casting adapter did not publish runtime/scheduler state");
}}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,mob,effect,kind,seconds,mutation,sp,mp,disable,in_zone,initializing;std::uint32_t cool,now;std::string expected,events;std::istringstream row(line);row>>n>>mob>>effect>>kind>>seconds>>cool>>now>>mutation>>sp>>mp>>disable>>in_zone>>initializing>>expected>>events;check(bool(row),"invalid fixture");
        Scene scene;EffectStatus status;status.skills.cool_time=std::bit_cast<std::int32_t>(cool);status.skills.equipment.attack_speed=4;status.skills.equipment.move_speed=2;status.in_zone=in_zone!=0;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);actor->initializing(initializing!=0);Actor peer(77,{}, {},{},scene,rules);MobRuntime target;Manager manager;manager.effect.effect=static_cast<std::uint8_t>(effect);manager.info.target_kind=static_cast<std::uint8_t>(kind);manager.info.reset_seconds=static_cast<std::uint16_t>(seconds);manager.info.sp_cost=static_cast<std::uint16_t>(sp);manager.info.mp_cost=static_cast<std::uint16_t>(mp);manager.info.disable_seconds=static_cast<std::uint16_t>(disable);manager.alternate.sp_cost=9;manager.alternate.mp_cost=9;manager.alternate.disable_seconds=9;UserSkillRuntime owner(actor,manager);seed(owner);Services services(owner,manager,now,mutation);scene.on_view=[&](const net::Packet& p){services.view(p);};
        if(mob)user_skill_runtime_attack_mob(owner,owner.slot(0),now,&target,services);else user_skill_runtime_attack_user(owner,owner.slot(0),now,&peer,services);
        if(events=="-")events.clear();check(services.snapshot()==expected&&services.events==events,"resolved attack mismatch case "+std::to_string(n)+" actual "+services.snapshot()+" expected "+expected+"\nactual events "+services.events+"\nexpected events "+events);++cases;
    }
    check(cases==768,"incomplete attack fixture");compositions(rules);std::cout<<cases<<" whole original resolved attacks and 8 actual casting scheduler compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
