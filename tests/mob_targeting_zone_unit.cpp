#include "eternity/game/mob_targeting_zone.hpp"
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
void put(std::span<std::byte> b,unsigned at,std::uint32_t n,unsigned size=1) {
    for(unsigned i=0;i<size;++i) b[at+i]=static_cast<std::byte>((n>>(8*i))&255);
}
struct User final:MobTargetingZoneUser {
    std::uint32_t id=99;bool visible=true;
    MobTargetingUser target() const override {return {id,0};}
    MobTargetingHelpUser help() const override {return {0,0,0,true,visible};}
};
struct World final:ZoneMobServices,MobTargetingZoneWorld {
    std::function<void(MobRuntime&)> on_dispatch;
    std::vector<std::uint32_t> dispatched;
    unsigned random_calls=0,ticks=0;std::int32_t users=1;
    std::int32_t user_count(const ZoneMobs&) const override {return users;}
    std::int32_t random(MobRuntime&) override {++random_calls;return 1234;}
    std::uint32_t tick_ms(MobRuntime&) override {++ticks;return 10000;}
    void dispatch(MobRuntime& m,std::uint32_t e,std::uint32_t a) override {
        check(e==1 && a==m.target_id,"Dispatch did not observe published target");
        dispatched.push_back(m.id);if(on_dispatch) on_dispatch(m);
    }
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected movement tick");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected movement");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua");}
};
MobTargetingBindingCatalog catalog() {
    auto c=MobTargetingBindingCatalog::load(8,1,{},{});
    auto s=c.mutable_skill_record(1);put(s,0,1,2);put(s,2,100);put(s,74,2,2);c.publish_skill_record(1);
    for(unsigned i=1;i<=8;++i) {
        auto b=c.mutable_mob_record(i);put(b,0,i,2);put(b,38,1);put(b,105,255);put(b,120,1,4);
        c.publish_mob_record(i);
    }
    return c;
}
int main() {
    try {
        auto c=catalog();World w;User u;std::array<MobRuntime,8> m;MobRuntimeTemplate definition;definition.id=1;
        ZoneMobs z(MapGeometry(120),w);MobTargetingZone targeting(z,c,w);
        for(unsigned i=0;i<m.size();++i) {m[i].id=i+1;m[i].type=7;m[i].definition=&definition;}
        m[0].status=1;m[0].position={40,0,40};
        m[1].position={0,0,0};m[1].state_stack=std::vector<std::int32_t>{0};
        m[2].position={40,0,40};m[3].position={80,0,80};m[3].type=8;
        for(unsigned i=0;i<4;++i) check(z.enter(m[i]),"Entry failed");
        // Real bridge dispatch removes current actor, appends another, changes
        // live source type/position and user ID before the next acquisition.
        w.on_dispatch=[&](MobRuntime& actor) {
            check(actor.id==2,"Unexpected FSM actor");
            z.leave(actor);m[4].position={0,0,0};check(z.enter(m[4]),"Append failed");
            m[0].type=8;m[0].position={80,0,80};u.id=101;
        };
        targeting.find_help(m[0],u,MobTargetingHelpKind::normal);
        check(w.dispatched==std::vector<std::uint32_t>{2},"Real FSM dispatch missing");
        check(m[1].target_id==99 && !m[1].zone,"Detached subject bridge publication lost");
        check(m[4].status==0 && m[2].status==0 && m[3].target_id==101,"Live source type/user projection not refreshed");
        check(w.random_calls==0 && w.ticks==0,"False-initialize acquired unexpected timers");
        // Published full catalog is read live, rather than copied canonical AI.
        definition.ai=99;auto b=c.mutable_mob_record(1);put(b,105,1);c.publish_mob_record(1);
        m[2].position={40,0,40};m[0].type=7;
        targeting.find_help(m[0],u,MobTargetingHelpKind::normal2);
        check(m[2].target_id==0,"Reloaded chase radius ignored");
        put(b,105,255);put(b,38,0);c.publish_mob_record(1);
        targeting.find_help(m[0],u,MobTargetingHelpKind::attack);
        check(m[2].target_id==0,"Reloaded candidate AI ignored");
        put(b,38,1);put(b,120,0,4);c.publish_mob_record(1);
        targeting.find_help(m[0],u,MobTargetingHelpKind::normal2);
        check(m[2].target_id==0,"Reloaded source country ignored");
        put(b,120,1,4);c.publish_mob_record(1);u.visible=false;
        targeting.find_help(m[0],u,MobTargetingHelpKind::normal2);
        check(m[2].target_id==0,"Live user visibility ignored");u.visible=true;
        w.users=0;targeting.find_help(m[0],u,MobTargetingHelpKind::normal2);
        check(m[2].target_id==0,"Real world user count ignored");w.users=1;
        targeting.find_help(m[0],u,MobTargetingHelpKind::attack);
        check(m[2].target_id==101,"Catalog AI and target runtime not composed");
        // Root SetTarget initialization routes actual AI help and skill clocks.
        put(b,38,2);put(b,150,255);put(b,164,1,4);c.publish_mob_record(1);
        m[0].status=0;m[0].type=7;m[2].status=0;m[2].target_id=0;
        targeting.set_target(m[0],u,true);
        check(m[2].target_id==101 && m[0].attack_times[1]==11234 && w.random_calls==1 && w.ticks==1,
              "Root SetTarget did not compose help/full skill cooldown");
        bool rejected=false;try {targeting.find_help(m[1],u,MobTargetingHelpKind::normal);}catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"Detached help source accepted");
        // Direct visitor may destroy a removed actor immediately: the traversal
        // itself must never touch that actor again (unlike SetTarget's retained subject).
        auto transient=std::make_unique<MobRuntime>();transient->id=90;check(z.enter(*transient),"Transient entry failed");
        z.visit_cell({0,0},[&](MobRuntime& actor) {if(actor.id==90) {z.leave(actor);transient.reset();}});
        // Nested direct visit consumes the same cursor; outer does not resume a
        // saved iterator. This differs observably from snapshots/save-restore.
        std::vector<std::uint32_t> trace;bool nested=false;
        z.visit_cell({1,1},[&](MobRuntime& actor) {
            trace.push_back(actor.id);
            if(!nested) {nested=true;z.visit_cell({1,1},[&](MobRuntime& inner) {trace.push_back(inner.id+100);});}
        });
        // First outer visit nests a complete traversal (cursor sentinel), then
        // restarts at the head and completes normally.
        check(trace==std::vector<std::uint32_t>({1,101,103,1,3}),"Nested visitor did not share cursor");
        std::cout<<"Direct cursor, real target/help bridge, live catalog/user and retained-lifetime composition passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
