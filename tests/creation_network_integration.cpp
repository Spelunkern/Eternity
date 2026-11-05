#include "eternity/net/world_server.hpp"
#include "login_socket.hpp"
#include <atomic>
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::uint64_t ticks() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct CreationIdClock:eternity::game::ItemIdClock {
    std::uint32_t ticks_ms() override {return 10000;}
    std::uint32_t packed_local_calendar() override {
        return eternity::game::pack_item_id_time(std::chrono::sys_days{std::chrono::year{2024}/1/1});
    }
};
struct Fixture {
    CreationIdClock id_clock; // Must outlive commands and its retained ItemIds.
    asio::io_context io;
    std::shared_ptr<eternity::session::Registry> registry=std::make_shared<eternity::session::Registry>();
    std::shared_ptr<eternity::game::CharacterCommands> commands;
    std::atomic<unsigned int> max_grow{0};
    std::unique_ptr<eternity::net::WorldServer> world;
    std::jthread runner;
    Fixture(const eternity::storage::SqlServerConfig& config,const std::string& catalog) {
        using namespace eternity;
        storage::SqlServer database(config);
        auto rules=std::make_shared<game::CreationRules>(game::CreationRules::load(game::Definitions::load(database)));
        const auto base=std::chrono::sys_days{std::chrono::year{2024}/1/1};
        commands=std::make_shared<game::CharacterCommands>(config,game::CharacterCommandOptions{1,base,catalog},rules,
            std::make_shared<game::ItemIds>(0,id_clock),[base] {return game::CreationTime{10000,base};},[](std::size_t){return 0;});
        net::WorldServerOptions options;options.port=0;
        world=std::make_unique<net::WorldServer>(io,options,registry,[config,catalog,base](const session::LoginSession& owner) {
            storage::SqlServer connection(config);return game::Characters(connection,1,base,catalog).load(owner.user_id);
        },[this](const session::LoginSession& owner,game::CharacterRoster& roster,const Packet& packet) {
            auto reply=commands->execute(owner.user_id,owner.username,roster,packet);max_grow=roster.max_grow;return reply;
        });
        world->start();runner=std::jthread([this]{io.run();});
    }
    ~Fixture() {world->stop();if(runner.joinable()) runner.join();}
    eternity::session::LoginSession reserve(std::uint64_t session) {
        eternity::session::LoginSession owner{5001,session,11,"tcp_user",{}, {}};owner.token.fill(static_cast<std::uint8_t>(session));
        const auto previous=registry->add(owner);if(previous && previous->disconnect) previous->disconnect();
        require(registry->select(owner.user_id,owner.session_id,0,ticks()),"TCP creation fixture could not reserve session");return owner;
    }
};
struct GamePeer {
    PeerSocket socket;
    eternity::net::LegacyCipher send{eternity::net::SessionSecrets{}},receive{eternity::net::SessionSecrets{}};
    GamePeer(std::uint16_t port,const eternity::session::LoginSession& owner) : socket(port) {
        Packet connect{1,0xa3};for(unsigned int i=0;i<4;++i) connect.push_back(static_cast<std::uint8_t>(owner.user_id>>(i*8)));
        connect.insert(connect.end(),owner.token.begin(),owner.token.end());socket.write(eternity::net::frame(connect));
        const auto greeting=packet();require(greeting.size()==20 && greeting[2]==0 && greeting[3]==2,"TCP creation world greeting failed");
    }
    Packet packet() {auto p=socket.packet();receive.crypt(p);return p;}
    Packet encoded(Packet p) {send.crypt(p);return eternity::net::frame(p);}
    void write(Packet p) {socket.write(encoded(std::move(p)));}
    void list(std::size_t populated,bool country) {
        if(country) require(packet().size()==4,"Initial country packet is missing");
        for(std::size_t i=0;i<5-populated;++i) require(packet().size()==7,"Original empty slots must precede populated records");
        for(std::size_t i=0;i<populated;++i) require(packet().size()==83,"Created character list wire record changed");
    }
};
Packet create(std::uint8_t slot,std::string name,std::uint8_t family=0,std::uint8_t grow=0,std::uint8_t job=0) {
    Packet p(31,0);p[0]=2;p[1]=1;p[2]=slot;p[3]=family;p[4]=grow;p[8]=job;
    std::copy(name.begin(),name.end(),p.begin()+10);return p;
}
int main(int argc,char** argv) {
    using namespace eternity;
    try {
        if(argc!=2 || !std::string_view(argv[1]).starts_with("Eternity_Test_Creation_")) throw std::runtime_error("TCP creation test requires disposable catalog");
        const std::string catalog=argv[1];const auto config=storage::SqlServerConfig::from_environment();
        Fixture fixture(config,catalog);auto owner=fixture.reserve(1);
        GamePeer first(fixture.world->port(),owner);first.list(0,true);
        first.write(create(0," Native Hero "));require(first.packet()==Packet({2,1,1}),"Creation without country did not return original result 1");
        auto stream=first.encoded({9,1,0});auto request=first.encoded(create(0," Native Hero "));
        stream.insert(stream.end(),request.begin(),request.end());first.socket.write(std::span(stream).first(1));first.socket.write(std::span(stream).subspan(1));
        require(first.packet()==Packet({2,1,0}),"Country/create TCP pipeline failed or emitted an invented country response");first.list(1,false);
        require(fixture.max_grow==2,"Character create unexpectedly reloaded the SQL max-grow override");
        first.write(create(1,"NativeHero"));require(first.packet()==Packet({2,1,1}),"Duplicate TCP name was accepted");
        first.write(create(1,"SecondHero",0,2));require(first.packet()==Packet({2,1,0}),"Second TCP creation failed");first.list(2,false);
        storage::SqlServer database(config);game::Characters repository(database,1,std::chrono::sys_days{std::chrono::year{2024}/1/1},catalog);
        const auto roster=repository.load(owner.user_id);
        require(roster.slots[0] && roster.slots[1] && roster.slots[0]->name=="NativeHero" && roster.slots[0]->hp==250 && roster.slots[1]->hp==270,
                "TCP-created character stats/name did not persist through original SQL");
        const auto character_id=roster.slots[0]->id;auto deletion=[&](std::uint8_t op){Packet p{op,1};for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(character_id>>(i*8)));return p;};
        auto request_delete=deletion(3);first.write(request_delete);auto expected_delete=request_delete;expected_delete.insert(expected_delete.begin()+2,0);
        require(first.packet()==expected_delete && repository.load(owner.user_id).slots[0]->remaining_time,"TCP deletion request did not persist");
        auto cancel_delete=deletion(15);first.write(cancel_delete);auto expected_cancel=cancel_delete;expected_cancel.insert(expected_cancel.begin()+2,0);
        require(first.packet()==expected_cancel && !repository.load(owner.user_id).slots[0]->remaining_time,"TCP deletion cancellation did not persist");
        const auto ids=database.query("SELECT COUNT(*),COUNT(DISTINCT ItemUID) FROM ["+catalog+"].dbo.CharItems WHERE CharID IN "
            "(SELECT CharID FROM ["+catalog+"].dbo.Chars WHERE UserUID=5001); SELECT COUNT(*) FROM ["+catalog+
            "].dbo.CharItems WHERE CharID IN (SELECT CharID FROM ["+catalog+"].dbo.Chars WHERE UserUID=5001) AND "
            "ItemUID / CAST(4294967296 AS bigint)=1614938112 AND ItemUID % 65536=0;");
        require(ids.size()==2 && ids[0].rows[0][0]==storage::Cell{"4"} && ids[0].rows[0][1]==storage::Cell{"4"} &&
                ids[1].rows[0][0]==storage::Cell{"4"},"TCP starter IDs did not retain original calendar/server layout and uniqueness");
        const std::array<storage::Parameter,1> pending_deadline={std::int64_t{game::pack_world_time(
            std::chrono::sys_days{std::chrono::year{2030}/1/1})}};
        database.query("UPDATE ["+catalog+"].dbo.Chars SET RemainTime=? WHERE UserUID=5001;",pending_deadline);
        owner=fixture.reserve(2);first.socket.closed();GamePeer second(fixture.world->port(),owner);second.list(2,true);
        stream=second.encoded({9,1,1});request=second.encoded(create(2,"VailHero",2,3,2));stream.insert(stream.end(),request.begin(),request.end());second.socket.write(stream);
        require(second.packet()==Packet({2,1,0}),"Faction change did not process pending deletion before create");second.list(1,false);
        const auto switched=repository.load(owner.user_id);
        require(switched.country==1 && !switched.slots[0] && !switched.slots[1] && switched.slots[2] &&
                switched.slots[2]->family==2 && switched.slots[2]->grow==3 && switched.slots[2]->hp==300 && switched.slots[2]->mp==145 &&
                fixture.max_grow==3,"Faction-change/relogin mode semantics changed");
        const auto deleted=database.query("SELECT COUNT(*) FROM ["+catalog+"].dbo._DeletedChars WHERE CharID IN "
            "(SELECT CharID FROM ["+catalog+"].dbo.Chars WHERE UserUID=5001 AND Del=1);");
        require(deleted[0].rows[0][0]==storage::Cell{"2"},"Original pending character deletion records were not written");
        second.write({7,1});second.socket.closed();require(fixture.registry->size()==0,"Character close retained world session");
        std::cout<<"SQL-backed TCP creation passed: faction, stats/items, duplicate names, cached modes, relogin and pending deletion.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
