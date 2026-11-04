#include "eternity/game/vertical_persistence.hpp"
#include <stdexcept>

namespace eternity::game {
std::unique_ptr<CharacterReplica> vertical_persistence_restore(const PlayerSnapshot& snapshot, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    for(const auto& item:snapshot.items) if(item.relocated) throw std::runtime_error("Relocated inventory requires load-time repair");
    for(const auto& item:snapshot.bank) if(item.relocated) throw std::runtime_error("Relocated bank requires load-time repair");
    if(!snapshot.overflow_items.empty() || !snapshot.overflow_bank.empty()) throw std::runtime_error("Overflow inventory requires load-time repair");
    auto cache=std::make_unique<CharacterReplica>(snapshot.core,pool,skills,quests,points);
    for(const auto& item:snapshot.items) if(!cache->inventory().load(item)) throw std::runtime_error("Inventory allocation failed");
    for(const auto& item:snapshot.bank) if(!cache->inventory().load(item,true)) throw std::runtime_error("Bank allocation failed");
    if(!cache->skills().load(snapshot.skills,snapshot.effects)) throw std::runtime_error("Skill loading failed");
    // DBCharacter::LoadCharacter 00420610 skips quest rows on allocation failure.
    for(const auto& quest:snapshot.quests) (void)cache->quests().load(quest);
    for(const auto& quest:snapshot.finished_quests) (void)cache->quests().load_finished(quest);
    if(!cache->interface().load(snapshot) || !cache->market().load(snapshot) || !cache->points().load(snapshot) || !cache->purchases().load(snapshot)) throw std::runtime_error("Character auxiliary loading failed");
    cache->growth().load(max_grow);
    return cache;
}
std::unique_ptr<CharacterReplica> vertical_persistence_restore_with_products(const PlayerSnapshot& snapshot, PointItemStore& products, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    auto replica=vertical_persistence_restore(snapshot,max_grow,pool,skills,quests,points);
    products.load_products(replica->points());
    return replica;
}
std::optional<VerticalPersistenceSelection> vertical_persistence_select(Players& players, PointItemStore& products, std::uint32_t user, std::uint32_t character, const std::string& username, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    auto snapshot=players.load(user,character,username);
    if(!snapshot) return std::nullopt;
    auto replica=vertical_persistence_restore_with_products(*snapshot,products,max_grow,pool,skills,quests,points);
    return VerticalPersistenceSelection{std::move(*snapshot),std::move(replica)};
}
}
