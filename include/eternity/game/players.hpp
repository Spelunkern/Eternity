#pragma once
#include "eternity/game/characters.hpp"
#include <map>

namespace eternity::game {
struct PlayerCore {
    std::uint32_t user_id = 0;
    CharacterSummary character;
    std::uint16_t stat_points = 0,skill_points = 0,direction = 0,hunger = 0;
    std::int16_t vigor = 0;
    std::uint8_t cg = 0,og = 0,ig = 0,guild_rank = 0,kill_level = 0,dead_level = 0;
    std::uint32_t experience = 0,money = 0,guild_id = 0;
    std::uint64_t stored_money = 0;
    float x = 0,y = 0,z = 0;
    std::array<std::uint32_t,4> kills{};
    bool reset_stats = false,reset_skills = false;
    std::optional<std::chrono::sys_seconds> last_left;
};
struct PlayerItem {
    std::uint32_t id = 0,made_at = 0;
    std::uint64_t uid = 0;
    std::uint8_t type = 0,type_id = 0,bag = 0,count = 0;
    std::uint16_t slot = 0,quality = 0;
    std::array<std::uint8_t,6> gems{};
    std::string craft,make_type;
    bool relocated = false;
};
struct PlayerSkill {
    std::uint16_t id = 0,delay = 0;
    std::uint8_t level = 0,number = 0,persisted_number = 0;
};
struct PlayerSnapshot {
    PlayerCore core;
    std::vector<PlayerItem> items,bank,overflow_items,overflow_bank;
    std::vector<PlayerSkill> skills;
    std::uint32_t cash_points = 0;
    std::int32_t guild_rejoin_hours = 0;
    // Preserve original rows for effects, quests, quick slots, save points, point log/items, social and market data.
    std::map<std::string,storage::ResultSet> catalogs;
    std::vector<std::string> warnings;
};

PlayerCore decode_player_core(const storage::Row& row);
void decode_player_inventory(PlayerSnapshot& player);

class Players {
public:
    Players(storage::SqlServer& database,std::uint8_t server_id,std::string catalog = "PS_GameData");
    // Validates selectable ownership, updates JoinDate and consumes gift notifications through original procedures.
    std::optional<PlayerSnapshot> load(std::uint32_t user_id,std::uint32_t character_id,const std::string& username);
private:
    storage::SqlServer& database_;
    std::uint8_t server_id_;
    std::string prefix_;
};
}
