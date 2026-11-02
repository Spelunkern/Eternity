#pragma once
#include "eternity/net/world_protocol.hpp"
#include "eternity/game/characters.hpp"
#include "eternity/game/character_commands.hpp"
#include <asio/io_context.hpp>

namespace eternity::net {
struct WorldServerOptions {
    std::string address = "127.0.0.1";
    std::uint16_t port = 30810;
    std::uint8_t realm = 0;
    std::size_t maximum_connections = 128;
    std::size_t database_workers = 2;
};
using LoadCharacters = std::function<game::CharacterRoster(const session::LoginSession&)>;
using DispatchCharacter = std::function<game::CharacterReply(const session::LoginSession&,game::CharacterRoster&,const Packet&)>;

// Native game TCP endpoint. SQL callbacks run outside the socket strand, in per-connection order.
class WorldServer {
public:
    WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> registry,
        LoadCharacters load,DispatchCharacter dispatch = {});
    ~WorldServer();
    WorldServer(const WorldServer&) = delete;
    WorldServer& operator=(const WorldServer&) = delete;
    void start();
    void stop();
    std::uint16_t port() const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
