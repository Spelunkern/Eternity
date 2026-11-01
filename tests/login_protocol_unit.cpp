#include "eternity/auth/login_protocol.hpp"
#include "login_peer.hpp"
#include <iostream>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::auth;
    using namespace eternity::net;
    try {
        const std::vector<std::pair<std::int32_t, std::uint8_t>> failures = {
            {-3,1}, {-2,2}, {-1,3}, {1,9}, {2,4}, {6,5}, {0x11,10},
            {0x21,11}, {0x31,12}, {0x41,13}, {0x51,14}, {0x80,6}, {0x100,7}, {0x200,8}};
        for (const auto [status, expected] : failures)
            require(login_decision({status, 42}).result == expected, "Original login status mapping changed");
        require(login_decision({0,0}).result == 1, "Account zero must not authenticate");
        require(login_decision({0,42}).permission == 11, "Normal account permission changed");
        require(login_decision({0x20,42}).permission == 2, "Staff permission bits changed");
        require(login_decision({0,42}, true).permission == 1, "Admin-open permission changed");

        LoginKeyPair key;
        LoginProtocol protocol(key, {{0,2,0,1000,"Eternity"}});
        LoginPeer peer(protocol.greeting());
        require(!protocol.receive(peer.exchange).close && protocol.state() == LoginProtocol::State::credentials,
                "RSA exchange did not advance login state");
        auto request = protocol.receive(peer.credentials("example_user", "testpass"));
        require(request.credentials && request.credentials->username == "example_user" &&
                request.credentials->password == "testpass", "Original credential offsets changed");
        require(protocol.state() == LoginProtocol::State::authenticating, "SQL dispatch state missing");
        // An already-pending request consumes cipher bytes but does not dispatch twice.
        require(!protocol.receive(peer.credentials("ignored", "ignored")).credentials, "Duplicate SQL dispatch admitted");
        auto failed = protocol.complete({-1,42}, {});
        require(failed.replies.size() == 1 && !failed.close, "First login failure closed the connection");
        peer.receive->crypt(failed.replies[0]);
        require(failed.replies[0].size() == 24 && failed.replies[0][2] == 3, "Encrypted failure packet changed");

        require(protocol.receive(peer.credentials("example_user", "testpass")).credentials.has_value(), "Retry lost cipher synchronization");
        std::array<std::uint8_t, 16> token{};
        token.fill(0x5a);
        auto accepted = protocol.complete({0,0x12345678}, token);
        require(accepted.replies.size() == 2 && protocol.state() == LoginProtocol::State::authenticated,
                "Authenticated login did not emit login and realm packets");
        for (auto& reply : accepted.replies) peer.receive->crypt(reply);
        require(accepted.replies[0][2] == 0 && accepted.replies[0][3] == 0x78 &&
                accepted.replies[0][6] == 0x12 && accepted.replies[0][7] == 11 &&
                std::equal(token.begin(), token.end(), accepted.replies[0].begin()+8), "Login identity or token layout changed");
        require(accepted.replies[1].size() == 41 && accepted.replies[1][0] == 1 && accepted.replies[1][1] == 0xa2 &&
                accepted.replies[1][2] == 1 && accepted.replies[1][4] == 2 && accepted.replies[1][9] == 'E', "Realm directory layout changed");
        Packet select{0x02,0xa2,0,0,0,0,0};
        peer.send->crypt(select);
        auto offline = protocol.receive(select);
        peer.receive->crypt(offline.replies[0]);
        require(offline.replies[0] == Packet({2,0xa2,0xfe,0,0,0,0}), "Offline realm result changed");

        LoginProtocol attempts(key);
        LoginPeer retry(attempts.greeting());
        attempts.receive(retry.exchange);
        for (unsigned int index=0; index<10; ++index) {
            require(attempts.receive(retry.credentials("wrong", "wrong")).credentials.has_value(), "Retry was not dispatched");
            auto response = attempts.complete({-1,42}, {});
            require(response.close == (index == 9), "Original ten-attempt limit changed");
        }
        LoginProtocol premature(key);
        require(premature.receive(Packet{2,0xa1}).close, "Credentials accepted before key exchange");
        LoginProtocol truncated(key);
        LoginPeer short_peer(truncated.greeting());
        truncated.receive(short_peer.exchange);
        Packet short_packet{2,0xa1}; short_peer.send->crypt(short_packet);
        require(truncated.receive(short_packet).close, "Truncated credential packet accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
