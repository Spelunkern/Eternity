#pragma once
#include "eternity/net/world_crypto.hpp"
#include "eternity/session/registry.hpp"
#include <optional>

namespace eternity::net {
struct WorldEvent {
    std::vector<Packet> replies;
    std::optional<Packet> application;
    bool connected = false;
    bool close = false;
};

// Socket-independent game connection state. All calls belong to one socket executor.
class WorldProtocol {
public:
    enum class State { connecting, character_selection, world, closed };
    WorldProtocol(std::shared_ptr<session::Registry> registry, std::uint8_t realm,
        std::uint64_t connection, std::shared_ptr<const WorldXor> world_xor,
        std::function<void()> disconnect = {});
    ~WorldProtocol();
    WorldProtocol(const WorldProtocol&) = delete;
    WorldProtocol& operator=(const WorldProtocol&) = delete;
    WorldEvent receive(Packet payload, std::uint64_t now_ms);
    Packet outgoing(Packet payload);
    void enter_world();
    void enter_character_selection();
    void close() noexcept;
    State state() const noexcept { return state_; }
    const std::optional<session::LoginSession>& owner() const noexcept { return owner_; }
private:
    std::shared_ptr<session::Registry> registry_;
    std::uint8_t realm_;
    std::uint64_t connection_;
    std::shared_ptr<const WorldXor> world_xor_;
    std::function<void()> disconnect_;
    std::optional<session::LoginSession> owner_;
    std::unique_ptr<WorldCipher> cipher_;
    State state_ = State::connecting;
    unsigned int undefined_packets_ = 0;
};
}
