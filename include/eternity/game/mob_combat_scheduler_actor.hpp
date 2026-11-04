#pragma once
#include "eternity/game/mob_combat_scheduler_users.hpp"
namespace eternity::game {
// Bind existing Actor effect/action/transport owners; no shadow replica or
// replacement cancellation logic. ActorScene and actual action/world providers
// must remain valid through nested sends and the complete polling frame.
class MobCombatSchedulerActorWorld:public MobCombatSchedulerUserWorld,public ActorActionWorld {
public:
    void remove_mob_transform(Actor&) final;
    void cancel_move(Actor&) final;
    void send_self(Actor&,const net::Packet&) final;
};
}
