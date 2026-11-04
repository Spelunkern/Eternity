#include "eternity/game/user_skill_runtime.hpp"
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
std::vector<std::uint8_t> unhex(const std::string& s){std::vector<std::uint8_t> v;for(std::size_t i=0;i<s.size();i+=2)v.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return v;}
struct Manager final:UserSkillRuntimeServices {
    SkillDefinition effect;UserSkillDefinition info{effect,"Runtime"};unsigned attempts=0,fail=0,counter=500;UserSkillHandle free;std::vector<UserSkillHandle> stranded;std::string events;
    void event(std::string s){if(!events.empty())events+='|';events+=s;}
    const UserSkillDefinition* definition(std::uint16_t id,std::uint8_t level)override{event("Q,"+std::to_string(id)+','+std::to_string(level));return id==999?nullptr:&info;}
    UserSkillHandle allocate()override{
        if(free){auto p=std::move(free);return p;}
        if(++attempts==fail)return {};
        auto p=std::make_shared<UserSkillRuntimeSkill>();p->instance_id=++counter;
        p->skill_id=p->level=p->number=p->attack_tick=p->reset_tick=p->end_tick=p->previous_tick=p->attacker_type=p->attacker_id=0x11223344;return p;
    }
    void destroy(UserSkillHandle p)noexcept override{event("D,"+std::to_string(p->instance_id));}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{}
    void disconnect(std::uint32_t error,std::uint32_t detail)override{check(error==0x68&&detail==0,"wrong disconnect");event("C");}
};
std::string slots(const UserSkillRuntime& owner){std::string out;for(unsigned i=0;i<256;++i)if(auto p=owner.slot(i)){
    if(!out.empty())out+=';';out+=std::to_string(i)+':';bool first=true;
    for(auto x:{p->instance_id,p->skill_id,p->level,p->number,p->attack_tick,p->reset_tick,p->end_tick,p->previous_tick,p->attacker_type,p->attacker_id}){if(!first)out+=',';first=false;out+=std::to_string(x);}
}return out.empty()?"-":out;}
struct World final:UserSkillRuntimeWorld {
    UserSkillRuntime& owner;Actor& peer;MobRuntime mob;unsigned actions=0;Manager* manager=nullptr;const UserSkillDefinition* replacement=nullptr;bool switch_slot=false,clear_on_action=false;std::uint32_t seen=0;
    World(UserSkillRuntime& o,Actor& p):UserSkillRuntimeWorld(o),owner(o),peer(p){}
    MobRuntime* enable_mob(Actor&,std::uint32_t)override{if(switch_slot)owner.selection()=255;if(replacement)owner.selected()->definition=replacement;return &mob;}
    bool is_both_mob(MobRuntime&)override{return false;}
    bool in_range(Actor&,MobRuntime&)override{unused();}
    void attack_mob(Actor&,MobRuntime&)override{unused();}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t type)override{check(type==owner.selected()->definition->effect.detail,"wrong actual definition");if(switch_slot)owner.selection()=255;if(replacement)owner.selected()->definition=replacement;return &peer;}
    void action(UserSkillHandle skill,bool casting){++actions;if(manager)manager->event(std::string(casting?"A,":"U,")+std::to_string(skill->instance_id)+','+std::to_string(skill->number));seen=skill->number;check(skill->target_type==skill->definition->effect.detail&&skill->name==skill->definition->name,"definition was not reread");check(skill==owner.selected(),"action did not capture actual runtime");if(clear_on_action){owner.reset_character();check(skill->skill_id==11,"captured runtime lost across reset");}}
    void use_runtime_mob(Actor&,MobRuntime*,UserSkillHandle s,std::uint32_t)override{action(std::move(s),false);}
    void attack_runtime_mob(Actor&,MobRuntime*,UserSkillHandle s,std::uint32_t)override{action(std::move(s),true);}
    void use_runtime_user(Actor&,Actor&,UserSkillHandle s,std::uint32_t)override{action(std::move(s),false);}
    void attack_runtime_user(Actor&,Actor&,UserSkillHandle s,std::uint32_t)override{action(std::move(s),true);}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected required world service");}
    Actor* find_user(Actor&,std::uint32_t)override{unused();}
    Actor* find_attack_user(Actor&,std::uint32_t)override{unused();}
    std::int32_t map_team_mode(const Actor&)const override{unused();}
    bool in_user_range(Actor&,Actor&)override{unused();}
    void attack_user(Actor&,Actor&)override{unused();}
    void send_move_cancel(Actor&,const net::Packet&)override{unused();}
};
void compositions(const ActorEquipmentRules& rules){
    {Scene scene;Manager manager;auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);std::weak_ptr<Actor> weak=actor;
    auto owner=std::make_unique<UserSkillRuntime>(actor,manager);actor.reset();check(!weak.expired(),"skill owner lost retained Actor");owner.reset();check(weak.expired(),"skill owner leaked retained Actor");}

    for(unsigned mode:{2U,3U})for(unsigned kind:{1U,2U,4U})for(bool reset:{false,true}){
        Scene scene;auto actor_owner=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);auto& actor=*actor_owner;Actor peer(77,{}, {},{},scene,rules);SkillDefinition second;second.detail=5;UserSkillDefinition alternate{second,"Other"};Manager manager;UserSkillRuntime owner(actor_owner,manager);
        auto wire=unhex("04052a000000020b00010000000b0001ff0300");check(owner.load(wire,100)==UserSkillLoadResult::loaded,"composition load failed");
        check(owner.learned_count()==2&&!owner.request_slot(255)&&owner.slot(255),"forced slot was count-gated");
        owner.selection()=0;actor.attack_state().mode=static_cast<std::int32_t>(mode);actor.attack_state().next_tick=2;actor.status().current_target_type=static_cast<std::int32_t>(kind);actor.status().current_target=77;actor.status().skills.equipment.attack_speed=4;
        World world(owner,peer);world.switch_slot=true;world.clear_on_action=reset;if(kind!=4)world.replacement=&alternate;if(mode==3||kind==4)owner.selection()=255;
        MobCombatSchedulerBindings binding;check(mob_combat_scheduler(actor,binding,100,world),"actual runtime scheduler rejected");check(world.actions==1&&world.seen==255,"selected runtime dispatch mismatch");
        bool rejected=false;try{static_cast<void>(world.selected_skill(peer));}catch(const std::invalid_argument&){rejected=true;}check(rejected,"cross-Actor owner binding accepted");
        owner.selection()=256;rejected=false;try{static_cast<void>(owner.selected());}catch(const std::out_of_range&){rejected=true;}check(rejected,"malformed slot silently clamped");
    }
}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        unsigned n,fail,reset,count,index,counter,free_count;std::uint32_t tick;unsigned poll_selected,poll_mode;std::string hex,expected,events,poll_final;std::istringstream row(line);row>>n>>fail>>reset>>tick>>hex>>count>>index>>expected>>events>>counter>>free_count>>poll_selected>>poll_mode>>poll_final;check(bool(row),"invalid fixture");
        Scene scene;auto actor_owner=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);auto& actor=*actor_owner;Actor peer(77,{}, {},{},scene,rules);Manager manager;scene.on_send=[&](const net::Packet& p){constexpr char d[]="0123456789abcdef";std::string out="P,";for(auto b:p){out+=d[b>>4];out+=d[b&15];}manager.event(out);};manager.fail=fail;auto old=manager.allocate();if(old){old->attack_tick=71;old->reset_tick=72;old->end_tick=73;old->previous_tick=74;old->attacker_type=75;old->attacker_id=76;manager.free=std::move(old);}manager.attempts=0;
        {
            UserSkillRuntime owner(actor_owner,manager);auto wire=unhex(hex);const auto result=owner.load(wire,tick);check((result==UserSkillLoadResult::disconnect_requested)==(n%16==7||n%16==15),"load status differs");owner.selection()=poll_selected;actor.attack_state().mode=static_cast<std::int32_t>(poll_mode);actor.attack_state().next_tick=2;actor.status().current_target_type=2;actor.status().current_target=77;actor.status().skills.equipment.attack_speed=4;
            World world(owner,peer);world.manager=&manager;MobCombatSchedulerBindings binding;static_cast<void>(mob_combat_scheduler(actor,binding,tick,world));
            const auto& attack=actor.attack_state();const auto actual=std::to_string(attack.mode)+','+std::to_string(attack.recovery)+','+std::to_string(attack.previous_tick)+','+std::to_string(attack.next_tick)+','+std::to_string(attack.resume_tick)+','+std::to_string(attack.motion);
            check(actual==poll_final,"original loaded runtime poll differs case "+std::to_string(n)+" actual "+actual+" expected "+poll_final);
            if(reset)owner.reset_character();
            check(static_cast<unsigned>(owner.learned_count())==count&&owner.selection()==index&&slots(owner)==expected,"original learned slots mismatch case "+std::to_string(n)+" actual "+slots(owner)+" expected "+expected);
        }
        check(manager.events==events&&manager.counter==counter&&unsigned(bool(manager.free))==free_count,"original manager/teardown mismatch case "+std::to_string(n)+" actual "+manager.events+" expected "+events);++cases;
    }
    check(cases==64,"incomplete fixture");compositions(rules);std::cout<<cases<<" complete original load/reuse/reset/destructor cases and 12 actual owner scheduler compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
