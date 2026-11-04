#include "eternity/game/mob_targeting_runtime.hpp"
#include "eternity/game/mobs.hpp"
#include <bit>
namespace eternity::game {
namespace {
class Bridge final:public MobTargetingServices {
public:
    Bridge(MobRuntime& mob,MobTargetingRuntimeServices& services):mob_(mob),services_(services) {read();}
    MobTargetingState state;
    void write() {
        mob_.status=state.status;mob_.move_mode=state.move_mode;
        mob_.vision_fixed_user=state.vision_fixed_user;
        mob_.target_kind=std::bit_cast<std::int32_t>(state.target_type);mob_.target_id=state.target_id;
        mob_.combat_timeout=state.combat_timeout;mob_.next_move=state.next_move_time;
        mob_.attack_times=state.attack_times;
    }
    std::int32_t random() override {write();const auto v=services_.random(mob_);read();return v;}
    std::uint32_t tick_ms() override {write();const auto v=services_.tick_ms(mob_);read();return v;}
    void help(MobTargetingState&,const MobTargetingUser& user,std::uint8_t kind) override {
        write();services_.help(mob_,user,kind);read();
    }
    void dispatch(MobTargetingState&,std::uint32_t event,std::uint32_t argument) override {
        write();services_.dispatch(mob_,event,argument);read();
    }
private:
    void read() {
        state.fsm_enabled=mob_.state_stack.has_value();
        state.scene_status=state.fsm_enabled && !mob_.state_stack->empty()?mob_.state_stack->back():0;
        state.status=mob_.status;state.move_mode=mob_.move_mode;
        state.vision_fixed_user=mob_.vision_fixed_user;
        state.target_type=static_cast<std::uint32_t>(mob_.target_kind);state.target_id=mob_.target_id;
        state.combat_timeout=mob_.combat_timeout;state.next_move_time=mob_.next_move;
        state.attack_times=mob_.attack_times;
        const auto binding=services_.bindings(mob_);state.ai=binding.ai;state.attacks=binding.attacks;
    }
    MobRuntime& mob_;MobTargetingRuntimeServices& services_;
};
}
void mob_set_target(MobRuntime& m,const MobTargetingUser& u,bool initialize,MobTargetingRuntimeServices& s) {
    Bridge bridge(m,s);mob_set_target(bridge.state,u,initialize,bridge);bridge.write();
}
void mob_set_target_ex(MobRuntime& m,const MobTargetingUser& u,MobTargetingRuntimeServices& s) {
    Bridge bridge(m,s);mob_set_target_ex(bridge.state,u,bridge);bridge.write();
}
}
