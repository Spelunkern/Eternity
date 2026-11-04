#include "eternity/game/mob_combat_scheduler.hpp"
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


std::string snapshot(const Actor& actor) {
    const auto& a=actor.attack_state();const auto& s=actor.status();const auto& actions=actor.actions();std::ostringstream out;
    out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<a.previous_tick<<','<<a.next_tick<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<static_cast<std::uint32_t>(actions.previous_target_type)<<','<<actions.previous_target<<','<<a.resume_tick;return out.str();
}
struct World final:MobCombatSchedulerWorld {
    unsigned mutation=0;std::string events;bool polling=false;unsigned hits=0;MobRuntime mob;
    void event(char tag,Actor& actor,const std::string& extra="") {if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot(actor);if(!extra.empty())events+=','+extra;}
    void send_self(Actor& actor,const net::Packet& packet) override {
        if(packet!=net::Packet{0x12,2})throw std::runtime_error("unexpected start packet");event('P',actor,"1202");
        auto& a=actor.attack_state();auto& s=actor.status();auto& actions=actor.actions();
        if(mutation&1){a.recovery=0;a.mode=1;}
        if(mutation&2){s.current_target_type=1;s.current_target=42;actions.previous_target_type=2;actions.previous_target=99;}
        if(mutation&4){a.previous_tick=0;a.next_tick=99;}
        if(mutation&8)actions.previous_target_type=0;
        if(mutation&16){a.mode=3;s.current_target_type=7;event('N',actor);mob_combat_scheduler_next(actor,true,*this);event('n',actor);}
    }
    void unused() const {if(!polling)throw std::runtime_error("StatusNextAttack invoked an unexpected world service");}
    std::int32_t attack_speed(Actor&) override{unused();return 4;}
    void remove_mob_transform(Actor&) override{unused();}
    MobRuntime* enable_mob(Actor&,std::uint32_t id) override{unused();if(id!=77)throw std::runtime_error("restored target not used");return &mob;}
    bool in_range(Actor&,MobRuntime&) override{unused();return true;}
    bool is_both_mob(MobRuntime&) override{unused();return false;}
    void attack_mob(Actor&,MobRuntime&) override{unused();++hits;}
    void cancel_move(Actor&) override{unused();}
    bool remainder(Actor&,std::uint32_t,MobCombatSchedulerRemainder) override{throw std::runtime_error("unexpected scheduler remainder");}
};
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};
        unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned force,mutation;std::uint32_t mode,recovery,previous,phase,kind,id,backup_kind,backup_id,resume;std::string final,events;
            std::istringstream row(line);row>>force>>mutation>>mode>>recovery>>previous>>phase>>kind>>id>>backup_kind>>backup_id>>resume>>final>>events;
            if(!row)throw std::runtime_error("invalid next-attack fixture");
            Scene scene;EffectStatus status;status.current_target_type=std::bit_cast<std::int32_t>(kind);status.current_target=id;
            Actor actor(42,status,{}, {},scene,rules);scene.calls=0;
            auto& a=actor.attack_state();a.mode=std::bit_cast<std::int32_t>(mode);a.recovery=std::bit_cast<std::int32_t>(recovery);a.previous_tick=previous;a.next_tick=phase;a.resume_tick=resume;a.motion=0xa5;
            actor.actions().previous_target_type=std::bit_cast<std::int32_t>(backup_kind);actor.actions().previous_target=backup_id;
            World world;world.mutation=mutation;mob_combat_scheduler_next(actor,force!=0,world);if(events=="-")events.clear();
            if(snapshot(actor)!=final||world.events!=events||scene.calls!=0||a.motion!=0xa5)
                throw std::runtime_error("next-attack mismatch "+std::to_string(count)+" actual "+snapshot(actor)+" expected "+final+"\nactual events "+world.events+"\nexpected events "+events);
            ++count;
        }
        if(count!=2048)throw std::runtime_error("incomplete fixture");
        // Same Actor owner: continuation restores a mob target, then the
        // existing normal poll consumes phase 1 without copying attack state.
        Scene scene;EffectStatus status;status.current_target_type=1;status.current_target=42;status.skills.equipment.attack_speed=4;
        Actor actor(42,status,{}, {},scene,rules);actor.attack_state().previous_tick=55;actor.actions().previous_target_type=2;actor.actions().previous_target=77;
        World world;mob_combat_scheduler_next(actor,false,world);world.polling=true;MobCombatSchedulerBindings bindings{true};
        if(!mob_combat_scheduler(actor,bindings,100,world)||world.hits!=1||actor.attack_state().resume_tick!=2350||actor.status().current_target!=77||actor.attack_state().mode!=2)
            throw std::runtime_error("next-attack/normal-poll composition failed");
        std::cout<<count<<" complete StatusNextAttack comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
