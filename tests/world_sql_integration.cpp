#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <atomic>
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace eternity;
    try {
        asio::io_context io;
        auto registry=std::make_shared<session::Registry>();
        session::LoginSession account{2147483000,1,11,"et_fixture",{}, {}};
        account.token.fill(0x5a);registry->add(account);
        const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        require(registry->select(account.user_id,account.session_id,0,now),"Cannot reserve isolated SQL test world session");
        const auto config=storage::SqlServerConfig::from_environment();
        std::atomic_bool loaded{false};net::WorldServerOptions options;options.port=0;
        net::WorldServer world(io,options,registry,[&](const session::LoginSession& identity) {
            storage::SqlServer database(config);
            game::Characters characters(database,255,std::chrono::sys_days{std::chrono::year{2024}/1/1});
            auto roster=characters.load(identity.user_id);
            for(const auto& slot:roster.slots) require(!slot,"Isolated SQL test account identifier is occupied");
            require(roster.country==2 && roster.max_grow==2,"Empty original SQL account defaults changed");
            loaded=true;return roster;
        });
        world.start();std::jthread runner([&] {io.run();});
        try {
            PeerSocket peer(world.port());
            Packet connect{1,0xa3};
            for(unsigned int index=0;index<4;++index) connect.push_back(static_cast<std::uint8_t>(account.user_id>>(index*8)));
            connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));
            net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});
            auto greeting=peer.packet();incoming.crypt(greeting);
            require(greeting.size()==20 && greeting[2]==0 && greeting[3]==2,"SQL world greeting failed");
            auto country=peer.packet();incoming.crypt(country);
            require(country==Packet({9,1,2,2}) && loaded,"SQL world country did not reach the peer");
            for(std::uint8_t index=0;index<5;++index) {
                auto slot=peer.packet();incoming.crypt(slot);
                require(slot==Packet({1,1,index,0,0,0,0}),"SQL world slot did not reach the peer");
            }
            Packet close{7,1};outgoing.crypt(close);peer.write(net::frame(close));peer.closed();
            require(registry->size()==0,"SQL-backed world disconnect retained the session");
            world.stop();runner.join();
        } catch(...) {world.stop();runner.join();throw;}
        std::cout<<"SQL-backed game TCP greeting and character roster passed without changing account data.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
