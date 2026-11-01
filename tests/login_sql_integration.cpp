#include "eternity/auth/sql_authenticator.hpp"
#include "eternity/net/login_server.hpp"
#include "login_peer.hpp"
#include "login_socket.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <thread>

struct SqlFixture {
    asio::io_context io;
    std::atomic<bool> backend_returned{false};
    eternity::net::LoginServer server;
    std::jthread runner;
    explicit SqlFixture(eternity::storage::SqlServerConfig config)
        : server(io,options(),[this,auth=eternity::auth::SqlAuthenticator(std::move(config))]
                 (const eternity::auth::Credentials& input,std::uint64_t id,const std::string& ip) {
                     const auto result=auth(input,id,ip);
                     backend_returned=true;
                     return result;
                 }) {
        server.start(); runner=std::jthread([this]{io.run();});
    }
    static eternity::net::LoginServerOptions options() { eternity::net::LoginServerOptions result; result.port=0; return result; }
    ~SqlFixture() { server.stop(); if(runner.joinable()) runner.join(); }
};

int main() {
    using namespace eternity::storage;
    using namespace eternity::net;
    try {
        const auto config=SqlServerConfig::from_environment();
        SqlServer database(config);
        const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count()%1000000;
        const std::string username="et_x"+std::to_string(suffix);
        const std::array<Parameter,1> parameters={username};
        const auto existing=database.query("SELECT COUNT(*) FROM PS_UserData.dbo.Users_Master WHERE UserID=?;",parameters);
        if(existing.size()!=1 || existing[0].rows[0][0]!=Cell{"0"})
            throw std::runtime_error("Synthetic missing account already exists");
        SqlFixture fixture(config);
        PeerSocket socket(fixture.server.port());
        LoginPeer peer(socket.packet());
        socket.write(frame(peer.exchange));
        socket.write(frame(peer.credentials(username,"testpass")));
        auto response=socket.packet(); peer.receive->crypt(response);
        if(response.size()!=24 || response[0]!=2 || response[1]!=0xa1 || response[2]!=1 ||
           fixture.server.session_count()!=0 || !fixture.backend_returned)
            throw std::runtime_error("SQL-backed TCP login did not preserve the missing-account result");
        Packet close{2,0}; peer.send->crypt(close); socket.write(frame(close)); socket.closed();
        std::cout<<"SQL-backed TCP authentication passed without creating or changing account data.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
