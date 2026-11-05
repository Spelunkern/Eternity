#pragma once
#include "eternity/game/guild_catalog.hpp"
#include <memory>

namespace eternity::game {
struct GuildApplicationDeclinePorts {
    // Return an aliasing session lease for an online user; null means offline.
    std::function<std::shared_ptr<GuildUser>(std::uint32_t)> find_online;
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
};
// ps_game.exe PacketGuild 00476f50 and CGuild::JoinNO 0042e630. Acceptance
// remains a separate operation; return unsupported without faking success.
inline EntryCatalogUpdate guild_client_decline(GuildUser& requester,GuildCatalog& catalog,
    std::span<const std::uint8_t> packet,const GuildApplicationDeclinePorts& ports) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=8 || packet[1]!=13)return EntryCatalogUpdate::unsupported;
    if(packet.size()<7)return EntryCatalogUpdate::rejected;
    auto* captured=requester.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    if(requester.rank>2 || !requester.guild)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(requester.guild);if(!guild)throw std::logic_error("Foreign current guild owner");
    if(packet[2]==1)return EntryCatalogUpdate::unsupported;
    std::uint32_t character=0;for(unsigned i=0;i<4;++i)character|=std::uint32_t{packet[3+i]}<<(8*i);
    auto& applications=guild->membership.applications;auto it=applications.find(character);
    if(it==applications.end())return EntryCatalogUpdate::applied;
    if(!it->second){applications.erase(it);return EntryCatalogUpdate::applied;}
    if(!ports.find_online || !ports.send_user || !ports.send_db)throw std::invalid_argument("Missing guild decline services");
    auto record=it->second;applications.erase(it);catalog.directory().remove(record->id);
    auto peer=ports.find_online(character);if(peer)peer->request_id=0;
    auto append=[](net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{8,11};append(db,guild->metadata.listing.id);append(db,record->id);ports.send_db(std::move(db));
    if(peer) {
        const auto& name=guild->metadata.listing.name;if(name.size()>24 || name.find('\0')!=name.npos)throw std::invalid_argument("Invalid guild name");
        net::Packet response{8,13,0};append(response,guild->metadata.listing.id);response.push_back(9);
        response.insert(response.end(),name.begin(),name.end());response.resize(33,0);ports.send_user(*peer,std::move(response));
    }
    net::Packet admin{0x18,0x0d};append(admin,record->id);guild->membership.broadcast_admin(std::move(admin));
    return EntryCatalogUpdate::applied;
}
}
