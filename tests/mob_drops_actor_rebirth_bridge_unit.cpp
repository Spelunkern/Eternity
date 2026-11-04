#include "mob_drops_actor_rebirth_support.hpp"
#include "eternity/game/mob_targeting_lifecycle.hpp"
#include <iostream>
using namespace eternity;using namespace eternity::game;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
struct World final:MobTargetingLifecycleWorld {
    std::vector<char> events;bool corpse=false;PlayerResources expected;
    GameLogSubject subject(const Actor& actor) const override {GameLogSubject s;s.character_id=actor.id();s.map=7;s.position={1,2,3};return s;}
    std::uint16_t instance(const Actor&) const override {return 4;}
    std::uint8_t presentation_motion(const Actor& actor) const override {return actor.presentation_motion();}
    void presentation_motion(Actor& actor,std::uint8_t value) override {
        check(actor.attack_state().mode==1 && actor.attack_state().recovery==-7,"Presentation must precede shared life/recovery tail");events.push_back('P');actor.presentation_motion(value);
    }
    void replace_resources(Actor& actor,PlayerResources value) override {events.push_back('R');actor.replace_resources_raw(value);}
    std::optional<std::uint32_t> find_dead(Actor& actor,std::uint32_t id) override {
        check(id==actor.id() && rebirth_test::same(actor.resources(),expected) && actor.attack_state().mode==1 && actor.presentation_motion()==37 && actor.attack_state().motion==0xa5,"FindDead did not observe authoritative raw resources before tail");events.push_back('F');return corpse?std::optional<std::uint32_t>{17}:std::nullopt;
    }
    void leave_dead(Actor& actor,std::uint32_t id) override {check(id==17,"Wrong corpse");events.push_back('L');actor.replace_resources_raw({7,8,9});}
    bool move_user(Actor& actor,std::uint16_t instance,const UserPosition&) override {check(instance==4 && rebirth_test::same(actor.resources(),{7,8,9}),"Live callback resources lost");events.push_back('M');return false;}
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected unbound world operation");}
    bool map_present(std::int32_t) override {unexpected();}
    void zone_leave_user_move(Actor&,std::int32_t,const UserPosition&) override {unexpected();}
    void send_cell_view(Actor&,const net::Packet&) override {unexpected();}
    void send_recover_change(Actor&,PlayerResources) override {unexpected();}
    std::chrono::sys_seconds local_calendar() override {unexpected();}
    void send_game_log(const net::Packet&) override {unexpected();}
};
int main() {try {
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;unsigned cases=0;
    for(bool restore:{false,true}) for(bool corpse:{false,true}) for(bool dead:{false,true}) {
        rebirth_test::Scene scene;EffectStatus status;status.skills.equipment.base.maximum={0xffffffffU,0x80000000U,65536};
        Actor actor(9,status,{1,2,3},{},scene,{items,enchant,mobs});actor.attack_state().mode=dead?1:2;actor.attack_state().recovery=-7;actor.attack_state().motion=0xa5;actor.presentation_motion(37);
        World world;world.corpse=corpse;world.expected=restore?status.skills.equipment.base.maximum:PlayerResources{1,2,3};
        mob_targeting_rebirth(actor,{4,5,6},0,7,restore,world);
        if(dead) {
            std::vector<char> expected;if(restore) expected.push_back('R');expected.push_back('F');if(corpse) {expected.push_back('L');expected.push_back('M');}expected.push_back('P');
            check(world.events==expected && actor.attack_state().mode==0 && actor.attack_state().recovery==1 && actor.presentation_motion()==0,"Actual bridge publication/tail order differs");
            check(rebirth_test::same(actor.resources(),corpse?PlayerResources{7,8,9}:world.expected),"Actual bridge clamped resources or lost live callback changes");
        } else check(world.events.empty() && actor.attack_state().mode==2 && actor.presentation_motion()==37 && rebirth_test::same(actor.resources(),{1,2,3}),"Nondead guard invoked owners");
        check(actor.attack_state().motion==0xa5 && scene.packets.empty() && scene.resets==0,"Bridge aliased scheduler motion or invoked resource callbacks");++cases;
    }
    std::cout<<cases<<" actual worker08 Rebirth bridge compositions use real Actor raw resource/presentation owners; no scratch fields or duplicate life state\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
