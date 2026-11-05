#include "eternity/net/login_server.hpp"
#include "eternity/net/login_reply.hpp"
#include "login_peer.hpp"
#include "login_socket.hpp"
#include "eternity/session/registry.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using asio::ip::tcp;
using eternity::net::Packet;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void verify_login_reply_clock() {
    using eternity::net::LoginReplyClock;
    struct Receive {std::uint32_t last,now,reason;};
    for(const auto test:std::array<Receive,8>{{{0,1,0},{1,110000,10},{1,110001,0},{1,720001,0},
        {1,720002,8},{1000,999,10},{0xfffffff0U,0x1ada0,0},{0x100,0x80000100U,10}}}) {
        LoginReplyClock clock;require(!clock.observe_reply(test.last),"first login reply failed");
        require(clock.observe_reply(test.now)==(test.reason?std::optional<std::uint32_t>{test.reason}:std::nullopt),"login reply timing differs from original");
    }
    struct Scan {std::uint32_t last,now;bool timeout;};
    for(const auto test:std::array<Scan,6>{{{0,720000,false},{0,720001,true},{1,720001,false},
        {1,720002,true},{1000,999,false},{0xfffffff0U,20,false}}}) {
        LoginReplyClock clock;require(!clock.observe_reply(test.last),"login timeout setup failed");
        require(clock.timed_out(test.now)==test.timeout,"login timeout differs from original");
    }
    eternity::net::LoginReplyScanGate gate;
    require(gate.due(0) && gate.next()==120000,"initial login scan gate differs");
    require(!gate.due(119999) && gate.due(120000) && gate.next()==240000,"login scan gate boundary differs");
    require(gate.due(0xffffff00U) && gate.next()==119744,"login scan gate did not wrap");
    require(!gate.due(0) && gate.due(119744) && gate.next()==239744,"login scan gate wrap comparison differs");
}


struct Fixture {
    asio::io_context io;
    std::shared_ptr<eternity::session::Registry> registry=std::make_shared<eternity::session::Registry>();
    std::shared_ptr<eternity::auth::RealmDirectory> realms=std::make_shared<eternity::auth::RealmDirectory>(
        std::vector<eternity::auth::Realm>{{0,2,0,1000,"Eternity"}});
    std::atomic<unsigned int> calls{0};
    eternity::net::LoginServer server;
    std::jthread runner;
    Fixture() : server(io, options(), [this](const eternity::auth::Credentials& input,std::uint64_t id,const std::string& ip) {
        ++calls;
        require(id>0 && ip=="127.0.0.1","Authentication lost transport identity");
        // Blocking here must not hold the socket strand.
        std::this_thread::sleep_for(20ms);
        if(input.username=="nonstd") throw 42;
        if(input.username=="exception") throw std::runtime_error("Authentication backend failed");
        if(input.username=="exists" && input.password=="testpass") return eternity::auth::LoginResult{0,42};
        return eternity::auth::LoginResult{-3,0};
    },registry,realms) {
        server.start();
        runner=std::jthread([this] { io.run(); });
    }
    static eternity::net::LoginServerOptions options() { eternity::net::LoginServerOptions result; result.port=0; return result; }
    ~Fixture() { server.stop(); if(runner.joinable()) runner.join(); }
};

void verify_live_reply() {
    using namespace eternity;
    asio::io_context io; std::atomic_uint64_t tick{1};
    net::LoginServerOptions options; options.port=0; options.reply_scan_period=10ms;
    options.clock=[&]{return tick.load();};
    net::LoginServer server(io,options,[](const auth::Credentials&,std::uint64_t,const std::string&) {return auth::LoginResult{0,42};});
    server.start(); std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server; ~Cleanup(){server.stop();}} cleanup{server};
    PeerSocket peer(server.port()); LoginPeer client(peer.packet());
    // A plaintext heartbeat before exchange and an encrypted one afterwards
    // share the same clock and leave cipher progression intact.
    peer.write(net::frame(Packet{3,0}));
    peer.write(net::frame(client.exchange));
    peer.write(net::frame(client.credentials("exists","testpass")));
    auto accepted=peer.packet();client.receive->crypt(accepted);
    auto list=peer.packet();client.receive->crypt(list);
    require(accepted[2]==0 && list[2]==1,"heartbeat disrupted authenticated login");
    tick=110001; Packet reply{3,0};client.send->crypt(reply);peer.write(net::frame(reply));
    // A subsequent valid request proves the first encrypted reply was processed.
    Packet selection{2,0xa2,0,0xff,0xff,0xff,0xff};client.send->crypt(selection);peer.write(net::frame(selection));
    auto selected=peer.packet();client.receive->crypt(selected);require(selected[2]==0xfe,"heartbeat changed cipher progression");
    tick=110002; reply={3,0};client.send->crypt(reply);peer.write(net::frame(reply));peer.closed();
    require(server.session_count()==0,"heartbeat close retained login owner");
    tick=1; PeerSocket timeout(server.port());(void)timeout.packet();
    tick=720001; timeout.closed();
    server.stop(); runner.join();
}

void verify_late_encrypted_reply() {
    using namespace eternity;
    asio::io_context io;std::atomic_uint64_t tick{1};std::atomic_uint ticks{0};
    net::LoginServerOptions options;options.port=0;options.reply_scan_period=std::chrono::hours(1);
    options.clock=[&]{const auto value=tick.load();++ticks;return value;};
    net::LoginServer server(io,options,[](const auth::Credentials&,std::uint64_t,const std::string&){return auth::LoginResult{0,42};});
    server.start();std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::jthread& runner;~Cleanup(){server.stop();io.stop();if(runner.joinable())runner.join();}} cleanup{server,io,runner};
    PeerSocket peer(server.port());LoginPeer client(peer.packet());peer.write(net::frame(client.exchange));peer.write(net::frame(client.credentials("exists","testpass")));
    auto accepted=peer.packet();client.receive->crypt(accepted);auto list=peer.packet();client.receive->crypt(list);
    require(accepted[2]==0 && server.session_count()==1,"late-reply setup failed");
    Packet heartbeat{3,0};client.send->crypt(heartbeat);peer.write(net::frame(heartbeat));
    const auto deadline=std::chrono::steady_clock::now()+3s;while(ticks.load()<1 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(ticks.load()>=1,"first encrypted heartbeat was not observed");tick=720002;
    heartbeat={3,0};client.send->crypt(heartbeat);peer.write(net::frame(heartbeat));peer.closed();
    require(server.session_count()==0,"late encrypted heartbeat retained authenticated owner");
    server.stop();runner.join();
}

void verify_authenticated_reply_scan() {
    using namespace eternity;
    asio::io_context io;std::atomic_uint64_t tick{1},seen{0};
    net::LoginServerOptions options;options.port=0;options.reply_scan_period=10ms;
    options.clock=[&]{auto value=tick.load();seen=value;return value;};
    net::LoginServer server(io,options,[](const auth::Credentials&,std::uint64_t,const std::string&){return auth::LoginResult{0,42};});
    server.start();std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::jthread& runner;~Cleanup(){server.stop();io.stop();if(runner.joinable())runner.join();}} cleanup{server,io,runner};
    PeerSocket peer(server.port());LoginPeer client(peer.packet());peer.write(net::frame(client.exchange));peer.write(net::frame(client.credentials("exists","testpass")));
    auto accepted=peer.packet();client.receive->crypt(accepted);auto list=peer.packet();client.receive->crypt(list);require(accepted[2]==0,"scan setup failed");
    Packet heartbeat{3,0};client.send->crypt(heartbeat);peer.write(net::frame(heartbeat));
    Packet selection{2,0xa2,0,0xff,0xff,0xff,0xff};client.send->crypt(selection);peer.write(net::frame(selection));
    auto selected=peer.packet();client.receive->crypt(selected);require(selected[2]==0xfe,"scan heartbeat was not processed");
    tick=720001;const auto deadline=std::chrono::steady_clock::now()+3s;while(seen.load()!=720001 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(seen.load()==720001,"reply timeout scan did not run at boundary");
    // The single IO runner completes the scan before this posted marker.
    std::promise<void> marker;auto marked=marker.get_future();asio::post(io,[&]{marker.set_value();});
    require(marked.wait_for(3s)==std::future_status::ready,"scan boundary marker stalled");
    std::uint8_t byte{};asio::error_code error;const auto count=peer.socket.read_some(asio::buffer(&byte,1),error);
    require(!count && (error==asio::error::would_block || error==asio::error::try_again) && server.session_count()==1,"boundary scan closed the authenticated owner");
    // The original scan gate advances by 120000 ms from its previous scan.
    tick=840001;peer.closed();require(server.session_count()==0,"post-reply scan retained timed-out owner");
    server.stop();runner.join();
}

void verify_heartbeat_during_authentication() {
    using namespace eternity;
    asio::io_context io;std::atomic_uint64_t tick{1};std::atomic_uint ticks{0};
    net::LoginServerOptions options;options.port=0;options.database_workers=1;options.reply_scan_period=std::chrono::hours(1);
    options.clock=[&]{const auto value=tick.load();++ticks;return value;};
    std::promise<void> entered,release,done;auto started=entered.get_future();auto gate=release.get_future().share();auto stopped=done.get_future();
    net::LoginServer server(io,options,[&](const auth::Credentials&,std::uint64_t,const std::string&){entered.set_value();gate.wait();return auth::LoginResult{0,42};});
    server.start();std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::jthread& runner;std::promise<void>& release;bool released=false;
        ~Cleanup(){if(!released)release.set_value();server.stop();io.stop();if(runner.joinable())runner.join();}} cleanup{server,io,runner,release};
    PeerSocket peer(server.port());LoginPeer client(peer.packet());peer.write(net::frame(client.exchange));peer.write(net::frame(client.credentials("blocked","testpass")));
    require(started.wait_for(3s)==std::future_status::ready,"authentication worker did not start");
    Packet heartbeat{3,0};client.send->crypt(heartbeat);peer.write(net::frame(heartbeat));
    const auto deadline=std::chrono::steady_clock::now()+3s;while(ticks.load()<1 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(ticks.load()>=1,"pending authentication heartbeat was not observed");tick=2;
    heartbeat={3,0};client.send->crypt(heartbeat);peer.write(net::frame(heartbeat));peer.closed();
    require(server.outstanding_authentication()==1 && server.session_count()==0,"heartbeat close lost pending authentication fence");
    server.stop([&]{done.set_value();});require(stopped.wait_for(100ms)==std::future_status::timeout,"stop completed before authentication returned");
    release.set_value();cleanup.released=true;require(stopped.wait_for(3s)==std::future_status::ready,"authentication did not drain");runner.join();
    require(server.outstanding_authentication()==0 && server.session_count()==0,"late successful authentication revived closed owner");
}

void verify_bounded_authentication() {
    using namespace eternity;
    asio::io_context io;net::LoginServerOptions options;options.port=0;options.maximum_connections=1;options.database_workers=1;
    std::promise<void> entered,release;auto started=entered.get_future();auto gate=release.get_future().share();std::atomic_uint calls{0};
    net::LoginServer server(io,options,[&](const auth::Credentials&,std::uint64_t,const std::string&) {
        ++calls;entered.set_value();gate.wait();return auth::LoginResult{-3,0};
    });
    server.start();std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::promise<void>& release;bool released=false;
        ~Cleanup(){if(!released)release.set_value();server.stop();io.stop();}} cleanup{server,io,release};
    PeerSocket first(server.port());LoginPeer a(first.packet());first.write(net::frame(a.exchange));first.write(net::frame(a.credentials("blocked","testpass")));
    require(started.wait_for(3s)==std::future_status::ready,"authentication did not start");
    first.write(Packet{0,0});first.closed();
    PeerSocket second(server.port());LoginPeer b(second.packet());second.write(net::frame(b.exchange));second.write(net::frame(b.credentials("second","testpass")));second.closed();
    require(calls==1,"closed socket allowed another unbounded authentication job");
    server.stop();runner.join();release.set_value();cleanup.released=true;
    const auto deadline=std::chrono::steady_clock::now()+3s;while(server.outstanding_authentication()!=0 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(server.outstanding_authentication()==0,"authentication job failed to drain");io.restart();require(io.poll()==0,"closed authentication posted after IO stopped");
}

struct ExpiryCopyFailure {
    std::shared_ptr<std::atomic_bool> fail;
    std::shared_ptr<std::atomic_uint> copies,calls;
    ExpiryCopyFailure(std::shared_ptr<std::atomic_bool> f,std::shared_ptr<std::atomic_uint> c,std::shared_ptr<std::atomic_uint> d):fail(std::move(f)),copies(std::move(c)),calls(std::move(d)){}
    ExpiryCopyFailure(const ExpiryCopyFailure& other):fail(other.fail),copies(other.copies),calls(other.calls) {
        if(fail->load() && ++*copies%2==0)throw std::runtime_error("expiry copy failure");
    }
    void operator()() const {++*calls;}
};
void verify_expired_copy_failure() {
    Fixture fixture;auto fail=std::make_shared<std::atomic_bool>(false);auto copies=std::make_shared<std::atomic_uint>(0),calls=std::make_shared<std::atomic_uint>(0);
    for(std::uint32_t id:{81U,82U}) {
        eternity::session::LoginSession old{id,id+100,1,"expired",{}, {}};old.disconnect=ExpiryCopyFailure(fail,copies,calls);
        fixture.registry->add(std::move(old));require(fixture.registry->select(id,id+100,0,0),"expiration copy setup failed");
    }
    *fail=true;const auto failed_by=std::chrono::steady_clock::now()+3s;
    while(!fixture.server.handoff_expiration_failures() && std::chrono::steady_clock::now()<failed_by)std::this_thread::sleep_for(1ms);
    require(fixture.server.handoff_expiration_failures()>0 && fixture.registry->size()==2 && *calls==0,"failed sweep lost expired owners or stopped listener");
    *fail=false;const auto drained_by=std::chrono::steady_clock::now()+3s;
    while(*calls!=2 && std::chrono::steady_clock::now()<drained_by)std::this_thread::sleep_for(1ms);
    require(*calls==2 && fixture.registry->size()==0,"expiry sweep did not retry both owners");
    PeerSocket peer(fixture.server.port());LoginPeer client(peer.packet());peer.write(eternity::net::frame(client.exchange));peer.write(eternity::net::frame(client.credentials("exists","testpass")));
    auto accepted=peer.packet();client.receive->crypt(accepted);auto realms=peer.packet();client.receive->crypt(realms);
    require(accepted[2]==0 && realms[2]==1,"listener failed after expiry sweep exception");peer.write(Packet{0,0});peer.closed();
}

void verify_expired_disconnect_failure() {
    std::promise<void> called,barrier;auto all_called=called.get_future();auto settled=barrier.get_future();std::atomic_uint calls{0},failed_attempts{0};
    Fixture fixture;
    for(std::uint32_t id:{42U,72U}) {
        eternity::session::LoginSession old{id,id+100,1,"expired",{}, {}};
        old.disconnect=[&,id]{if(++calls==2)called.set_value();if(id==42 && ++failed_attempts<=2)throw 42;};
        fixture.registry->add(std::move(old));require(fixture.registry->select(id,id+100,0,0),"expiration setup failed");
    }
    require(all_called.wait_for(3s)==std::future_status::ready,"expired callbacks were not all attempted");
    asio::post(fixture.io,[&]{barrier.set_value();});require(settled.wait_for(3s)==std::future_status::ready,"expiry failure stopped IO runner");
    require(fixture.server.handoff_disconnect_failures()==1 && fixture.server.pending_handoff_disconnects()==1 && fixture.server.session_count()==0,"expiry callback failure was lost or reservation retained");
    PeerSocket blocked(fixture.server.port());LoginPeer blocked_peer(blocked.packet());blocked.write(eternity::net::frame(blocked_peer.exchange));blocked.write(eternity::net::frame(blocked_peer.credentials("exists","testpass")));blocked.closed();
    require(fixture.server.pending_handoff_disconnects()==1 && fixture.server.session_count()==0,"unresolved handoff accepted the account");
    eternity::net::LoginServerOptions other_options;other_options.port=0;
    eternity::net::LoginServer other(fixture.io,other_options,[](const eternity::auth::Credentials&,std::uint64_t,const std::string&){return eternity::auth::LoginResult{0,42};},fixture.registry,fixture.realms);
    other.start();PeerSocket foreign(other.port());LoginPeer foreign_peer(foreign.packet());foreign.write(eternity::net::frame(foreign_peer.exchange));foreign.write(eternity::net::frame(foreign_peer.credentials("exists","testpass")));foreign.closed();
    require(fixture.server.pending_handoff_disconnects()==1 && fixture.server.session_count()==0,"other login owner bypassed expired handoff request");
    auto retry=[&]{std::promise<void> barrier2;auto ready=barrier2.get_future();fixture.server.retry_handoff_disconnects();asio::post(fixture.io,[&]{barrier2.set_value();});require(ready.wait_for(3s)==std::future_status::ready,"handoff retry stalled");};
    retry();require(failed_attempts==2 && fixture.server.pending_handoff_disconnects()==1 && fixture.server.handoff_disconnect_failures()==2,"failed handoff retry lost callback");
    retry();require(failed_attempts==3 && fixture.server.pending_handoff_disconnects()==0 && calls==4,"handoff retry did not invoke real callback");
    PeerSocket peer(other.port());LoginPeer client(peer.packet());peer.write(eternity::net::frame(client.exchange));peer.write(eternity::net::frame(client.credentials("exists","testpass")));
    auto accepted=peer.packet();client.receive->crypt(accepted);auto realms=peer.packet();client.receive->crypt(realms);
    require(accepted[2]==0 && realms[2]==1,"listener failed after expiry callback exception");peer.write(Packet{0,0});peer.closed();other.stop();
}

void verify_reentrant_handoff_stop() {
    Fixture fixture;std::atomic_uint attempts{0},other{0};std::atomic_bool early{false};std::promise<void> finished;auto done=finished.get_future();
    eternity::session::LoginSession first{91,191,1,"expired",{}, {}};
    first.disconnect=[&]{if(++attempts==1){fixture.server.stop([&]{finished.set_value();});early=done.wait_for(0s)==std::future_status::ready;throw std::runtime_error("handoff failure");}};
    eternity::session::LoginSession second{92,192,1,"expired",{}, {}};second.disconnect=[&]{++other;};
    fixture.registry->add(std::move(first));fixture.registry->add(std::move(second));
    require(fixture.registry->select(91,191,0,0) && fixture.registry->select(92,192,0,0),"reentrant handoff setup failed");
    const auto deadline=std::chrono::steady_clock::now()+3s;
    while(fixture.server.pending_handoff_disconnects()!=1 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(!early && fixture.server.pending_handoff_disconnects()==1 && done.wait_for(100ms)==std::future_status::timeout && other==1,
        "reentrant stop lost expired callback or skipped batch");
    fixture.server.retry_handoff_disconnects();
    require(done.wait_for(3s)==std::future_status::ready && attempts==2 && fixture.server.pending_handoff_disconnects()==0,
        "reentrant stop did not await actual handoff retry");
}

void verify_replacement_disconnect_failure(bool stop_first) {
    using namespace eternity;
    Fixture fixture;std::atomic_uint attempts{0};
    PeerSocket original(fixture.server.port());LoginPeer old_peer(original.packet());original.write(net::frame(old_peer.exchange));original.write(net::frame(old_peer.credentials("exists","testpass")));
    auto accepted=original.packet();old_peer.receive->crypt(accepted);auto list=original.packet();old_peer.receive->crypt(list);require(accepted[2]==0,"replacement recovery setup failed");
    auto old=fixture.registry->find(42);require(old && old->disconnect,"original login owner missing");auto actual=old->disconnect;
    old->disconnect=[actual,&attempts]{if(++attempts<3)throw std::runtime_error("injected disconnect failure");actual();};
    fixture.registry->add(std::move(*old));
    PeerSocket replacement(fixture.server.port());LoginPeer next(replacement.packet());replacement.write(net::frame(next.exchange));replacement.write(net::frame(next.credentials("exists","testpass")));replacement.closed();
    require(attempts==1 && fixture.server.pending_replacement_disconnects()==1 && fixture.server.replacement_disconnect_failures()==1,
        "failed replacement lost its owner callback");
    PeerSocket blocked(fixture.server.port());LoginPeer blocked_peer(blocked.packet());blocked.write(net::frame(blocked_peer.exchange));blocked.write(net::frame(blocked_peer.credentials("exists","testpass")));blocked.closed();
    require(fixture.server.pending_replacement_disconnects()==1 && fixture.server.session_count()==0,"unresolved replacement accepted the account");
    std::unique_ptr<net::LoginServer> other;
    if(!stop_first) {
        net::LoginServerOptions options;options.port=0;
        other=std::make_unique<net::LoginServer>(fixture.io,options,[](const auth::Credentials&,std::uint64_t,const std::string&){return auth::LoginResult{0,42};},fixture.registry,fixture.realms);
        other->start();PeerSocket foreign(other->port());LoginPeer foreign_peer(foreign.packet());foreign.write(net::frame(foreign_peer.exchange));foreign.write(net::frame(foreign_peer.credentials("exists","testpass")));foreign.closed();
        require(fixture.server.pending_replacement_disconnects()==1 && fixture.server.session_count()==0,"another login owner bypassed pending replacement");
    }
    std::promise<void> drained;auto done=drained.get_future();
    if(stop_first) {
        fixture.server.stop([&]{drained.set_value();});original.closed();
        require(done.wait_for(100ms)==std::future_status::timeout,"login shutdown ignored failed replacement request");
    }
    auto retry=[&]{std::promise<void> barrier;auto settled=barrier.get_future();fixture.server.retry_replacement_disconnects();asio::post(fixture.io,[&]{barrier.set_value();});require(settled.wait_for(3s)==std::future_status::ready,"replacement retry stalled");};
    retry();require(attempts==2 && fixture.server.pending_replacement_disconnects()==1 && fixture.server.replacement_disconnect_failures()==2,
        "failed replacement retry lost its callback");
    if(stop_first)require(done.wait_for(0s)==std::future_status::timeout,"failed retry reported shutdown complete");
    retry();require(attempts==3 && fixture.server.pending_replacement_disconnects()==0,"actual replacement disconnect was not retried");
    if(stop_first)require(done.wait_for(3s)==std::future_status::ready,"shutdown did not resume after real retry");
    else {
        original.closed();PeerSocket recovered(other->port());LoginPeer peer(recovered.packet());recovered.write(net::frame(peer.exchange));recovered.write(net::frame(peer.credentials("exists","testpass")));
        auto reply=recovered.packet();peer.receive->crypt(reply);auto realms=recovered.packet();peer.receive->crypt(realms);
        require(reply[2]==0 && fixture.server.session_count()==1,"account remained blocked after disconnect retry");recovered.write(Packet{0,0});recovered.closed();other->stop();
    }
}

void verify_reentrant_replacement_stop() {
    using namespace eternity;
    Fixture fixture;std::atomic_uint attempts{0};std::atomic_bool early{false};std::promise<void> finished;auto done=finished.get_future();
    PeerSocket original(fixture.server.port());LoginPeer old_peer(original.packet());original.write(net::frame(old_peer.exchange));original.write(net::frame(old_peer.credentials("exists","testpass")));
    auto accepted=original.packet();old_peer.receive->crypt(accepted);auto list=original.packet();old_peer.receive->crypt(list);require(accepted[2]==0,"reentrant replacement setup failed");
    auto old=fixture.registry->find(42);require(old && old->disconnect,"reentrant previous owner missing");auto actual=old->disconnect;
    old->disconnect=[&]{
        if(++attempts==1) {
            fixture.server.stop([&]{finished.set_value();});
            early=done.wait_for(0s)==std::future_status::ready;
            throw std::runtime_error("reentrant disconnect failure");
        }
        actual();
    };
    fixture.registry->add(std::move(*old));
    PeerSocket replacement(fixture.server.port());LoginPeer next(replacement.packet());replacement.write(net::frame(next.exchange));replacement.write(net::frame(next.credentials("exists","testpass")));replacement.closed();original.closed();
    require(!early && done.wait_for(100ms)==std::future_status::timeout && fixture.server.pending_replacement_disconnects()==1,
        "reentrant stop completed before the real disconnect failure was retained");
    fixture.server.retry_replacement_disconnects();
    require(done.wait_for(3s)==std::future_status::ready && attempts==2 && fixture.server.pending_replacement_disconnects()==0,
        "reentrant stop did not await retry of the real previous callback");
}

void verify_authentication_delivery_failure(bool stop_instead) {
    using namespace eternity;
    asio::io_context io;net::LoginServerOptions options;options.port=0;
    std::atomic_bool inject{true};std::atomic_uint posts{0},calls{0};
    options.before_authentication_delivery_post=[&](bool){if(inject.load()){++posts;throw std::runtime_error("injected delivery post failure");}};
    net::LoginServer server(io,options,[&](const auth::Credentials&,std::uint64_t,const std::string&){++calls;return auth::LoginResult{0,42};});
    server.start();std::jthread runner([&]{io.run();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::jthread& runner;~Cleanup(){server.stop();io.stop();if(runner.joinable())runner.join();}} cleanup{server,io,runner};
    PeerSocket peer(server.port());LoginPeer client(peer.packet());peer.write(net::frame(client.exchange));peer.write(net::frame(client.credentials("exists","testpass")));
    const auto deadline=std::chrono::steady_clock::now()+3s;
    while((server.pending_authentication_delivery_failures()!=1 || posts!=2 || server.outstanding_authentication()!=0) && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(calls==1 && posts==2 && server.authentication_delivery_failures()==1 && server.pending_authentication_delivery_failures()==1 &&
        server.outstanding_authentication()==0 && server.session_count()==0,"failed authentication delivery lost ownership or created a session");
    inject=false;std::promise<void> stopped;auto done=stopped.get_future();
    if(stop_instead)server.stop([&]{stopped.set_value();});else server.retry_authentication_delivery_failures();
    peer.closed();
    require(server.pending_authentication_delivery_failures()==0 && server.session_count()==0,"real close did not clear failed authentication delivery");
    if(stop_instead)require(done.wait_for(3s)==std::future_status::ready,"stop did not drain failed delivery");
    else {
        PeerSocket recovered(server.port());LoginPeer again(recovered.packet());recovered.write(net::frame(again.exchange));recovered.write(net::frame(again.credentials("exists","testpass")));
        auto accepted=recovered.packet();again.receive->crypt(accepted);auto realms=recovered.packet();again.receive->crypt(realms);
        require(accepted[2]==0 && realms[2]==1 && calls==2 && server.session_count()==1,"listener did not recover after delivery failure");
        recovered.write(Packet{0,0});recovered.closed();
    }
}

void verify_login_stop_drain() {
    using namespace eternity;
    asio::io_context io;net::LoginServerOptions options;options.port=0;options.database_workers=1;
    std::atomic_bool fail_timer{true};options.before_authentication_drain_wait=[&]{if(fail_timer.exchange(false))throw std::runtime_error("injected timer setup failure");};
    std::promise<void> entered,release,done,done2,done3,exited;auto started=entered.get_future();auto completed=done.get_future();auto completed2=done2.get_future();auto completed3=done3.get_future();auto stopped=exited.get_future();auto gate=release.get_future().share();std::atomic_uint calls{0};
    net::LoginServer server(io,options,[&](const auth::Credentials&,std::uint64_t,const std::string&){if(++calls==1){entered.set_value();gate.wait();}return auth::LoginResult{-3,0};});
    server.start();std::jthread runner([&]{io.run();exited.set_value();});
    struct Cleanup {net::LoginServer& server;asio::io_context& io;std::promise<void>& release;bool released=false;
        ~Cleanup(){if(!released)release.set_value();server.stop();io.stop();}} cleanup{server,io,release};
    PeerSocket peer(server.port());LoginPeer client(peer.packet());peer.write(net::frame(client.exchange));peer.write(net::frame(client.credentials("blocked","testpass")));
    require(started.wait_for(3s)==std::future_status::ready,"drain authentication did not start");
    PeerSocket queued(server.port());LoginPeer waiting(queued.packet());queued.write(net::frame(waiting.exchange));queued.write(net::frame(waiting.credentials("queued","testpass")));
    const auto deadline=std::chrono::steady_clock::now()+3s;while(server.outstanding_authentication()!=2 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
    require(server.outstanding_authentication()==2,"second authentication was not queued");
    server.stop([&]{server.stop([&]{done3.set_value();});done.set_value();});peer.closed();queued.closed();
    const auto error_deadline=std::chrono::steady_clock::now()+3s;
    while(!server.authentication_drain_error() && std::chrono::steady_clock::now()<error_deadline)std::this_thread::sleep_for(1ms);
    require(bool(server.authentication_drain_error()),"timer setup failure was not exposed");
    require(completed.wait_for(100ms)==std::future_status::timeout && stopped.wait_for(100ms)==std::future_status::timeout && server.outstanding_authentication()!=0,
        "failed timer lost IO ownership or fabricated drain");
    server.retry_authentication_drain();
    const auto retry_deadline=std::chrono::steady_clock::now()+3s;
    while(server.authentication_drain_error() && std::chrono::steady_clock::now()<retry_deadline)std::this_thread::sleep_for(1ms);
    require(!server.authentication_drain_error(),"authentication timer retry did not rearm");
    server.stop([&]{done2.set_value();});
    const bool held=completed.wait_for(100ms)==std::future_status::timeout && completed2.wait_for(100ms)==std::future_status::timeout && stopped.wait_for(100ms)==std::future_status::timeout;
    release.set_value();cleanup.released=true;const bool drained=completed.wait_for(3s)==std::future_status::ready && completed2.wait_for(3s)==std::future_status::ready && completed3.wait_for(3s)==std::future_status::ready;
    if(!drained)io.stop();runner.join();require(held && drained && calls==1 && server.outstanding_authentication()==0 && !server.authentication_drain_error(),"login stop did not skip queued work and drain actual authentication");
}

int main() {
    using namespace eternity::net;
    try {
        verify_login_reply_clock();verify_live_reply();verify_late_encrypted_reply();verify_authenticated_reply_scan();verify_heartbeat_during_authentication();verify_bounded_authentication();verify_expired_disconnect_failure();verify_expired_copy_failure();verify_reentrant_handoff_stop();verify_replacement_disconnect_failure(false);verify_replacement_disconnect_failure(true);verify_reentrant_replacement_stop();verify_authentication_delivery_failure(false);verify_authentication_delivery_failure(true);verify_login_stop_drain();Fixture fixture;
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

        // Backend exceptions must fail authentication without killing the DB
        // worker or creating a session; the same encrypted connection can retry.
        for(const auto username:{"exception","nonstd"}) {
            Fixture failure;PeerSocket socket(failure.server.port());LoginPeer client(socket.packet());
            socket.write(frame(client.exchange));socket.write(frame(client.credentials(username,"testpass")));
            auto rejected=socket.packet();client.receive->crypt(rejected);
            require(rejected.size()==24 && rejected[2]==1 && failure.server.session_count()==0,"Backend exception did not fail closed");
            socket.write(frame(client.credentials("exists","testpass")));
            auto recovered=socket.packet();client.receive->crypt(recovered);auto recovered_realms=socket.packet();client.receive->crypt(recovered_realms);
            require(recovered[2]==0 && recovered_realms[2]==1 && failure.calls==2,"Login worker failed to recover after backend exception");
            socket.write(Packet{0,0});socket.closed();require(failure.server.session_count()==0,"Exception retry leaked a session");
        }

        // A replacement disconnect is an external owner callback, too. A
        // throwing callback must not escape the socket runner or authenticate
        // the replacement after the old owner failed to disconnect.
        {
            Fixture failure;eternity::session::LoginSession old{42,123,1,"previous",{}, {}};
            std::atomic_uint attempts{0};old.disconnect=[&]{if(++attempts==1)throw 42;};failure.registry->add(std::move(old));
            PeerSocket socket(failure.server.port());LoginPeer client(socket.packet());
            socket.write(frame(client.exchange));socket.write(frame(client.credentials("exists","testpass")));socket.closed();
            require(failure.server.session_count()==0,"throwing replacement callback left a new session");
            PeerSocket blocked(failure.server.port());LoginPeer blocked_peer(blocked.packet());
            blocked.write(frame(blocked_peer.exchange));blocked.write(frame(blocked_peer.credentials("exists","testpass")));blocked.closed();
            require(failure.server.pending_replacement_disconnects()==1,"failed replacement was bypassed");
            failure.server.retry_replacement_disconnects();
            const auto deadline=std::chrono::steady_clock::now()+3s;
            while(failure.server.pending_replacement_disconnects() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
            require(attempts==2 && !failure.server.pending_replacement_disconnects(),"replacement callback did not recover");
            PeerSocket retry(failure.server.port());LoginPeer next(retry.packet());
            retry.write(frame(next.exchange));retry.write(frame(next.credentials("exists","testpass")));
            auto accepted=retry.packet();next.receive->crypt(accepted);auto directory=retry.packet();next.receive->crypt(directory);
            require(accepted[2]==0 && directory[2]==1 && failure.calls==3,"listener failed after replacement callback recovery");
            retry.write(Packet{0,0});retry.closed();
        }

        Fixture handoff;
        handoff.realms->replace({{0,0,0,1000,"Eternity"}});
        PeerSocket selected(handoff.server.port());LoginPeer c(selected.packet());
        selected.write(frame(c.exchange));selected.write(frame(c.credentials("exists","testpass")));
        auto identity=selected.packet();c.receive->crypt(identity);
        auto directory=selected.packet();c.receive->crypt(directory);
        std::array<std::uint8_t,16> token{};std::copy_n(identity.begin()+8,16,token.begin());
        Packet selection{2,0xa2,0,0xff,0xff,0xff,0xff};c.send->crypt(selection);selected.write(frame(selection));
        auto endpoint=selected.packet();c.receive->crypt(endpoint);
        require(endpoint==Packet({2,0xa2,0,127,0,0,1}),"TCP realm handoff did not return the original IPv4 packet");
        const auto pending=handoff.registry->find(42);
        require(pending && pending->location==eternity::session::LoginSession::Location::handoff,
                "TCP selection did not reserve its world session");
        Packet disconnect{2,0};c.send->crypt(disconnect);selected.write(frame(disconnect));selected.closed();
        require(handoff.server.session_count()==1,"Login socket close destroyed the world reservation");
        const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        const auto claimed=handoff.registry->claim(42,token,0,99,now);
        require(claimed && claimed->game_key==eternity::session::game_key(c.secrets),
                "World consumer could not recover the login-derived key");
        require(!handoff.registry->claim(42,token,0,100,now),"World handoff token was replayable");
        require(handoff.registry->release_world(42,claimed->session_id,99),"World owner could not release the transferred account");
        std::cout<<"TCP login passed: fragmented/coalesced frames, RSA/AES, retries, pending dispatch and session replacement.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
