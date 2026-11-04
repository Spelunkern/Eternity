#include "eternity/game/vertical_entry.hpp"

namespace eternity::game {
CharacterSelectionResult vertical_entry_select(CharacterSelectionState& state, std::span<const std::uint8_t> packet,
    Players& players, const std::string& username, const VerticalEntryPorts& ports) {
    if (!ports.loaded || !ports.unavailable || !ports.close)
        throw std::invalid_argument("Incomplete character entry handoff");
    CharacterSelectionPorts gate;
    gate.close=ports.close;
    gate.send_db=[&](net::Packet request) {
        // PacketCharacter 0046c0f0 emits 0407, UID and the requested DWORD.
        const auto read=[&](std::size_t offset) {
            std::uint32_t value=0;
            for (unsigned int i=0;i<4;++i) value|=std::uint32_t(request[offset+i])<<(8*i);
            return value;
        };
        auto snapshot=players.load(read(2),read(6),username);
        if (snapshot) ports.loaded(std::move(*snapshot));
        else ports.unavailable();
    };
    return request_character_selection(state,packet,gate);
}
}
