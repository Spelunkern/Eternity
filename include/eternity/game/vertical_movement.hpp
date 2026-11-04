#pragma once
#include "eternity/game/zone_lifecycle.hpp"
#include <functional>
namespace eternity::game {
// ps_game.exe stUserMoveChk::Update 0046a120 and Reset 0046a0b0.
// The callback supplies each actual GetTickCount sample. The caller must pass
// the canonical actor's retained movement check, serialized with that actor.
// Requires x86-64 SSE2, nearest rounding and gradual underflow, finite operands
// and finite staged results. Unsupported cases throw; exception flags are not reproduced.
// Original SVector::Distance2 0041b1f0; same finite/environment contract.
float vertical_movement_planar_distance(UserPosition,UserPosition);
bool vertical_movement_update(ZoneMovementCheck&, UserPosition destination, float speed,
                              const std::function<std::uint32_t()>& tick);
}
