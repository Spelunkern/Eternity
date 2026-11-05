#include "eternity/game/world_assets.hpp"
#include <iostream>
#include <stdexcept>
using namespace eternity;
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original Map.ini");
        storage::SqlServer database(storage::SqlServerConfig::from_environment());
        auto assets=game::WorldAssets::load(database,argv[1]);
        require(assets->maps.size()!=0 && assets->quests.size()!=0 && !assets->definitions.catalogs.empty(),"Original assets were not loaded");
        require(&assets->actor_rules.items==&assets->items && &assets->actor_rules.quests==&assets->quests &&
            &assets->actor_rules.skills==&assets->skills && &assets->actor_rules.statuses==&assets->statuses &&
            &assets->actor_rules.enchant==&assets->enchant && &assets->actor_rules.mobs==&assets->mob_attacks,
            "Actor rules do not reference retained original definitions");
        auto maps=game::WorldAssets::retain_maps(assets);auto quests=game::WorldAssets::retain_quests(assets);
        auto items=game::WorldAssets::retain_items(assets);auto links=game::WorldAssets::retain_item_quests(assets);
        auto blessings=game::WorldAssets::retain_blessings(assets);
        auto guild_house=game::WorldAssets::retain_guild_house(assets);
        const auto expected_house=game::guild_house_load_profile(std::filesystem::path(argv[1]).parent_path()/"GuildHouse.ini");
        require(guild_house->config.purchase_price==expected_house.config.purchase_price &&
            guild_house->config.npc.default_keep==expected_house.config.npc.default_keep &&
            guild_house->price_diagnostic==expected_house.price_diagnostic,"Guild house configuration differs from original loader");
        const auto map_count=maps->size(),quest_count=quests->size(),blessing_count=blessings->size(0);std::weak_ptr<game::WorldAssets> weak=assets;
        require(blessing_count>0 && blessings->size(1)>0,"Original blessing definitions were not loaded");
        assets.reset();require(!weak.expired() && maps->size()==map_count && quests->size()==quest_count && blessings->size(0)==blessing_count && !links->records().empty(),"Runtime aliases failed to retain original definitions");
        maps.reset();quests.reset();items.reset();links.reset();require(!weak.expired(),"Blessings did not retain original definitions");
        blessings.reset();require(!weak.expired(),"Guild configuration did not retain original definitions");
        guild_house.reset();require(weak.expired(),"Runtime aliases leaked original definitions");
        std::cout<<"Original world definitions and retained runtime aliases passed.\n";return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
