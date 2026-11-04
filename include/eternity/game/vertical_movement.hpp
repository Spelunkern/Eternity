#pragma once
#include "eternity/game/zone_lifecycle.hpp"
#include <functional>
namespace eternity::game {
// ps_game.exe stUserMoveChk::Update 0046a120 and Reset 0046a0b0.
// The callback supplies each actual GetTickCount sample. The caller must pass
// the canonical actor's retained movement check, serialized with that actor.
bool vertical_movement_update(ZoneMovementCheck&, UserPosition destination, float speed,
                              const std::function<std::uint32_t()>& tick);
}
