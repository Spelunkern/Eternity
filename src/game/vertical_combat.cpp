#include "eternity/game/vertical_combat.hpp"
namespace eternity::game {
namespace {
void word(net::Packet& p,std::uint16_t v){p.push_back(v&255);p.push_back(v>>8);}
void dword(net::Packet& p,std::uint32_t v){word(p,v&65535);word(p,v>>16);}
void send(Actor& actor,MobRuntime& mob,VerticalCombatWorld& world,std::uint8_t result,const std::array<std::int32_t,3>& damage){
    net::Packet p;word(p,0x0503);p.push_back(result);dword(p,actor.id());dword(p,mob.id);
    for(auto value:damage)word(p,static_cast<std::uint16_t>(value));
    world.send_attack_view(actor,p);
}
}
void vertical_combat_attack_mob(Actor& actor,MobRuntime& mob,VerticalCombatWorld& world){
    world.remove_sleep(mob);
    const auto type=world.weapon(actor).real_type;
    const unsigned kind=(type==11||type==13||type==14)?1:0;
    if(actor.status().skills.attack_disabled[kind])return;
    if(!world.attack_success(actor,mob,kind)){
        send(actor,mob,world,3,{});return;
    }
    std::array<std::int32_t,3> damage{};
    const bool critical=world.attack_damage(actor,mob,kind,damage);
    if(critical)world.wear_weapon(actor);
    const bool killed=world.apply_hit(actor,mob,damage);
    send(actor,mob,world,critical?1:0,damage);
    if(!killed&&actor.attack_state().mode!=1){
        world.set_target(mob,actor);
        mob.attack_at_moment=true;mob.attacked=true;mob.in_battle=true;
        world.attack_item_option(actor,mob);
    }
}
}
