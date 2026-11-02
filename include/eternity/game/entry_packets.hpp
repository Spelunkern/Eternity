#pragma once
#include "eternity/game/effects.hpp"
#include <span>

namespace eternity::game {
struct LearnedSkillPacketEntry {
    std::uint16_t id=0;
    std::uint8_t level=0,number=0;
    std::uint32_t end_tick=0;
};
struct ActiveEffectPacketEntry {
    std::uint32_t instance_id=0;
    std::uint16_t skill_id=0;
    std::uint8_t level=0;
    std::uint32_t end_tick=0;
    bool offline_countdown=false;
};
struct QuickSlot {std::uint8_t bar=0,slot=0,bag=0;std::uint16_t number=0;};
struct QuestPacketEntry {std::uint16_t id=0;std::uint32_t end_tick=0;bool timed=false;std::array<std::uint8_t,3> counters{};};
struct FinishedQuest {std::uint16_t id=0;std::uint8_t count=0;};
std::vector<net::Packet> character_items(std::span<const PlayerItem> items,std::int32_t maximum_bag);
std::vector<net::Packet> character_bank(std::span<const PlayerItem> items,std::uint64_t money);
net::Packet character_skills(std::span<const LearnedSkillPacketEntry> skills,std::uint32_t tick);
std::vector<net::Packet> character_effects(std::span<const ActiveEffectPacketEntry> effects,std::uint32_t tick);
net::Packet character_quick_slots(std::span<const QuickSlot> slots);
std::vector<net::Packet> character_quests(std::span<const QuestPacketEntry> quests,std::uint32_t tick);
std::vector<net::Packet> character_finished_quests(std::span<const FinishedQuest> quests);
net::Packet character_speed(std::uint32_t character_id,const EffectStatus& status);
}
