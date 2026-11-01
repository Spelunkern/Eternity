#include "eternity/auth/login_protocol.hpp"
#include <algorithm>
#include <set>

namespace eternity::auth {
namespace {
void word(net::Packet& packet, std::size_t offset, std::uint16_t value) {
    packet.at(offset) = static_cast<std::uint8_t>(value & 0xff);
    packet.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

std::string text(std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), 0);
    return std::string(bytes.begin(), end);
}
}

LoginDecision login_decision(LoginResult login, bool admin_open) {
    std::uint8_t result = 0;
    std::uint8_t permission = 11;
    if (login.user_id == 0) return {1, permission};
    switch (login.status) {
        case -3: result = 1; break;
        case -2: result = 2; break;
        case -1: result = 3; break;
        case 1: result = 9; break;
        case 2: result = 4; break;
        case 6: result = 5; break;
        case 0x11: result = 10; break;
        case 0x21: result = 11; break;
        case 0x31: result = 12; break;
        case 0x41: result = 13; break;
        case 0x51: result = 14; break;
        case 0x80: result = 6; break;
        case 0x100: result = 7; break;
        case 0x200: result = 8; break;
        default:
            if ((login.status & 2) != 0) result = 10;
            else if ((login.status & 1) != 0) result = 11;
            else if (((static_cast<std::uint32_t>(login.status) >> 7) & 7) != 0) result = 6;
            else {
                permission = static_cast<std::uint8_t>((login.status >> 4) & 7);
                if (permission == 0) permission = 11;
            }
    }
    if (admin_open) permission = 1;
    return {result, permission};
}

LoginProtocol::LoginProtocol(const net::LoginKeyPair& key_pair, std::vector<Realm> realms)
    : key_pair_(key_pair), realms_(std::move(realms)) {
    std::set<std::uint8_t> identifiers;
    for (const auto& realm : realms_) {
        if (realm.name.size() > 31 || realm.name.find('\0') != std::string::npos || realm.id >= 254 ||
            !identifiers.insert(realm.id).second)
            throw std::invalid_argument("Invalid original realm directory entry");
    }
}

net::Packet LoginProtocol::greeting() const { return key_pair_.public_packet(); }

const net::SessionSecrets& LoginProtocol::secrets() const {
    if (!secrets_) throw std::logic_error("Login exchange is not complete");
    return *secrets_;
}

net::Packet LoginProtocol::outgoing(net::Packet payload) {
    if (sender_) sender_->crypt(payload);
    return payload;
}

LoginEvent LoginProtocol::receive(net::Packet payload) {
    if (state_ == State::closed) return {{}, {}, true};
    const auto reject = [this]() { state_ = State::closed; return LoginEvent{{}, {}, true}; };
    if (receiver_) receiver_->crypt(payload);
    if (payload.size() < 2) return reject();
    const auto opcode = static_cast<std::uint16_t>(payload[0]) |
                        (static_cast<std::uint16_t>(payload[1]) << 8);
    // SAgent::PacketSystem 004127b0: close=2, reply/liveness=3.
    if (opcode == 2) return reject();
    if (opcode == 3) return {};
    if (opcode == 0xa101 && state_ == State::key_exchange) {
        if (payload.size() < 3 || payload[2] == 0 || payload[2] > 128 ||
            payload.size() < static_cast<std::size_t>(payload[2]) + 3) return reject();
        secrets_ = key_pair_.accept(std::span(payload).subspan(3, payload[2]));
        sender_ = std::make_unique<net::LegacyCipher>(*secrets_);
        receiver_ = std::make_unique<net::LegacyCipher>(*secrets_);
        state_ = State::credentials;
        return {};
    }
    if (opcode == 0xa102 && state_ != State::key_exchange) {
        if (state_ != State::credentials) return {};
        if (payload.size() < 47) return reject();
        // CUser::OnRecv 004042f0 explicitly terminates at payload 0x0e and 0x2e.
        Credentials credentials{text(std::span(payload).subspan(2, 12)),
                                text(std::span(payload).subspan(34, 12))};
        state_ = State::authenticating;
        return {{}, std::move(credentials), false};
    }
    if (opcode == 0xa202 && state_ == State::authenticated) {
        if (payload.size() < 7) return reject();
        // No world endpoint is available yet. Preserve the original offline result.
        net::Packet reply{0x02, 0xa2, 0xfe, 0, 0, 0, 0};
        return {{outgoing(std::move(reply))}, {}, false};
    }
    return reject();
}

LoginEvent LoginProtocol::complete(LoginResult login, const std::array<std::uint8_t, 16>& token) {
    if (state_ != State::authenticating) return {{}, {}, state_ == State::closed};
    const auto decision = login_decision(login);
    if (decision.result != 0 && ++attempts_ >= 10) {
        state_ = State::closed;
        return {{}, {}, true};
    }
    net::Packet reply(24, 0);
    word(reply, 0, 0xa102);
    reply[2] = decision.result;
    if (decision.result != 0) {
        state_ = State::credentials;
        return {{outgoing(std::move(reply))}, {}, false};
    }
    state_ = State::authenticated;
    const auto user = static_cast<std::uint32_t>(login.user_id);
    for (unsigned int index = 0; index < 4; ++index)
        reply[3 + index] = static_cast<std::uint8_t>(user >> (index * 8));
    reply[7] = decision.permission;
    std::copy(token.begin(), token.end(), reply.begin() + 8);
    LoginEvent event{{outgoing(std::move(reply))}, {}, false};
    // CServerList::SendList 004072c0 emits at most 48 packed GSERVER_UNITs per packet.
    for (std::size_t start = 0; start < realms_.size(); start += 48) {
        const auto count = std::min<std::size_t>(48, realms_.size() - start);
        net::Packet list(3 + count * 38, 0);
        word(list, 0, 0xa201);
        list[2] = static_cast<std::uint8_t>(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto& realm = realms_[start + index];
            const auto offset = 3 + index * 38;
            list[offset] = realm.id;
            list[offset + 1] = realm.status;
            word(list, offset + 2, realm.users);
            word(list, offset + 4, realm.capacity);
            std::copy(realm.name.begin(), realm.name.end(), list.begin() + static_cast<std::ptrdiff_t>(offset + 6));
        }
        event.replies.push_back(outgoing(std::move(list)));
    }
    return event;
}
}
