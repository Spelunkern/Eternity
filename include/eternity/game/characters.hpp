#pragma once
#include "eternity/storage/sql_server.hpp"
#include "eternity/net/framing.hpp"
#include <array>
#include <chrono>

namespace eternity::game {
// The original calendar is packed as year-2000:6, month:4, day:5, hour:5, minute:6, second:6.
std::chrono::sys_seconds unpack_world_time(std::uint32_t packed);
std::uint32_t pack_world_time(std::chrono::sys_seconds time);

struct CharacterSummary {
    std::uint32_t id = 0;
    std::uint32_t created = 0;
    std::uint8_t slot = 0;
    std::uint16_t level = 0;
    std::uint8_t family = 0, grow = 0, hair = 0, face = 0, size = 0, job = 0, sex = 0;
    std::uint16_t map = 0;
    std::uint16_t strength = 0, dexterity = 0, recovery = 0, intelligence = 0, wisdom = 0, luck = 0;
    std::uint16_t hp = 0, mp = 0, sp = 0;
    std::array<std::uint8_t,8> equipment_type{}, equipment_id{};
    std::array<std::uint8_t,6> cloak{};
    std::string name;
    bool rename = false;
    bool remaining_time = false;
};

struct CharacterRoster {
    std::uint8_t country = 2;
    std::uint8_t max_grow = 2;
    bool new_account = true;
    std::array<std::optional<CharacterSummary>,5> slots;
};

class Characters {
public:
    Characters(storage::SqlServer& database, std::uint8_t server_id, std::chrono::sys_seconds world_base)
        : database_(database), server_id_(server_id), world_base_(world_base) {}
    CharacterRoster load(std::uint32_t user_id);
private:
    storage::SqlServer& database_;
    std::uint8_t server_id_;
    std::chrono::sys_seconds world_base_;
};

// Map normalization is a separate world-map responsibility; these encode an already resolved roster.
std::vector<net::Packet> character_list(const CharacterRoster& roster, bool include_country = true);
}
