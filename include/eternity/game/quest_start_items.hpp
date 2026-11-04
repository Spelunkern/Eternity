#pragma once
#include "eternity/game/inventory.hpp"
namespace eternity::game {
struct QuestStartItemsUser {std::uint32_t& billing_id;std::int32_t& open_bags;};
struct QuestStartItemsResult {std::int32_t bag=0,slot=0;const ItemDefinition* definition=nullptr;};
class QuestStartItemsWorld {
public:
    virtual ~QuestStartItemsWorld()=default;
    virtual const ItemDefinition* find_definition(std::int32_t type,std::int32_t id)=0;
    virtual bool slot_empty(std::int32_t bag,std::int32_t slot)=0;
    virtual GroundItem* create_item()=0; // CObjectMgr::CreateItem / GroundItemPool::allocate, already reset.
    virtual std::uint32_t local_make_time()=0; // Packed local calendar, after definition binding.
    virtual std::uint64_t next_uid()=0; // Required server-wide UID owner, not runtime item IDs.
    // Claim the pointer without pickup/party/log effects. This precedes default
    // craft assignment; keep pointer authoritative and sync any value mirror by DB send.
    virtual void store(std::int32_t bag,std::int32_t slot,GroundItem&)=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
};
// ps_game.exe CUser::QuestAddItem 0047e6f0. Full inventory leaves bag/slot outputs
// untouched; unknown definition/allocation failure clears them to zero.
bool quest_start_items_add(std::int32_t type,std::int32_t id,std::int32_t count,
    QuestStartItemsResult&,QuestStartItemsUser&,QuestStartItemsWorld&);
// CUser::QuestAddItem 0047e620: only successful adds receive the 16-byte 0205.
bool quest_start_items_grant(std::int32_t type,std::int32_t id,std::int32_t count,
    QuestStartItemsUser&,QuestStartItemsWorld&);
}
