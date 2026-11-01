#pragma once
#include "eternity/auth/accounts.hpp"
#include "eternity/net/login_crypto.hpp"
#include <optional>

namespace eternity::auth {
struct LoginDecision {
    std::uint8_t result;
    std::uint8_t permission;
};
// ps_login.exe GetUser/GetUserMD5, 00406b50/00406e50.
LoginDecision login_decision(LoginResult result, bool admin_open = false);

struct Realm {
    std::uint8_t id;
    std::uint8_t status = 2; // 0 is connectable; 2 is offline.
    std::uint16_t users = 0;
    std::uint16_t capacity = 1000;
    std::string name;
};

struct Credentials {
    std::string username;
    std::string password;
};

struct LoginEvent {
    // Reply payloads are already encrypted when the exchange has completed.
    std::vector<net::Packet> replies;
    std::optional<Credentials> credentials;
    bool close = false;
};

class LoginProtocol {
public:
    enum class State { key_exchange, credentials, authenticating, authenticated, closed };
    explicit LoginProtocol(const net::LoginKeyPair& key_pair, std::vector<Realm> realms = {});
    net::Packet greeting() const;
    LoginEvent receive(net::Packet payload);
    LoginEvent complete(LoginResult result, const std::array<std::uint8_t, 16>& token);
    State state() const noexcept { return state_; }
    const net::SessionSecrets& secrets() const;
    void close() noexcept { state_ = State::closed; }
private:
    net::Packet outgoing(net::Packet payload);
    const net::LoginKeyPair& key_pair_;
    std::vector<Realm> realms_;
    State state_ = State::key_exchange;
    std::optional<net::SessionSecrets> secrets_;
    std::unique_ptr<net::LegacyCipher> sender_;
    std::unique_ptr<net::LegacyCipher> receiver_;
    unsigned int attempts_ = 0;
};
}
