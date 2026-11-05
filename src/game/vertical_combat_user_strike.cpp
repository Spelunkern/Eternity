#include "eternity/game/vertical_combat_user_strike.hpp"
namespace eternity::game {
void VerticalCombatViewStrikeWorld::announce_multi(Actor& actor,Actor& target,const UserSkillDefinition& skill){vertical_combat_announce_user(actor,target,skill,view_);}
VerticalCombatUserStrikeServices::VerticalCombatUserStrikeServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& z,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h,VerticalCombatSkillSlotServices& s,UserSkillSpendGroupWorld& g,const VerticalCombatUserSkillRecordSource& u,const MobRuntimeTemplates& t,const MobTargetingBindingCatalog& m,UserSkillStrikeWorld& w):VerticalCombatSkillViewServices(r,d,z,i,v,h,s,g,u,t,m),strike_(w){}
void VerticalCombatUserStrikeServices::user_effect(UserSkillAction action,Actor& actor,Actor* target,const UserSkillDefinition& info){
 if(action==UserSkillAction::attack)user_skill_runtime_strike(actor,target,info,strike_);
 else remaining_user_effect(action,actor,target,info);
}
}
