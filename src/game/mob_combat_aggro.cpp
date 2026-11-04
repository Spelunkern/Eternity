#include "eternity/game/mob_combat_aggro.hpp"
#include "eternity/game/mobs.hpp"
#include <stdexcept>
namespace eternity::game {
void mob_combat_aggro(MobCombatState& mob,MobDamageLedger& ledger,MobCombatAggroServices& services,
                      std::uint32_t user_id,std::int32_t amount) {
    if(amount<=0 || services.status(mob)==2 || mob.hp==0) return;
    ledger.add(1,user_id,0,amount,0);
    // Add can fail to allocate. The original still rechecks status and attempts
    // retarget; it does not recheck HP or gate on successful ledger insertion.
    if(services.status(mob)==1) services.retarget(mob,mob.stacked_fsm);
}
namespace {
class RuntimeServices final:public MobCombatAggroServices {
public:
    RuntimeServices(MobRuntime& mob,MobCombatState& projection,MobCombatAggroWorld& world)
        :mob_(mob),projection_(projection),world_(world) {}
    std::int32_t status(const MobCombatState&) override {
        // The shared allocator can synchronously update the live subject. Pull
        // again after Add rather than copying stale state back over its writes.
        projection_.hp=mob_.hp;
        projection_.state=mob_.status;
        projection_.stacked_fsm=mob_.state_stack.has_value();
        if(!mob_.state_stack)return mob_.status;
        return mob_.state_stack->empty()?0:mob_.state_stack->back();
    }
    void retarget(MobCombatState&,bool stacked) override {world_.retarget(mob_,stacked);}
private:
    MobRuntime& mob_;MobCombatState& projection_;MobCombatAggroWorld& world_;
};
}
void mob_combat_aggro(MobRuntime& mob,MobCombatAggroWorld& world,std::uint32_t user_id,std::int32_t amount) {
    if(!mob.damage)throw std::invalid_argument("Aggro mob lacks its damage owner");
    MobCombatState projection;
    RuntimeServices services(mob,projection,world);
    mob_combat_aggro(projection,*mob.damage,services,user_id,amount);
}
}
