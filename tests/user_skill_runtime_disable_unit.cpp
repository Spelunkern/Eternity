#include "eternity/game/user_skill_runtime_disable.hpp"
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
    mutable unsigned calls=0;std::function<void(const net::Packet&)> on_send,on_view;std::function<std::uint32_t()> on_tick;
    void send_self(const net::Packet& p) noexcept override{++calls;if(on_send)on_send(p);}
    void send_nearby(const net::Packet& p) override{++calls;if(on_view)on_view(p);}
    void send_cell_view(const net::Packet&) override{++calls;}
    std::optional<bool> party_kind() const noexcept override{++calls;return {};}
    void send_party(const net::Packet&) noexcept override{++calls;}
    void reset_damage() override{++calls;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{++calls;return 1;}
    void release_effect_id(std::uint32_t) noexcept override{++calls;}
    std::uint32_t tick_ms() const noexcept override{++calls;return on_tick?on_tick():123;}
    void stop_target_mob(std::uint32_t) override{++calls;}
    void taunt(std::uint16_t,float) override{++calls;}
    ZoneUsers* user_zone() override{++calls;return nullptr;}
    UserShapeSocial user_shape_social() override{++calls;return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{++calls;}
};
std::string hex(std::span<const std::uint8_t> p){constexpr char d[]="0123456789abcdef";std::string out;for(auto b:p){out+=d[b>>4];out+=d[b&15];}return out;}
std::array<std::uint32_t,3> state(std::string s){for(auto& c:s)if(c==',')c=' ';std::array<std::uint32_t,3> v{};std::istringstream in(s);for(auto& x:v)in>>x;check(bool(in),"invalid state");return v;}
std::string snapshot(const Actor& actor){return std::to_string(std::bit_cast<std::uint32_t>(actor.status().casting))+','+std::to_string(actor.status().action_block_b)+','+std::to_string(actor.attack_state().mode);}
struct Manager:UserSkillRuntimeServices {
    SkillDefinition effect;UserSkillDefinition info{effect,"Disable"};
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{return std::make_shared<UserSkillRuntimeSkill>();}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle)noexcept override{}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("missing definition");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected disconnect");}
};
void composition(const ActorEquipmentRules& rules){
    Scene scene;EffectStatus status;status.in_zone=true;status.skills.equipment.attack_speed=4;status.skills.equipment.move_speed=2;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Manager manager;manager.info.reset_seconds=7;manager.info.disable_seconds=1;UserSkillRuntime owner(actor,manager);net::Packet wire{4,5,42,0,0,0,1,11,0,1,0,0,0};check(owner.load(wire,0)==UserSkillLoadResult::loaded,"composition load failed");
    struct Services:UserSkillAttackServices {
        void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected effect");}
        void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected effect");}
        void spend(Actor&,std::uint16_t,std::uint16_t)override{throw std::runtime_error("unexpected spending");}
        std::uint32_t tick_ms()override{return 1000;}
    } services;
    std::string packets;scene.on_view=[&](const net::Packet& p){if(!packets.empty())packets+='|';packets+=hex(p);};
    user_skill_runtime_attack_mob(owner,owner.slot(0),10,nullptr,services);check(actor->status().casting==2000,"resolved attack did not publish disable");
    std::uint32_t now=2000;scene.on_tick=[&]{return now;};user_skill_runtime_keep_tail(*actor,false);check(actor->status().casting==2000,"equality expired disable");now=2001;user_skill_runtime_keep_tail(*actor,false);
    check(actor->status().casting==0&&owner.slot(0)->reset_tick==7010&&packets=="1c052a000000ffff|1c052a0000000402","disable lifecycle changed runtime reset or speed ordering");
}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,kind,mutation,in_zone,initializing,sleep,mode,damage,periodic;std::uint32_t initial,fresh,caller;std::string pre,cleanup,expected,events;std::istringstream row(line);row>>n>>kind>>initial>>fresh>>caller>>mutation>>in_zone>>initializing>>sleep>>mode>>pre>>damage>>periodic>>cleanup>>expected>>events;check(bool(row),"invalid fixture");check(periodic==unsigned(kind==2),"invalid original periodic projection");const auto observed=state(pre);
        Scene scene;EffectStatus status;status.casting=std::bit_cast<std::int32_t>(observed[0]);status.action_block_b=std::bit_cast<std::int32_t>(observed[1]);status.in_zone=in_zone!=0;status.skills.equipment.attack_speed=4;status.skills.equipment.move_speed=2;SkillDefinition applied;applied.id=11;applied.level=1;applied.detail=kind==2?61:0;applied.effect=3;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);actor->initializing(initializing!=0);actor->attack_state().mode=std::bit_cast<std::int32_t>(observed[2]);
        if(kind){EffectTimer timer;timer.effect={11,1,0};timer.previous_tick=17;actor->effects().append(123,timer,applied);if(kind==2)actor->effects().reset_keep_tick(caller);} // Existing owner after observed periodic prefix, no private effect list.
        std::string actual;const auto event=[&](const std::string& tag){if(!actual.empty())actual+='|';actual+=tag+':'+snapshot(*actor);};
        scene.on_tick=[&]{event("T");if(mutation==1)actor->status().casting=0;if(mutation==2)actor->status().casting=std::bit_cast<std::int32_t>(fresh+1U);if(mutation==4)actor->status().casting=std::bit_cast<std::int32_t>(fresh);return fresh;};
        scene.on_view=[&](const net::Packet& p){event("V"+hex(p));if(mutation==3)actor->attack_state().mode=1;if(mutation==5)actor->status().casting=777;};
        user_skill_runtime_keep_tail(*actor,damage!=0);if(events=="-")events.clear();check(snapshot(*actor)==expected&&actual==events&&actor->effects().size()==unsigned(kind!=0)&&(!kind||actor->effects().contains(123)),"disable tail mismatch case "+std::to_string(n)+" actual "+snapshot(*actor)+" expected "+expected+"\nactual events "+actual+"\nexpected events "+events);++cases;
    }
    check(cases==512,"incomplete keep tail fixture");composition(rules);std::cout<<cases<<" complete original keep frames / actual Actor tail comparisons and disable publication/expiry lifecycle passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
