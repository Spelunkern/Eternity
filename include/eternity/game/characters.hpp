#pragma once
#include "eternity/storage/sql_server.hpp"
#include "eternity/net/framing.hpp"
#include <array>
#include <chrono>

namespace eternity::game {
// The original calendar is packed as year-2000:6, month:4, day:5, hour:5, minute:6, second:6.
std::chrono::sys_seconds unpack_world_time(std::uint32_t packed);
std::uint32_t pack_world_time(std::chrono::sys_seconds time);
// CWorld::DBWorldDayLoad (ps_dbAgent.exe 0040ef40). Initializes a missing/zero base through the original save procedure.
std::chrono::sys_seconds load_world_base(storage::SqlServer& database);

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
    std::uint8_t rename = 0;
    std::int32_t remaining_time = 0;
};

struct CharacterRoster {
    std::uint8_t country = 2;
    std::uint8_t max_grow = 2;
    bool new_account = true;
    std::array<std::optional<CharacterSummary>,5> slots;
};

struct CharacterDraft;
struct CreateResult { std::uint8_t result; std::uint32_t character_id; std::string error = {}; };

class Characters {
public:
    Characters(storage::SqlServer& database, std::uint8_t server_id, std::chrono::sys_seconds world_base,
        std::string catalog = "PS_GameData");
    CharacterRoster load(std::uint32_t user_id);
    CreateResult create(std::uint32_t user_id,const std::string& username,const CharacterDraft& draft,
        std::chrono::sys_seconds made_at);
    bool save_country(std::uint32_t user_id,std::uint8_t country,bool new_account);
    bool erase(std::uint32_t user_id,std::uint32_t character_id);
    // ps_dbAgent DBCharacter::DeleteCharacterReq 0041e5e0 / RecoverCharacterReq
    // 0041e740. A zero timestamp cancels the pending deletion.
    bool request_deletion(std::uint32_t user_id,std::uint32_t character_id,std::uint32_t packed_time);
private:
    storage::SqlServer& database_;
    std::uint8_t server_id_;
    std::chrono::sys_seconds world_base_;
    std::string prefix_;
};

// Map normalization is a separate world-map responsibility; these encode an already resolved roster.
std::vector<net::Packet> character_list(const CharacterRoster& roster, bool include_country = true);
}
