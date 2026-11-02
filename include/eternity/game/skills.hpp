#pragma once
#include "eternity/game/equipment.hpp"

namespace eternity::game {
struct SkillAbility {std::uint8_t type = 0;std::uint16_t value = 0;};
struct SkillDefinition {
    std::uint16_t id = 0,detail = 0,aggro = 0,change_type = 0,change_level = 0;
    std::uint8_t level = 0,effect = 0,weapon1 = 0,weapon2 = 0,weapon_value = 0,bag = 0;
    std::uint8_t attribute = 0,defense_type = 0,defense_value = 0,apply_range = 0;
    std::array<SkillAbility,3> abilities{};
};
class SkillDefinitions {
public:
    static SkillDefinitions load(const storage::ResultSet& definitions);
    const SkillDefinition* find(std::uint16_t id,std::uint8_t level) const;
private:
    std::map<std::pair<std::uint16_t,std::uint8_t>,SkillDefinition> skills_;
};
struct SkillBenefits {
    std::int32_t amulet = 0,mob_gold = 0,no_durability_loss = 0,no_exp_loss = 0,no_item_loss = 0,
        no_equipped_item_loss = 0,bank_enabled = 0,bank_double = 0,experience_double = 0,experience_half_bonus = 0,rebirth = 0;
};
struct SkillStatus {
    EquipmentStatus equipment;
    std::array<std::int32_t,3> sitting_recovery_percent{},attack_disabled{};
    std::int32_t mp_cost_reduction = 0,sp_cost_reduction = 0,cool_time = 0,max_move_speed = 0,max_bags = 2,cross_road = 0;
    bool hp_ability_applied = false;
    std::uint16_t hp_ability_id = 0;
    std::uint8_t hp_ability_level = 0;
    std::uint32_t hp_ability_previous = 0;
    SkillBenefits benefits;
};
// SetSkillAbility's stat/benefit changes; network broadcasts are a separate responsibility.
void add_skill_ability(SkillStatus& status,std::uint8_t effect,std::uint8_t type,std::int32_t value);
void add_passive(SkillStatus& status,const SkillDefinition& skill,std::optional<std::uint8_t> weapon_type,std::optional<std::uint8_t> shield_type);
void initialize_passives(SkillStatus& status,const std::vector<PlayerSkill>& skills,const std::vector<PlayerItem>& items,
    const SkillDefinitions& definitions,const ItemDefinitions& item_definitions);
void apply_billing(SkillStatus& status,std::int32_t billing_type);
}
