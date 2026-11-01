#include "eternity/net/login_server.hpp"
#include "login_peer.hpp"
#include "login_socket.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using asio::ip::tcp;
using eternity::net::Packet;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}


struct Fixture {
    asio::io_context io;
    std::atomic<unsigned int> calls{0};
    eternity::net::LoginServer server;
    std::jthread runner;
    Fixture() : server(io, options(), [this](const eternity::auth::Credentials& input,std::uint64_t id,const std::string& ip) {
        ++calls;
        require(id>0 && ip=="127.0.0.1","Authentication lost transport identity");
        // Blocking here must not hold the socket strand.
        std::this_thread::sleep_for(20ms);
        if(input.username=="exists" && input.password=="testpass") return eternity::auth::LoginResult{0,42};
        return eternity::auth::LoginResult{-3,0};
    }) {
        server.start();
        runner=std::jthread([this] { io.run(); });
    }
    static eternity::net::LoginServerOptions options() { eternity::net::LoginServerOptions result; result.port=0; return result; }
    ~Fixture() { server.stop(); if(runner.joinable()) runner.join(); }
};

int main() {
    using namespace eternity::net;
    try {
        Fixture fixture;
        PeerSocket first(fixture.server.port());
        LoginPeer a(first.packet());
        // Coalesce the exchange and a complete encrypted credential packet in one write.
        auto stream=frame(a.exchange);
        auto login=frame(a.credentials("wrong","wrong"));
        stream.insert(stream.end(),login.begin(),login.end());
        first.write(std::span(stream).first(1));
        first.write(std::span(stream).subspan(1));
        auto failed=first.packet(); a.receive->crypt(failed);
        require(failed.size()==24 && failed[2]==1,"Unknown account response changed over TCP");
        // Duplicate credential requests while SQL is pending must not dispatch twice.
        stream=frame(a.credentials("exists","testpass"));
        auto duplicate=frame(a.credentials("ignored","ignored"));
        stream.insert(stream.end(),duplicate.begin(),duplicate.end()); first.write(stream);
        auto accepted=first.packet(); a.receive->crypt(accepted);
        auto realms=first.packet(); a.receive->crypt(realms);
        require(accepted[2]==0 && accepted[3]==42 && realms[2]==1 && realms[4]==2,"Encrypted login or offline realm advertisement failed");
        require(fixture.calls==2 && fixture.server.session_count()==1,"Duplicate SQL dispatch or missing session");

        PeerSocket second(fixture.server.port());
        LoginPeer b(second.packet()); second.write(frame(b.exchange)); second.write(frame(b.credentials("exists","testpass")));
        auto replaced=second.packet(); b.receive->crypt(replaced);
        auto list=second.packet(); b.receive->crypt(list);
        require(replaced[2]==0 && fixture.server.session_count()==1,"Replacement login duplicated the account");
        require(!std::equal(accepted.begin()+8,accepted.end(),replaced.begin()+8),"Replacement reused its session token");
        first.closed();
        require(fixture.server.session_count()==1,"Stale TCP close removed the current session");
        second.write(Packet{0,0}); second.closed();
        require(fixture.server.session_count()==0,"Malformed frame left an authenticated session behind");

        PeerSocket premature(fixture.server.port()); (void)premature.packet();
        premature.write(frame(Packet{2,0xa1})); premature.closed();
        require(fixture.calls==3,"Malformed pre-exchange input reached SQL");
        std::cout<<"TCP login passed: fragmented/coalesced frames, RSA/AES, retries, pending dispatch and session replacement.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
