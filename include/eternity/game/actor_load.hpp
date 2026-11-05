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
    std::uint8_t country=2;
    GroundItemPool* inventory_pool=nullptr; // Live runtime owner must outlive the loaded actor.
};
struct LoadedActor {
    std::unique_ptr<Actor> actor;
    RestoredSkills learned;
    RestoredQuests quests;
    std::array<std::int32_t,28> blessing{};
    std::array<std::int32_t,3> attack_recovery{};
};
// Separates DB restoration from the four 0407 initialization calls, so a live
// entry owner can preserve manager notification / initialization / ACK order.
// Snapshot, rules and scene outlive the pipeline; discard it after any error.
class ActorLoadPipeline {
public:
    ActorLoadPipeline(const PlayerSnapshot&,const ActorLoadRules&,ActorLoadOptions,ActorScene&);
    const LoadedActor& result() const noexcept {return result_;}
    const PlayerSnapshot& snapshot() const noexcept {return player_;}
    void equipment();
    void passive();
    void effects();
    void billing();
    LoadedActor release();
private:
    void advance(unsigned int expected);
    const PlayerSnapshot& player_;const ActorLoadRules& rules_;ActorLoadOptions options_;
    LoadedActor result_;unsigned int stage_=0;
};
// Recovered loading/initialization phases. Unknown learned definitions return learned.invalid
// and no actor: the connection owner must log/disconnect, as the original does.
LoadedActor load_actor(const PlayerSnapshot& player,const ActorLoadRules& rules,ActorLoadOptions options,ActorScene& scene);
}
