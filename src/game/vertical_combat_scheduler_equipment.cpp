#include "eternity/game/vertical_combat_scheduler_equipment.hpp"
#include "eternity/game/vertical_combat_eligibility.hpp"
namespace eternity::game {
namespace {
class EquipmentWorld final:public MobCombatSchedulerWorld {
public:
 EquipmentWorld(MobCombatSchedulerWorld& world,MobCombatSchedulerBindings& binding):world_(world),binding_(binding){}
 void remove_mob_transform(Actor& actor) override {
  world_.remove_mob_transform(actor);
  if(actor.attack_state().next_tick==1)binding_.weapon_present=vertical_combat_equipped(actor,5)!=nullptr;
 }
 MobRuntime* enable_mob(Actor& actor,std::uint32_t id) override{return world_.enable_mob(actor,id);}
 bool in_range(Actor& actor,MobRuntime& mob) override{return world_.in_range(actor,mob);}
 bool is_both_mob(MobRuntime& mob) override{return world_.is_both_mob(mob);}
 void attack_mob(Actor& actor,MobRuntime& mob) override{world_.attack_mob(actor,mob);}
 void cancel_move(Actor& actor) override{world_.cancel_move(actor);}
 void send_self(Actor& actor,const net::Packet& packet) override{world_.send_self(actor,packet);}
 bool remainder(Actor& actor,std::uint32_t now,MobCombatSchedulerRemainder stage) override{return world_.remainder(actor,now,stage);}
private:
 MobCombatSchedulerWorld& world_;MobCombatSchedulerBindings& binding_;
};
}
bool vertical_combat_scheduler_equipment(Actor& actor,std::uint32_t now,MobCombatSchedulerWorld& world){
 MobCombatSchedulerBindings binding;
 EquipmentWorld adapter(world,binding);
 return mob_combat_scheduler(actor,binding,now,adapter);
}
}
