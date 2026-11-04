#include "eternity/game/mob_targeting_lifecycle.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void position(net::Packet& p,UserPosition v) {append(p,std::bit_cast<std::uint32_t>(v.x));append(p,std::bit_cast<std::uint32_t>(v.y));append(p,std::bit_cast<std::uint32_t>(v.z));}
net::Packet rebirth(const MobTargetingRebirthState& s,std::int32_t kind) {
    net::Packet p{0x51,5};append(p,s.id);append(p,static_cast<std::uint32_t>(kind),1);append(p,s.experience);position(p,s.position);return p;
}
void log(MobTargetingRebirthServices& w) {net::Packet p(74,0);w.prepare_game_log(p);p[0]=4;p[1]=3;w.send_game_log(p);}
}
void mob_targeting_rebirth(MobTargetingRebirthState& s,const UserPosition& destination,std::int32_t kind,std::int32_t map,bool restore,MobTargetingRebirthServices& w) {
    if(s.life!=1) return;
    if(restore) s.current=s.maximum;
    if(const auto dead=w.find_dead(s.id)) {
        w.leave_dead(*dead); // Ignore original LeaveDead bool; never dereference corpse again.
        if(map==s.map) {
            if(w.move_user(s.instance,destination)) {
                const auto p=rebirth(s,kind);if(s.in_zone) w.send_cell_view(p);
                log(w);w.send_recover_change(s.current);
            }
        } else if(w.map_present(map)) {
            w.send_self(rebirth(s,kind));w.send_recover_change(s.current);
            w.zone_leave_user_move(map,destination);
            net::Packet p{0xb,2};append(p,s.id);append(p,s.map,2);position(p,s.position);w.send_self(p);
            log(w);
        }
    }
    s.motion=0;if(s.recovery<1) s.recovery=1;s.life=0;
}
namespace {
bool same(PlayerResources a,PlayerResources b) {return a.hp==b.hp && a.mp==b.mp && a.sp==b.sp;}
class ActorBridge final:public MobTargetingRebirthServices {
public:
    ActorBridge(Actor& a,MobTargetingLifecycleWorld& w):actor(a),world(w) {read();}
    MobTargetingRebirthState state;
    void write() {
        if(!same(actor.resources(),state.current)) {
            world.replace_resources(actor,state.current);
            if(!same(actor.resources(),state.current)) throw std::logic_error("Lifecycle resource provider did not publish exact raw values");
        }
        if(world.presentation_motion(actor)!=state.motion) {
            world.presentation_motion(actor,state.motion);
            if(world.presentation_motion(actor)!=state.motion) throw std::logic_error("Lifecycle motion provider did not publish raw byte");
        }
        // Original tail publishes presentation, recovery, then shared life.
        actor.attack_state().recovery=state.recovery;actor.attack_state().mode=state.life;
    }
    std::optional<std::uint32_t> find_dead(std::uint32_t id) override {write();const auto r=world.find_dead(actor,id);read();return r;}
    void leave_dead(std::uint32_t id) override {write();world.leave_dead(actor,id);read();}
    bool move_user(std::uint16_t instance,const UserPosition& p) override {write();const bool r=world.move_user(actor,instance,p);read();return r;}
    bool map_present(std::int32_t map) override {write();const bool r=world.map_present(map);read();return r;}
    void zone_leave_user_move(std::int32_t map,const UserPosition& p) override {write();world.zone_leave_user_move(actor,map,p);read();}
    void send_cell_view(const net::Packet& p) override {write();world.send_cell_view(actor,p);read();}
    void send_self(const net::Packet& p) override {write();actor.send_self(p);read();}
    void prepare_game_log(net::Packet& p) override {write();write_game_log_main(p,world.subject(actor),world.local_calendar());read();}
    void send_game_log(const net::Packet& p) override {write();world.send_game_log(p);read();}
    void send_recover_change(PlayerResources p) override {write();world.send_recover_change(actor,p);read();}
private:
    void read() {
        const auto s=world.subject(actor);
        state.life=actor.attack_state().mode;state.recovery=actor.attack_state().recovery;state.motion=world.presentation_motion(actor);
        state.id=actor.id();state.experience=s.experience;state.map=s.map;state.instance=world.instance(actor);state.position=s.position;
        state.current=actor.resources();state.maximum=actor.status().skills.equipment.base.maximum;state.in_zone=actor.status().in_zone;
    }
    Actor& actor;MobTargetingLifecycleWorld& world;
};
}
void mob_targeting_rebirth(Actor& a,const UserPosition& p,std::int32_t kind,std::int32_t map,bool restore,MobTargetingLifecycleWorld& w) {
    // Preserve the original dead-only no-op without querying unrelated owners.
    if(a.attack_state().mode!=1) return;
    ActorBridge bridge(a,w);mob_targeting_rebirth(bridge.state,p,kind,map,restore,bridge);bridge.write();
}
}
