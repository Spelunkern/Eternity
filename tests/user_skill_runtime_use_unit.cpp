#include "eternity/game/user_skill_runtime_use.hpp"
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
    SkillDefinition effect,other;UserSkillDefinition info{effect,"Original"},alternate{other,"Alternate"};std::vector<UserSkillHandle> stranded;
    Manager(){effect.effect=other.effect=1;info.target_kind=alternate.target_kind=3;info.reset_seconds=7;alternate.reset_seconds=9;}
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{return std::make_shared<UserSkillRuntimeSkill>();}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("missing definition");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("disconnect");}
};
struct Services:UserSkillUseServices {
    UserSkillRuntime& owner;Manager& manager;bool eligible;unsigned mutation,eligibility_calls=0,actions=0;std::string events;
    Services(UserSkillRuntime& o,Manager& m,bool enabled,unsigned flags):owner(o),manager(m),eligible(enabled),mutation(flags){}
    std::string snapshot()const{const auto& a=owner.actor().attack_state();auto p=owner.slot(0);return std::to_string(std::bit_cast<std::uint32_t>(a.mode))+','+std::to_string(std::bit_cast<std::uint32_t>(a.recovery))+','+std::to_string(a.previous_tick)+','+std::to_string(a.next_tick)+','+std::to_string(a.resume_tick)+','+std::to_string(p->reset_tick)+','+std::to_string(p->definition==&manager.alternate)+','+std::to_string(manager.info.ready_time)+','+std::to_string(manager.alternate.ready_time)+','+std::to_string(p->skill_id)+','+std::to_string(p->level)+','+std::to_string(std::bit_cast<std::uint32_t>(owner.actor().status().skills.equipment.attack_speed));}
    void event(std::string tag){if(!events.empty())events+='|';events+=tag+':'+snapshot();}
    bool eligibility(){++eligibility_calls;event("E");if(mutation&1){auto& a=owner.actor().attack_state();a.mode=0;a.recovery=-1;a.previous_tick=55;owner.slot(0)->definition=&manager.alternate;}return eligible;}
    bool eligible_user(UserSkillRuntime&,Actor*,UserSkillHandle,std::uint32_t)override{return eligibility();}
    bool eligible_mob(UserSkillRuntime&,MobRuntime*,UserSkillHandle,std::uint32_t)override{return eligibility();}
    void cast(const UserSkillDefinition& captured,const net::Packet& p){event("V"+hex(p));if(mutation&2){auto& info=&captured==&manager.info?manager.info:manager.alternate;info.ready_time=0;auto& a=owner.actor().attack_state();a.resume_tick=1234;a.mode=4;auto r=owner.slot(0);r->skill_id=99;r->level=3;r->definition=&manager.alternate;}}
    void cast_user(Actor&,Actor&,const UserSkillDefinition& info,const net::Packet& p)override{cast(info,p);}
    void cast_mob(Actor&,MobRuntime&,const UserSkillDefinition& info,const net::Packet& p)override{cast(info,p);}
    void action(){++actions;event("A");if(mutation&4){auto& a=owner.actor().attack_state();a.previous_tick=55;a.mode=0;a.recovery=-1;a.resume_tick=777;owner.actor().status().skills.equipment.attack_speed=9;auto p=owner.slot(0);p->reset_tick=77;p->definition=&manager.alternate;}}
    void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)override{action();}
    void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{action();}
    void spend(Actor&,std::uint16_t,std::uint16_t)override{throw std::runtime_error("unexpected spending");}
    std::uint32_t tick_ms()override{throw std::runtime_error("unexpected clock");}
    void self(const net::Packet& p){event("S"+hex(p));if(mutation&8){auto& a=owner.actor().attack_state();a.recovery=-1;a.mode=1;a.previous_tick=0;a.resume_tick=444;owner.actor().status().skills.equipment.attack_speed=1;}}
};
void seed(UserSkillRuntime& owner){net::Packet wire{4,5,42,0,0,0,1,11,0,2,0,0,0};check(owner.load(wire,0)==UserSkillLoadResult::loaded,"seed failed");owner.slot(0)->reset_tick=999;}
struct World:UserSkillRuntimeUseWorld {
    Actor& peer;MobRuntime target;
    World(UserSkillRuntime& o,Services& s,Actor& p):UserSkillRuntimeUseWorld(o,s),peer(p){target.id=77;}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected scheduler provider");}
    MobRuntime* enable_mob(Actor&,std::uint32_t)override{return &target;}
    bool is_both_mob(MobRuntime&)override{return false;}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t)override{return &peer;}
    bool in_range(Actor&,MobRuntime&)override{unused();}
    void attack_mob(Actor&,MobRuntime&)override{unused();}
    Actor* find_user(Actor&,std::uint32_t)override{unused();}
    Actor* find_attack_user(Actor&,std::uint32_t)override{unused();}
    std::int32_t map_team_mode(const Actor&)const override{unused();}
    bool in_user_range(Actor&,Actor&)override{unused();}
    void attack_user(Actor&,Actor&)override{unused();}
    void send_move_cancel(Actor&,const net::Packet&)override{unused();}
};
void compositions(const ActorEquipmentRules& rules){for(bool mob:{false,true})for(bool ready:{false,true}){
    Scene scene;EffectStatus status;status.in_zone=true;status.skills.equipment.attack_speed=4;status.current_target_type=mob?2:1;status.current_target=77;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Actor peer(77,{}, {},{},scene,rules);Manager manager;manager.info.ready_time=ready?1:0;UserSkillRuntime owner(actor,manager);seed(owner);Services services(owner,manager,true,0);World world(owner,services,peer);actor->attack_state().mode=2;actor->attack_state().next_tick=2;MobCombatSchedulerBindings bindings;
    check(mob_combat_scheduler(*actor,bindings,100,world),"use scheduler failed");if(ready){check(actor->attack_state().mode==3&&actor->attack_state().resume_tick==350&&owner.slot(0)->reset_tick==999,"cast preparation state differs");check(mob_combat_scheduler(*actor,bindings,350,world),"casting resolution failed");}
    check(services.eligibility_calls==1&&services.actions==1&&owner.slot(0)->reset_tick==(ready?7350U:7100U)&&actor->attack_state().mode==0&&actor->attack_state().motion==1&&actor->attack_state().resume_tick==(ready?2600U:2350U),"actual use/casting adapter composition differs");
}}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,mob,eligible,ready,alt_ready,in_zone,mutation;std::uint32_t mode,previous,recovery,speed,now;std::string final,events;std::istringstream row(line);row>>n>>mob>>eligible>>ready>>alt_ready>>mode>>previous>>recovery>>speed>>now>>in_zone>>mutation>>final>>events;check(bool(row),"invalid fixture");
        Scene scene;EffectStatus status;status.in_zone=in_zone!=0;status.skills.equipment.attack_speed=std::bit_cast<std::int32_t>(speed);auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Actor peer(77,{}, {},{},scene,rules);MobRuntime target;target.id=77;Manager manager;manager.info.ready_time=static_cast<std::uint8_t>(ready);manager.alternate.ready_time=static_cast<std::uint8_t>(alt_ready);UserSkillRuntime owner(actor,manager);seed(owner);auto& a=actor->attack_state();a.mode=std::bit_cast<std::int32_t>(mode);a.previous_tick=previous;a.recovery=std::bit_cast<std::int32_t>(recovery);a.next_tick=2;a.resume_tick=555;Services services(owner,manager,eligible!=0,mutation);scene.on_send=[&](const net::Packet& p){services.self(p);};
        if(mob)user_skill_runtime_use_mob(owner,owner.slot(0),now,&target,services);else user_skill_runtime_use_user(owner,owner.slot(0),now,&peer,services);
        if(events=="-")events.clear();check(services.snapshot()==final&&services.events==events,"use mismatch case "+std::to_string(n)+" actual "+services.snapshot()+" expected "+final+"\nactual events "+services.events+"\nexpected events "+events);++cases;
    }
    check(cases==1024,"incomplete use fixture");compositions(rules);std::cout<<cases<<" complete original use/instant resolution sequences and 4 actual scheduler pipelines passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
