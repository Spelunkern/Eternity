#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <condition_variable>
#include <iostream>
#include <mutex>

using namespace eternity;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
enum class Behavior {normal,block_open,block_receive,pause_entry,factory_error,null_factory,open_error,receive_error,close_error,bulk};
struct State {
    std::mutex mutex;
    std::condition_variable changed;
    Behavior behavior=Behavior::normal;
    bool entered=false,released=false;
    unsigned int opens=0,receives=0,closes=0,destroyed=0;
    session::LoginSession identity{};
    net::WorldChannel channel;
    void wait(const std::function<bool()>& predicate) {
        std::unique_lock lock(mutex);
        require(changed.wait_for(lock,3s,predicate),"Application lifecycle timed out");
    }
    void release() {std::lock_guard lock(mutex);released=true;changed.notify_all();}
    void block() {
        std::unique_lock lock(mutex);entered=true;changed.notify_all();
        changed.wait(lock,[&]{return released;});
    }
};
class Application final : public net::WorldApplication {
    std::shared_ptr<State> state;
    net::WorldChannel channel;
    unsigned int sequence=0;
public:
    Application(std::shared_ptr<State> value,net::WorldChannel transport) : state(std::move(value)),channel(std::move(transport)) {}
    ~Application() override {std::lock_guard lock(state->mutex);++state->destroyed;state->changed.notify_all();}
    void open() override {
        {std::lock_guard lock(state->mutex);++state->opens;state->changed.notify_all();}
        if(state->behavior==Behavior::block_open) state->block();
        if(state->behavior==Behavior::open_error) throw 42; // Also catch non-std exceptions.
        if(state->behavior==Behavior::bulk) {
            for(unsigned int i=0;i<40;++i) {Packet bytes(1024,static_cast<std::uint8_t>(i));bytes[0]=9;bytes[1]=1;channel.send(std::move(bytes));}
        } else channel.send({9,1,0});
    }
    void receive(const Packet& packet) override {
        {std::lock_guard lock(state->mutex);++state->receives;state->changed.notify_all();}
        if(state->behavior==Behavior::block_receive) state->block();
        if(state->behavior==Behavior::receive_error) throw std::runtime_error("Test receive failure");
        ++sequence;
        if(packet.at(2)==1) {
            if(state->behavior==Behavior::pause_entry) channel.pause_input();
            channel.send({9,1,static_cast<std::uint8_t>(sequence)});
            channel.enter_world();channel.send({1,2,static_cast<std::uint8_t>(sequence)});
        } else if(packet.at(2)==2) {
            channel.send({1,2,static_cast<std::uint8_t>(sequence)});
            channel.enter_character_selection();channel.send({9,1,static_cast<std::uint8_t>(sequence)});
        } else channel.send({9,1,static_cast<std::uint8_t>(sequence)});
    }
    void close() override {
        {std::lock_guard lock(state->mutex);++state->closes;state->changed.notify_all();}
        if(state->behavior==Behavior::close_error) throw std::runtime_error("Test close failure");
    }
};
struct Fixture {
    asio::io_context io;
    std::shared_ptr<session::Registry> registry=std::make_shared<session::Registry>();
    std::shared_ptr<State> state=std::make_shared<State>();
    net::WorldServer world;
    std::jthread runner;
    static net::WorldServerOptions options(std::size_t limit) {
        net::WorldServerOptions result;result.port=0;result.maximum_queued_bytes=limit;return result;
    }
    explicit Fixture(Behavior behavior=Behavior::normal,std::size_t limit=0xc0000)
        : world(io,options(limit),registry,[state=state](const session::LoginSession& account,net::WorldChannel channel) -> std::unique_ptr<net::WorldApplication> {
            {std::lock_guard lock(state->mutex);state->identity=account;state->channel=channel;}
            if(state->behavior==Behavior::factory_error) throw std::runtime_error("Test factory failure");
            if(state->behavior==Behavior::null_factory) return {};
            return std::make_unique<Application>(state,std::move(channel));
        }) {
        state->behavior=behavior;world.start();runner=std::jthread([this]{io.run();});
    }
    ~Fixture() {state->release();world.stop();if(runner.joinable()) runner.join();}
    Packet reserve(std::uint32_t id=42,std::uint64_t sid=101) {
        session::LoginSession account{id,sid,11,"native",{}, {}};account.token.fill(static_cast<std::uint8_t>(sid));
        auto previous=registry->add(account);if(previous && previous->disconnect) previous->disconnect();
        const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        require(registry->select(id,sid,0,now),"Cannot reserve application session");
        Packet connect{1,0xa3};for(unsigned int i=0;i<4;++i) connect.push_back(static_cast<std::uint8_t>(id>>(i*8)));
        connect.insert(connect.end(),account.token.begin(),account.token.end());return connect;
    }
};
struct Peer {
    PeerSocket socket;
    net::LegacyCipher incoming{net::SessionSecrets{}},outgoing{net::SessionSecrets{}};
    std::unique_ptr<net::WorldXor> world;
    Peer(Fixture& fixture,Packet connect,bool early=false) : socket(fixture.world.port()) {
        auto bytes=net::frame(connect);
        if(early) {Packet command{9,1,0};outgoing.crypt(command);auto framed=net::frame(command);bytes.insert(bytes.end(),framed.begin(),framed.end());}
        socket.write(bytes);auto greeting=selection();require(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"Missing authenticated greeting");
        std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());world=std::make_unique<net::WorldXor>(seed);
    }
    Packet selection() {auto packet=socket.packet();incoming.crypt(packet);return packet;}
    Packet in_world() {auto packet=socket.packet();world->crypt(packet);return packet;}
    void command(std::uint8_t action) {Packet packet{9,1,action};outgoing.crypt(packet);socket.write(net::frame(packet));}
    void close() {asio::error_code ignored;socket.socket.shutdown(tcp::socket::shutdown_both,ignored);socket.socket.close(ignored);}
};
int main() {
    try {
        {
            Fixture fixture;Peer first(fixture,fixture.reserve(),true);
            require(first.selection()==Packet({9,1,0}) && first.selection()==Packet({9,1,1}),"Early command ran before open or lost application state");
            first.command(1);require(first.selection()==Packet({9,1,2}),"Selection send changed before wire transition");
            require(first.in_world()==Packet({1,2,2}),"World XOR send failed");
            first.command(2);require(first.in_world()==Packet({1,2,3}),"World mode changed too early");
            require(first.selection()==Packet({9,1,3}),"AES stream did not resume after returning to selection");
            net::WorldChannel retained;
            {std::lock_guard lock(fixture.state->mutex);
                require(fixture.state->identity.user_id==42 && fixture.state->identity.session_id==101 && fixture.state->identity.world_connection!=0,
                        "Application identity lost authenticated connection");retained=fixture.state->channel;}
            retained.send({9,1,99});require(first.selection()==Packet({9,1,99}),"External asynchronous delivery failed");
            Peer second(fixture,fixture.reserve(43,102),true);
            require(second.selection()==Packet({9,1,0}) && second.selection()==Packet({9,1,1}),"Connections shared application sequence state");
            first.close();second.close();fixture.state->wait([&]{return fixture.state->closes==2 && fixture.state->destroyed==2;});
            retained.send({9,1,88});retained.enter_world();retained.disconnect();
            require(fixture.registry->size()==0 && fixture.world.application_failures()==0,"Disconnect leaked account or application failure");
        }
        // A second frame can arrive while the first application call is still
        // preparing the transition. It must be decoded after the posted state
        // change rather than discarded as a character-selection opcode.
        {
            Fixture fixture(Behavior::block_receive);Peer peer(fixture,fixture.reserve());
            require(peer.selection()==Packet({9,1,0}),"Missing initial application packet");
            peer.command(1);
            fixture.state->wait([&]{return fixture.state->entered;});
            Packet entry{1,2,0};peer.outgoing.crypt(entry);peer.socket.write(net::frame(entry));
            std::this_thread::sleep_for(50ms);
            fixture.state->release();
            require(peer.selection()==Packet({9,1,1}),"Selection packet order changed");
            require(peer.in_world()==Packet({1,2,1}),"World transition packet changed");
            require(peer.in_world()==Packet({9,1,2}),"Pipelined world entry was discarded");
            peer.close();fixture.state->wait([&]{return fixture.state->destroyed==1;});
        }
        // Zone selection can complete after the character-selection call has
        // returned. A received 0201 must wait for explicit zone admission.
        {
            Fixture fixture(Behavior::pause_entry);Peer peer(fixture,fixture.reserve());
            require(peer.selection()==Packet({9,1,0}),"Missing initial application packet");
            peer.command(1);
            require(peer.selection()==Packet({9,1,1}),"Selection packet order changed");
            require(peer.in_world()==Packet({1,2,1}),"World mode marker missing");
            Packet entry{1,2,0};peer.outgoing.crypt(entry);peer.socket.write(net::frame(entry));
            net::WorldChannel retained;
            {
                std::unique_lock lock(fixture.state->mutex);
                require(!fixture.state->changed.wait_for(lock,100ms,[&]{return fixture.state->receives>1;}),
                        "Zone entry dispatched before admission resumed input");
                retained=fixture.state->channel;
            }
            retained.resume_input();
            require(peer.in_world()==Packet({9,1,2}),"Queued zone entry was lost after admission");
            peer.close();fixture.state->wait([&]{return fixture.state->destroyed==1;});
        }
        // A remote close, account replacement and server stop must all wait for open/receive.
        for(const auto behavior:{Behavior::block_open,Behavior::block_receive}) {
            for(unsigned int cause=0;cause<3;++cause) {
                Fixture fixture(behavior);Peer peer(fixture,fixture.reserve(),true);
                if(behavior==Behavior::block_receive) require(peer.selection()==Packet({9,1,0}),"Missing open before blocked receive");
                fixture.state->wait([&]{return fixture.state->entered;});
                if(cause==0) peer.close();
                else if(cause==1) {fixture.reserve(42,102);peer.socket.closed();}
                else {fixture.world.stop();peer.socket.closed();}
                // Seeing registry release proves the socket close ran, before releasing the worker.
                if(cause!=1) {
                    const auto deadline=std::chrono::steady_clock::now()+3s;
                    while(fixture.registry->size()!=0 && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
                    require(fixture.registry->size()==0,"Connection did not close during blocked application");
                }
                {std::lock_guard lock(fixture.state->mutex);require(fixture.state->closes==0,"Close overlapped in-flight application call");}
                fixture.state->release();fixture.state->wait([&]{return fixture.state->destroyed==1;});
                {std::lock_guard lock(fixture.state->mutex);
                    require(fixture.state->closes==1 && fixture.state->receives==(behavior==Behavior::block_open?0u:1u),"Queued receive ran after close or close ran more than once");}
                if(cause==1) require(fixture.registry->find(42)->session_id==102,"Old application erased replacement account");
            }
        }
        for(const auto behavior:{Behavior::factory_error,Behavior::null_factory,Behavior::open_error,Behavior::receive_error,Behavior::close_error}) {
            Fixture fixture(behavior);Peer peer(fixture,fixture.reserve());
            if(behavior==Behavior::receive_error || behavior==Behavior::close_error) require(peer.selection()==Packet({9,1,0}),"Missing failure-test open");
            if(behavior==Behavior::receive_error) peer.command(0);
            else if(behavior==Behavior::close_error) peer.close();
            if(behavior!=Behavior::close_error) peer.socket.closed();
            if(behavior!=Behavior::factory_error && behavior!=Behavior::null_factory) {
                fixture.state->wait([&]{return fixture.state->destroyed==1;});
                std::lock_guard lock(fixture.state->mutex);require(fixture.state->closes==1,"Failed application skipped close");
            }
            require(fixture.world.application_failures()==1,"Application failure was not observable");
        }
        {
            Fixture fixture(Behavior::bulk);Peer peer(fixture,fixture.reserve());
            for(unsigned int i=0;i<40;++i) {
                auto packet=peer.selection();Packet expected(1024,static_cast<std::uint8_t>(i));expected[0]=9;expected[1]=1;
                require(packet==expected,"Catalog-sized output exceeded obsolete 16 KiB cap or lost order");
            }
            peer.close();fixture.state->wait([&]{return fixture.state->destroyed==1;});
        }
        {
            Fixture fixture(Behavior::bulk,512);Peer peer(fixture,fixture.reserve());peer.socket.closed();
            fixture.state->wait([&]{return fixture.state->destroyed==1;});
            require(fixture.world.application_failures()==0,"Configured transport bound counted as application failure");
        }
        std::cout<<"World application ownership passed: independent state, queued input, async delivery, AES/XOR transitions, in-flight close, failures and bounded catalogs.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
