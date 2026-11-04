#include "eternity/game/mob_combat_scheduler.hpp"
#include "eternity/game/mob_combat_scheduler_skills.hpp"
#include "eternity/game/mob_combat_scheduler_users.hpp"
#include "eternity/game/mob_combat_stop.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
namespace {
// ps_game.exe g_ConstUserAttackSpeed 005424e0, indexed by GetAttackSpeedNow.
constexpr std::uint32_t delays[]={4000,3000,2750,2500,2250,2000,1750,1500,1250,1000};
void deadline(Actor& actor,std::uint32_t now) {
    const auto speed=std::clamp(actor.status().skills.equipment.attack_speed,0,9);
    actor.attack_state().resume_tick=now+delays[speed];
}
void failure(Actor& actor,std::uint8_t reason,MobCombatSchedulerWorld& world) {
    net::Packet packet{2,5,reason};const auto id=actor.id();
    for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(id>>(8*i)));
    packet.resize(17,0);world.send_self(actor,packet);
}
}
bool mob_combat_scheduler(Actor& actor,MobCombatSchedulerBindings& bindings,std::uint32_t now,MobCombatSchedulerWorld& world) {
    auto& attack=actor.attack_state();
    if(attack.resume_tick>now)return false; // unsigned absolute comparison
    if(attack.mode==3)return world.remainder(actor,now,MobCombatSchedulerRemainder::casting);
    if(attack.mode!=2)return true;
    if(world.attack_speed(actor)==255){mob_combat_stop(actor);failure(actor,10,world);return false;}
    world.remove_mob_transform(actor);
    if(attack.next_tick==2)return world.remainder(actor,now,MobCombatSchedulerRemainder::skill_after_transform);
    if(attack.next_tick!=1)return true;
    if(!bindings.weapon_present){mob_combat_stop(actor);failure(actor,7,world);return false;}
    const auto kind=actor.status().current_target_type;
    if(kind==1) {
        if(actor.status().current_target==actor.id()){mob_combat_stop(actor);return false;}
        return world.remainder(actor,now,MobCombatSchedulerRemainder::normal_user);
    }
    if(kind==2) {
        auto* mob=world.enable_mob(actor,actor.status().current_target);
        if(!mob){mob_combat_stop(actor);return false;}
        if(!world.in_range(actor,*mob)) {
            deadline(actor,now);mob_combat_stop(actor);failure(actor,5,world);return false;
        }
        const bool naming=world.is_both_mob(*mob);
        if(naming) {
            const auto& name=actor.appearance().identity.name;
            const auto length=std::min(name.size(),name.find('\0'));
            if(length>=mob->previous_attack_user.size())throw std::invalid_argument("Actor name exceeds mob attack context");
            std::copy_n(name.begin(),length,mob->previous_attack_user.begin());mob->previous_attack_user[length]=0;
        }
        world.attack_mob(actor,*mob);
        if(naming){mob->previous_attack_user[0]=0;mob->previous_attack_skill[0]=0;}
        const auto status=mob->state_stack?(mob->state_stack->empty()?0:mob->state_stack->back()):mob->status;
        if(status==2)mob_combat_stop(actor);
    }
    deadline(actor,now);world.cancel_move(actor);attack.motion=0;return true;
}
void mob_combat_scheduler_next(Actor& actor,bool force_stop,MobCombatSchedulerWorld& world) {
    auto& attack=actor.attack_state();
    if(force_stop || attack.previous_tick==0){mob_combat_stop(actor);return;}
    if(attack.mode!=1 && attack.mode!=2) {
        if(attack.mode==0) {
            world.send_self(actor,net::Packet{0x12,0x02});
            if(attack.recovery<2)attack.recovery=2;
        }
        attack.mode=2;
    }
    attack.next_tick=1;
    auto& status=actor.status();
    if(status.current_target_type==1 && status.current_target==actor.id()) {
        const auto& actions=actor.actions();
        if(actions.previous_target_type==0){mob_combat_stop(actor);return;}
        status.current_target_type=actions.previous_target_type;
        status.current_target=actions.previous_target;
    }
}

bool MobCombatSchedulerSkillWorld::remainder(Actor& actor,std::uint32_t now,MobCombatSchedulerRemainder stage) {
    if(stage==MobCombatSchedulerRemainder::normal_user)return normal_user_attack(actor,now);
    const bool casting=stage==MobCombatSchedulerRemainder::casting;
    const auto* initial=selected_skill(actor);
    if(!initial){mob_combat_stop(actor);failure(actor,10,*this);return false;}
    const auto reload=[&]() -> const MobCombatSchedulerSkillBinding& {
        const auto* skill=selected_skill(actor);
        if(!skill)throw std::invalid_argument("Selected skill disappeared within attack dispatch");
        return *skill;
    };
    const auto copy_name=[](auto& out,std::string_view name) {
        const auto length=std::min(name.size(),name.find('\0'));
        if(length>=out.size())throw std::invalid_argument("Attack naming context exceeds retained buffer");
        std::copy_n(name.begin(),length,out.begin());out[length]=0;
    };
    bool dead=false;
    const auto kind=actor.status().current_target_type;
    if(kind==2) {
        auto* mob=enable_mob(actor,actor.status().current_target);
        if(!mob){mob_combat_stop(actor);return false;}
        const bool naming=is_both_mob(*mob);
        if(naming){copy_name(mob->previous_attack_user,actor.appearance().identity.name);copy_name(mob->previous_attack_skill,reload().name);}
        const auto& skill=reload();
        if(casting)attack_skill_mob(actor,mob,skill,now);else use_skill_mob(actor,mob,skill,now);
        if(naming){mob->previous_attack_user[0]=0;mob->previous_attack_skill[0]=0;}
        const auto status=mob->state_stack?(mob->state_stack->empty()?0:mob->state_stack->back()):mob->status;
        dead=status==2;
    } else if(kind==1) {
        auto* target=enable_skill_user(actor,actor.status().current_target,initial->target_type);
        if(!target){mob_combat_stop(actor);return false;}
        const auto& skill=reload();
        if(casting)attack_skill_user(actor,*target,skill,now);else use_skill_user(actor,*target,skill,now);
        dead=target->attack_state().mode==1;
    } else if(kind==4) {
        if(casting)attack_skill_mob(actor,nullptr,*initial,now);else use_skill_mob(actor,nullptr,*initial,now);
    }
    if(casting || actor.attack_state().mode!=3)mob_combat_scheduler_next(actor,dead,*this);
    if(casting)deadline(actor,now);
    cancel_move(actor);actor.attack_state().motion=1;return true;
}

bool MobCombatSchedulerUserWorld::normal_user_attack(Actor& actor,std::uint32_t now) {
    const auto requested=actor.status().current_target;
    auto* target=find_attack_user(actor,requested);
    if(!target || target->attack_state().mode==1 ||
       ((!target->appearance().visible || !target->appearance().attackable) && requested!=actor.id()) ||
       target->status().shape==13 || target->id()==actor.id() ||
       same_team(actor,*target,map_team_mode(actor))) {
        mob_combat_stop(actor);return false;
    }
    if(!in_user_range(actor,*target)) {
        deadline(actor,now);mob_combat_stop(actor);failure(actor,5,*this);return false;
    }
    attack_user(actor,*target);
    if(target->attack_state().mode==1)mob_combat_stop(actor);
    deadline(actor,now);cancel_move(actor);actor.attack_state().motion=0;return true;
}

}
