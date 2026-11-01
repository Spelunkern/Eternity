#include "eternity/session/registry.hpp"
#include <iostream>
#include <thread>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::session;
    try {
        Registry registry;
        LoginSession first{42,1,11,"example",{}, {}};
        require(!registry.add(first), "New account unexpectedly replaced a session");
        auto replacement = first; replacement.session_id = 2; replacement.token.fill(0x5a);
        const auto previous = registry.add(replacement);
        require(previous && previous->session_id == 1, "Replacement did not identify the old owner");
        require(!registry.remove(42,1), "Stale disconnect removed the replacement session");
        require(registry.find(42)->token == replacement.token && registry.size() == 1, "Replacement state was not atomic");
        require(registry.remove(42,2) && !registry.find(42), "Current owner could not remove its session");
        std::vector<std::jthread> threads;
        for (unsigned int index=0; index<16; ++index)
            threads.emplace_back([&registry,index] { for(unsigned int count=0; count<100; ++count)
                registry.add(LoginSession{42,static_cast<std::uint64_t>(index)*100+count+1,11,"example",{}, {}}); });
        threads.clear();
        require(registry.size() == 1 && registry.find(42), "Concurrent replacements duplicated an account");
        const auto key = game_key(eternity::net::SessionSecrets{});
        const std::array<std::uint8_t,16> expected_hash = {0x37,0x47,0x08,0xff,0xf7,0x71,0x9d,0xd5,0x97,0x9e,0xc8,0x75,0xd5,0x6c,0xd2,0x28};
        require(std::equal(expected_hash.begin(), expected_hash.end(), key.begin()+16), "World session key derivation changed");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
