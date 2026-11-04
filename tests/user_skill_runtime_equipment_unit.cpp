#include "eternity/game/user_skill_runtime_equipment.hpp"
#include "eternity/game/item_pool.hpp"
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
    SkillDefinition effect;UserSkillDefinition info{effect,"Equipment"};std::vector<UserSkillHandle> stranded;
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{return std::make_shared<UserSkillRuntimeSkill>();}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("missing definition");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected disconnect");}
};
// Explicit raw equipment input projection for tests. Pointers refer to actual
// existing pool objects. Production MUST use canonical Actor retained borrow;
// this worker's old Actor header does not yet expose that accessor.
struct Services:UserSkillRuntimeEligibilityServices {
    const GroundItem* weapon=nullptr;const GroundItem* shield=nullptr;std::string events;unsigned borrows=0,ticks=0,ranges=0;bool range=true;
    void event(std::string s){if(!events.empty())events+='|';events+=s;}
    const GroundItem* equipped(Actor&,std::uint16_t slot)override{++borrows;event("B"+std::to_string(slot));if(slot==5)return weapon;if(slot==6)return shield;throw std::runtime_error("invalid equipment slot");}
    std::uint32_t tick_ms()override{++ticks;return 100;}
    bool in_user_range(Actor&,Actor&,std::uint8_t)override{++ranges;return range;}
    bool in_mob_range(Actor&,MobRuntime&,std::uint8_t)override{++ranges;return range;}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected actual use/effect provider");}
    void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)override{unused();}
    void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{unused();}
    void spend(Actor&,std::uint16_t,std::uint16_t)override{unused();}
    void cast_user(Actor&,Actor&,const UserSkillDefinition&,const net::Packet&)override{unused();}
    void cast_mob(Actor&,MobRuntime&,const UserSkillDefinition&,const net::Packet&)override{unused();}
};
void seed(UserSkillRuntime& owner,bool prerequisite=false){net::Packet p{4,5,42,0,0,0,static_cast<std::uint8_t>(prerequisite?2:1),11,0,2,0,0,0};if(prerequisite){net::Packet q{22,0,1,1,0,0};p.insert(p.end(),q.begin(),q.end());}check(owner.load(p,0)==UserSkillLoadResult::loaded,"seed failed");}
void boundaries(const ActorEquipmentRules& rules){
    Scene scene;auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Manager manager;UserSkillRuntime owner(actor,manager);seed(owner);GroundItemPool pool;ItemDefinition info;info.real_type=22;auto* item=pool.allocate();check(item!=nullptr,"pool failed");item->definition=&info;Services services;services.weapon=item;unsigned sent=0;scene.on_send=[&](const net::Packet&){++sent;};
    bool rejected=false;try{static_cast<void>(user_skill_runtime_check_equipment(owner,owner.slot(0),manager.info,false,services));}catch(const std::logic_error&){rejected=true;}check(rejected&&sent==0,"unknown padding silently defaulted");
    manager.info.weapon_padding=0;check(!user_skill_runtime_check_equipment(owner,owner.slot(0),manager.info,false,services)&&sent==1,"explicit zero padding did not reject");manager.info.weapon_padding=0xa5;check(user_skill_runtime_check_equipment(owner,owner.slot(0),manager.info,false,services)&&sent==1,"explicit nonzero padding did not permit");pool.release(item);
}
void compositions(const ActorEquipmentRules& rules){for(bool mob:{false,true})for(unsigned failure=0;failure<7;++failure){
    Scene scene;EffectStatus status;status.skills.equipment.attack_speed=4;auto actor=std::make_shared<Actor>(42,status,PlayerResources{1,10,failure==1?0U:10U},std::vector<PersistedEffect>{},scene,rules);Actor peer(77,{}, {},{},scene,rules);MobRuntime target;Manager manager;manager.info.need_weapon[0]=1;manager.info.sp_cost=2;manager.info.mp_cost=3;manager.info.required_skill=22;manager.info.attack_kind=1;manager.info.attack_range=7;UserSkillRuntime owner(actor,manager);seed(owner,true);owner.slot(1)->reset_tick=failure==4?100:101;if(failure==3)owner.slot(0)->reset_tick=101;if(failure==5)actor->status().skills.attack_disabled[1]=1;
    GroundItemPool pool;ItemDefinition item;item.real_type=1;auto* weapon=pool.allocate();check(weapon!=nullptr,"pool failed");weapon->definition=&item;Services services;services.weapon=failure==2?nullptr:weapon;services.range=failure!=6;scene.on_send=[&](const net::Packet& p){services.event("S"+hex(p));};
    const bool result=mob?services.eligible_mob(owner,&target,owner.slot(0),100):services.eligible_user(owner,&peer,owner.slot(0),100);
    check(result==(failure==0)&&services.borrows==(failure==1?0U:1U)&&services.ticks==(failure>=4||failure==0?1U:0U)&&services.ranges==(failure==0||failure==6?1U:0U),"full cost/equipment/readiness ordering differs");pool.release(weapon);
}}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,mob,type,weapon_present,shield_present,need_shield,pattern,detail,sp,mp,ready,padding,reset,radius,state,attribute,result;std::string events;std::istringstream row(line);row>>n>>mob>>type>>weapon_present>>shield_present>>need_shield>>pattern>>detail>>sp>>mp>>ready>>padding>>reset>>radius>>state>>attribute>>result>>events;check(bool(row),"invalid fixture");
        Scene scene;auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{1,0x7fffffff,0x7fffffff},std::vector<PersistedEffect>{},scene,rules);Manager manager;manager.effect.detail=static_cast<std::uint16_t>(detail);manager.effect.defense_type=2;manager.info.state_type=static_cast<std::uint8_t>(state);manager.effect.attribute=static_cast<std::uint8_t>(attribute);auto& info=manager.info;for(unsigned i=0;i<15;++i)info.need_weapon[i]=static_cast<std::uint8_t>((i+pattern)%3==0);info.need_weapon[15]=static_cast<std::uint8_t>(need_shield);info.sp_cost=static_cast<std::uint16_t>(sp);info.mp_cost=static_cast<std::uint16_t>(mp);info.ready_time=static_cast<std::uint8_t>(ready);info.weapon_padding=static_cast<std::uint8_t>(padding);info.reset_seconds=static_cast<std::uint16_t>(reset);info.attack_range=static_cast<std::uint8_t>(radius);UserSkillRuntime owner(actor,manager);seed(owner);
        GroundItemPool pool;ItemDefinition definition;definition.real_type=static_cast<std::uint8_t>(type);auto* weapon=pool.allocate();auto* shield=pool.allocate();check(weapon&&shield,"pool allocation failed");weapon->definition=&definition;Services services;services.weapon=weapon_present?weapon:nullptr;services.shield=shield_present?shield:nullptr;scene.on_send=[&](const net::Packet& p){services.event("S"+hex(p));};
        const bool actual=user_skill_runtime_check_equipment(owner,owner.slot(0),info,mob!=0,services);check(actual==bool(result)&&services.events==events,"original equipment middle differs case "+std::to_string(n)+" actual "+services.events+" expected "+events);pool.release(weapon);pool.release(shield);++cases;
    }
    check(cases==1344,"incomplete equipment fixture");compositions(rules);boundaries(rules);std::cout<<cases<<" whole original eligibility/equipment comparisons and 14 actual combined eligibility compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
