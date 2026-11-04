#pragma once
#include "eternity/game/user_skill_runtime_cost.hpp"
#include "eternity/game/user_skill_runtime_ready.hpp"
#include "eternity/game/user_skill_runtime_use.hpp"
namespace eternity::game {
class UserSkillEquipmentWorld {
public:
    virtual ~UserSkillEquipmentWorld()=default;
    // Side-effect-free borrow from actual Actor ownership, bag0 slot5/6.
    // Pin Actor/pool/definition; null only for absence, never an unbound mirror.
    virtual const GroundItem* equipped(Actor&,std::uint16_t slot)=0;
};
bool user_skill_runtime_check_equipment(UserSkillRuntime&,UserSkillHandle,
    const UserSkillDefinition&,bool mob,UserSkillEquipmentWorld&);
// Complete recovered eligibility coordination. Actual equipment borrow, ranges,
// clocks and use/effect/transports remain REQUIRED; no default provider/storage.
class UserSkillRuntimeEligibilityServices:public UserSkillUseServices,
    public UserSkillReadyWorld,public UserSkillEquipmentWorld {
public:
    bool eligible_user(UserSkillRuntime&,Actor*,UserSkillHandle,std::uint32_t) final;
    bool eligible_mob(UserSkillRuntime&,MobRuntime*,UserSkillHandle,std::uint32_t) final;
};
}
