#include "eternity/session/registry.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

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

        Registry handoffs;
        first.token.fill(0x5a);handoffs.add(first);
        require(!handoffs.select(42,2,0,1000),"A stale login selected a world for the current session");
        require(handoffs.select(42,1,0,1000),"Original session handoff was not started");
        require(!handoffs.release_login(42,1) && handoffs.size()==1,"Closing login erased a pending world handoff");
        auto wrong=first.token;wrong[0]^=1;
        require(!handoffs.claim(42,wrong,0,9,1001),"Wrong world token accepted");
        require(!handoffs.claim(42,first.token,1,9,1001),"Token accepted by the wrong realm");
        require(!handoffs.claim(42,first.token,0,9,31000),"Expired token accepted at its deadline");
        require(handoffs.expire(30999).empty() && handoffs.expire(31000).size()==1 && handoffs.size()==0,
                "Original thirty-second handoff lifetime changed");
        handoffs.add(first);require(handoffs.select(42,1,0,1000),"Second handoff failed");
        std::atomic<unsigned int> accepted{0};
        for(unsigned int index=0;index<8;++index)
            threads.emplace_back([&handoffs,&accepted,&first,index] {
                if(handoffs.claim(42,first.token,0,index+1,1001)) ++accepted;
            });
        threads.clear();require(accepted==1,"Concurrent world connections consumed one token more than once");
        const auto world=handoffs.find(42);
        require(world && world->location==LoginSession::Location::world && !handoffs.release_login(42,1),
                "Transferred account did not belong to the world service");
        require(!handoffs.release_world(42,1,99),"Wrong world connection removed a session");
        replacement=first;replacement.session_id=2;
        const auto old_world=handoffs.add(replacement);
        require(old_world && old_world->location==LoginSession::Location::world,
                "Relogin did not return the previous world owner for disconnection");
        require(!handoffs.release_world(42,1,world->world_connection) && handoffs.find(42)->session_id==2,
                "Stale world close removed a replacement login");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
