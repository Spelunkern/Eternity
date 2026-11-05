#pragma once
#include "eternity/game/guild_catalog.hpp"
#include <algorithm>

namespace eternity::game {
// ps_game.exe CUser::PacketGuild 004764a0, 0d20 at 004770af..00477180;
// CGuild::UpdateRemark 0042efb0 / SendDBGuildUpdateRemark 0042b750.
// The original's fixed-size outbound buffers have uninitialized bytes after the
// terminating NUL. Native packets zero those nonsemantic tails deterministically.
inline EntryCatalogUpdate guild_client_remark(GuildUser& user,GuildCatalog& catalog,std::span<const std::uint8_t> packet,
    const std::function<void(GuildUser&,net::Packet)>& send_self,const std::function<void(net::Packet)>& send_db) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x20 || packet[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    if(packet.size()<67)return EntryCatalogUpdate::rejected;
    const auto end=std::find(packet.begin()+2,packet.begin()+66,0);
    std::string text(packet.begin()+2,end);
    const auto response=[&](std::uint8_t status){net::Packet value{0x20,0x0d,status};value.insert(value.end(),text.begin(),text.end());value.resize(68,0);return value;};
    if(user.rank!=1 || text.find('\'')!=std::string::npos) {
        if(!send_self)throw std::invalid_argument("Missing guild remark client transport");
        send_self(user,response(0));return EntryCatalogUpdate::applied;
    }
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    auto* current=user.guild;if(!current)return EntryCatalogUpdate::applied;
    auto* guild=catalog.find_membership(current);if(!guild)throw std::logic_error("Foreign current guild owner");
    if(!send_db)throw std::invalid_argument("Missing guild remark DB transport");
    guild->metadata.listing.notice=text;
    guild->membership.broadcast(response(1));
    net::Packet db{0x0a,0x0b};const auto id=guild->metadata.listing.id;
    for(unsigned i=0;i<4;++i)db.push_back(static_cast<std::uint8_t>(id>>(8*i)));
    db.insert(db.end(),text.begin(),text.end());db.resize(71,0);send_db(std::move(db));
    return EntryCatalogUpdate::applied;
}
}
