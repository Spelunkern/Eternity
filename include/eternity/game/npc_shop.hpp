#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"
#include "eternity/game/npcs.hpp"

namespace eternity::game {
// CNpc+34 (kind, 1 is a shop) and CNpc+38 (definition type ID).
struct NpcShopEntity {std::uint32_t kind=0,type_id=0;};
// CGuild::GetGuildHouseShopBuyRate 00430ac0: nonzero result rejects with detail.
struct NpcShopBuyRate {std::uint32_t result=0;double rate=1.0;std::uint8_t detail=0;};
enum class NpcShopResult {ignored,closed,bought,sold,no_money,no_space,guild_rejected};
// Required live services, serialized with the actor on the world strand.
class NpcShopWorld:public MoneyWorld {
public:
    virtual std::optional<NpcShopEntity> find_zone_npc(Actor&,std::uint32_t id)=0; // CZone::FindNpc 00416d10.
    virtual const NpcDefinition* find_shop_npc(std::uint32_t type_id)=0; // CNpcData::GetShopNpc 00466c30.
    virtual const ItemDefinition* find_shop_item(std::uint8_t type,std::uint8_t type_id)=0; // CGameData::GetItemInfo 00402710.
    virtual void cancel_action_exchange(Actor&)=0; // CUser::CancleActionExc 0044b3d0.
    virtual void end_my_shop(Actor&)=0; // stMyShop::Ended 00478520.
    virtual bool guild_house_zone(Actor&)=0; // Zone map settings +1a4 equals 6.
    // Holds CGuildLock 0042b800 for the query; nullopt means the user has no guild.
    virtual std::optional<NpcShopBuyRate> guild_house_buy_rate(Actor&,std::uint16_t npc_type_id,std::uint8_t shop_type)=0;
    virtual GroundItemPool& shop_item_pool()=0; // CObjectMgr::CreateItem 0040a510 / FreeItem 0040a590.
    virtual std::uint32_t shop_system_time()=0; // GetSystemTime 004cb8e0.
    virtual std::uint64_t next_shop_item_uid()=0; // SUniqueIDGenByServer::GenID 00457a50.
    virtual void send_shop_database(const net::Packet&)=0;
    virtual std::string shop_item_name(const ItemDefinition&)=0; // Original bytes, at most 31.
    virtual void close_shop_connection(Actor&,std::uint32_t reason)=0; // SConnection::Close 004d43a0.
    // CUser+1564 CNpc+38 at sale time, or zero without a retained interaction.
    virtual std::uint32_t shop_interaction_npc_type_id(Actor&)=0;
};
// ps_game.exe CUser::PacketShop 0046af10, opcodes 0702 (buy) and 0703 (sell).
NpcShopResult npc_shop_packet(Actor&,const net::Packet&,NpcShopWorld&);
}
