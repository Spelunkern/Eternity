#include "eternity/game/vertical_combat_retarget.hpp"
#include "eternity/game/vertical_movement.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
VerticalCombatRetargetServices::VerticalCombatRetargetServices(Actor& a,PartyMembershipUser& m,const MobDeathExperience& r,MobDeathWorld& w,MobAiStateServices& f,VerticalCombatClock& c,ZoneMobs& mobs,ZoneUsers& users):VerticalCombatMobStateServices(a,m,r,w,f,c),mobs_(mobs),users_(users){}
void VerticalCombatRetargetServices::retarget(MobRuntime& mob,bool stacked){
 if(stacked)mob.lua_attacked_count+=1u;
 const auto now=tick_ms();
 if(mob.next_target>now)return;
 if(stacked){
  if(mob.vision_fixed_user==0){const auto id=maximum_aggro_ex(mob);if(id){mob.target_kind=1;mob.target_id=id;}}
  mob.next_target=now+mob.retarget_time;return;
 }
 if(mob.vision_fixed_user==0){
  if(!mob.damage)throw std::logic_error("Retarget requires retained mob damage ledger");
  MobDamageRecord best;
  if(mob.damage->maximum_aggro(best)&&best.type==1&&best.id!=mob.target_id){
   if(mob.zone!=&mobs_)throw std::logic_error("Retarget requires the mob's retained paired zone");
   if(auto* user=users_.find_actor(best.id)){
    const auto record=mob_definition_record(mob);if(record.size()<0x7c)throw std::logic_error("Retarget requires mob definition country");
    std::uint32_t country=0;for(unsigned i=0;i<4;++i)country|=std::uint32_t(std::to_integer<unsigned char>(record[0x78+i]))<<(8*i);
    const auto& appearance=user->appearance();
    if(country!=appearance.identity.country && user->attack_state().mode!=1 && user->status().no_mob_targeting==0 && appearance.attackable && appearance.visible){
     const auto position=users_.position(user->id());if(!position)throw std::logic_error("Retarget user lacks live position");
     if(vertical_movement_planar_distance(mob.position,*position)<=45.0f){
      if(mob_status(mob)!=2){mob.status=1;mob.next_move=0;}
      mob.target_kind=1;mob.target_id=user->id();
     }
    }
   }
  }
 }
 mob.next_target=now+5000u;
}
std::uint32_t VerticalCombatAggroExServices::maximum_aggro_ex(MobRuntime& mob){
 if(!mob.damage)throw std::logic_error("Stacked retarget requires retained mob damage ledger");
 const MobDamageRecord* best=nullptr;std::int32_t best_aggro=0;
 for(const auto* record:mob.damage->records()){
  if(record->type!=1)continue;
  if(mob.zone!=&mobs_)throw std::logic_error("Stacked retarget requires the mob's retained paired zone");
  auto* user=users_.find_actor(record->id);if(!user)continue;
  const auto definition=mob_definition_record(mob);if(definition.size()<0x7c)throw std::logic_error("Stacked retarget requires mob definition country");
  std::uint32_t country=0;for(unsigned i=0;i<4;++i)country|=std::uint32_t(std::to_integer<unsigned char>(definition[0x78+i]))<<(8*i);
  const auto& appearance=user->appearance();
  if(country==appearance.identity.country || user->attack_state().mode==1 || user->status().no_mob_targeting!=0 || !appearance.attackable || !appearance.visible)continue;
  const auto position=users_.position(user->id());if(!position)throw std::logic_error("Stacked retarget user lacks live position");
  if(vertical_movement_planar_distance(mob.position,*position)<=45.0f && record->aggro>best_aggro){best=record;best_aggro=record->aggro;}
 }
 return best?best->id:0;
}
}
