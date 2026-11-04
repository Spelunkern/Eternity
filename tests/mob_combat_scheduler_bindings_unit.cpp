#include "eternity/game/mob_combat_scheduler_skills.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
struct Scene final:ActorScene {
    mutable unsigned calls=0;
    void send_self(const net::Packet&) noexcept override{++calls;}
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
// Explicit original-input projection for tests ONLY. Actor has no canonical
// retained active CSkill owner yet; this must not become production storage.
struct World final:MobCombatSchedulerSkillWorld,ActorActionWorld {
    unsigned profile,index;MobCombatSchedulerBindings& binding;MobRuntime mob;
    std::array<MobCombatSchedulerSkillBinding,2> views{{{0,"Runtime"},{0,"Runtime"}}};std::string events;
    World(unsigned p,unsigned i,MobCombatSchedulerBindings& b):profile(p),index(i),binding(b){}
    void event(const std::string& s){if(!events.empty())events+='|';events+=s;}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&)const override{
        if((profile==18||profile==26)&&index==0)return nullptr;
        if(index==0)return &views[0];if(index==1||index==17||index==255)return &views[1];
        throw std::runtime_error("invalid fixture slot");
    }
    void remove_mob_transform(Actor&)override{event("transform");if(profile==19)index=17;if(profile==20||profile==24)binding.weapon_present=false;}
    MobRuntime* enable_mob(Actor&,std::uint32_t)override{event("mob");return &mob;}
    bool is_both_mob(MobRuntime&)override{return false;}
    bool in_range(Actor&,MobRuntime&)override{event("range");return true;}
    void attack_mob(Actor&,MobRuntime&)override{event("normal");}
    void cancel_move(Actor& a)override{event("cancel");a.cancel_move(*this);}
    void send_self(Actor&,const net::Packet& p)override{
        constexpr char d[]="0123456789abcdef";std::string s="send";for(auto v:p){s+=d[v>>4];s+=d[v&15];}event(s);
    }
    void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding& s,std::uint32_t)override{event("use"+std::to_string(&s-views.data()));}
    void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding& s,std::uint32_t)override{event("cast"+std::to_string(&s-views.data()));}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected world provider");}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t)override{unused();}
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)override{unused();}
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)override{unused();}
    bool normal_user_attack(Actor&,std::uint32_t)override{unused();}
    Actor* find_user(Actor&,std::uint32_t)override{unused();}
    void send_move_cancel(Actor&,const net::Packet&)override{unused();}
};
std::array<std::uint32_t,7> parse(std::string s){for(auto& c:s)if(c==',')c=' ';std::istringstream in(s);std::array<std::uint32_t,7> values{};for(auto& v:values)in>>v;check(bool(in),"invalid state");return values;}
std::string snapshot(const Actor& a,unsigned index){const auto& x=a.attack_state();return std::to_string(x.mode)+','+std::to_string(x.recovery)+','+std::to_string(x.previous_tick)+','+std::to_string(x.next_tick)+','+std::to_string(x.resume_tick)+','+std::to_string(index)+','+std::to_string(x.motion);}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned rows=0,polls=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        std::string opcode,request_events,pre,post,events;unsigned profile,mode,slot,selected,final_index,final_mode,phase;int count;
        std::istringstream row(line);row>>opcode>>profile>>mode>>slot>>count>>selected>>final_index>>final_mode>>phase>>request_events>>pre>>post>>events;check(bool(row),"invalid row");++rows;
        if(pre=="-")continue;
        const auto values=parse(pre);Scene scene;EffectStatus status;status.current_target_type=2;status.current_target=77;status.skills.equipment.attack_speed=4;
        Actor actor(42,status,{}, {},scene,rules);auto& a=actor.attack_state();a.mode=values[0];a.recovery=values[1];a.previous_tick=values[2];a.next_tick=values[3];a.resume_tick=values[4];a.motion=static_cast<std::uint8_t>(values[6]);
        MobCombatSchedulerBindings binding{profile!=25&&profile!=27};World world(profile,values[5],binding);
        static_cast<void>(mob_combat_scheduler(actor,binding,100,world));
        // Native attack_speed uses the existing owner, not an observable query
        // port. This speed token belongs only to original executable setup.
        if(events.starts_with("speed|"))events.erase(0,6);
        check(snapshot(actor,world.index)==post&&world.events==events,"poll mismatch profile "+std::to_string(profile)+" actual "+snapshot(actor,world.index)+" expected "+post+" events "+world.events+" expected "+events);++polls;
    }
    check(rows==112&&polls==48,"incomplete binding fixture");std::cout<<rows<<" complete original selection cases; "<<polls<<" native polls from observed input projections passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
