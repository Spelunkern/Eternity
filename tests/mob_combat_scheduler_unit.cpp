#include "eternity/game/mob_combat_scheduler.hpp"
#include "eternity/game/mob_combat_attack.hpp"
#include "eternity/game/mob_combat_stop.hpp"
#include <functional>
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
struct World final:MobCombatSchedulerWorld,ActorActionWorld {
    Actor& actor;MobRuntime& mob;MobCombatSchedulerBindings& bindings;
    unsigned blocked=0,found=0,range=0,both=0,post=0,mutation=0;std::string events;std::function<void()> on_attack;
    World(Actor& a,MobRuntime& m,MobCombatSchedulerBindings& b):actor(a),mob(m),bindings(b){}
    std::string snapshot() const {
        const auto& a=actor.attack_state();const auto& s=actor.status();std::ostringstream out;
        out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<a.previous_tick<<','<<a.next_tick<<','<<a.resume_tick<<','<<static_cast<unsigned>(a.motion)<<','<<static_cast<std::uint32_t>(s.skills.equipment.attack_speed)<<','<<bindings.weapon_present<<','<<static_cast<std::uint32_t>(mob.status)<<','<<hex(mob.previous_attack_user)<<','<<hex(mob.previous_attack_skill);return out.str();
    }
    void event(char tag,const std::string& arg="") {if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot();if(!arg.empty())events+=','+arg;}
    void remove_mob_transform(Actor& a) override{event('T');if(mutation&1)bindings.weapon_present=false;if(mutation&2)a.attack_state().next_tick=3;}
    MobRuntime* enable_mob(Actor&,std::uint32_t id) override{event('E',std::to_string(id));return found?&mob:nullptr;}
    bool in_range(Actor&,MobRuntime&) override{event('R');return range!=0;}
    bool is_both_mob(MobRuntime&) override{event('B');return both!=0;}
    void attack_mob(Actor& a,MobRuntime& m) override{event('A');if(on_attack)on_attack();else m.status=static_cast<std::int32_t>(post);if(mutation&4){a.status().skills.equipment.attack_speed=9;a.attack_state().mode=1;}}
    void cancel_move(Actor& a) override{event('C');if(mutation&8){a.attack_state().resume_tick=1234;a.attack_state().motion=7;}a.cancel_move(*this);}
    void send_self(Actor& a,const net::Packet& packet) override{event('P',hex(packet));if(mutation&16){a.attack_state().resume_tick=9;a.attack_state().motion=8;}}
    bool remainder(Actor&,std::uint32_t,MobCombatSchedulerRemainder) override{throw std::runtime_error("unexpected unimplemented scheduler branch");}
    Actor* find_user(Actor&,std::uint32_t) override{throw std::runtime_error("unexpected move lookup");}
    void send_move_cancel(Actor&,const net::Packet&) override{throw std::runtime_error("unexpected movement send");}
};
struct Heap final:MobDamagePoolServices {
    std::unique_ptr<MobDamageRecord> construct() override{return std::make_unique<MobDamageRecord>();}
};
struct Hit final:MobCombatAttackServices {
    Actor& actor;MobDamageLedger& ledger;unsigned deaths=0;
    Hit(Actor& a,MobDamageLedger& l):actor(a),ledger(l){}
    std::int32_t status(const MobCombatState& m) override{return m.state;}
    void dispatch_death_event(MobCombatState&) override{throw std::runtime_error("unexpected hit FSM");}
    std::uint32_t tick() override{return 123;}
    void log_damage(MobCombatState&,std::uint32_t) override{throw std::runtime_error("unexpected hit logging");}
    void retarget(MobCombatState&,bool) override{throw std::runtime_error("unexpected lethal retarget");}
    void stop_attack(MobCombatAttacker&) override{mob_combat_stop(actor);}
    void death(MobCombatState&,std::int32_t type,std::uint32_t id) override {
        if(type!=1||id!=42||ledger.records().size()!=2||actor.attack_state().mode!=0||actor.status().current_target_type!=0||actor.status().current_target!=77)
            throw std::runtime_error("scheduler lethal hit ordering violated");
        ++deaths;
    }
};
void compose(const ActorEquipmentRules& rules) {
    Scene scene;EffectStatus status;status.current_target_type=2;status.current_target=77;status.skills.equipment.attack_speed=4;
    Actor actor(42,status,{}, {},scene,rules);scene.calls=0;auto& attack=actor.attack_state();attack.mode=2;attack.next_tick=1;attack.resume_tick=100;attack.previous_tick=55;attack.recovery=0;attack.motion=0xa5;
    Heap heap;MobDamagePool pool(heap);MobRuntime mob;mob.hp=10;mob.damage=std::make_unique<MobDamageLedger>(pool);
    Hit hit(actor,*mob.damage);MobCombatSchedulerBindings bindings{true};World world(actor,mob,bindings);world.found=1;world.range=1;
    world.on_attack=[&] {
        MobCombatState state;state.hp=mob.hp;state.state=mob.status;
        MobCombatAttacker user{42,3,77};
        if(!mob_combat_user_hit(state,*mob.damage,hit,user,std::array<std::int32_t,3>{20,0,0},20,0))throw std::runtime_error("lethal scheduler hit rejected");
        mob.hp=state.hp;mob.status=state.state;
    };
    if(!mob_combat_scheduler(actor,bindings,100,world)||hit.deaths!=1||actor.attack_state().mode!=0||actor.attack_state().resume_tick!=2350||actor.attack_state().motion!=0||mob.hp!=0||scene.calls!=0)
        throw std::runtime_error("native scheduler/hit/stop/death composition failed");
}

}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};
        unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::uint32_t now,resume,mode,next,recovery,kind,speed;unsigned weapon,blocked,found,range,both,post,mutation,result;
            std::string final,events;std::istringstream row(line);row>>now>>resume>>mode>>next>>recovery>>kind>>speed>>weapon>>blocked>>found>>range>>both>>post>>mutation>>result>>final>>events;
            if(!row)throw std::runtime_error("invalid fixture");
            Scene scene;EffectStatus status;status.current_target_type=std::bit_cast<std::int32_t>(kind);status.current_target=kind==2?77:42;status.skills.equipment.attack_speed=std::bit_cast<std::int32_t>(speed);
            Actor actor(42,status,{}, {},scene,rules);scene.calls=0;actor.appearance().identity.name="Hero";
            auto& attack=actor.attack_state();attack.mode=std::bit_cast<std::int32_t>(mode);attack.recovery=std::bit_cast<std::int32_t>(recovery);attack.previous_tick=55;attack.next_tick=next;attack.resume_tick=resume;attack.motion=0xa5;
            MobRuntime mob;mob.previous_attack_user.fill(0xa5);mob.previous_attack_skill.fill(0x5a);MobCombatSchedulerBindings bindings{weapon!=0};World world(actor,mob,bindings);
            actor.status().action_block_b=static_cast<std::int32_t>(blocked);world.blocked=blocked;world.found=found;world.range=range;world.both=both;world.post=post;world.mutation=mutation;
            const bool actual=mob_combat_scheduler(actor,bindings,now,world);if(events!="-") {
                std::istringstream trace(events);std::string event,filtered;
                while(std::getline(trace,event,'|'))if(!event.starts_with("Q:")){if(!filtered.empty())filtered+='|';filtered+=event;}
                events=filtered;
            }
            if(events=="-")events.clear();
            if(actual!=static_cast<bool>(result)||world.snapshot()!=final||world.events!=events||scene.calls!=0)
                throw std::runtime_error("scheduler mismatch "+std::to_string(count)+" actual "+world.snapshot()+" expected "+final+"\nactual events "+world.events+"\nexpected events "+events);
            ++count;
        }
        compose(rules);
        if(count!=1536)throw std::runtime_error("incomplete scheduler fixture");std::cout<<count<<" complete normal-mob scheduler comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
