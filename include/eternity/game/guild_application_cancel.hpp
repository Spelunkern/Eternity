#pragma once
#include "eternity/game/guild_catalog.hpp"

namespace eternity::game {
// CGuild::JoinRequestCancle, ps_game.exe 0042e040. The caller owns the
// manager lock and keeps the guild, user and DB transport alive during sends.
inline bool guild_application_cancel(GuildUser& user,LoadedGuild& guild,GuildCatalog& catalog,
    const std::function<void(net::Packet)>& send_db) {
    if(catalog.find_membership(&guild.membership)!=&guild)throw std::logic_error("Guild application owner mismatch");
    std::lock_guard guard(guild.mutex);
    auto found=guild.membership.applications.find(user.character_id);
    if(found==guild.membership.applications.end())return false;
    if(found->second && !send_db)throw std::invalid_argument("Missing guild application DB transport");
    auto node=guild.membership.applications.extract(found);auto record=std::move(node.mapped());
    if(!record)return false;
    catalog.directory().remove(record->id);user.request_id=0;
    auto append=[](net::Packet& packet,std::uint32_t value){for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{8,11};append(db,guild.metadata.listing.id);append(db,record->id);send_db(std::move(db));
    net::Packet admin{0x18,13};append(admin,user.character_id);guild.membership.broadcast_admin(std::move(admin));
    return true;
}
}
