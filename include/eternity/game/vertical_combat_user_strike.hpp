#pragma once
#include "eternity/game/vertical_combat_skill_view.hpp"
#include "eternity/game/user_skill_runtime_strike.hpp"
namespace eternity::game {
// Real user strike callbacks remain mandatory; only the view announcement is
// supplied. Actor, target, view and their owners survive synchronous callbacks.
class VerticalCombatViewStrikeWorld:public UserSkillStrikeWorld {
public:
 explicit VerticalCombatViewStrikeWorld(VerticalCombatView& view):view_(view){}
 void announce_multi(Actor&,Actor&,const UserSkillDefinition&) final;
private:VerticalCombatView& view_;
};
// AttackUser 00451c80 effect 1 routes through SkillAttack 00451d90.
// Other effect kinds require the actual user effect owners.
class VerticalCombatUserStrikeServices:public VerticalCombatSkillViewServices {
public:
 VerticalCombatUserStrikeServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&,UserSkillStrikeWorld&);
 void user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&) final;
 virtual void remaining_user_effect(UserSkillAction,Actor&,Actor*,const UserSkillDefinition&)=0;
private:UserSkillStrikeWorld& strike_;
};
}
