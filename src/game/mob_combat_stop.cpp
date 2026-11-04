#include "eternity/game/mob_combat_stop.hpp"
#include "eternity/game/actor.hpp"
namespace eternity::game {
void mob_combat_stop(Actor& actor) {actor.stop_attack(actor.status());}
}
