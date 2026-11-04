#pragma once
#include "eternity/game/quests.hpp"

namespace eternity::game {
class QuestRequestsTimerWorld {
public:
    virtual ~QuestRequestsTimerWorld()=default;
    virtual void lock_active_quests()=0;
    virtual void unlock_active_quests() noexcept=0;
    // Bind the SAME shared cursor as QuestFind/QuestRemove. Removing its current
    // quest backs the cursor to its predecessor/head; next must read it afresh.
    virtual RestoredQuest* first_active_quest()=0;
    virtual RestoredQuest* next_active_quest()=0;
    // Required real QuestEnd provider: forwards NPC=0, success=false. Owns the
    // 0903 client failure, QuestRemove/0a02 DB transition and 0502 game log.
    virtual void end_quest(RestoredQuest&,std::uint32_t npc,bool success)=0;
};
// ps_game.exe CUser::UpdateQuest 0048d1a0. Caller supplies one fixed DWORD tick;
// definition maximum_minutes != 0 and unsigned end_tick <= tick mean expiration.
void quest_requests_update_timers(std::uint32_t tick,QuestRequestsTimerWorld&);
}
