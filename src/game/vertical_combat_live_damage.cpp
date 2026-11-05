#include "eternity/game/vertical_combat_live_damage.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
std::int32_t status(const MobRuntime& mob){return mob.state_stack?(mob.state_stack->empty()?0:mob.state_stack->back()):mob.status;}
std::int32_t signed_bits(std::uint32_t n){return std::bit_cast<std::int32_t>(n);}
}
bool VerticalCombatLiveDamageServices::damage(Actor&,MobRuntime& mob,std::array<std::int32_t,3>& value,const MobCombatCredit& credit){
 if(!mob.damage)throw std::logic_error("Mob damage requires retained ledger");
 if(status(mob)==2||mob.hp==0)return false;
 if(mob.check_attack_count)mob.attacked_count=signed_bits(static_cast<std::uint32_t>(mob.attacked_count)+1u);
 const auto hp_damage=static_cast<std::uint32_t>(value[0]);
 if(mob.hp>hp_damage)mob.hp-=hp_damage;
 else {
  mob.hp=0;
  if(status(mob)!=2){mob.status=2;if(mob.state_stack)dispatch_death_event(mob);}
  mob.target_kind=0;
  const auto delay=long_corpse_delay(mob)?10000u:5000u;
  mob.death_time=tick_ms()+delay;
 }
 if(damage_logging_enabled(mob))log_damage(mob,static_cast<std::uint32_t>(value[0]));
 mob.mp=static_cast<std::int32_t>(mob.mp)<value[1]?0:static_cast<std::uint16_t>(mob.mp-static_cast<std::uint32_t>(value[0]));
 mob.sp=static_cast<std::int32_t>(mob.sp)<value[2]?0:static_cast<std::uint16_t>(mob.sp-static_cast<std::uint32_t>(value[2]));
 const auto aggro=signed_bits(static_cast<std::uint32_t>(credit.base_aggro)+static_cast<std::uint32_t>(credit.bonus)*10u);
 mob.damage->add(credit.type,credit.id,value[0],aggro,credit.party);
 if(mob.hp==0)return true;
 if(status(mob)==1)retarget(mob,mob.state_stack.has_value());
 return false;
}
}
