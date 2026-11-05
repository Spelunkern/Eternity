#pragma once
#include "eternity/game/character_save.hpp"
#include "eternity/game/players.hpp"
#include <memory>

namespace eternity::game {
// Builds the DB-agent cache from the selected snapshot. Pools and the returned
// replica must share the lifetime and serialization of the real DB user owner.
// Duplicates with free slots are marked for the original later save; excess
// duplicates return to the shared item pool after the catalog fetch. When raw
// item catalogs exist, snapshot slots are updated to match allocation results.
std::unique_ptr<CharacterReplica> vertical_persistence_restore(
    PlayerSnapshot&, std::uint8_t max_grow, InventoryReplicaPool&,
    SkillReplicaPool* = nullptr, QuestReplicaPool* = nullptr, PointItemPool* = nullptr);
// Use after the entry owner has performed its one Players::load call.
// Product loading can throw; the caller must discard the incomplete selection.
std::unique_ptr<CharacterReplica> vertical_persistence_restore_with_products(
    PlayerSnapshot&, PointItemStore&, std::uint8_t max_grow, InventoryReplicaPool&,
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
