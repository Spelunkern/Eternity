#pragma once
#include "eternity/game/character_entry.hpp"
#include "eternity/game/players.hpp"
#include "eternity/game/characters.hpp"

namespace eternity::game {
// Per-connection state belongs to the caller. The success callback must retain
// the snapshot for ActorLoadPipeline and the eventual session lifetime.
struct VerticalEntryPorts {
    std::function<void(PlayerSnapshot)> loaded;
    std::function<void()> unavailable;
    std::function<void(std::uint32_t)> close;
};
// Uses the retained roster in slot order. A zero ID encountering an empty slot
// cannot be resolved: the original retains status residues for empty slots.
// Throws on that unrepresentable case, before any SQL load.
bool vertical_entry_roster_allows(const CharacterRoster&, std::uint32_t character_id);
// Synchronous DB-agent boundary: exceptions propagate to the connection owner.
// A rejected load does not fabricate a DB 0407 response or client success.
// ps_dbAgent.exe CUser::PacketUserChar 0040591f and LoadCharacter 00404b89
// use state-dependent teardown; the caller owns that missing lifecycle. SQL
// ownership checks do not replace the original retained DB-agent roster gate.
CharacterSelectionResult vertical_entry_select(CharacterSelectionState&, std::span<const std::uint8_t>,
    const CharacterRoster&, Players&, const std::string& username, const VerticalEntryPorts&);
}
