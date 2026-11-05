#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"

namespace eternity::game {
// PacketGuild 00476fbc..00476fd7. The remaining member removal and guild
// destruction need their actual owners; eligible requests return unsupported.
inline EntryCatalogUpdate guild_client_leave_guard(GuildUser& user,Actor& actor,GuildCatalog* catalog,
    ZoneGuildCreationServices* creation,std::span<const std::uint8_t> packet) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=9 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    auto* captured=user.guild;
    if(!captured)return EntryCatalogUpdate::applied;
    if(!catalog)throw std::logic_error("Missing live guild catalog");
    auto* guild=catalog->find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(guild->mutex);
    if(!user.guild)return EntryCatalogUpdate::applied;
    if(creation && creation->current(actor))return EntryCatalogUpdate::applied;
    return EntryCatalogUpdate::unsupported;
}
}
