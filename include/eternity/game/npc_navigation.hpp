#pragma once
#include "eternity/game/zone_npcs.hpp"

namespace eternity::game {
// CNpc::Chase 004bca00 and ReturnHome 004bcf30. The owner supplies the actual
// zone/map, target position and serialized update tick. Target selection/combat
// and idle dispatch remain separate parts of guard AI.
bool chase_npc(Npc&,UserPosition target,std::int32_t range,std::uint32_t tick,const MapDefinition&,NpcNavigationWorld&);
bool return_npc_home(Npc&,UserPosition home,std::uint32_t tick,const MapDefinition&,NpcNavigationWorld&);
}
