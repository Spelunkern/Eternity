#include "eternity/game/vertical_combat_mob_stop.hpp"
#include "eternity/game/mob_applied_skills_catalog.hpp"
#include "eternity/game/zone_mobs.hpp"
namespace eternity::game {
void vertical_combat_mob_stop(MobRuntime& mob,VerticalCombatMobStopWorld& world){
 if(mob_status(mob)==2)return;
 if(mob.vision_fixed_user!=0)mob.applied_skills().owner().remove_vision_fix(mob);
 if(mob.use_pais){if(!world.find_target(mob))world.reset_fsm(mob);return;}
 if(mob_status(mob)!=2)mob.status=0;
 mob.target_kind=0;mob.target_id=0;
}
VerticalCombatMobStopServices::VerticalCombatMobStopServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const VerticalCombatUserSkillRecordSource& u,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& m,UserSkillStrikeWorld& w,const SkillDefinitions& skills,VerticalCombatMobStopWorld& stop):VerticalCombatMobSelfDamageServices(r,d,z,i,v,h,s,g,u,t,m,w,skills),stop_(stop){}
void VerticalCombatMobStopServices::stop_mob_attack(MobRuntime& mob){vertical_combat_mob_stop(mob,stop_);}
}
