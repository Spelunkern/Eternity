#include "eternity/game/vertical_combat_hit.hpp"
#include "eternity/game/mob_combat_stop.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
class PartyServices:public MobCombatServices {
public:
 explicit PartyServices(MobRuntime& mob):mob_(mob){}
 std::int32_t status(const MobCombatState&)override{return mob_.state_stack?(mob_.state_stack->empty()?0:mob_.state_stack->back()):mob_.status;}
 void dispatch_death_event(MobCombatState&)override{throw std::logic_error("Unexpected party death event");}
 std::uint32_t tick()override{throw std::logic_error("Unexpected party clock");}
 void log_damage(MobCombatState&,std::uint32_t)override{throw std::logic_error("Unexpected party log");}
 void retarget(MobCombatState&,bool)override{throw std::logic_error("Unexpected party retarget");}
private:MobRuntime& mob_;
};
}
bool vertical_combat_hit(Actor& actor,MobRuntime& mob,const UserSkillDefinition* skill,std::array<std::int32_t,3>& party_damage,std::array<std::int32_t,3>& damage,VerticalCombatHitServices& services){
 if(!mob.damage)throw std::logic_error("Combat hit requires retained mob damage ledger");
 auto party=services.party_id(actor);
 std::uint32_t id=0;
 if(party){
  MobCombatState state;state.hp=mob.hp;PartyServices party_services(mob);
  mob_combat_party(state,*mob.damage,party_services,*party,party_damage[0]);
  party=services.party_id(actor);if(!party)throw std::logic_error("Combat hit lost party during credit");id=*party;
 }
 const auto bonus=services.aggro_bonus(actor);
 const MobCombatCredit credit{1,actor.id(),id,bonus,skill?skill->effect.aggro:0};
 const bool killed=services.damage(actor,mob,damage,credit);
 if(killed){mob_combat_stop(actor);services.death(mob,1,actor.id());}
 return killed;
}
VerticalCombatHitNormalWorld::VerticalCombatHitNormalWorld(RandomSource& r,ZoneUsers& u,VerticalCombatRangedPassive& p,std::span<const float,81> e,MobTargetingZone& t,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h):VerticalCombatEquippedNormalWorld(r,u,p,e,t,i,v),hits_(h){}
bool VerticalCombatHitNormalWorld::apply_hit(Actor& a,MobRuntime& m,std::array<std::int32_t,3>& d){return vertical_combat_hit(a,m,nullptr,d,d,hits_);}
VerticalCombatHitSkillServices::VerticalCombatHitSkillServices(RandomSource& r,VerticalCombatSkillRuntimeDamage& d,MobTargetingZone& t,const ItemDefinitions& i,VerticalCombatView& v,VerticalCombatHitServices& h):VerticalCombatViewSkillServices(r,d,t,i,v),hits_(h){}
bool VerticalCombatHitSkillServices::apply_primary(Actor& a,MobRuntime& m,const UserSkillDefinition& s,std::array<std::int32_t,3>& d){return vertical_combat_hit(a,m,&s,d,d,hits_);}
bool VerticalCombatHitSkillServices::apply_secondary(Actor& a,MobRuntime& m,const UserSkillDefinition& s,std::array<std::int32_t,3>& primary,std::array<std::int32_t,3>& secondary){return vertical_combat_hit(a,m,&s,primary,secondary,hits_);}
}
