#pragma once
#include "eternity/game/character_entry.hpp"
#include "eternity/game/players.hpp"

namespace eternity::game {
// Per-connection state belongs to the caller. The success callback must retain
// the snapshot for ActorLoadPipeline and the eventual session lifetime.
struct VerticalEntryPorts {
    std::function<void(PlayerSnapshot)> loaded;
    std::function<void()> unavailable;
    std::function<void(std::uint32_t)> close;
};
// Synchronous DB-agent boundary: exceptions propagate to the connection owner.
// A rejected load does not fabricate a DB 0407 response or client success.
// ps_dbAgent.exe CUser::PacketUserChar 0040591f and LoadCharacter 00404b89
// use state-dependent teardown; the caller owns that missing lifecycle. SQL
// ownership checks do not replace the original retained DB-agent roster gate.
CharacterSelectionResult vertical_entry_select(CharacterSelectionState&, std::span<const std::uint8_t>,
    Players&, const std::string& username, const VerticalEntryPorts&);
}
