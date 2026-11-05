#pragma once
#include "eternity/game/vertical_combat_definition.hpp"
#include "eternity/game/mob_log.hpp"
namespace eternity::game {
// Same retained clock used by MobTargetingActorWorld and the mob FSM. Each
// call samples the actual owner; the owner outlives all combat callbacks.
class VerticalCombatFsmClock final:public VerticalCombatClock {
public:
 explicit VerticalCombatFsmClock(MobAiStateServices& fsm):fsm_(fsm){}
 std::uint32_t tick_ms() final {return fsm_.tick();}
private:MobAiStateServices& fsm_;
};
// Pure read of the actual mob zone map ID and its live definition name. The
// returned name remains retained through send_game_log; no lookup may mutate
// the mob, its definition, or the serialized world during this call.
class VerticalCombatMobLogContextServices {
public:
 virtual ~VerticalCombatMobLogContextServices()=default;
 virtual MobLogContext context(const MobRuntime&)=0;
};
// CMob::Damage 00490ee0 -> CMob::SendLogBothMob 004a1810, event 2.
// The transport, context, shared clock and all mob/world owners remain live
// across the synchronous call, including reentry. No transport is fabricated.
class VerticalCombatLogServices:public VerticalCombatDefinitionServices {
public:
 VerticalCombatLogServices(Actor& actor,PartyMembershipUser& member,const MobDeathExperience& rules,MobDeathWorld& world,MobAiStateServices& fsm,VerticalCombatClock& clock,ZoneMobs& mobs,ZoneUsers& users,const MobRuntimeTemplates& templates,const MobTargetingBindingCatalog& catalog,VerticalCombatMobLogContextServices& context,MobLogTransport& transport)
  :VerticalCombatDefinitionServices(actor,member,rules,world,fsm,clock,mobs,users,templates,catalog),context_(context),transport_(transport){}
 void log_damage(MobRuntime& mob,std::uint32_t damage) final {
  if(!mob.zone)return;
  const auto context=context_.context(mob);
  const std::string_view user(reinterpret_cast<const char*>(mob.previous_attack_user.data()),mob.previous_attack_user.size());
  const std::string_view skill(reinterpret_cast<const char*>(mob.previous_attack_skill.data()),mob.previous_attack_skill.size());
  (void)send_mob_log(mob,context,2,user,skill,damage,transport_);
 }
private:VerticalCombatMobLogContextServices& context_;MobLogTransport& transport_;
};
}
