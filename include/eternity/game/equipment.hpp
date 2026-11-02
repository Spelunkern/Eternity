#pragma once
#include "eternity/game/stats.hpp"
#include <filesystem>

namespace eternity::game {
struct ItemDefinition {
    std::uint32_t id = 0;
    std::uint8_t type = 0,type_id = 0,attribute = 0,special = 0,attack_time = 0,speed = 0,experience = 0,server = 0,real_type = 0;
    std::uint16_t range = 0,required_wisdom = 0;
    std::array<std::uint16_t,3> quality{};
    std::array<std::array<std::uint16_t,3>,4> effects{};
    Attributes attributes;
    PlayerResources resources{};
};
class ItemDefinitions {
public:
    static ItemDefinitions load(const storage::ResultSet& definitions);
    const ItemDefinition* find(std::uint8_t type,std::uint8_t id) const;
private:
    std::map<std::pair<std::uint8_t,std::uint8_t>,ItemDefinition> items_;
};
struct EnchantValues {
    std::array<std::array<std::uint16_t,2>,21> additions{};
    static EnchantValues read(const std::filesystem::path& file);
};
struct CraftValues {
    std::string name;
    std::array<std::uint16_t,11> additions{};
    std::uint8_t enchant_step = 0;
};
// CopyCraftExpassion on a freshly initialized CItem; reroll/removal is a separate operation.
CraftValues decode_craft(const ItemDefinition& item,std::string_view name,const EnchantValues& enchant);
struct EquipmentStatus {
    BaseStatus base{};
    AttackModifiers attack;
    std::int32_t variance = 0,range = 0,attack_speed = 0,move_speed = 2,damage_reduction = 0;
    std::uint32_t attack_attribute = 0,defense_attribute = 0;
    std::array<std::int32_t,3> standing_recovery{4,4,4},sitting_recovery{28,28,28}; // HP, SP, MP.
    std::array<std::int32_t,17> state_blocks{};
    std::array<std::uint8_t,13> quality{};
    std::array<std::uint8_t,20> weapon_speed{},weapon_damage{};
    std::array<std::uint8_t,8> attack_attribute_buffs{};
    std::uint8_t shield_value = 0;
    bool transformed = false,defense_attribute_skill = false;
};
// Applies the original stat/element changes at initialization, without broadcasting equipment changes.
void add_equipment(EquipmentStatus& status,std::uint8_t slot,const PlayerItem& item,const ItemDefinitions& definitions,const EnchantValues& enchant);
std::uint32_t item_attribute(const PlayerItem& item,const ItemDefinitions& definitions);
EquipmentStatus initialize_equipment(BaseStatus base,const std::vector<PlayerItem>& items,const ItemDefinitions& definitions,const EnchantValues& enchant);
}
