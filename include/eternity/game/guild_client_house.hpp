#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/money.hpp"

namespace eternity::game {
struct GuildHousePurchasePorts {
    std::uint32_t purchase_price,default_keep;
    MoneyWorld& money;
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    std::function<void(GuildUser&,const LoadedGuild&)> game_log;
};
// ps_game.exe CUser::PacketGuild 004764a0, 0d34 branch 00477211.
// The caller retains actor, guild catalog, services and user across callbacks.
inline EntryCatalogUpdate guild_client_house(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    std::span<const std::uint8_t> packet,const GuildHousePurchasePorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x34 || packet[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    if(!ports.send_user)throw std::invalid_argument("Missing guild house client transport");
    auto* captured=user.guild;auto* locked=captured?catalog.find_membership(captured):nullptr;
    if(captured && !locked)throw std::logic_error("Foreign captured guild owner");
    std::unique_lock<std::recursive_mutex> guard;if(locked)guard=std::unique_lock(locked->mutex);
    auto current=[&]() -> LoadedGuild* {if(!user.guild)return nullptr;auto* result=catalog.find_membership(user.guild);if(!result)throw std::logic_error("Foreign current guild owner");return result;};
    auto* guild=current();std::uint8_t status=0;
    if(!guild || user.rank!=1)status=3;
    else if(guild->metadata.buy_house)status=1;
    else if(actor.money()<ports.purchase_price)status=2;
    const auto reply=[&](std::uint8_t code){net::Packet p{0x34,0x0d,code};auto value=actor.money();for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));ports.send_user(user,std::move(p));};
    if(status){reply(status);return EntryCatalogUpdate::applied;}
    if(!ports.send_db || !ports.game_log)throw std::invalid_argument("Missing guild house database or game-log transport");
    guild->metadata.buy_house=1;actor.money()-=ports.purchase_price;guild->metadata.keep_etin=ports.default_keep;
    reply(0);actor.send_money_database(ports.money);
    guild=current();if(!guild)throw std::logic_error("Guild house owner removed during purchase");
    net::Packet db{0x10,0x0b};for(auto value:{guild->metadata.listing.id,guild->metadata.keep_etin})for(unsigned i=0;i<4;++i)db.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    ports.send_db(std::move(db));guild=current();if(!guild)throw std::logic_error("Guild house owner removed before game log");
    ports.game_log(user,*guild);return EntryCatalogUpdate::applied;
}
}
