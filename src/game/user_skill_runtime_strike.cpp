#include "eternity/game/user_skill_runtime_strike.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
void user_skill_runtime_strike(Actor& actor,Actor* target,const UserSkillDefinition& info,UserSkillStrikeWorld& world){
    if(info.target_kind==6){world.range(actor,target,info);return;}
    if(info.target_kind==7){world.range_select(actor,target,info);return;}
    if(info.target_kind!=3)return;
    const auto detail=info.effect.detail;
    if(detail<21||detail>27)return;
    if(!target)throw std::invalid_argument("Selected user strike requires retained actual target");
    if(detail==22){
        world.announce_multi(actor,*target,info);
        unsigned count=0;
        if(info.multi_attack==0)return;
        do {
            world.hit(actor,*target,info,false,true);
            if(target->attack_state().mode==1)return;
            if(actor.attack_state().mode==1)return;
            ++count;
        }while(count<info.multi_attack);
        return;
    }
    if(detail==23){
        const auto& relation=actor.relations();
        const bool forced=(relation.duel_state==4&&target->relations().duel_opponent==actor.id()&&relation.duel_opponent==target->id())||relation.opposing_guild_id!=0;
        world.hit(actor,*target,info,true,true);
        if(actor.attack_state().mode==1)return;
        const auto hp=std::bit_cast<std::int32_t>(actor.resources().hp);
        if(world.self_damage(actor,hp))world.self_death(actor,forced);
        return;
    }
    world.hit(actor,*target,info,true,true);
}
}
