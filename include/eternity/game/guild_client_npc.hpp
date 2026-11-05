#pragma once
#include "eternity/game/guild_catalog.hpp"
#include <array>
#include <bit>

namespace eternity::game {
struct GuildNpcUpgradeConfig {
    std::uint32_t default_keep;
    std::array<std::array<std::uint16_t,7>,8> costs;
    std::array<std::array<std::uint16_t,6>,8> upkeep;
};
// Executable initial values before InitGuildHouseIniFile 004319a0 overrides.
inline GuildNpcUpgradeConfig guild_npc_executable_defaults(std::uint32_t default_keep) {
    return {default_keep,{{
        {{2,1000,1500,2000,0,0,0}},{{0x101,100,200,500,0,0,0}},{{0x201,100,200,500,0,0,0}},{{0x301,50,100,200,0,0,0}},
        {{3,500,1000,2000,10000,15000,0}},{{0x401,2000,5000,10000,0,0,0}},{{6,200,500,1000,2000,5000,0}},{{0,0,0,0,0,0,0}}}},{{
        {{2,200,500,800,0,0}},{{0x101,0,0,0,0,0}},{{0x201,0,0,0,0,0}},{{0x301,0,0,0,0,0}},
        {{3,100,200,500,800,1000}},{{0x401,50,200,500,0,0}},{{6,100,200,500,800,1000}},{{0,0,0,0,0,0}}}}};
}
struct GuildNpcUpgradePorts {
    // Actual GuildHouse.ini configuration, including its table overrides.
    const GuildNpcUpgradeConfig& config;
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    // Emit the original 121-byte 0b09 game log through the real game-log owner.
    std::function<void(GuildUser&,const LoadedGuild&,std::uint16_t,std::uint8_t)> game_log;
};
// ps_game.exe PacketGuild 004764a0 (0d25), CGuild::NpcUpgrade 00430640,
// CalculateKeepEtin 00430a50; fixed cost/upkeep tables 00541b78/00541b18.
inline EntryCatalogUpdate guild_client_npc(GuildUser& user,GuildCatalog& catalog,std::span<const std::uint8_t> p,
    const GuildNpcUpgradePorts* services) {
    if(p.size()<2)return EntryCatalogUpdate::rejected;
    if(p[0]!=0x25 || p[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    if(p.size()<5)return EntryCatalogUpdate::rejected;
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    if(!user.guild || user.rank>=3)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(user.guild);if(!guild)throw std::logic_error("Foreign current guild owner");
    if(!services || !services->send_user)throw std::invalid_argument("Missing guild NPC client transport");
    const auto kind=static_cast<std::uint16_t>(p[2]|(std::uint16_t{p[3]}<<8));const auto level=p[4];
    const auto reply=[&](std::uint8_t status){services->send_user(user,net::Packet{0x25,0x0d,status,p[2],p[3],level,0});};
    const auto rank=guild->metadata.listing.rank;
    if(rank>=31 || guild->metadata.use_house!=1 || !guild->metadata.buy_house || level<1 || level>5) {reply(3);return EntryCatalogUpdate::applied;}
    if(kind==2 || kind==0x101 || kind==0x201) {if(level>3){reply(3);return EntryCatalogUpdate::applied;}}
    else if(kind==3) {if(rank>14){reply(4);return EntryCatalogUpdate::applied;}}
    else if(kind==0x401) {if(rank>6){reply(4);return EntryCatalogUpdate::applied;}if(level>3){reply(3);return EntryCatalogUpdate::applied;}}
    else if(kind!=6 && kind!=0x301) {reply(3);return EntryCatalogUpdate::applied;}
    const auto available=std::bit_cast<std::int32_t>(guild->metadata.etin-guild->metadata.keep_etin);
    if(available<1){reply(1);return EntryCatalogUpdate::applied;}
    std::uint16_t cost=0;for(const auto& row:services->config.costs)if(row[0]==kind) {cost=row[level];if(available<cost){reply(1);return EntryCatalogUpdate::applied;}}
    if(!cost){reply(3);return EntryCatalogUpdate::applied;}
    auto& slots=guild->npcs.slots;std::size_t slot=0;while(slot<slots.size() && slots[slot].type!=kind)++slot;
    if(slot<slots.size()) {if(level!=slots[slot].level+1U){reply(2);return EntryCatalogUpdate::applied;}}
    else {if(level!=1){reply(3);return EntryCatalogUpdate::applied;}slot=0;while(slot<slots.size() && slots[slot].type)++slot;}
    if(!services->send_db || !services->game_log)throw std::invalid_argument("Missing guild NPC DB or game-log transport");
    if(slot<slots.size()){slots[slot].type=kind;slots[slot].level=level;}
    guild->membership.broadcast(net::Packet{0x25,0x0d,0,p[2],p[3],level,static_cast<std::uint8_t>(slot)});
    auto keep=services->config.default_keep;
    for(const auto& item:slots)if(item.type)for(const auto& row:services->config.upkeep)if(row[0]==item.type) {
        if(item.level>=row.size())throw std::runtime_error("Guild NPC upkeep level outside recovered table");
        keep+=row[item.level];break;
    }
    guild->metadata.keep_etin=keep;guild->metadata.etin-=cost;
    const auto append=[](net::Packet& out,std::uint32_t value){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet etin{0x23,0x0d};append(etin,guild->metadata.etin);services->send_user(user,std::move(etin));
    net::Packet db{0x0f,0x0b};append(db,guild->metadata.listing.id);db.push_back(p[2]);db.push_back(p[3]);db.push_back(level);db.push_back(static_cast<std::uint8_t>(slot));append(db,guild->metadata.etin);append(db,guild->metadata.keep_etin);services->send_db(std::move(db));
    services->game_log(user,*guild,kind,level);return EntryCatalogUpdate::applied;
}
}
