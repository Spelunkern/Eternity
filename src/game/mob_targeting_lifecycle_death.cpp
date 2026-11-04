#include "eternity/game/mob_targeting_lifecycle_death.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
void put(net::Packet& p,std::size_t at,std::uint32_t n,unsigned size) {for(unsigned i=0;i<size;++i) p[at+i]=static_cast<std::uint8_t>(n>>(8*i));}
}
void mob_targeting_npc_death_finish(MobTargetingDeathState& s,std::int32_t type,std::uint32_t id,MobTargetingNpcDeathServices& w) {
    w.pvp_leave_by_death();w.clear_effects();w.reset_damage();w.enter_dead();
    s.pending_move=0;const auto tick=w.tick();s.rebirth_deadline=tick+200000U;s.rebirth_code=0;
    if(s.in_zone) {
        net::Packet p(11,0);put(p,0,0x504,2);put(p,2,s.id,4);put(p,6,static_cast<std::uint32_t>(type),1);put(p,7,id,4);
        w.send_death_view(p,s.id);
    }
    auto* held=s.party;
    if(held) {w.lock_party(held);if(!s.party) {w.unlock_party(held);held=nullptr;}}
    // NPC variant always uses 0c06, even if the live party owner is a raid.
    // A changed non-null party is sent under the OLD captured lock, as original.
    try {
        if(s.party) {net::Packet p(6,0);put(p,0,0xc06,2);put(p,2,s.id,4);w.send_party(s.party,p);}
    } catch(...) {if(held) w.unlock_party(held);throw;}
    if(held) w.unlock_party(held);
}
void mob_targeting_death_by_npc(MobTargetingDeathState& s,std::int32_t type,std::uint32_t id,MobTargetingNpcDeathServices& w,const std::array<std::uint8_t,110>& scratch) {
    if(s.life==1) return;
    s.penalty=5;
    if(s.no_exp_loss!=0 || s.amulet==3) s.penalty=0;
    else if(s.amulet==1 || s.amulet==2) s.penalty=1;
    s.life=1;s.soul=0;
    net::Packet log(scratch.begin(),scratch.end());w.prepare_game_log(log);
    put(log,0,0x301,2);put(log,74,id,2);put(log,76,0,2);
    if(const auto name=w.npc_name(type,id)) {
        bool ended=false;
        for(std::size_t i=0;i<32;++i) {const auto c=ended?std::uint8_t{0}:(*name)[i];log[78+i]=c;ended=ended || c==0;}
        log[109]=0;
    } else log[78]=0;
    w.send_game_log(log);
    mob_targeting_npc_death_finish(s,type,id,w);
}
namespace {
template<class T> void reset_actor_damage(T& actor) {
    if constexpr(requires(T& a) {a.reset_damage_ledger();}) actor.reset_damage_ledger();
    else throw std::logic_error("Missing canonical Actor::reset_damage_ledger lifecycle API");
}
class ActorBridge final:public MobTargetingNpcDeathServices {
public:
    ActorBridge(Actor& a,const NpcDefinitions& n,MobTargetingNpcDeathWorld& w):actor(a),npcs(n),world(w) {read();}
    MobTargetingDeathState state;
    void write() noexcept {
        world.death_penalty(actor)=state.penalty;actor.attack_state().mode=state.life;world.soul_state(actor)=state.soul;
        actor.actions().pending_move=state.pending_move;world.rebirth_deadline(actor)=state.rebirth_deadline;world.rebirth_code(actor)=state.rebirth_code;
    }
    void prepare_game_log(net::Packet& p) override {write();write_game_log_main(p,world.subject(actor),world.local_calendar());read();}
    std::optional<std::array<std::uint8_t,32>> npc_name(std::int32_t type,std::uint32_t id) override {
        write();const auto* n=npcs.find(type,std::bit_cast<std::int32_t>(id));std::optional<std::array<std::uint8_t,32>> out;
        if(n) {out.emplace();std::copy_n(n->record.begin()+20,32,out->begin());}read();return out;
    }
    void send_game_log(const net::Packet& p) override {write();world.send_game_log(p);read();}
    void pvp_leave_by_death() override {write();world.pvp_leave_by_death(actor);read();}
    void clear_effects() override {write();actor.clear_effects_on_death(world.death_penalty(actor));read();}
    void reset_damage() override {write();world.reset_damage(actor);read();}
    void enter_dead() override {write();world.enter_dead(actor);read();}
    std::uint32_t tick() override {write();const auto t=actor.tick_ms();read();return t;}
    void send_death_view(const net::Packet& p,std::uint32_t id) override {write();world.send_death_view(actor,p,id);read();}
    void lock_party(PartyRegistryParty* p) override {write();world.lock_party(p);read();}
    // No flush here: a throwing send may have changed the actual Actor after
    // our last read. Exception cleanup must preserve those partial mutations.
    void unlock_party(PartyRegistryParty* p) noexcept override {world.unlock_party(p);read();}
    void send_party(PartyRegistryParty* p,const net::Packet& b) override {write();world.send_party(p,actor,b);read();}
private:
    void read() noexcept {
        state.life=actor.attack_state().mode;state.penalty=world.death_penalty(actor);state.amulet=actor.status().skills.benefits.amulet;state.no_exp_loss=actor.status().skills.benefits.no_exp_loss;
        state.soul=world.soul_state(actor);state.id=actor.id();state.pending_move=actor.actions().pending_move;state.rebirth_deadline=world.rebirth_deadline(actor);state.rebirth_code=world.rebirth_code(actor);
        state.current=actor.resources();state.party=world.party(actor);state.in_zone=actor.status().in_zone;
    }
    Actor& actor;const NpcDefinitions& npcs;MobTargetingNpcDeathWorld& world;
};
}
void MobTargetingNpcDeathWorld::reset_damage(Actor& a) {reset_actor_damage(a);}
void mob_targeting_death_by_npc(Actor& a,std::int32_t type,std::uint32_t id,const NpcDefinitions& n,MobTargetingNpcDeathWorld& w) {
    if(a.attack_state().mode==1) return;
    ActorBridge bridge(a,n,w);mob_targeting_death_by_npc(bridge.state,type,id,bridge);bridge.write();
}
}
