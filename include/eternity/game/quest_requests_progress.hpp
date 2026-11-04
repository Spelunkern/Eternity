#pragma once
#include "eternity/game/quests.hpp"
#include "eternity/game/item_quests.hpp"

namespace eternity::game {
class QuestRequestsProgressWorld {
public:
    virtual ~QuestRequestsProgressWorld()=default;
    // Original recursive active-list lock/cursor. Null represents sentinel/end.
    virtual void lock_active_quests()=0;
    virtual void unlock_active_quests() noexcept=0;
    virtual RestoredQuest* first_active_quest()=0;
    virtual RestoredQuest* next_active_quest()=0;
    virtual RestoredQuest* find_quest(std::uint16_t)=0;
    virtual std::uint32_t tick_ms()=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    // QuestEndChkItemPick(0): query/consume the actual inventory, updating the
    // quest's live counters. Required body; may compose quest_find_items.
    virtual bool find_required_items(RestoredQuest&,bool consume)=0;
    // QuestEnd(0,true) may unlink, but the current quest and definitions must
    // remain alive until this update returns (later matching slots still run).
    virtual void complete_quest(RestoredQuest&,std::uint32_t request_npc,bool success)=0;
};
// ps_game.exe CUser::QuestUpdateKillPC 00480020: no victim/time filtering;
// only the FIRST active type-3 quest is considered, including saturation.
void quest_requests_progress_kill_pc(std::uint32_t account_id,std::int32_t have_pc_quests,QuestRequestsProgressWorld&);
// ps_game.exe CUser::QuestUpdatePickItem 00480390. One increment per matching
// slot/event, regardless of received stack size. Link order/duplicates retained.
void quest_requests_progress_pick_item(std::uint32_t account_id,std::int32_t type,std::int32_t id,const ItemQuestLinks&,QuestRequestsProgressWorld&);
}
