#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/blessing.hpp"
#include "eternity/game/quests.hpp"
#include <memory>

namespace eternity::game {
struct ActorLoadRules {
    const StatusRules& statuses;
    const ItemDefinitions& items;
    const EnchantValues& enchant;
    const SkillDefinitions& skills;
    const QuestDefinitions& quests;
    const MobAttackTemplates& mobs;
};
struct ActorLoadOptions {
    std::uint32_t tick_ms=0,disconnected_seconds=0;
    std::int32_t billing_type=0;
    const BlessingDefinition* blessing=nullptr;
};
struct LoadedActor {
    std::unique_ptr<Actor> actor;
    RestoredSkills learned;
    RestoredQuests quests;
    std::array<std::int32_t,28> blessing{};
    std::array<std::int32_t,3> attack_recovery{};
};
// Recovered loading/initialization phases. Unknown learned definitions return learned.invalid
// and no actor: the connection owner must log/disconnect, as the original does.
LoadedActor load_actor(const PlayerSnapshot& player,const ActorLoadRules& rules,ActorLoadOptions options,ActorScene& scene);
}
