#pragma once
#include "eternity/net/framing.hpp"
#include <functional>

namespace eternity::game {
struct CharacterSelectionState {
    std::uint32_t user_id=0;
    std::uint8_t country=2;
    bool enabled=true;
};
enum class CharacterSelectionResult {ignored,requested,closed};
struct CharacterSelectionPorts {
    std::function<void(net::Packet)> send_db;
    std::function<void(std::uint32_t reason)> close;
};
// ps_game.exe PacketCharacter 0046bf20, client 0104 -> DB-agent 0407.
CharacterSelectionResult request_character_selection(CharacterSelectionState&,std::span<const std::uint8_t>,const CharacterSelectionPorts&);

enum class CharacterEntryStep {
    equipment,passive,effects,billing,stop_attack,wait_user,init_guild,init_friends,
    data,items,bank,skills,effect_packets,quests,finished_quests,friends,speed,kc,
    save_points,honor,purchase_history,stored_points,guild_bank,guild_npcs,
    pvp_reset,run_mode,quick,flush
};
struct CharacterEntryState {
    std::uint32_t user_id=0,character_id=0,runtime_id=0,auto_character_id=0;
    std::uint16_t level=0;
    std::array<std::int32_t,3> current{},maximum{}; // HP, MP, SP, signed comparisons.
    std::uint8_t motion=0,flag_584c=0,flag_584d=0;
    bool guild_present=false,ready=false;
};
struct CharacterEntryPorts {
    // All steps are required services. This coordinator never substitutes empty
    // gameplay/social/guild work. Step handlers and sends must not throw.
    std::function<void(CharacterEntryStep)> step;
    std::function<void(net::Packet)> send_client,send_manager;
    std::function<std::uint32_t()> world_day,system_time;
    // CGuildLock captures a guild lock, optionally clearing guild_present if it
    // disappears. Return whether the captured lock needs release at scope end.
    std::function<bool()> lock_guild;
    std::function<void()> unlock_guild;
};
// PacketUserDBChar 0046cf60, successful 0407 branch 0046d4c1..0046d717.
// This prepares a waiting user; it does not itself enter a live map/zone.
void complete_character_selection(CharacterEntryState&,const CharacterEntryPorts&);
}
