#include "eternity/game/mob_combat_scheduler_actor.hpp"
namespace eternity::game {
void MobCombatSchedulerActorWorld::remove_mob_transform(Actor& actor){actor.cancel_shape_on_attack();}
void MobCombatSchedulerActorWorld::cancel_move(Actor& actor){actor.cancel_move(*this);}
void MobCombatSchedulerActorWorld::send_self(Actor& actor,const net::Packet& packet){actor.send_self(packet);}
}
