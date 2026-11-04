#define main unused_vertical_gate_fixture_main
#include "vertical_protocol_gate_unit.cpp"
#undef main
#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <condition_variable>
#include <mutex>
using namespace std::chrono_literals;
struct TcpState {std::mutex mutex;std::condition_variable cv;bool blocked=false,release=false;unsigned entered=0,other=0;};
struct EntryApplication final:net::WorldApplication {
    net::WorldChannel channel;std::shared_ptr<TcpState> shared;ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;EffectStatus status;
    Actor actor{42,status,{1,1,1},{},scene,{items,enchant,mobs}};ConfiguredMap map;RuntimeZone zone;ZoneEntryState position;ZoneActorEntryServices entry;
    Services services;WorldAdmission admission;WorldAdmissionUser user;VerticalEntrySession session;
    EntryApplication(net::WorldChannel c,std::shared_ptr<TcpState> s):channel(std::move(c)),shared(std::move(s)),services(zone,actor,position,entry),admission(services),session(admission,user) {
        map.settings.id=20;map.definition.size=81;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);
        position.character_id=42;position.position={40,0,40};entry.send_view=[](CellCoordinates,net::Packet){};entry.cell_info=[](CellCoordinates){};entry.obelisks=[](){};entry.nprotect=[](){};entry.diagnostic=[](){};entry.close=[](std::uint32_t){};
        actor.account_id()=11;user.user_id=11;user.map=20;admission.track_selection(user);
    }
    void open() override {}
    void receive(const net::Packet& p) override {
        if(p==net::Packet{4,1}) {
            {std::unique_lock lock(shared->mutex);shared->blocked=true;shared->cv.notify_all();shared->cv.wait(lock,[&]{return shared->release;});}
            check(session.wait(),"wait failed");update_vertical_admission(admission);check(user.location==3,"zone wait missing");
            channel.send({4,1,0});channel.enter_world();channel.send({0x77,0x77});
        } else if(p==net::Packet{1,2}) {
            check(session.receive(p)==VerticalEntryRequest::entered,"entry failed");
            {std::lock_guard lock(shared->mutex);++shared->entered;}channel.send({0x78,0x77});
        } else {std::lock_guard lock(shared->mutex);++shared->other;shared->cv.notify_all();}
    }
    void close() override {if(user.location==4) zone.users->detach(42);admission.untrack(user);}
};
int main(){try {
    asio::io_context io;auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<TcpState>();net::WorldServerOptions options;options.port=0;
    net::WorldServer server(io,options,registry,[state](const session::LoginSession&,net::WorldChannel c){return std::make_unique<EntryApplication>(std::move(c),state);});server.start();std::jthread thread([&]{io.run();});
    struct Cleanup {net::WorldServer& server;std::jthread& thread;std::shared_ptr<TcpState> state;~Cleanup(){ {std::lock_guard lock(state->mutex);state->release=true;state->cv.notify_all();} server.stop();if(thread.joinable()) thread.join();}} cleanup{server,thread,state};
    session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
    PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"greeting failed");std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());net::WorldXor xor_key(seed);
    auto send=[&](net::Packet p){outgoing.crypt(p);peer.write(net::frame(p));};send({1,2});send({4,1});
    {std::unique_lock lock(state->mutex);check(state->cv.wait_for(lock,3s,[&]{return state->blocked;}),"selection worker did not block");}
    send({9,1});
    // The earlier 0201 precedes 0104 in the stream and was filtered in selection state.
    {std::lock_guard lock(state->mutex);state->release=true;state->cv.notify_all();}
    auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0}),"selection AES changed");auto marker=peer.packet();xor_key.crypt(marker);check(marker==net::Packet({0x77,0x77}),"world XOR transition failed");
    {std::unique_lock lock(state->mutex);check(state->cv.wait_for(lock,3s,[&]{return state->other==1;}),"selection-family sentinel lost");check(state->entered==0,"early request unexpectedly dispatched");}
    send({1,2});auto accepted=peer.packet();xor_key.crypt(accepted);check(accepted==net::Packet({0x78,0x77}),"synchronized client entry failed");
    {std::lock_guard lock(state->mutex);check(state->entered==1,"entry count differs");}asio::error_code ignored;peer.socket.close(ignored);std::cout<<"TCP confirmed selection filtering and ordered AES-to-XOR/real admission handoff\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
