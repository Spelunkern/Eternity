#include "eternity/net/login_server.hpp"
#include "eternity/net/world_server.hpp"
#include "login_peer.hpp"
#include "login_socket.hpp"
#include <atomic>
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Fixture {
    asio::io_context io;
    std::shared_ptr<eternity::session::Registry> registry=std::make_shared<eternity::session::Registry>();
    std::atomic<unsigned int> loads{0},requests{0};
    eternity::net::WorldServer world;
    eternity::net::LoginServer login;
    std::jthread runner;
    static eternity::net::WorldServerOptions world_options() {eternity::net::WorldServerOptions options;options.port=0;return options;}
    static eternity::net::LoginServerOptions login_options() {
        eternity::net::LoginServerOptions options;options.port=0;options.realms={{0,0,0,1000,"Eternity"}};return options;
    }
    Fixture() : world(io,world_options(),registry,[this](const eternity::session::LoginSession& owner) {
        ++loads;require(owner.user_id==42 && owner.username=="exists","World loader lost authenticated identity");
        std::this_thread::sleep_for(20ms);
        eternity::game::CharacterRoster roster;roster.country=1;
        eternity::game::CharacterSummary character;character.id=7;character.slot=3;character.name="NativeHero";
        roster.slots[3]=character;return roster;
    },[this](const eternity::session::LoginSession& owner,eternity::game::CharacterRoster&,const Packet& packet) {
        ++requests;require(owner.user_id==42 && packet.size()==3 && packet[0]==9 && packet[1]==1 && packet[2]<16,"World command lost decrypted identity/payload");
        // Synthetic dispatcher reply checks ordering and persistent AES; country persistence is tested separately.
        return eternity::game::CharacterReply{{{9,1,packet[2],2}},false};
    }),login(io,login_options(),[](const eternity::auth::Credentials&,std::uint64_t,const std::string&) {
        return eternity::auth::LoginResult{0,42};
    },registry) {
        world.start();login.start();runner=std::jthread([this]{io.run();});
    }
    ~Fixture() {login.stop();world.stop();if(runner.joinable()) runner.join();}
};
int main() {
    using namespace eternity;
    try {
        Fixture fixture;
        PeerSocket login(fixture.login.port());LoginPeer peer(login.packet());
        login.write(net::frame(peer.exchange));login.write(net::frame(peer.credentials("exists","testpass")));
        auto accepted=login.packet();peer.receive->crypt(accepted);auto directory=login.packet();peer.receive->crypt(directory);
        require(accepted[2]==0 && directory[4]==0,"Login failed to advertise test world online");
        Packet select{2,0xa2,0,0xff,0xff,0xff,0xff};peer.send->crypt(select);login.write(net::frame(select));
        auto endpoint=login.packet();peer.receive->crypt(endpoint);
        require(endpoint==Packet({2,0xa2,0,127,0,0,1}),"Realm transfer endpoint changed");
        Packet leave_login{2,0};peer.send->crypt(leave_login);login.write(net::frame(leave_login));login.closed();
        require(fixture.registry->size()==1,"Login disconnect erased transfer reservation");
        const auto key=session::game_key(peer.secrets);net::SessionSecrets game_secrets{};
        std::copy_n(key.begin(),16,game_secrets.key.begin());std::copy_n(key.begin()+16,16,game_secrets.iv.begin());
        net::LegacyCipher outgoing(game_secrets),incoming(game_secrets);
        Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),accepted.begin()+8,accepted.begin()+24);
        PeerSocket world(fixture.world.port());auto stream=net::frame(connect);
        Packet command{9,1,0};outgoing.crypt(command);const auto early=net::frame(command);stream.insert(stream.end(),early.begin(),early.end());
        for(int i=1;i<16;++i) {Packet next{9,1,static_cast<std::uint8_t>(i)};outgoing.crypt(next);const auto encoded=net::frame(next);stream.insert(stream.end(),encoded.begin(),encoded.end());}
        world.write(std::span(stream).first(1));world.write(std::span(stream).subspan(1));
        auto greeting=world.packet();incoming.crypt(greeting);
        require(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3 && greeting[2]==0 && greeting[3]==2,
                "World did not send encrypted original greeting");
        auto country=world.packet();incoming.crypt(country);require(country==Packet({9,1,1,2}),"Country did not follow game greeting");
        for(const auto slot:{0,1,2,4}) {
            auto empty=world.packet();incoming.crypt(empty);
            require(empty==Packet({1,1,static_cast<std::uint8_t>(slot),0,0,0,0}),"Empty character slot changed over TCP");
        }
        auto populated=world.packet();incoming.crypt(populated);
        require(populated.size()==83 && populated[2]==3 && populated[3]==7 && populated[56]=='N',"Populated character corrupted over TCP");
        auto answer=world.packet();incoming.crypt(answer);
        require(answer==Packet({9,1,0,2}),"Pending command ran before initial character load");
        for(int i=1;i<16;++i) {auto reply=world.packet();incoming.crypt(reply);require(reply==Packet({9,1,static_cast<std::uint8_t>(i),2}),"Queued encrypted command lost or reordered");}
        require(fixture.loads==1 && fixture.requests==16,"Queued commands were not delivered once");
        PeerSocket replay(fixture.world.port());auto replay_frame=net::frame(connect);auto replay_stream=replay_frame;
        for(int i=1;i<11;++i) replay_stream.insert(replay_stream.end(),replay_frame.begin(),replay_frame.end());
        replay.write(replay_stream);replay.closed();
        require(fixture.registry->find(42)->location==session::LoginSession::Location::world && fixture.loads==1,
                "Token replay reached world data loader or erased owner");
        PeerSocket replacement(fixture.login.port());LoginPeer replacement_peer(replacement.packet());
        replacement.write(net::frame(replacement_peer.exchange));replacement.write(net::frame(replacement_peer.credentials("exists","testpass")));
        auto new_login=replacement.packet();replacement_peer.receive->crypt(new_login);
        auto new_directory=replacement.packet();replacement_peer.receive->crypt(new_directory);world.closed();
        require(new_login[2]==0 && fixture.registry->find(42)->location==session::LoginSession::Location::login,
                "Account relogin failed to replace the live world connection");
        replacement.write(Packet{0,0});replacement.closed();require(fixture.registry->size()==0,"Replacement disconnect leaked registry state");
        PeerSocket malformed(fixture.world.port());malformed.write(Packet{1,0});malformed.closed();
        require(fixture.loads==1,"Malformed unauthenticated frame reached world loader");
        std::cout<<"TCP login-to-world passed: token handoff, encrypted greeting/roster, queued commands, replay and live-world replacement.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
