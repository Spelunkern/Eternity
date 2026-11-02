#pragma once
#include "eternity/game/characters.hpp"
#include "eternity/game/definitions.hpp"
#include <filesystem>

namespace eternity::game {
struct CreateRequest {
    std::uint8_t slot = 0, family = 0, grow = 0, hair = 0, face = 0, size = 0, job = 0, sex = 0;
    std::string name;
};
struct CreateCheck {
    std::optional<CreateRequest> request;
    bool close = false;
    std::optional<std::uint8_t> result;
};
CreateCheck check_create(const net::Packet& packet,const CharacterRoster& roster);

struct StartPosition { std::uint16_t map = 0; float x = 0,y = 0,z = 0; };
using StartPositions = std::array<std::vector<StartPosition>,4>;
StartPositions original_start_positions();
StartPositions read_start_positions(const std::filesystem::path& map_ini);

struct StarterItem {
    std::uint32_t item_id = 0;
    std::uint8_t type = 0,type_id = 0,count = 0,bag = 0,slot = 0;
    std::uint16_t quality = 0;
    std::uint64_t uid = 0;
};
struct CharacterDraft {
    CreateRequest request;
    CharacterSummary character;
    StartPosition position;
    std::vector<StarterItem> items;
};
class CreationRules {
public:
    static CreationRules load(const Definitions& definitions,StartPositions positions = original_start_positions());
    CharacterDraft prepare(const CreateRequest& request,std::size_t start_index) const;
    std::size_t start_count(std::uint8_t family) const;
private:
    // HP, SP, MP at level one, before mode/race/stat adjustments.
    std::array<std::array<std::uint16_t,3>,6> resources_{};
    std::array<std::array<std::vector<StarterItem>,6>,4> items_;
    StartPositions starts_;
};
}
