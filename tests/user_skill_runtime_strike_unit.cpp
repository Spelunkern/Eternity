#include "eternity/game/user_skill_runtime_strike.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
struct Scene final:ActorScene {
    mutable unsigned calls=0;std::function<void(const net::Packet&)> on_send;
    void send_self(const net::Packet& p) noexcept override{++calls;if(on_send)on_send(p);}
    void send_nearby(const net::Packet&) override{++calls;}
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

struct World final:UserSkillStrikeWorld,UserSkillAttackServices {
    UserSkillDefinition& info;unsigned announced,changed,source_dead,target_dead,killed,hits=0;std::string trace;
    explicit World(UserSkillDefinition& i):info(i){}
    void event(std::string s){if(!trace.empty())trace+=',';trace+=s;}
    void announce_multi(Actor&,Actor&,const UserSkillDefinition&)override{event("C");info.multi_attack=static_cast<std::uint8_t>(announced);}
    void hit(Actor& source,Actor& target,const UserSkillDefinition&,bool first,bool second) override {
        event("H"+std::to_string(first)+std::to_string(second));
        if(++hits==1){
            info.multi_attack=static_cast<std::uint8_t>(changed);source.attack_state().mode=static_cast<std::int32_t>(source_dead);target.attack_state().mode=static_cast<std::int32_t>(target_dead);
            source.heal({std::bit_cast<std::int32_t>(0x80000011u-source.resources().hp),0,0});
            source.relations().duel_state=0;source.relations().duel_opponent=0;source.relations().opposing_guild_id=0;
        }
    }
    bool self_damage(Actor&,std::int32_t hp)override{event("D"+std::to_string(std::bit_cast<std::uint32_t>(hp)));return killed!=0;}
    void self_death(Actor&,bool forced)override{event("F"+std::to_string(forced));}
    void range(Actor&,Actor*,const UserSkillDefinition&)override{event("R");}
    void range_select(Actor&,Actor*,const UserSkillDefinition&)override{event("S");}
    void user_effect(UserSkillAction action,Actor& source,Actor* target,const UserSkillDefinition& definition)override{
        check(action==UserSkillAction::attack&& &definition==&info,"wrong resolved action/definition");user_skill_runtime_strike(source,target,definition,*this);
    }
    void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected mob effect");}
    void spend(Actor&,std::uint16_t,std::uint16_t)override{throw std::runtime_error("unexpected spend");}
    std::uint32_t tick_ms()override{throw std::runtime_error("unexpected clock");}
};
struct Manager:UserSkillRuntimeServices {
    SkillDefinition effect;UserSkillDefinition info{effect,"Snapshot"};std::uint32_t counter=500;std::vector<UserSkillHandle> stranded;
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{auto p=std::make_shared<UserSkillRuntimeSkill>();p->instance_id=++counter;return p;}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("unexpected missing definition");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected disconnect");}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        unsigned n,kind,detail,initial,announced,changed,sd,td,killed,relation;std::string expected;std::istringstream row(line);row>>n>>kind>>detail>>initial>>announced>>changed>>sd>>td>>killed>>relation>>expected;check(bool(row),"invalid fixture");
        Scene scene;EffectStatus status;status.skills.equipment.base.maximum={0x7fffffff,0x7fffffff,0x7fffffff};Actor source(42,status,PlayerResources{13,100,100},std::vector<PersistedEffect>{},scene,rules),target(77,status,PlayerResources{13,100,100},std::vector<PersistedEffect>{},scene,rules);
        source.relations().duel_state=relation&1?4:0;source.relations().duel_opponent=relation&2?77:9;target.relations().duel_opponent=relation&4?42:9;source.relations().opposing_guild_id=relation==6?5:0;
        SkillDefinition effect;effect.detail=static_cast<std::uint16_t>(detail);UserSkillDefinition info{effect,"Strike"};info.target_kind=static_cast<std::uint8_t>(kind);info.multi_attack=static_cast<std::uint8_t>(initial);
        World world(info);world.announced=announced;world.changed=changed;world.source_dead=sd;world.target_dead=td;world.killed=killed;
        user_skill_runtime_strike(source,&target,info,world);
        check((world.trace.empty()?"-":world.trace)==expected,"original strike differs case "+std::to_string(n)+" actual "+world.trace+" expected "+expected);++cases;
    }
    check(cases==6400,"incomplete fixture");
    for(unsigned detail:{22u,23u})for(unsigned killed:{0u,1u}){
        Scene scene;EffectStatus status;status.skills.equipment.base.maximum={0x7fffffff,0x7fffffff,0x7fffffff};auto source=std::make_shared<Actor>(42,status,PlayerResources{13,100,100},std::vector<PersistedEffect>{},scene,rules);
        Actor target(77,status,PlayerResources{13,100,100},std::vector<PersistedEffect>{},scene,rules);Manager manager;
        manager.effect.detail=static_cast<std::uint16_t>(detail);manager.effect.effect=1;manager.info.target_kind=3;manager.info.multi_attack=1;manager.info.reset_seconds=7;
        UserSkillRuntime owner(source,manager);net::Packet seed{4,5,42,0,0,0,1,11,0,2,0,0,0};check(owner.load(seed,0)==UserSkillLoadResult::loaded,"seed failed");
        World world(manager.info);world.announced=2;world.changed=3;world.source_dead=0;world.target_dead=0;world.killed=killed;
        source->relations().opposing_guild_id=1;
        user_skill_runtime_attack_user(owner,owner.slot(0),100,&target,world);
        check(owner.slot(0)->reset_tick==7100,"resolved reset deadline lost");
        check(world.trace==(detail==22?"C,H01,H01,H01":killed?"H11,D2147483665,F1":"H11,D2147483665"),"resolved strike composition differs");
    }
    std::cout<<cases<<" whole original user strike coordinator comparisons and 4 resolved-attack compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
