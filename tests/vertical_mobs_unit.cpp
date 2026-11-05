#include "eternity/game/vertical_mobs.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
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
struct Targeting final : MobTargetingFindServices {
    unsigned visits=0,selected=0;
    bool found=false;
    void visit_cell_users(std::int32_t,std::int32_t,const std::function<bool(const MobTargetingFindUser&)>& visitor) override {
        ++visits;
        if(found) visitor({123,0,0,0,true,true,0,0});
    }
    void set_target_with_help(std::uintptr_t token) override {assert(token==123);++selected;}
};
struct FixedRandom final : RandomSource {
    unsigned index=0;
    std::int32_t next() override {constexpr std::int32_t values[]{1234,567,891};assert(index<3);return values[index++];}
};
struct MovementServices final : ZoneMobServices {
    unsigned broadcasts=0;
    struct Broadcast final:MobBroadcast {void send_cell(CellCoordinates) override {}};
    std::uint32_t tick_ms() override {return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {++broadcasts;return std::make_unique<Broadcast>();}
    void lua_init(MobRuntime&) override {assert(false);}
};
int main(int argc,char** argv) {
    assert(argc==3);
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
    std::ifstream oracle(argv[1]);assert(oracle);std::string line;unsigned cases=0;
    while(std::getline(oracle,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::uint32_t now,next_target,next_move,result;unsigned obelisk,found,calls,expected_moves;
        std::istringstream row(line);row>>now>>next_target>>next_move>>obelisk>>found>>calls>>expected_moves>>result;assert(row);
        MobTargetingFindSource source{1,1,1,0,0,0,0,1,10};Targeting target;target.found=found;unsigned moves=0;
        mob.next_new_target=next_target;mob.next_move=next_move;
        vertical_mobs_ordinary_idle(mob,now,obelisk,source,target,[&](MobRuntime& subject,std::uint32_t tick){assert(&subject==&mob && tick==now);++moves;});
        assert(target.visits==calls && target.selected==(calls && found) && moves==expected_moves && mob.next_new_target==result);
        ++cases;
    }
    assert(cases==6);
    std::ifstream movement_oracle(argv[2]);assert(movement_oracle);cases=0;
    while(std::getline(movement_oracle,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::uint32_t now,normal,expected_next;int x,chase,ai,speed,step,stop,sleep,stun,hold,current_speed,expected_status;unsigned expected_random;
        std::istringstream row(line);row>>now>>x>>chase>>ai>>speed>>step>>normal>>stop>>sleep>>stun>>hold>>current_speed>>expected_status>>expected_next>>expected_random;assert(row);
        MobRuntime subject;subject.position={static_cast<float>(x),0,0};subject.stop=stop;subject.sleep=sleep;subject.stun=stun;subject.hold_position=hold;subject.move_speed=current_speed;
        MobAiIdleProjection data{static_cast<std::uint8_t>(ai),static_cast<std::uint8_t>(speed),static_cast<std::uint8_t>(step),normal,false,chase!=0,false};
        MapDefinition map;FixedRandom random;vertical_mobs_ordinary_movement(subject,now,data,map,random);
        assert(subject.status==expected_status && subject.next_move==expected_next && random.index==expected_random);++cases;
    }
    assert(cases==7);
    MovementServices movement_services;ZoneMobs zone(MapGeometry(81),movement_services);MobRuntime moving;moving.id=7;moving.position={10,0,10};assert(zone.enter(moving));
    MapDefinition actual_map;actual_map.size=81;actual_map.tiles.resize((81*81+7)/8);
    MobAiIdleProjection movement_data{1,1,2,100,false,false,true};FixedRandom random;Targeting no_target;MobTargetingFindSource live_source{};
    vertical_mobs_ordinary_idle(moving,50,movement_data,live_source,no_target,actual_map,random);
    assert((moving.position.x!=10 || moving.position.z!=10) && moving.previous_move_time==77 && movement_services.broadcasts==1 && random.index==3);
    const auto previous=moving.position;std::fill(actual_map.tiles.begin(),actual_map.tiles.end(),0xff);FixedRandom blocked_random;
    vertical_mobs_ordinary_movement(moving,50,movement_data,actual_map,blocked_random);
    assert(moving.position.x==previous.x && moving.position.z==previous.z && movement_services.broadcasts==1 && blocked_random.index==3);
    assert(zone.leave(moving));
}
