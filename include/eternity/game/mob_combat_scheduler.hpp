#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/mobs.hpp"
namespace eternity::game {
enum class MobCombatSchedulerRemainder { casting, skill_after_transform, normal_user };
struct MobCombatSchedulerBindings {bool weapon_present=false;};
class MobCombatSchedulerWorld {
public:
    virtual ~MobCombatSchedulerWorld()=default;
    virtual std::int32_t attack_speed(Actor&)=0; // GetAttackSpeed 0048a460
    virtual void remove_mob_transform(Actor&)=0; // RemApplySkillToMOB 00489560
    virtual MobRuntime* enable_mob(Actor&,std::uint32_t id)=0; // 0044cd80
    virtual bool in_range(Actor&,MobRuntime&)=0; // 0044c030, argument 0
    virtual bool is_both_mob(MobRuntime&)=0; // 0044c610
    virtual void attack_mob(Actor&,MobRuntime&)=0; // normal AttackMob 0044ce10
    virtual void cancel_move(Actor&)=0; // bind Actor::cancel_move to real action world
    virtual void send_self(Actor&,const net::Packet&)=0; // SConnection::Send 004d4d20
    // Explicit remaining UpdateAttack branches, NOT default success/no-op routes.
    // Stage identifies which recovered prefix has already executed.
    virtual bool remainder(Actor&,std::uint32_t now,MobCombatSchedulerRemainder)=0;
};
// CUser::UpdateAttack 0048ca50 normal-mob dispatch, reusing Actor's existing
// attack/target state. Bindings stay live across callbacks. No private loop,
// clock sample, target owner or damage implementation is created here.
bool mob_combat_scheduler(Actor&,MobCombatSchedulerBindings&,std::uint32_t now,MobCombatSchedulerWorld&);
// CUser::StatusNextAttack 0048c970. Reuses the same live Actor attack/action
// owners; self delivery may synchronously mutate them before continuation.
void mob_combat_scheduler_next(Actor&,bool force_stop,MobCombatSchedulerWorld&);
}
