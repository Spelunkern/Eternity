#pragma once
#include "eternity/game/guild_npc_config.hpp"
#include "eternity/game/guild_npcs.hpp"
#include "eternity/game/npc_shop.hpp"

namespace eternity::game {
// ps_game.exe CGuild::GetGuildHouseShopBuyRate 00430ac0. Caller holds the guild
// lock. Stored levels above 3 and countries above 1 are clamped in place, as in
// the original. A nonzero result means the rank (1) or NPC level (2) is too low.
inline NpcShopBuyRate guild_house_shop_buy_rate(GuildNpcs& npcs,std::int32_t& country,std::uint8_t rank,
    const GuildHouseShopConfig& config,std::uint16_t npc_type_id,std::uint8_t shop_type) {
    if(country<0)throw std::out_of_range("Guild country indexes outside the original table");
    std::uint16_t level=0;
    for(auto& slot:npcs.slots) if(slot.type==static_cast<std::uint16_t>(shop_type<<8|1)) {
        if(slot.level>3)slot.level=3;
        level=slot.level;break;
    }
    NpcShopBuyRate result;result.rate=(shop_type==4?config.potion_buy_rate:config.buy_rate)[level];
    if(country>1)country=1;
    if(shop_type==4 || shop_type==3) {
        const std::uint8_t limit=shop_type==4?6:0x1e;
        if(rank>limit) {result.result=1;result.detail=limit;return result;}
        if(shop_type==4)return result;
    }
    if(shop_type<1 || shop_type>3 || level==3)return result;
    for(std::size_t next=level+1u;next<=3;++next) for(const auto id:config.npc_ids[static_cast<std::size_t>(country)][shop_type][next])
        if(id==npc_type_id) {result.result=2;result.detail=static_cast<std::uint8_t>(next);return result;}
    return result;
}
}
