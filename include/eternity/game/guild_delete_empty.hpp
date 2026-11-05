#pragma once
#include "eternity/game/guild_release_empty.hpp"

namespace eternity::game {
struct GuildDeleteEmptyPorts {
    std::function<void(Actor*,const LoadedGuild&)> game_log;
    std::function<void(net::Packet)> send_db;
    std::function<void(std::uint8_t,net::Packet)> send_country;
};
// CGuildMgr::DeleteGuild 004325e0, restricted to an already empty guild.
// The world executor, catalog, guild, actor and transports survive callbacks.
inline void guild_delete_empty(LoadedGuild& guild,GuildCatalog& catalog,Actor* actor,const GuildDeleteEmptyPorts& ports) {
    if(!ports.game_log || !ports.send_db || !ports.send_country)throw std::invalid_argument("Missing guild deletion services");
    if(catalog.find(guild.metadata.listing.id)!=&guild)throw std::logic_error("Guild deletion lost active owner");
    std::lock_guard guard(guild.mutex);if(guild.deleting)throw std::logic_error("Guild deletion already started");
    struct Reset {bool& value;~Reset(){value=false;}} reset{guild.deleting};guild.deleting=true;
    guild_release_empty(guild,catalog,actor,ports.game_log);catalog.retire(guild);
    auto append=[](net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
    const auto id=guild.metadata.listing.id;net::Packet db{5,11};append(db,id);ports.send_db(std::move(db));
    net::Packet notice{0x11,13};append(notice,id);ports.send_country(static_cast<std::uint8_t>(guild.metadata.listing.country),std::move(notice));
}
}
