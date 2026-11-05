#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include <algorithm>
#include <optional>

namespace eternity::game {
// ps_game.exe CUser::PacketComm 00475870, block-add branch 00475e70..00475faa.
// The lookup must use the live CWorld::FindUser name index.
struct SocialBlockTarget {
    std::uint32_t list_id, wire_id; // Character ID CUser+128; entity ID CUser+dc.
    std::uint8_t country; std::string name; bool is_owner;
};
using SocialBlockLookup = std::function<std::optional<SocialBlockTarget>(std::string_view)>;
inline EntryCatalogUpdate social_block_add(SocialCatalog& owner, std::uint8_t country,
    std::span<const std::uint8_t> packet, const SocialBlockLookup& lookup,
    const std::function<void(net::Packet)>& send_db, const std::function<void(net::Packet)>& send_self) {
    if(packet.size()<2) return EntryCatalogUpdate::rejected;
    if(packet[0]!=9 || packet[1]!=0x22) return EntryCatalogUpdate::unsupported;
    if(!lookup || !send_db || !send_self) throw std::invalid_argument("Missing block-add services");
    if(packet.size()<23) return EntryCatalogUpdate::rejected;
    const auto end=std::find(packet.begin()+2,packet.begin()+22,0);
    const std::string name(packet.begin()+2,end);
    const auto already_name=std::any_of(owner.blocked().begin(),owner.blocked().end(),[&](const auto& row){return row.name==name;});
    std::optional<SocialBlockTarget> target;
    if(!already_name) target=lookup(name);
    const bool valid=target && !target->is_owner && target->country==country &&
        std::none_of(owner.blocked().begin(),owner.blocked().end(),[&](const auto& row){return row.id==target->list_id;});
    if(!valid) {send_self({0x0e,0x22,0});return EntryCatalogUpdate::applied;}
    if(target->name.size()>20 || target->name.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid live character name");
    owner.add_block({target->list_id,target->name,{}});
    const auto append=[](net::Packet& p,std::uint32_t v){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    net::Packet db{6,0x0c};append(db,owner.account_id());append(db,target->wire_id);db.insert(db.end(),target->name.begin(),target->name.end());db.resize(31,0);send_db(std::move(db));
    send_self({0x0e,0x22,1});
    net::Packet response{9,0x22};append(response,target->wire_id);response.insert(response.end(),target->name.begin(),target->name.end());response.resize(27,0);send_self(std::move(response));
    return EntryCatalogUpdate::applied;
}
}
