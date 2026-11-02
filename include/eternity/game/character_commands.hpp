#pragma once
#include "eternity/game/creation.hpp"
#include "eternity/game/item_ids.hpp"
#include <functional>
#include <memory>

namespace eternity::game {
struct CharacterReply {
    std::vector<net::Packet> packets;
    bool close = false;
};
struct CharacterCommandOptions {
    std::uint8_t database_id = 1;
    std::chrono::sys_seconds world_base;
    std::string catalog = "PS_GameData";
};

// Roster belongs to one connection; its commands execute sequentially on database workers.
class CharacterCommands {
public:
    CharacterCommands(storage::SqlServerConfig config,CharacterCommandOptions options,
        std::shared_ptr<const CreationRules> rules,std::shared_ptr<ItemIds> identifiers,
        std::function<CreationTime()> clock = creation_time,
        std::function<std::size_t(std::size_t)> choose_start = {});
    CharacterReply execute(std::uint32_t user_id,const std::string& username,CharacterRoster& roster,const net::Packet& packet) const;
private:
    storage::SqlServerConfig config_;
    CharacterCommandOptions options_;
    std::shared_ptr<const CreationRules> rules_;
    std::shared_ptr<ItemIds> identifiers_;
    std::function<CreationTime()> clock_;
    std::function<std::size_t(std::size_t)> choose_start_;
};
}
