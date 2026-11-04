#include "eternity/game/vertical_mobs.hpp"
#include <cassert>
#include <vector>
using namespace eternity::game;
struct World final : VerticalMobsWorld, MobAiConfiguredServices, MobAiStateServices {
    MobAiConfiguredFsm fsm{1,1}; std::vector<int> calls;
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override {return fsm;}
    MobAiConfiguredServices& configured_services(MobRuntime&) override {return *this;}
    MobAiStateServices& stack_services(MobRuntime&) override {return *this;}
    void release_failed_stack(MobRuntime& m) override {calls.push_back  (4);m.state_stack.reset();}
    void return_home(MobRuntime&,std::uint32_t) override {calls.push_back(5);}
    void ordinary_idle(MobRuntime&,std::uint32_t) override {calls.push_back(6);}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {return 0;}
    std::uint32_t tick() override {return 0;}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {}
    void update(MobRuntime& m,MobAiState) override {calls.push_back(3);m.state_stack->back()=0;}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {calls.push_back(7);}
};
int main() {
    World w; MobRuntime mob;
    w.fsm.states[1].transitions.push_back({2});
    w.fsm.states[1].exit=[&](auto&,auto&){w.calls.push_back(1);};
    w.fsm.states[2].enter=[&](auto&,auto&){w.calls.push_back(2);};
    w.fsm.states[2].update=[&](auto& m,auto&){assert(m.current_time==42);w.calls.push_back(3);};
    mob.use_pais=true;mob.status=100;vertical_mobs_idle(mob,42,w);
    assert((w.calls==std::vector<int>{1,2,3}));
    w.calls.clear();mob.status=3;vertical_mobs_idle(mob,43,w);assert((w.calls==std::vector<int>{5}));
    w.calls.clear();mob.use_pais=false;mob.status=2;vertical_mobs_idle(mob,44,w);assert(w.calls.empty());
    mob.state_stack=std::vector<std::int32_t>{1};vertical_mobs_idle(mob,45,w);assert((w.calls==std::vector<int>{3,6}));
    w.calls.clear();mob.state_stack=std::vector<std::int32_t>{};vertical_mobs_idle(mob,46,w);assert((w.calls==std::vector<int>{7,4}));
}
