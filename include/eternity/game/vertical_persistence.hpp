#pragma once
#include "eternity/game/character_save.hpp"
#include "eternity/game/players.hpp"
#include <memory>

namespace eternity::game {
// Builds the DB-agent cache from the selected snapshot. Pools and the returned
// replica must share the lifetime and serialization of the real DB user owner.
// Relocated/overflow inventory requires the original load-time SQL repair path,
// which is not yet connected; reject it rather than silently lose those rows.
std::unique_ptr<CharacterReplica> vertical_persistence_restore(
    const PlayerSnapshot&, std::uint8_t max_grow, InventoryReplicaPool&,
    SkillReplicaPool* = nullptr, QuestReplicaPool* = nullptr, PointItemPool* = nullptr);
// Selection uses Players' original SQL loaders; product rows are account-owned
// and loaded by the existing PointItemStore on the same selected replica.
struct VerticalPersistenceSelection {
    PlayerSnapshot snapshot;
    std::unique_ptr<CharacterReplica> replica;
};
std::optional<VerticalPersistenceSelection> vertical_persistence_select(
    Players&, PointItemStore&, std::uint32_t user, std::uint32_t character,
    const std::string& username, std::uint8_t max_grow, InventoryReplicaPool&,
    SkillReplicaPool* = nullptr, QuestReplicaPool* = nullptr, PointItemPool* = nullptr);
}
