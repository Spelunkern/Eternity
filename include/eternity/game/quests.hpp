#pragma once
#include "eternity/game/entry_packets.hpp"

namespace eternity::game {
class QuestDefinition {
public:
    std::uint16_t id() const;
    std::string name() const;
    std::uint32_t minimum_minutes() const;
    std::uint32_t maximum_minutes() const;
    std::uint8_t end_type() const {return record_[0x147];}
    // Portable, pointer-free recovered definition fields, including all original requirements/rewards.
    const std::array<std::uint8_t,464>& record() const noexcept {return record_;}
private:
    friend class QuestDefinitions;
    std::array<std::uint8_t,464> record_{};
};
class QuestDefinitions {
public:
    static QuestDefinitions decode(std::span<const std::uint8_t> quest_block);
    static QuestDefinitions npc_quest(std::span<const std::uint8_t> file);
    const QuestDefinition* find(std::uint16_t id) const;
    std::size_t size() const noexcept {return definitions_.size();}
    std::size_t consumed() const noexcept {return consumed_;}
private:
    std::vector<QuestDefinition> definitions_;
    std::size_t consumed_=0;
};
struct RestoredQuest {
    QuestPacketEntry packet;
    std::uint32_t start_tick=0,success_tick=0;
    std::uint8_t success=0;
};
struct RestoredQuests {
    std::vector<RestoredQuest> active;
    std::vector<FinishedQuest> finished;
    std::vector<std::uint16_t> invalid_active;
    std::uint32_t player_kill_quests=0,mob_kill_quests=0;
};
RestoredQuests restore_quests(const PlayerSnapshot& player,const QuestDefinitions& definitions,std::uint32_t tick);
}
