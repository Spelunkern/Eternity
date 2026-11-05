#pragma once
#include "eternity/game/vertical_combat_skill_select.hpp"
#include "eternity/game/vertical_combat_success.hpp"
namespace eternity::game {
// Actual owners called by CUser::AttackMob(CMob*,stSkillInfo*,bool,bool)
// 0044fd90 before its AttackDamage call. Retain every argument through callbacks.
class VerticalCombatSkillHitPrefixWorld {
public:
 virtual ~VerticalCombatSkillHitPrefixWorld()=default;
 virtual bool skill_success(Actor&,MobRuntime&,const UserSkillDefinition&)=0;
 virtual void remove_sleep(MobRuntime&)=0; // CMob::RemApplySkillSleep 004a16c0
 // Actual CZone::PSendView: zone-presence check, radius 60, source sent immediately, priority 5.
 virtual void send_skill_view(Actor&,const net::Packet&)=0;
 virtual void set_target(MobRuntime&,Actor&)=0; // CMob::SetTarget 0049fad0, help=true
 // Resume at AttackDamage 0044ff47 (first) or 00450337 (otherwise).
 // This callback must execute the remaining damage, effects, packets and death.
 virtual void continue_hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second)=0;
};
void vertical_combat_skill_hit_prefix(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,VerticalCombatSkillHitPrefixWorld&);
// Concrete success calculation; the provider supplies bytes from the actual
// retained mob and skill definitions. RNG is the shared serialized owner.
class VerticalCombatSkillHitSuccessWorld:public VerticalCombatSkillHitPrefixWorld {
public:
 explicit VerticalCombatSkillHitSuccessWorld(RandomSource& r):random_(r){}
 bool skill_success(Actor&,MobRuntime&,const UserSkillDefinition&) final;
 virtual VerticalCombatSkillSuccessInputs success_inputs(MobRuntime&,const UserSkillDefinition&)=0;
private:RandomSource& random_;
};
// The selected skill coordinator delegates hits here, preserving its captured
// first/second flags. Other actions and all continuation services stay required.
class VerticalCombatSkillHitSelectServices:public VerticalCombatSkillSelectServices,public VerticalCombatSkillHitPrefixWorld {
public:
 void hit(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second) final;
 // Effect 3 / target 3 invokes the same 0044fd90 hit directly with both flags.
 void other_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&) final;
 virtual void remaining_mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)=0;
};
class VerticalCombatSkillHitSelectSuccessServices:public VerticalCombatSkillHitSelectServices {
public:
 explicit VerticalCombatSkillHitSelectSuccessServices(RandomSource& r):random_(r){}
 bool skill_success(Actor&,MobRuntime&,const UserSkillDefinition&) final;
 virtual VerticalCombatSkillSuccessInputs success_inputs(MobRuntime&,const UserSkillDefinition&)=0;
private:RandomSource& random_;
};
}
