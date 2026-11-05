#pragma once
#include "eternity/game/vertical_combat_user_strike.hpp"
namespace eternity::game {
// CUser::Damage 0044c120, kind 1 and source equal to the actor's live ID.
// Actor, actual skill definitions, scene, equipment and damage ledger survive
// synchronous callbacks. Death is a separate required continuation.
bool vertical_combat_self_damage(Actor&,std::int32_t amount,const SkillDefinitions&);
class VerticalCombatUserSelfDamageWorld:public VerticalCombatViewStrikeWorld {
public:
 VerticalCombatUserSelfDamageWorld(VerticalCombatView& view,const SkillDefinitions& skills):VerticalCombatViewStrikeWorld(view),skills_(skills){}
 bool self_damage(Actor&,std::int32_t) final;
private:const SkillDefinitions& skills_;
};
class VerticalCombatMobSelfDamageServices:public VerticalCombatUserStrikeServices {
public:
 VerticalCombatMobSelfDamageServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&,UserSkillStrikeWorld&,const SkillDefinitions&);
 bool self_damage(Actor&,std::int32_t) final;
private:const SkillDefinitions& skills_;
};
}
