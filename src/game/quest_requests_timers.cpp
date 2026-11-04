#include "eternity/game/quest_requests_timers.hpp"
#include <stdexcept>

namespace eternity::game {
void quest_requests_update_timers(std::uint32_t tick,QuestRequestsTimerWorld& world) {
    world.lock_active_quests();
    struct Unlock {QuestRequestsTimerWorld& world;~Unlock(){world.unlock_active_quests();}} unlock{world};
    auto* quest=world.first_active_quest();
    while(quest) {
        if(!quest->definition) throw std::invalid_argument("Quest timer lacks its live definition");
        if(quest->definition->maximum_minutes()!=0 && quest->packet.end_tick<=tick) world.end_quest(*quest,0,false);
        // No precomputed successor or quest dereference after end_quest: removal
        // and reentrant lookups can change the shared owner cursor.
        quest=world.next_active_quest();
    }
}
}
