#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_selection_sql_loader.hpp"

namespace eternity::game {
// Builds each account's real SQL selection loader from its transferred roster.
// SQL connection setup and queries happen only on the supplied DB executor.
// The selection and resulting replica retain the loader and shared pools.
// ps_game.exe PacketCharacter 0046bf20; ps_dbAgent.exe LoadCharacter 00404b89.
inline RosterWorldHandoff::Select vertical_world_session_sql_select(VerticalWorldSession::Strand strand,
    asio::any_io_executor database,VerticalWorldSession::Registry& registry,WorldAdmission& world,
    VerticalWorldSessionServices& services,storage::SqlServerConfig config,std::uint8_t server,std::string catalog,
    std::shared_ptr<InventoryReplicaPool> inventory,std::shared_ptr<SkillReplicaPool> skills={},
    std::shared_ptr<QuestReplicaPool> quests={},std::shared_ptr<PointItemPool> points={}) {
    if(!inventory || catalog.empty())throw std::invalid_argument("Incomplete SQL selection owners");
    return vertical_world_session_select(std::move(strand),std::move(database),registry,world,services,
        [config=std::move(config),server,catalog=std::move(catalog),inventory=std::move(inventory),skills=std::move(skills),quests=std::move(quests),points=std::move(points)]
        (const session::LoginSession& account,const CharacterRoster& roster) {
            return VerticalSelectionSqlLoader::bind(std::make_shared<VerticalSelectionSqlLoader>(
                config,server,catalog,account.username,roster.max_grow,inventory,skills,quests,points));
        });
}
}
