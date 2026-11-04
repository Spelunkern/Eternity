#include "eternity/game/mob_combat_scheduler_skills.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity;using namespace eternity::game;
namespace {
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


std::string hex(const auto& bytes){const char* digits="0123456789abcdef";std::string out;for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
struct World final:MobCombatSchedulerSkillWorld,ActorActionWorld {
    Actor& actor;Actor& peer;MobRuntime& mob;
    MobCombatSchedulerSkillBinding skills[2]={{5,"Fire"},{6,"Ice"}};
    unsigned index=0,present=0,found=0,both=0,post_mob=0,post_user=0,mutation=0;std::string events;
    World(Actor& a,Actor& p,MobRuntime& m):actor(a),peer(p),mob(m){}
    std::string snapshot() const {
        const auto& a=actor.attack_state();const auto& s=actor.status();std::ostringstream out;
        out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<a.previous_tick<<','<<a.next_tick<<','<<a.resume_tick<<','<<static_cast<unsigned>(a.motion)<<','<<static_cast<std::uint32_t>(s.skills.equipment.attack_speed)<<','<<index<<','<<static_cast<std::uint32_t>(mob.status)<<','<<static_cast<std::uint32_t>(peer.attack_state().mode)<<','<<hex(mob.previous_attack_user)<<','<<hex(mob.previous_attack_skill);return out.str();
    }
    void event(char tag,const std::string& extra=""){if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot();if(!extra.empty())events+=','+extra;}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&) const override{return present?&skills[index]:nullptr;}
    void remove_mob_transform(Actor&) override{event('T');}
    MobRuntime* enable_mob(Actor&,std::uint32_t id) override{event('E',std::to_string(id));if(mutation&1)index=1;return found?&mob:nullptr;}
    Actor* enable_skill_user(Actor&,std::uint32_t id,std::uint16_t target) override{event('e',std::to_string(id)+','+std::to_string(target));if(mutation&1)index=1;return found?&peer:nullptr;}
    bool is_both_mob(MobRuntime&) override{event('B');if(mutation&2)index=1;return both!=0;}
    void action(char tag,const MobCombatSchedulerSkillBinding& skill,std::uint32_t now,bool target) {
        const auto slot=&skill-skills;event(tag,std::to_string(slot)+','+std::to_string(now)+','+(target?"1":"0"));
        if(mutation&4){actor.attack_state().mode=3;actor.status().skills.equipment.attack_speed=9;}
        if(mutation&8){actor.attack_state().mode=0;actor.status().current_target_type=1;actor.status().current_target=42;actor.actions().previous_target_type=2;actor.actions().previous_target=77;}
        mob.status=static_cast<std::int32_t>(post_mob);peer.attack_state().mode=static_cast<std::int32_t>(post_user);
    }
    void use_skill_mob(Actor&,MobRuntime* target,const MobCombatSchedulerSkillBinding& skill,std::uint32_t now) override{action('U',skill,now,target!=nullptr);}
    void attack_skill_mob(Actor&,MobRuntime* target,const MobCombatSchedulerSkillBinding& skill,std::uint32_t now) override{action('A',skill,now,target!=nullptr);}
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding& skill,std::uint32_t now) override{action('u',skill,now,true);}
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding& skill,std::uint32_t now) override{action('a',skill,now,true);}
    void cancel_move(Actor& a) override{event('C');if(mutation&16){a.attack_state().resume_tick=1234;a.attack_state().motion=7;}a.cancel_move(*this);}
    void send_self(Actor& a,const net::Packet& packet) override{event('P',hex(packet));if(mutation&32){a.attack_state().recovery=0;index=1;}}
    bool normal_user_attack(Actor&,std::uint32_t) override{throw std::runtime_error("unexpected normal PvP route");}
    bool in_range(Actor&,MobRuntime&) override{throw std::runtime_error("unexpected normal range gate");}
    void attack_mob(Actor&,MobRuntime&) override{throw std::runtime_error("unexpected normal attack");}
    Actor* find_user(Actor&,std::uint32_t) override{throw std::runtime_error("unexpected movement lookup");}
    void send_move_cancel(Actor&,const net::Packet&) override{throw std::runtime_error("unexpected movement send");}
};
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned mode,kind,present,found,both,post_mob,post_user,mutation,result;std::uint32_t now,resume,previous,recovery,speed;
            std::string final,events;std::istringstream row(line);row>>mode>>kind>>present>>found>>both>>post_mob>>post_user>>mutation>>now>>resume>>previous>>recovery>>speed>>result>>final>>events;
            if(!row)throw std::runtime_error("invalid skill scheduler fixture");
            Scene scene;EffectStatus status;status.current_target_type=static_cast<std::int32_t>(kind);status.current_target=77;status.skills.equipment.attack_speed=std::bit_cast<std::int32_t>(speed);
            Actor actor(42,status,{}, {},scene,rules);Actor peer(77,{}, {},{},scene,rules);scene.calls=0;actor.appearance().identity.name="Hero";
            auto& attack=actor.attack_state();attack.mode=static_cast<std::int32_t>(mode);attack.recovery=std::bit_cast<std::int32_t>(recovery);attack.previous_tick=previous;attack.next_tick=2;attack.resume_tick=resume;attack.motion=0xa5;
            actor.actions().previous_target_type=2;actor.actions().previous_target=77;
            MobRuntime mob;mob.previous_attack_user.fill(0xa5);mob.previous_attack_skill.fill(0x5a);World world(actor,peer,mob);world.present=present;world.found=found;world.both=both;world.post_mob=post_mob;world.post_user=post_user;world.mutation=mutation;
            MobCombatSchedulerBindings bindings;const bool actual=mob_combat_scheduler(actor,bindings,now,world);if(events!="-") {
                std::istringstream trace(events);std::string event,filtered;
                while(std::getline(trace,event,'|'))if(!event.starts_with("Q:")){if(!filtered.empty())filtered+='|';filtered+=event;}
                events=filtered;
            }
            if(events=="-")events.clear();
            if(actual!=static_cast<bool>(result)||world.snapshot()!=final||world.events!=events||scene.calls!=0)
                throw std::runtime_error("skill scheduler mismatch "+std::to_string(count)+" actual "+world.snapshot()+" expected "+final+"\nactual events "+world.events+"\nexpected events "+events);
            ++count;
        }
        if(count!=2048)throw std::runtime_error("incomplete skill scheduler fixture");std::cout<<count<<" complete skill/cast scheduler comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
