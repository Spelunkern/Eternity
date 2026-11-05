#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/zone_leave_requests.hpp"

namespace eternity::game {
// Complete rejection cases in ps_game.exe PacketGuild 00476e32..00476f4b.
// Call after the deadline prefix. Remaining requests need the guild-creation
// state and actual application owner; they return unsupported without effects.
inline EntryCatalogUpdate guild_client_application_gate(GuildUser& user,const Actor& actor,const ZoneLeaveRequestState& requests,
    GuildCatalog* catalog,std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=7 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<6)return EntryCatalogUpdate::rejected;
    auto reject=[&]{if(!send)throw std::invalid_argument("Missing guild application client transport");send(user,net::Packet{7,13,0});return EntryCatalogUpdate::applied;};
    if(actor.appearance().identity.grow==0)return reject();
    bool blocked=false;
    {
        auto* captured=user.guild;std::unique_lock<std::recursive_mutex> lock;
        if(captured) {
            if(!catalog)throw std::logic_error("Missing live guild catalog");
            auto* guild=catalog->find_membership(captured);if(!guild)throw std::logic_error("Foreign captured guild owner");
            lock=std::unique_lock(guild->mutex);
        }
        blocked=user.guild!=nullptr || requests.request_1754!=0;
    }
    if(blocked)return reject();
    return EntryCatalogUpdate::unsupported;
}
}
