#pragma once
#include "eternity/game/vertical_combat_sleep.hpp"
namespace eternity::game {
// CUser::QualityByAttack 0045b260 -> QualityDec 0045b350 -> SetAttack
// 00454280. The catalog is the Actor's actual retained item definition owner;
// the equipped pooled item and Actor must survive this synchronous operation.
void vertical_combat_wear_weapon(Actor&,const ItemDefinitions&);
class VerticalCombatWearNormalWorld:public VerticalCombatSleepNormalWorld {
public:
 VerticalCombatWearNormalWorld(RandomSource& random,ZoneUsers& users,VerticalCombatRangedPassive& passive,std::span<const float,81> elements,MobTargetingZone& targeting,const ItemDefinitions& items)
  :VerticalCombatSleepNormalWorld(random,users,passive,elements,targeting),items_(items){}
 void wear_weapon(Actor&) final;
private:const ItemDefinitions& items_;
};
class VerticalCombatWearSkillServices:public VerticalCombatSleepSkillServices {
public:
 VerticalCombatWearSkillServices(RandomSource& random,VerticalCombatSkillRuntimeDamage& damage,MobTargetingZone& targeting,const ItemDefinitions& items)
  :VerticalCombatSleepSkillServices(random,damage,targeting),items_(items){}
 void wear_weapon(Actor&) final;
private:const ItemDefinitions& items_;
};
}
