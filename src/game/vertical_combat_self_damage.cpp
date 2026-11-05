#include "eternity/game/vertical_combat_self_damage.hpp"
namespace eternity::game {
bool vertical_combat_self_damage(Actor& actor,std::int32_t amount,const SkillDefinitions& skills){return actor.damage(1,actor.id(),amount,skills);}
bool VerticalCombatUserSelfDamageWorld::self_damage(Actor& actor,std::int32_t amount){return vertical_combat_self_damage(actor,amount,skills_);}
VerticalCombatMobSelfDamageServices::VerticalCombatMobSelfDamageServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const VerticalCombatUserSkillRecordSource& u,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& m,UserSkillStrikeWorld& w,const SkillDefinitions& skills):VerticalCombatUserStrikeServices(r,d,z,i,v,h,s,g,u,t,m,w),skills_(skills){}
bool VerticalCombatMobSelfDamageServices::self_damage(Actor& actor,std::int32_t amount){return vertical_combat_self_damage(actor,amount,skills_);}
}
