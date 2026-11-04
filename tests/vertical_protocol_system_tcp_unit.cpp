#include "eternity/game/vertical_protocol.hpp"
#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <condition_variable>
#include <iostream>
#include <mutex>
using namespace eternity;using namespace eternity::game;using namespace std::chrono_literals;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Record {std::mutex mutex;std::condition_variable cv;unsigned nprotect=0,normal=0,closes=0;std::optional<std::uint32_t> reason;};
struct Application final:net::WorldApplication {
    std::shared_ptr<Record> record;net::WorldChannel channel;NProtectState protect{};
    Application(std::shared_ptr<Record> r,net::WorldChannel c):record(std::move(r)),channel(std::move(c)){}
    void open() override {channel.send({9,1,0});}
    void receive(const net::Packet& p) override {check(p==net::Packet({9,1,5}),"unexpected application packet");{std::lock_guard lock(record->mutex);++record->normal;}channel.send({9,1,5});}
    void receive_nprotect(const net::Packet& p) override {
        check(send_nprotect_query(false,protect,{}),"disabled query failed");check(receive_vertical_nprotect(p,protect,1,{})==VerticalNProtectResult::absent,"absent NProtect owner changed");
        {std::lock_guard lock(record->mutex);++record->nprotect;}channel.send({9,1,4});
    }
    void close() override {std::lock_guard lock(record->mutex);++record->closes;record->cv.notify_all();}
    void close_with_reason(std::optional<std::uint32_t> r) override {{std::lock_guard lock(record->mutex);record->reason=r;}close();}
};
int main(){try{
    asio::io_context io;auto registry=std::make_shared<session::Registry>();auto record=std::make_shared<Record>();net::WorldServerOptions options;options.port=0;
    net::WorldServer server(io,options,registry,[record](const session::LoginSession&,net::WorldChannel c){return std::make_unique<Application>(record,std::move(c));});server.start();std::jthread thread([&]{io.run();});
    struct Guard {net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
    session::LoginSession account{42,7,11,"native",{}, {}};account.token.fill(0x7a);registry->add(account);const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(42,7,0,now),"reserve failed");
    PeerSocket peer(server.port());net::Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher receiver(net::SessionSecrets{}),sender(net::SessionSecrets{});auto read=[&](){auto p=peer.packet();receiver.crypt(p);return p;};auto send=[&](net::Packet p){sender.crypt(p);peer.write(net::frame(p));};
    auto greeting=read();check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"greeting failed");check(read()==net::Packet({9,1,0}),"open failed");
    send({3,0});send({2,0xa3,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xef,0xcd,0xab,0x89,0xcc,0xdd,0xee,0xff});check(read()==net::Packet({9,1,4}),"NProtect did not reach connection owner");send({9,1,5});check(read()==net::Packet({9,1,5}),"AES continuity changed");send({2,0});peer.closed();
    {std::unique_lock lock(record->mutex);check(record->cv.wait_for(lock,3s,[&]{return record->closes==1;}),"close callback missing");check(record->reason==1 && record->nprotect==1 && record->normal==1,"system routing or close reason changed");}
    check(server.application_failures()==0,"unexpected application failure");std::cout<<"TCP system routing, retained NProtect state and close reason passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
