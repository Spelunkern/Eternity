#pragma once
#include "eternity/game/skills.hpp"

namespace eternity::game {
struct EffectDefense {std::int32_t block_mode = 0,block_value = 0,mirror_skill = 0,mirror_level = 0;};
struct EffectStatus {
    SkillStatus skills;
    std::uint16_t level = 0;
    std::array<AttackValues,3> calculated_attack{};
    std::array<EffectDefense,3> defense{};
    // Offsets 133c, 1340, 1344, 1348 have conflicting PDB member names; behavior follows assembly.
    std::int32_t movement_block = 0,action_block_a = 0,action_block_b = 0,fixed_form = 0,casting = 0,movement_restricted = 0;
    std::int32_t giveup_sp_mp = 0,block_all = 0;
    std::uint32_t chase_id = 0,shape_template = 0,current_target = 0;
    std::uint16_t form_mob = 0;
    std::uint8_t shape = 0;
    bool no_mob_targeting = false,zero_attack_attribute_buff = false,in_zone = false;
    bool block_death = false,block_dying = false; // CUser+1364/+1365.
    std::int32_t current_target_type = 0;
};
// World operations are required dependencies, not optional callbacks. The world runtime
// must implement them before effects can be connected to live character selection.
class EffectWorld {
public:
    virtual ~EffectWorld() = default;
    virtual void maximum_changed(EffectStatus&,std::uint8_t resource) = 0; // HP=0, MP=1, SP=2.
    virtual void speed_changed(EffectStatus&) = 0;
    virtual void shape_changed(EffectStatus&) = 0;
    virtual void run_mode_changed(EffectStatus&,bool running) = 0;
    virtual void stop_attack(EffectStatus&) = 0;
    virtual void stop_target_mob(EffectStatus&,std::uint32_t id) = 0;
    virtual void taunt(EffectStatus&,std::uint16_t aggro,float range) = 0;
    virtual void detect_hidden(EffectStatus&,const SkillDefinition&) = 0;
    virtual void rebuild_attack(EffectStatus&,bool equipment) = 0;
    virtual void user_shape_changed(EffectStatus&) = 0;
    virtual void replace_attack_elements(EffectStatus&,std::uint8_t attribute) = 0;
};
struct EffectSource {
    std::uint32_t instance_id = 0,attacker_id = 0,previous_tick = 0;
    std::uint8_t attacker_kind = 1;
};
struct EffectEquipment {
    const PlayerItem* weapon = nullptr;
    const PlayerItem* armor = nullptr;
    bool defense_element_remains = false;
};
class EffectRemovalWorld:public EffectWorld {
public:
    virtual void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>& amounts) = 0;
    // Combat owns periodic damage, mirror/reflection, death and its notifications.
    virtual void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed_ms) = 0;
};
void apply_active_effect(EffectStatus& status,const SkillDefinition& skill,std::uint32_t attacker_id,EffectWorld& world);
void recalculate_attack(EffectStatus& status);
void apply_effect_ability(EffectStatus& status,std::uint8_t effect,std::uint8_t type,std::int32_t value,EffectWorld& world);
std::array<std::int32_t,3> periodic_heal(std::uint32_t elapsed_ms,const std::array<std::uint16_t,3>& rates);
void remove_active_effect(EffectStatus& status,const SkillDefinition& skill,const EffectSource& source,std::uint32_t tick_ms,
    EffectEquipment equipment,const ItemDefinitions& definitions,EffectRemovalWorld& world);
std::uint8_t attack_speed(const EffectStatus& status);
std::uint8_t move_speed(const EffectStatus& status);
}
