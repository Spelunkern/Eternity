#pragma once
#include "eternity/game/actor_load.hpp"
#include "eternity/game/definitions.hpp"
#include "eternity/game/guild_npc_config.hpp"
#include "eternity/game/item_quests.hpp"
#include "eternity/game/linked_mob_config.hpp"
#include "eternity/game/npcs.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// Retains the original definitions for the complete lifetime of sessions,
// zones and linked-mob references. No runtime objects are started here.
class WorldAssets final {
public:
    static std::shared_ptr<WorldAssets> load(storage::SqlServer& database,const std::filesystem::path& map_ini) {
        return std::shared_ptr<WorldAssets>(new WorldAssets(database,map_ini));
    }
    WorldAssets(const WorldAssets&)=delete;
    WorldAssets& operator=(const WorldAssets&)=delete;
    static std::shared_ptr<ServerMaps> retain_maps(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->maps};
    }
    static std::shared_ptr<const ItemDefinitions> retain_items(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->items};
    }
    static std::shared_ptr<const QuestDefinitions> retain_quests(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->quests};
    }
    static std::shared_ptr<const ItemQuestLinks> retain_item_quests(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->item_quests};
    }
    static std::shared_ptr<const BlessingDefinitions> retain_blessings(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->blessings};
    }
    static std::shared_ptr<const GuildHouseConfigurationLoad> retain_guild_house(const std::shared_ptr<WorldAssets>& owner) {
        if(!owner)throw std::invalid_argument("Missing world assets owner");
        return {owner,&owner->guild_house};
    }
    Definitions definitions;
    net::Packet npc_quest_data;
    NpcDefinitions npcs;
    QuestDefinitions quests;
    ItemQuestLinks item_quests;
    ItemDefinitions items;
    StatusRules statuses;
    EnchantValues enchant;
    SkillDefinitions skills;
    MobAttackTemplates mob_attacks;
    Experience experience;
    BlessingDefinitions blessings;
    GuildHouseConfigurationLoad guild_house;
    ServerMaps maps;
    MobRuntimeTemplates mob_runtime;
    LinkedMobConfiguration linked_mobs;
    ActorLoadRules actor_rules;
private:
    static net::Packet read_asset(const std::filesystem::path& file) {
        std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open original asset: "+file.string());
        return net::Packet(std::istreambuf_iterator<char>(input),{});
    }
    WorldAssets(storage::SqlServer& database,const std::filesystem::path& map_ini)
        :definitions(Definitions::load(database)),npc_quest_data(read_asset(map_ini.parent_path()/"NpcQuest.SData")),
         npcs(NpcDefinitions::decode(npc_quest_data)),quests(QuestDefinitions::npc_quest(npc_quest_data)),
         item_quests(ItemQuestLinks::npc_quest(npc_quest_data)),items(ItemDefinitions::load(definitions.catalogs.at("Items"))),
         statuses(StatusRules::load(definitions.catalogs.at("StatusDefs"))),enchant(EnchantValues::read(map_ini.parent_path()/"ItemEnchant.ini")),
         skills(SkillDefinitions::load(definitions.catalogs.at("Skills"))),mob_attacks(MobAttackTemplates::load(definitions.catalogs.at("Mobs"))),
         experience(Experience::load(definitions.catalogs.at("ExpDefs"))),
         blessings(BlessingDefinitions::decode(read_asset(map_ini.parent_path()/"KillStatus.SData"))),
         guild_house(guild_house_load_profile(map_ini.parent_path()/"GuildHouse.ini")),
         maps(ServerMaps::load(MapConfiguration::load(map_ini),map_ini.parent_path()/"Map")),
         mob_runtime(MobRuntimeTemplates::load(definitions.catalogs.at("Mobs"))),
         linked_mobs(LinkedMobConfiguration::load(map_ini.parent_path()/"Obelisk.ini",maps.all(),mob_runtime)),
         actor_rules{statuses,items,enchant,skills,quests,mob_attacks} {items.initialize_quest_flags(item_quests);}
};
}
