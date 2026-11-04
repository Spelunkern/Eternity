#pragma once
#include "eternity/game/entry_delivery.hpp"

namespace eternity::game {
// The connection owner retains the pipeline and all referenced catalogs until
// this synchronous operation completes. External steps are mandatory services.
struct VerticalProtocolServices {
    EntryDeliveryPorts delivery;
    std::function<void(CharacterEntryStep, Actor&)> lifecycle;
    std::function<void(net::Packet)> send_manager;
    std::function<std::uint32_t()> world_day,system_time;
    std::function<bool()> lock_guild;
    std::function<void()> unlock_guild;
};
// Successful ps_game.exe PacketUserDBChar 0046cf60, 0407 branch
// 0046d4c1..0046d717. The caller owns state, pipeline and their lifetimes.
// Discard the pipeline after failure; release it only after successful return.
void deliver_vertical_entry(CharacterEntryState&, ActorLoadPipeline&, const PlayerSnapshot&,
    const SocialCatalog&, const EntryCatalogs&, ExperienceBounds, std::string guild_name,
    const VerticalProtocolServices&);
}
