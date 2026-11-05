#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/ground_items.hpp"

namespace eternity::game {
// Independently complete no-eligible-item path of ps_game.exe PacketGuild
// 004764a0, 0d35 branch 0047734d. Eligible items return unsupported before
// effects: the original detaches them before DB/log callbacks and only frees
// them afterwards, which requires a split Actor inventory ownership primitive.
inline EntryCatalogUpdate guild_client_donation_empty(GuildUser& user,Actor& actor,GuildCatalog& catalog,
    std::span<const std::uint8_t> packet,const std::function<void(GuildUser&,net::Packet)>& send) {
    if(packet.size()<2)return EntryCatalogUpdate::rejected;
    if(packet[0]!=0x35 || packet[1]!=0x0d)return EntryCatalogUpdate::unsupported;
    auto* captured=user.guild;if(!captured)return EntryCatalogUpdate::applied;
    auto* locked=catalog.find_membership(captured);if(!locked)throw std::logic_error("Foreign captured guild owner");
    std::lock_guard guard(locked->mutex);
    if(!user.guild)return EntryCatalogUpdate::applied;
    if(!catalog.find_membership(user.guild))throw std::logic_error("Foreign current guild owner");
    const auto bags=actor.status().skills.max_bags;
    if(bags>9)throw std::out_of_range("Guild donation inventory exceeds original Actor bags");
    for(int bag=1;bag<=bags;++bag)for(std::uint8_t slot=0;slot<24;++slot) {
        const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(bag),slot);
        if(!held) {if(!actor.inventory_slot_empty(static_cast<std::uint8_t>(bag),slot))throw std::logic_error("Guild donation requires retained inventory owner");continue;}
        const auto& item=held->item->item;
        if(!item.make_type.empty() && item.make_type.front()=='+')return EntryCatalogUpdate::unsupported;
    }
    if(!send)throw std::invalid_argument("Missing guild donation client transport");
    send(user,net::Packet{0x35,0x0d,0});return EntryCatalogUpdate::applied;
}
}
