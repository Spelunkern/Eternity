#pragma once
#include "eternity/game/quests.hpp"
#include <optional>

namespace eternity::game {
// Explicit live-inventory projection: bags 1..unlocked, 24 slots each; no equipment.
struct QuestRequestItem {
    bool present=false;
    std::uint8_t type=0,id=0,count=0;
    std::int32_t effect=0;
};
struct QuestRequestInventory {
    std::uint32_t account_id=0;
    std::span<std::array<QuestRequestItem,24>> bags;
};
class QuestRequestItems {
public:
    virtual ~QuestRequestItems()=default;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    // Release original item ownership before the projected slot is cleared.
    virtual void release_item(std::uint8_t bag,std::uint8_t slot)=0;
};
bool quest_find_item(QuestRequestInventory,int type,int id,bool consume,QuestRequestItems&);
// Counters are the live quest counters: both original call sites alias this output.
bool quest_find_items(QuestRequestInventory,const std::array<std::uint8_t,464>& definition,
                      std::array<std::uint8_t,3>& counters,bool consume,QuestRequestItems&);
struct QuestRequestNpc {std::int32_t type=0,id=0;};
class QuestRequestWorld:public QuestRequestItems {
public:
    virtual const std::array<std::uint8_t,464>* definition(std::uint16_t)=0;
    virtual RestoredQuest* find_quest(std::uint16_t)=0;
    virtual std::optional<QuestRequestNpc> find_npc(std::uint32_t)=0;
    virtual QuestRequestInventory inventory()=0;
    // Required coordinator provider: original QuestEnd(request NPC value,success=true).
    virtual void complete_quest(RestoredQuest&,std::uint32_t request_npc,bool success)=0;
};
// ps_game.exe CUser::QuestEndRequest 0047f2a0, checks 0047f630..0047f740.
void quest_end_request(std::uint16_t id,std::uint32_t npc,std::int32_t definition_count,QuestRequestWorld&);
}
