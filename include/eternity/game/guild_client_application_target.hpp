#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
// PacketGuild 00476e82..00476ead. On the world executor, the retained catalog
// owns the lookup. Matching targets remain for the full application owner.
inline EntryCatalogUpdate guild_client_application_target(GuildUser& user,const Actor& actor,GuildCatalog& catalog,
    std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=7 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t(packet[2+i])<<(i*8);
    const auto* guild=catalog.find(id);
    if(guild && guild->metadata.listing.country==actor.appearance().identity.country)return EntryCatalogUpdate::unsupported;
    if(!send)throw std::invalid_argument("Missing guild application client transport");
    send(user,net::Packet{7,13,0});return EntryCatalogUpdate::applied;
}
}
