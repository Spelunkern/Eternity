#pragma once
#include "eternity/game/vertical_combat_user_skill_records.hpp"
#include "eternity/game/vertical_combat_range.hpp"
namespace eternity::game {
// CUser::SendUseSkill 0044e900, mob multi-hit announcement, source kind 4.
void vertical_combat_announce_mob(Actor&,MobRuntime&,const UserSkillDefinition&,VerticalCombatView&);
void vertical_combat_announce_user(Actor&,Actor&,const UserSkillDefinition&,VerticalCombatView&);
class VerticalCombatSkillViewServices:public VerticalCombatUserSkillRecordsServices {
public:
 VerticalCombatSkillViewServices(RandomSource&,VerticalCombatSkillRuntimeDamage&,MobTargetingZone&,const ItemDefinitions&,VerticalCombatView&,VerticalCombatHitServices&,VerticalCombatSkillSlotServices&,UserSkillSpendGroupWorld&,const VerticalCombatUserSkillRecordSource&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&);
 void announce_multi(Actor&,MobRuntime&,const UserSkillDefinition&) final;
private:VerticalCombatView& view_;
};
// Ready mob cast 0044f3bc: original 13-byte 0516 packet, radius 60.
// User-target cast delivery remains a separate required actual provider.
class VerticalCombatMobCastEligibilityServices:public VerticalCombatRangeEligibilityServices {
public:
 VerticalCombatMobCastEligibilityServices(UserSkillRuntime&,VerticalCombatViewSkillServices&,VerticalCombatRange&,VerticalCombatView&);
 void cast_mob(Actor&,MobRuntime&,const UserSkillDefinition&,const net::Packet&) final;
private:VerticalCombatView& view_;
};
class VerticalCombatCastEligibilityServices:public VerticalCombatMobCastEligibilityServices {
public:
 VerticalCombatCastEligibilityServices(UserSkillRuntime&,VerticalCombatViewSkillServices&,VerticalCombatRange&,VerticalCombatView&);
 void cast_user(Actor&,Actor&,const UserSkillDefinition&,const net::Packet&) final;
private:VerticalCombatView& user_view_;
};
}
