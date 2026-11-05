#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// PacketGuild 0047701f..0047704d. Eligible requests need the original
// online/offline member expulsion owner and deliberately remain unsupported.
inline EntryCatalogUpdate guild_client_expel_guard(GuildUser& user,Actor& actor,GuildCatalog* catalog,
    ZoneGuildCreationServices* creation,std::span<const std::uint8_t> packet) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=10 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    auto* captured=user.guild;
    if(!captured)return EntryCatalogUpdate::applied;
    if(!catalog)throw std::logic_error("Missing live guild catalog");
    auto* guild=catalog->find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);
    if(user.rank>2 || !user.guild)return EntryCatalogUpdate::applied;
    if(creation && creation->current(actor))return EntryCatalogUpdate::applied;
    return EntryCatalogUpdate::unsupported;
}
}
