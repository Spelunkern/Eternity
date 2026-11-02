#pragma once
#include "eternity/game/skills.hpp"

namespace eternity::game {
struct EffectDefense {std::int32_t block_mode = 0,block_value = 0,mirror_skill = 0,mirror_level = 0;};
struct EffectStatus {
    SkillStatus skills;
    std::array<EffectDefense,3> defense{};
    // Offsets 133c, 1340, 1344, 1348 have conflicting PDB member names; behavior follows assembly.
    std::int32_t movement_block = 0,action_block_a = 0,action_block_b = 0,fixed_form = 0,casting = 0,movement_restricted = 0;
    std::int32_t giveup_sp_mp = 0,block_all = 0;
    std::uint32_t chase_id = 0,shape_template = 0,current_target = 0;
    std::uint16_t form_mob = 0;
    std::uint8_t shape = 0;
    bool no_mob_targeting = false,zero_attack_attribute_buff = false,in_zone = false;
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
void apply_active_effect(EffectStatus& status,const SkillDefinition& skill,std::uint32_t attacker_id,EffectWorld& world);
std::uint8_t attack_speed(const EffectStatus& status);
std::uint8_t move_speed(const EffectStatus& status);
}
