#define main unused_vertical_gate_fixture_main
#include "vertical_protocol_gate_unit.cpp"
#undef main
#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <condition_variable>
#include <mutex>
using namespace std::chrono_literals;
struct HandoffState {
    std::mutex mutex;std::condition_variable cv;bool pause=false,waiting=false,first=false,second=false,location=false,done=false,closed=false,failed=false,cancelled=false;
    bool mode_first=false,bad_position=false;unsigned entries=0;net::WorldChannel channel;
    void wait(const std::function<bool()>& condition){std::unique_lock lock(mutex);check(cv.wait_for(lock,3s,condition),"handoff timed out");}
    void release(bool& flag){std::lock_guard lock(mutex);flag=true;cv.notify_all();}
};
struct HandoffApp final:net::WorldApplication {
    std::shared_ptr<HandoffState> state;net::WorldChannel channel;ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;EffectStatus status;
    Actor actor{42,status,{1,1,1},{},scene,{items,enchant,mobs}};ConfiguredMap map;RuntimeZone zone;ZoneEntryState pos;ZoneActorEntryServices entry;
    Services services;WorldAdmission world;WorldAdmissionUser user;VerticalEntrySession session;
    HandoffApp(std::shared_ptr<HandoffState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)),services(zone,actor,pos,entry),world(services),session(world,user){
        map.settings.id=20;map.definition.size=81;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);pos.character_id=42;pos.position={40,0,40};
        entry.send_view=[](CellCoordinates,net::Packet){};entry.cell_info=[](CellCoordinates){};entry.obelisks=[](){};entry.nprotect=[](){};entry.diagnostic=[](){};entry.close=[](std::uint32_t){};
        actor.account_id()=11;user.user_id=11;user.map=20;world.track_selection(user);
    }
    void open() override {}
    void receive(const net::Packet& p) override {
        if(p==net::Packet{4,1,42,0,0,0}) {
            channel.send({4,1,0,42,0,0,0});
            channel.pause_input([s=state](bool ok){std::lock_guard lock(s->mutex);s->pause=ok;s->cv.notify_all();});
            state->wait([&]{return state->pause;});check(session.wait(),"real wait failed");state->release(state->waiting);
            state->wait([&]{return state->first;});
            if(state->mode_first){channel.enter_world();channel.send({0x77,0x77,1});}
            else {update_vertical_admission(world);check(user.location==3,"zone publication failed");state->release(state->location);channel.send({0x77,0x77,0});}
            state->wait([&]{return state->second;});
            if(state->mode_first){update_vertical_admission(world);check(user.location==3,"zone publication failed");state->release(state->location);}
            else channel.enter_world();
            pos.position.x=state->bad_position?81.0F:40.0F;channel.send({0x78,0x77,1});channel.resume_input();
        } else if(p==net::Packet{1,2}) {
            const auto result=session.receive(p);{std::lock_guard lock(state->mutex);++state->entries;state->failed=result==VerticalEntryRequest::rejected;check(result!=VerticalEntryRequest::unhandled,"gate ignored");if(state->failed)check(user.location==3 && world.zone_waiting_count(zone)==1,"failed entry lost queue");state->cv.notify_all();}
            if(result==VerticalEntryRequest::entered)channel.send({0x79,0x77});else channel.disconnect();
        } else if(p==net::Packet{9,1,55}) {channel.send({0x7a,0x77});}
        else throw std::runtime_error("unexpected inbound packet");
    }
    void close() override {if(user.location==4)zone.users->detach(42);world.untrack(user);std::lock_guard lock(state->mutex);state->closed=true;state->cv.notify_all();}
};
void run(bool mode_first,bool fail,bool overflow,bool abort=false){
    asio::io_context io;auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<HandoffState>();state->mode_first=mode_first;state->bad_position=fail;net::WorldServerOptions options;options.port=0;
    net::WorldServer server(io,options,registry,[state](const session::LoginSession&,net::WorldChannel c){state->channel=c;return std::make_unique<HandoffApp>(state,std::move(c));});server.start();std::jthread thread([&]{io.run();});
    struct Guard {net::WorldServer& server;std::jthread& thread;std::shared_ptr<HandoffState> state;~Guard(){{std::lock_guard lock(state->mutex);state->pause=true;state->first=true;state->second=true;state->cv.notify_all();}server.stop();if(thread.joinable())thread.join();}} guard{server,thread,state};
    session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
    PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});
    auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"greeting failed");std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());net::WorldXor xor_key(seed);
    auto send=[&](net::Packet p){outgoing.crypt(p);peer.write(net::frame(p));};send({4,1,42,0,0,0});auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0,42,0,0,0}),"selection acknowledgement AES differs");state->wait([&]{return state->waiting;});send({1,2});
    if(abort){state->channel.disconnect();state->channel.pause_input([state](bool active){std::lock_guard lock(state->mutex);state->cancelled=!active;state->cv.notify_all();});state->wait([&]{return state->cancelled;});state->release(state->first);state->release(state->second);state->wait([&]{return state->closed;});check(state->entries==0,"disconnected handoff entered world");return;}
    if(overflow){for(int i=0;i<8;++i)send({9,1,55});peer.closed();state->release(state->first);state->release(state->second);state->wait([&]{return state->closed;});check(state->entries==0,"overflow entered world");return;}
    send({9,1,55});
    state->release(state->first);auto first=peer.packet();if(mode_first)xor_key.crypt(first);else incoming.crypt(first);check(first==net::Packet({0x77,0x77,static_cast<std::uint8_t>(mode_first)}),"first barrier or outbound cipher differs");
    {std::lock_guard lock(state->mutex);check(state->entries==0,"entry ran before both barriers");if(!mode_first)check(state->location,"zone-first barrier missing");}
    state->release(state->second);auto second=peer.packet();xor_key.crypt(second);check(second==net::Packet({0x78,0x77,1}),"final barrier or XOR differs");
    if(fail){peer.closed();state->wait([&]{return state->closed;});check(state->failed && state->entries==1,"failed admission did not close or ran twice");return;}
    auto entered=peer.packet();xor_key.crypt(entered);check(entered==net::Packet({0x79,0x77}),"real actor did not enter");auto continuity=peer.packet();xor_key.crypt(continuity);check(continuity==net::Packet({0x7a,0x77}),"inbound AES stream lost continuity");
    {std::lock_guard lock(state->mutex);check(state->entries==1 && state->location,"entry did not run once after zone update");}asio::error_code ignored;peer.socket.close(ignored);
}
int main(){try{run(false,false,false);run(true,false,false);run(false,true,false);run(false,false,true);run(false,false,false,true);std::cout<<"Two barrier orders, real zone admission/failure, AES continuity, eight-frame overflow and disconnect passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
