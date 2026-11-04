#include "eternity/game/mob_combat_scheduler.hpp"
#include <algorithm>
#include <array>
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


std::string hex(const net::Packet& bytes){const char* digits="0123456789abcdef";std::string out;for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
EffectStatus status_from(const std::array<std::uint32_t,7>& values) {
    EffectStatus s;s.skills.equipment.attack_speed=std::bit_cast<std::int32_t>(values[0]);
    s.action_block_a=std::bit_cast<std::int32_t>(values[1]);s.action_block_b=std::bit_cast<std::int32_t>(values[2]);s.casting=std::bit_cast<std::int32_t>(values[3]);s.fixed_form=std::bit_cast<std::int32_t>(values[4]);s.movement_block=std::bit_cast<std::int32_t>(values[5]);s.movement_restricted=std::bit_cast<std::int32_t>(values[6]);return s;
}
struct World final:MobCombatSchedulerWorld,ActorActionWorld {
    Actor& actor;MobRuntime mob;unsigned mutation=0;std::string events;
    explicit World(Actor& a):actor(a){}
    std::string snapshot() const {
        const auto& a=actor.attack_state();const auto& s=actor.status();std::ostringstream out;
        out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<a.previous_tick<<','<<a.next_tick<<','<<a.resume_tick<<','<<static_cast<unsigned>(a.motion)<<','<<static_cast<std::uint32_t>(s.skills.equipment.attack_speed)<<','<<static_cast<std::uint32_t>(s.action_block_a)<<','<<static_cast<std::uint32_t>(s.action_block_b)<<','<<static_cast<std::uint32_t>(s.casting)<<','<<static_cast<std::uint32_t>(s.fixed_form)<<','<<static_cast<std::uint32_t>(s.movement_block)<<','<<static_cast<std::uint32_t>(s.movement_restricted)<<','<<static_cast<std::uint32_t>(mob.status);return out.str();
    }
    void event(char tag,const std::string& extra=""){if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot();if(!extra.empty())events+=','+extra;}
    void remove_mob_transform(Actor&) override{event('T');}
    MobRuntime* enable_mob(Actor&,std::uint32_t) override{event('E');return &mob;}
    bool in_range(Actor& a,MobRuntime&) override {
        event('R');if(mutation&1){auto& s=a.status();s.action_block_a=-1;s.action_block_b=-1;s.casting=-1;s.fixed_form=-1;s.skills.equipment.attack_speed=9;}return true;
    }
    bool is_both_mob(MobRuntime&) override{event('B');return false;}
    void attack_mob(Actor& a,MobRuntime& m) override{event('A');if((mutation&1)&&attack_speed(a.status())!=255)throw std::runtime_error("late blocker missing");m.status=mutation&2?2:0;}
    void cancel_move(Actor& a) override{event('C');a.cancel_move(*this);}
    void send_self(Actor&,const net::Packet& packet) override{event('P',hex(packet));}
    bool remainder(Actor&,std::uint32_t,MobCombatSchedulerRemainder) override{throw std::runtime_error("unexpected remaining route");}
    Actor* find_user(Actor&,std::uint32_t) override{throw std::runtime_error("unexpected movement lookup");}
    void send_move_cancel(Actor&,const net::Packet&) override{throw std::runtime_error("unexpected movement send");}
};
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};unsigned getters=0,frames=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            char tag;std::array<std::uint32_t,7> values{};std::istringstream row(line);row>>tag;for(auto& v:values)row>>v;
            auto status=status_from(values);
            if(tag=='G') {
                unsigned gate,now;row>>gate>>now;
                const auto before=status;
                if(!row||attack_speed(status)!=gate||static_cast<unsigned>(std::clamp(status.skills.equipment.attack_speed,0,9))!=now||status.action_block_a!=before.action_block_a||status.skills.equipment.attack_speed!=before.skills.equipment.attack_speed)
                    throw std::runtime_error("getter mismatch "+std::to_string(getters));
                ++getters;
            } else if(tag=='F') {
                std::uint32_t now,resume;unsigned mutation,result;std::string final,events;row>>now>>resume>>mutation>>result>>final>>events;if(!row)throw std::runtime_error("invalid frame");
                status.current_target_type=2;status.current_target=77;Scene scene;Actor actor(42,status,{}, {},scene,rules);scene.calls=0;
                auto& a=actor.attack_state();a.mode=2;a.previous_tick=55;a.next_tick=1;a.resume_tick=resume;a.motion=0xa5;a.recovery=0;
                World world(actor);world.mutation=mutation;MobCombatSchedulerBindings bindings{true};
                const bool actual=mob_combat_scheduler(actor,bindings,now,world);if(events=="-")events.clear();
                if(actual!=static_cast<bool>(result)||world.snapshot()!=final||world.events!=events||scene.calls!=0)
                    throw std::runtime_error("real-speed polling mismatch "+std::to_string(frames)+" actual "+world.snapshot()+" expected "+final+"\nactual events "+world.events+"\nexpected events "+events);
                ++frames;
            } else throw std::runtime_error("invalid fixture tag");
        }
        if(getters!=2048||frames!=1024)throw std::runtime_error("incomplete real-speed fixture");
        std::cout<<getters<<" original getter pairs and "<<frames<<" real-speed polling comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
