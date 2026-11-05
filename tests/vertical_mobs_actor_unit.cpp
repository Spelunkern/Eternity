#include "eternity/game/vertical_mobs.hpp"
#include "eternity/game/mob_targeting_actor.hpp"
#include "eternity/game/actor.hpp"
#include <cassert>
#include <stdexcept>
using namespace eternity::game;
struct Scene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::logic_error("unexpected reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("unexpected stop");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::logic_error("unexpected zone query");}
    UserShapeSocial user_shape_social() override {throw std::logic_error("unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("unexpected damage");}
};
struct Cells final:ZoneMobServices {
    std::function<void()> on_move;
    struct Broadcast final:MobBroadcast {void send_cell(CellCoordinates) override {}};
    std::uint32_t tick_ms() override {return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {if(!on_move) throw std::logic_error("unexpected movement");on_move();return std::make_unique<Broadcast>();}
    void lua_init(MobRuntime&) override {throw std::logic_error("unexpected Lua");}
};
struct Fsm final:MobAiStateServices {
    unsigned updates=0,errors=0;
    std::uint32_t tick() override {throw std::logic_error("unexpected FSM tick");}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::logic_error("unexpected Lua");}
    void update(MobRuntime& mob,MobAiState state) override {assert(state==MobAiState::idle);++updates;mob.state_stack->back()=2;}
    void error(MobRuntime&,MobAiError error,std::int32_t,std::int32_t) override {assert(error==MobAiError::empty_poll);++errors;}
};
struct AiOwners final:VerticalMobsAiOwners,MobAiConfiguredServices {
    explicit AiOwners(Fsm& states):states_(states) {}
    MobAiConfiguredFsm configured{1,1};Fsm& states_;MobRuntime* returned=nullptr;std::uint32_t return_tick=0;unsigned releases=0;
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override {return configured;}
    MobAiConfiguredServices& configured_services(MobRuntime&) override {return *this;}
    MobAiStateServices& stack_services(MobRuntime&) override {return states_;}
    void release_failed_stack(MobRuntime& mob) override {++releases;mob.state_stack.reset();}
    void return_home(MobRuntime& mob,std::uint32_t tick) override {returned=&mob;return_tick=tick;}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::logic_error("unexpected condition");}
};
int main() {
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;
    ActorEquipmentRules equipment{items,enchant,attacks};Scene scene;Cells cells;Fsm fsm;
    Actor hidden(10,{}, {0,0,0},{},scene,equipment),target(11,{}, {0,0,0},{},scene,equipment);
    hidden.status().shape=1;
    ZoneUsers users(3,3,0);users.attach(hidden,0,0,{11,0,10});users.attach(target,0,0,{12,0,10});
    ZoneMobs mobs(MapGeometry(81),cells);MobRuntimeTemplate definition;definition.id=1;MobRuntime mob;mob.id=1;mob.definition=&definition;mob.position={10,0,10};mob.next_move=100;assert(mobs.enter(mob));
    auto catalog=MobTargetingBindingCatalog::load(1,0,{},{});auto record=catalog.mutable_mob_record(1);record[0]=std::byte{1};record[38]=std::byte{1};record[105]=std::byte{20};catalog.publish_mob_record(1);
    MicrosoftRandom random(1);MobTargetingActorWorld world(mobs,users,random,fsm);MobTargetingZone targeting(mobs,catalog,world);
    record[0x60]=std::byte{2};record[0x68]=std::byte{1};record[0x5c]=std::byte{0x78};record[0x5d]=std::byte{0x56};record[0x5e]=std::byte{0x34};record[0x5f]=std::byte{0x12};
    MapDefinition map;map.size=81;map.tiles.resize((81*81+7)/8);
    auto projection=vertical_mobs_idle_projection(mob,catalog);
    assert(projection.ai==1 && projection.move_speed==1 && projection.normal_step==2 && projection.normal_time==0x12345678 && !projection.obelisk && !projection.chase_path_present && projection.zone_available);
    record[0x7c]=std::byte{1};mob.trail.add(map,{10,10});projection=vertical_mobs_idle_projection(mob,catalog);
    assert(projection.obelisk && projection.chase_path_present);record[0x7c]=std::byte{0};mob.trail.clear();
    AiOwners ai(fsm);VerticalMobsActorWorld adapter(mobs,users,targeting,catalog,map,random,ai);
    vertical_mobs_idle(mob,50,adapter);
    assert(mob.status==1 && mob.target_kind==1 && mob.target_id==11 && mob.next_move==0);
    mob.use_pais=true;mob.status=3;vertical_mobs_idle(mob,51,adapter);assert(ai.returned==&mob && ai.return_tick==51);
    unsigned updates=0;ai.configured.states[1].update=[&](MobRuntime& subject,MobAiConfiguredFsm&){assert(&subject==&mob && subject.current_time==52);++updates;};
    mob.status=100;vertical_mobs_idle(mob,52,adapter);assert(updates==1);mob.use_pais=false;
    mob.status=0;mob.state_stack=std::vector<std::int32_t>{0};vertical_mobs_idle(mob,53,adapter);assert(fsm.updates==1 && mob.state_stack->back()==2);
    mob.state_stack=std::vector<std::int32_t>{};vertical_mobs_idle(mob,54,adapter);assert(fsm.errors==1 && ai.releases==1 && !mob.state_stack);
    mob.status=0;mob.target_id=0;mob.next_move=100;target.status().no_mob_targeting=true;
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(mob.status==0 && mob.target_id==0 && mob.next_move==100);
    target.status().no_mob_targeting=false;target.appearance().visible=false;
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(mob.status==0 && mob.target_id==0);
    target.appearance().visible=true;record[105]=std::byte{0};
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(mob.status==0 && mob.target_id==0);
    record[105]=std::byte{20};
    record[0x26]=std::byte{4};vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(mob.status==0 && mob.target_id==0);record[0x26]=std::byte{1};
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(mob.status==1 && mob.target_id==11);
    mob.status=0;mob.target_id=0;mob.next_move=0;record[0x26]=std::byte{0};
    record[0x5c]=std::byte{100};record[0x5d]=record[0x5e]=record[0x5f]=std::byte{0};
    unsigned broadcasts=0;cells.on_move=[&]{++broadcasts;record[0x5c]=std::byte{0xf4};record[0x5d]=std::byte{1};};
    MicrosoftRandom control(random.state());static_cast<void>(control.next());static_cast<void>(control.next());const auto delay=static_cast<std::uint32_t>(control.next()%30000);
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(broadcasts==1 && mob.previous_move_time==77 && mob.next_move==50+10000+500+delay && mob.next_move==16884 && random.state()==control.state());
    mob.position=mob.respawn_position;mob.next_move=0;random=MicrosoftRandom(1);record[0x5c]=std::byte{100};record[0x5d]=record[0x5e]=record[0x5f]=std::byte{0};
    cells.on_move=[&]{++broadcasts;for(unsigned i=0;i<4;++i) record[0x5c+i]=std::byte{0xff};};
    vertical_mobs_actor_idle(mob,50,mobs,users,targeting,catalog,map,random);
    assert(broadcasts==2 && mob.next_move==16383);
    assert(users.detach(hidden.id()) && users.detach(target.id()) && mobs.leave(mob));
}
