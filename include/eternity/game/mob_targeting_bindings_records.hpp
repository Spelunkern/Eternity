#pragma once
#include "eternity/game/mob_targeting_bindings.hpp"
namespace eternity::game {
// Complete original normalized startup records; skill pointers use stable slot
// tokens, never truncated native addresses. Quest scratch is retained across
// mob rows just as the original loader's separate GetData destination.
std::array<std::byte, 160>
decode_mob_targeting_skill_record(const storage::Row &);
std::array<std::byte, 268>
decode_mob_targeting_mob_record(const storage::Row &,
                                const MobTargetingBindingCatalog &,
                                std::uint32_t &quest_scratch);
} // namespace eternity::game
