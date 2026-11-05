#pragma once
#include "eternity/game/vertical_combat_self_damage.hpp"
namespace eternity::game {
// Must call the actual CMob::FindTarget 0049fd20 and ResetFSM 00492310 owners.
// Mob, applied skills, AI and target services remain retained and serialized.
class VerticalCombatMobStopWorld {
public:
 virtual ~VerticalCombatMobStopWorld()=default;
 virtual bool find_target(MobRuntime&)=0;
 virtual void reset_fsm(MobRuntime&)=0;
};
void vertical_combat_mob_stop(MobRuntime&,VerticalCombatMobStopWorld&);
class VerticalCombatMobStopServices:public VerticalCombatMobSelfDamageServices {
public:
 VerticalCombatMobStopServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&,UserSkillStrikeWorld&,const SkillDefinitions&,VerticalCombatMobStopWorld&);
 void stop_mob_attack(MobRuntime&) final;
private:VerticalCombatMobStopWorld& stop_;
};
}
