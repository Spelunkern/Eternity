#include "eternity/game/vertical_combat_skill_select.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
void vertical_combat_skill_select(Actor& actor,MobRuntime& mob,const UserSkillDefinition& info,VerticalCombatSkillSelectWorld& world){
    const auto detail=info.effect.detail;
    if(detail<21||detail>27)return;
    if(detail==22){
        world.announce_multi(actor,mob,info);
        unsigned count=0;
        if(info.multi_attack==0)return;
        do {
            world.hit(actor,mob,info,false,true);
            if(mob_status(mob)==2||actor.attack_state().mode==1)return;
            ++count;
        }while(count<info.multi_attack);
        return;
    }
    world.hit(actor,mob,info,true,true);
    if(detail==23&&actor.attack_state().mode!=1){
        const auto hp=std::bit_cast<std::int32_t>(actor.resources().hp);
        if(world.self_damage(actor,hp)){
            world.stop_mob_attack(mob);
            world.self_death(actor);
        }
    }
}
void VerticalCombatSkillSelectServices::mob_effect(UserSkillAction action,Actor& actor,MobRuntime* mob,const UserSkillDefinition& info){
    if(action!=UserSkillAction::attack_select){other_mob_effect(action,actor,mob,info);return;}
    if(!mob)throw std::invalid_argument("Selected mob attack requires retained target");
    vertical_combat_skill_select(actor,*mob,info,*this);
}
}
