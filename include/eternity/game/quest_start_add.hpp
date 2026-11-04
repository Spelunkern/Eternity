#pragma once
#include "eternity/game/quests.hpp"
namespace eternity::game {
// Live projection: callbacks may change billing/counters before later reads.
struct QuestStartAddUser {
    std::uint32_t& billing_id;
    std::int32_t& mob_kill_quests;
    std::int32_t& player_kill_quests;
};
class QuestStartAddWorld {
public:
    virtual ~QuestStartAddWorld()=default;
    virtual RestoredQuest* allocate()=0; // Pool owns storage; nullptr is ordinary failure.
    virtual std::uint32_t tick_ms()=0;
    // Append/claim the initialized quest and increment active-list size only.
    // MUST NOT update mob/player counters: quest_start_add does that afterwards.
    virtual void insert_active(RestoredQuest&)=0;
    virtual void send_database(const std::array<std::uint8_t,10>&)=0;
};
// ps_game.exe CUser::QuestAdd 0047e010. Definition and pool outlive the quest.
// No duplicate, admission, capacity, client or item checks occur in this function.
bool quest_start_add(const QuestDefinition&,QuestStartAddUser&,QuestStartAddWorld&);
}
