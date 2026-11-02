#pragma once
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/skill_replica.hpp"
#include "eternity/game/quest_replica.hpp"
#include "eternity/game/interface_replica.hpp"
#include "eternity/game/account_state.hpp"
#include "eternity/game/point_items.hpp"

namespace eternity::game {
// Serialized DB-agent character state. Money has one authority:
// core() projects the inventory cache's balance, including gem/craft/repair updates.
// Optional shared skill/quest pools must outlive this character.
class CharacterReplica {
public:
    CharacterReplica(PlayerCore core,InventoryReplicaPool& pool,SkillReplicaPool* skills=nullptr,QuestReplicaPool* quests=nullptr,PointItemPool* points=nullptr):core_(std::move(core)),inventory_(core_.user_id,core_.money,pool),points_(inventory_,points?*points:owned_point_pool_),skills_(core_.user_id,core_.skill_points,skills?*skills:owned_skill_pool_),quests_(core_.user_id,quests?*quests:owned_quest_pool_),interface_(core_.user_id),market_(core_.user_id) {}
    PlayerCore core() const {auto result=core_;result.money=inventory_.money();return result;}
    InventoryReplica& inventory() noexcept {return inventory_;}
    const InventoryReplica& inventory() const noexcept {return inventory_;}
    PointItemReplica& points() noexcept {return points_;}
    const PointItemReplica& points() const noexcept {return points_;}
    SkillReplica& skills() noexcept {return skills_;}
    const SkillReplica& skills() const noexcept {return skills_;}
    QuestReplica& quests() noexcept {return quests_;}
    const QuestReplica& quests() const noexcept {return quests_;}
    InterfaceReplica& interface() noexcept {return interface_;}
    const InterfaceReplica& interface() const noexcept {return interface_;}
    MarketReplica& market() noexcept {return market_;}
    const MarketReplica& market() const noexcept {return market_;}
    GrowthReplica& growth() noexcept {return growth_;}
    const GrowthReplica& growth() const noexcept {return growth_;}
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    bool body_changed() const noexcept {return body_changed_;}
    // Core portion of CUser::SaveSuccess 00409560, after complete character commit.
    void core_save_success() noexcept {body_changed_=false;}
private:
    PlayerCore core_;InventoryReplica inventory_;PointItemPool owned_point_pool_;PointItemReplica points_;SkillReplicaPool owned_skill_pool_;SkillReplica skills_;
    QuestReplicaPool owned_quest_pool_;QuestReplica quests_;InterfaceReplica interface_;MarketReplica market_;GrowthReplica growth_;bool body_changed_=false;
};
}
