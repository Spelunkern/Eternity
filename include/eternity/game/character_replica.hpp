#pragma once
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/skill_replica.hpp"
#include "eternity/game/quest_replica.hpp"

namespace eternity::game {
// Serialized DB-agent owner for core, inventory, skills and quests. Money has one authority:
// core() projects the inventory cache's balance, including gem/craft/repair updates.
// Optional shared skill/quest pools must outlive this character.
class CharacterReplica {
public:
    CharacterReplica(PlayerCore core,InventoryReplicaPool& pool,SkillReplicaPool* skills=nullptr,QuestReplicaPool* quests=nullptr):core_(std::move(core)),inventory_(core_.user_id,core_.money,pool),skills_(core_.user_id,core_.skill_points,skills?*skills:owned_skill_pool_),quests_(core_.user_id,quests?*quests:owned_quest_pool_) {}
    PlayerCore core() const {auto result=core_;result.money=inventory_.money();return result;}
    InventoryReplica& inventory() noexcept {return inventory_;}
    const InventoryReplica& inventory() const noexcept {return inventory_;}
    SkillReplica& skills() noexcept {return skills_;}
    const SkillReplica& skills() const noexcept {return skills_;}
    QuestReplica& quests() noexcept {return quests_;}
    const QuestReplica& quests() const noexcept {return quests_;}
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    bool body_changed() const noexcept {return body_changed_;}
    // Core portion of CUser::SaveSuccess 00409560, after complete character commit.
    void core_save_success() noexcept {body_changed_=false;}
private:
    PlayerCore core_;InventoryReplica inventory_;SkillReplicaPool owned_skill_pool_;SkillReplica skills_;
    QuestReplicaPool owned_quest_pool_;QuestReplica quests_;bool body_changed_=false;
};
}
