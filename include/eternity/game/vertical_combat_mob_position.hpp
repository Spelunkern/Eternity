#pragma once
#include "eternity/game/vertical_combat_range.hpp"
namespace eternity::game {
// Supplies the live CMob::GetCurrentPos 004a6b90 and definition-size reads
// required by combat range. Definition record and secondary state are borrowed
// from their actual owners; implementations must be pure, with no side effects.
// tick_ms remains the shared actual GetTickCount owner. The mob, definition,
// FSM and all providers stay retained and serialized throughout each call.
class VerticalCombatMobPositionServices:public VerticalCombatRangeServices {
public:
 UserPosition mob_current_position(MobRuntime&) final;
 std::uint8_t mob_size(const MobRuntime&) final;
 virtual std::uint32_t mob_secondary_state(const MobRuntime&)=0; // CMob +128
 virtual std::span<const std::byte> mob_definition_record(const MobRuntime&)=0; // actual m_pInfo
};
}
