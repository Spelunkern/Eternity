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
    // ps_game.exe CServer::Start 00405a30: accepted socket sender threshold.
    std::size_t maximum_queued_bytes = 0xc0000;
};
using LoadCharacters = std::function<game::CharacterRoster(const session::LoginSession&)>;
using DispatchCharacter = std::function<game::CharacterReply(const session::LoginSession&,game::CharacterRoster&,const Packet&)>;

// Copyable, thread-safe transport ports. Calls are ordered on the socket executor;
// ports retained after disconnect become inert. Payloads are plaintext.
struct WorldChannel {
    std::function<void(Packet)> send;
    std::function<void()> enter_world;
    std::function<void()> enter_character_selection;
    std::function<void()> disconnect;
};

// Factory, open, receive and close run on one per-connection worker strand.
// Every returned application is closed once, after its last in-flight call,
// including when open/receive throws. close must finish before shutdown drains.
class WorldApplication {
public:
    virtual ~WorldApplication() = default;
    virtual void open() = 0;
    virtual void receive(const Packet& packet) = 0;
    virtual void close() = 0;
};
using WorldApplicationFactory = std::function<std::unique_ptr<WorldApplication>(const session::LoginSession&,WorldChannel)>;

// Native game TCP endpoint. Stop and drain the io_context before destruction.
class WorldServer {
public:
    WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> registry,
        WorldApplicationFactory factory);
    WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> registry,
        LoadCharacters load,DispatchCharacter dispatch = {});
    ~WorldServer();
    WorldServer(const WorldServer&) = delete;
    WorldServer& operator=(const WorldServer&) = delete;
    void start();
    void stop();
    std::uint16_t port() const;
    std::uint64_t application_failures() const noexcept;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
