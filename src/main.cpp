#include "eternity/storage/sql_server.hpp"
#include "eternity/game/definitions.hpp"
#include "eternity/game/map_config.hpp"
#include "eternity/game/linked_mob_config.hpp"
#include "eternity/game/zone_environment.hpp"
#include "eternity/game/npcs.hpp"
#include "eternity/game/quests.hpp"
#include "eternity/game/item_quests.hpp"
#include "eternity/game/equipment.hpp"
#include "eternity/auth/sql_authenticator.hpp"
#include "eternity/net/login_server.hpp"
#include "eternity/net/world_server.hpp"
#include <asio.hpp>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string_view>

namespace {
unsigned int environment_number(const char* name,unsigned int fallback,unsigned int maximum) {
    const char* raw=std::getenv(name);
    if(!raw) return fallback;
    const std::string_view value(raw);unsigned int number=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || number>maximum)
        throw std::invalid_argument(std::string(name)+" must be in 0.."+std::to_string(maximum));
    return number;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Eternity 0.1.0 (native reconstruction in progress)\n";
        return 0;
    }
    if (argc != 2 || (std::string_view(argv[1]) != "--check-database" &&
                      std::string_view(argv[1]) != "--check-original-loaders" &&
                      std::string_view(argv[1]) != "--serve" &&
                      std::string_view(argv[1]) != "--serve-login")) {
        std::cerr << "Usage: eternity-server --version | --check-database | --check-original-loaders | --serve-login | --serve\n"
                     "Game TCP currently supports character listings/creation; world entry and gameplay remain incomplete.\n";
        return 2;
    }
    try {
        const auto database_config = eternity::storage::SqlServerConfig::from_environment();
        eternity::storage::SqlServer database(database_config);
        if (std::string_view(argv[1]) == "--serve-login" || std::string_view(argv[1]) == "--serve") {
            (void)database.query("SELECT 1;");
            eternity::net::LoginServerOptions options;
            if (const char* address=std::getenv("ETERNITY_LOGIN_BIND")) options.address=address;
            options.port=static_cast<std::uint16_t>(environment_number("ETERNITY_LOGIN_PORT",options.port,65535));
            asio::io_context io;
            auto sessions=std::make_shared<eternity::session::Registry>();
            auto realms=std::make_shared<eternity::auth::RealmDirectory>(options.realms);
            std::unique_ptr<eternity::net::WorldServer> world;
            if(std::string_view(argv[1])=="--serve") {
                eternity::net::WorldServerOptions game_options;
                if(const char* address=std::getenv("ETERNITY_WORLD_BIND")) game_options.address=address;
                game_options.port=static_cast<std::uint16_t>(environment_number("ETERNITY_WORLD_PORT",game_options.port,65535));
                game_options.realm=static_cast<std::uint8_t>(environment_number("ETERNITY_WORLD_REALM",0,253));
                const auto database_id=static_cast<std::uint8_t>(environment_number("ETERNITY_WORLD_DATABASE_ID",1,255));
                auto advertised=asio::ip::make_address_v4("127.0.0.1");
                if(const char* address=std::getenv("ETERNITY_WORLD_ADVERTISE")) advertised=asio::ip::make_address_v4(address);
                const auto base=eternity::game::load_world_base(database);
                auto starts=eternity::game::original_start_positions();
                const char* configured_map=std::getenv("ETERNITY_MAP_INI");
                const std::filesystem::path map_path=configured_map ? configured_map : ".local/server-data/Map.ini";
                if(configured_map || std::filesystem::is_regular_file(map_path)) starts=eternity::game::read_start_positions(map_path);
                auto rules=std::make_shared<eternity::game::CreationRules>(eternity::game::CreationRules::load(
                    eternity::game::Definitions::load(database),std::move(starts)));
                auto commands=std::make_shared<eternity::game::CharacterCommands>(database_config,
                    eternity::game::CharacterCommandOptions{database_id,base,"PS_GameData"},rules,
                    std::make_shared<eternity::game::ItemIds>(game_options.realm));
                world=std::make_unique<eternity::net::WorldServer>(io,game_options,sessions,
                    [database_config,database_id,base](const eternity::session::LoginSession& account) {
                        eternity::storage::SqlServer connection(database_config);
                        return eternity::game::Characters(connection,database_id,base).load(account.user_id);
                    },[commands](const eternity::session::LoginSession& account,eternity::game::CharacterRoster& roster,const eternity::net::Packet& packet) {
                        return commands->execute(account.user_id,account.username,roster,packet);
                    });
                eternity::auth::Realm realm{game_options.realm,0,0,128,"Eternity"};realm.address=advertised.to_bytes();
                realms->replace({realm});
                std::cout<<"Game listening on "<<game_options.address<<":"<<world->port()
                         <<" (character listings/creation; world entry and gameplay incomplete)."<<std::endl;
            }
            eternity::net::LoginServer server(io,options,eternity::auth::SqlAuthenticator(database_config),sessions,realms);
            asio::signal_set signals(io,SIGINT,SIGTERM);
            signals.async_wait([&server,&world](const asio::error_code& error,int) {
                if(!error) {server.stop();if(world) world->stop();}
            });
            server.start();
            if(world) world->start();
            std::cout<<"Login listening on "<<options.address<<":"<<server.port()
                     <<(world ? " (legacy SQL path; character listings available)." : " (legacy SQL path; worlds offline).")<<std::endl;
            io.run();
            return 0;
        }
        if (std::string_view(argv[1]) == "--check-original-loaders") {
            const auto definitions = eternity::game::Definitions::load(database);
            for (const auto& [name, catalog] : definitions.catalogs)
                std::cout << name << ": " << catalog.rows.size() << " rows, " << catalog.columns.size() << " columns\n";
            std::cout << "Original definition procedures loaded successfully.\n";
            const char* configured=std::getenv("ETERNITY_MAP_INI");
            const std::filesystem::path map_ini=configured?configured:".local/server-data/Map.ini";
            std::ifstream npc_file(map_ini.parent_path()/"NpcQuest.SData",std::ios::binary);
            if(!npc_file) throw std::runtime_error("Cannot open original NpcQuest.SData");
            const eternity::net::Packet npc_data((std::istreambuf_iterator<char>(npc_file)),{});
            const auto npcs=eternity::game::NpcDefinitions::decode(npc_data);
            const auto quests=eternity::game::QuestDefinitions::npc_quest(npc_data);
            const auto item_links=eternity::game::ItemQuestLinks::npc_quest(npc_data);
            auto items=eternity::game::ItemDefinitions::load(definitions.catalogs.at("Items"));items.initialize_quest_flags(item_links);
            std::size_t linked_items=0;for(const auto& link:item_links.records()) if(!link.start.empty() || !link.end.empty()) ++linked_items;
            std::size_t npc_count=0;for(int type=1;type<=13;++type) npc_count+=npcs.count(type);
            std::cout << "Original NPC/quest definitions loaded: " << npc_count << " NPCs, " << quests.size() << " quests (object runtime not started).\n";
            std::cout << "Original item quest links loaded: " << linked_items << " linked slots; native quest-start flags initialized.\n";
            const auto maps=eternity::game::ServerMaps::load(eternity::game::MapConfiguration::load(map_ini),map_ini.parent_path()/"Map");
            std::cout << "Original map configuration and files loaded: " << maps.size() << " maps (runtime zones not initialized).\n";
            const auto mobs=eternity::game::MobRuntimeTemplates::load(definitions.catalogs.at("Mobs"));
            const auto linked=eternity::game::LinkedMobConfiguration::load(map_ini.parent_path()/"Obelisk.ini",maps.all(),mobs);
            std::size_t linked_groups=0;for(std::size_t id=0;id<maps.size();++id) linked_groups+=linked.for_map(static_cast<std::uint16_t>(id)).size();
            std::cout << "Original linked mob configuration loaded: " << linked_groups << " groups (object runtime not started).\n";
            std::size_t doors=0,regions=0;
            for(std::size_t id=0;id<maps.size();++id) {
                const auto environment=eternity::game::ZoneEnvironment::prepare(*maps.find(static_cast<std::uint16_t>(id)));
                doors+=environment.doors().size();regions+=environment.regions().size();
            }
            std::cout << "Native environment templates prepared: " << doors << " doors, " << regions << " regions (object runtime not started).\n";
            return 0;
        }
        // Read-only inspection of the recovered definition database.
        const auto results = database.query(
            "SELECT 'Mobs' AS Catalog, COUNT_BIG(*) AS Rows FROM dbo.Mobs "
            "UNION ALL SELECT 'Items', COUNT_BIG(*) FROM dbo.Items "
            "UNION ALL SELECT 'Skills', COUNT_BIG(*) FROM dbo.Skills;");
        if (results.size() != 1 || results.front().rows.size() != 3)
            throw std::runtime_error("Unexpected definition catalog result");
        for (const auto& row : results.front().rows) {
            if (row.size() != 2 || !row[0] || !row[1]) throw std::runtime_error("Incomplete catalog row");
            std::cout << *row[0] << ": " << *row[1] << '\n';
        }
        std::cout << "SQL Server definition catalog check passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Eternity: " << error.what() << '\n';
        return 1;
    }
}
