#pragma once
#include "eternity/game/vertical_async_selection.hpp"

namespace eternity::game {
// Each call uses its own ODBC connection and must run on a database worker.
// Shared pools serialize their own allocations.
class VerticalSelectionSqlLoader final {
public:
    VerticalSelectionSqlLoader(storage::SqlServerConfig config,std::uint8_t server,std::string catalog,
        std::string username,std::uint8_t max_grow,std::shared_ptr<InventoryReplicaPool> inventory,
        std::shared_ptr<SkillReplicaPool> skills={},std::shared_ptr<QuestReplicaPool> quests={},std::shared_ptr<PointItemPool> points={})
        :config_(std::move(config)),server_(server),catalog_(std::move(catalog)),username_(std::move(username)),max_grow_(max_grow),
         inventory_(std::move(inventory)),skills_(std::move(skills)),quests_(std::move(quests)),points_(std::move(points)) {
        if(!inventory_)throw std::invalid_argument("Selection requires retained inventory pool");
    }
    // The resulting replica itself retains this loader and its pools.
    static VerticalAsyncSelection::Load bind(std::shared_ptr<VerticalSelectionSqlLoader> owner) {
        if(!owner)throw std::invalid_argument("Missing selection SQL loader");
        return [owner=std::move(owner)](std::uint32_t user,std::uint32_t character){
            auto result=owner->load(user,character);if(result)result->replica->retain_resources(owner);return result;
        };
    }
private:
    std::optional<VerticalPersistenceSelection> load(std::uint32_t user,std::uint32_t character) {
        storage::SqlServer connection(config_);Players players(connection,server_,catalog_);PointItemStore products(connection,server_,catalog_);
        return vertical_persistence_select(players,products,user,character,username_,max_grow_,*inventory_,skills_.get(),quests_.get(),points_.get());
    }
    storage::SqlServerConfig config_;std::uint8_t server_;std::string catalog_,username_;std::uint8_t max_grow_;
    std::shared_ptr<InventoryReplicaPool> inventory_;std::shared_ptr<SkillReplicaPool> skills_;
    std::shared_ptr<QuestReplicaPool> quests_;std::shared_ptr<PointItemPool> points_;
};
}
