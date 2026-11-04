#include "eternity/game/mob_targeting_lifecycle_ordinary.hpp"
#include "eternity/game/mobs.hpp"
#include <algorithm>
namespace eternity::game {
namespace {
void put(net::Packet& p,std::size_t at,std::uint32_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p[at+i]=static_cast<std::uint8_t>(v>>(8*i));}
void name(net::Packet& p,const std::string& value) {
    if(value.size()>31 || value.find('\0')!=std::string::npos) throw std::invalid_argument("Death log name exceeds recovered bounded catalog field");
    std::copy(value.begin(),value.end(),p.begin()+78);p[78+value.size()]=0;
}
void finish(MobTargetingOrdinaryDeathState& s,std::int32_t type,std::uint32_t id,MobTargetingOrdinaryDeathServices& w,std::uint16_t opcode) {
    w.pvp_leave_by_death();w.clear_effects();w.reset_damage();w.enter_dead();s.pending_move=0;
    const auto tick=w.tick();s.rebirth_deadline=tick+200000U;s.rebirth_code=0;
    if(s.in_zone) {net::Packet p(11,0);put(p,0,0x504,2);put(p,2,s.id);put(p,6,static_cast<std::uint32_t>(type),1);put(p,7,id);w.send_death_view(p,s.id);}
    auto* held=s.party;if(held) {w.lock_party(held);if(!s.party) {w.unlock_party(held);held=nullptr;}}
    try {
        if(s.party) {
            const auto kind=w.party_kind(s.party);if(kind==0) opcode=0xc06;else if(kind==1) opcode=0xc11;
            net::Packet p(6,0);put(p,0,opcode,2);put(p,2,s.id);w.send_party(s.party,p);
        }
    } catch(...) {if(held) w.unlock_party(held);throw;}
    if(held) w.unlock_party(held);
}
}
void mob_targeting_death_ordinary(MobTargetingOrdinaryDeathState& s,std::int32_t type,std::uint32_t source,MobTargetingOrdinaryDeathServices& w,const MobTargetingOrdinaryDeathScratch& scratch) {
    if(s.life==1) return;
    s.penalty=s.no_exp_loss!=0 || s.amulet==3?0:s.amulet==1 || s.amulet==2?1:5;
    s.life=1;s.soul=0;s.current={0,0,0};
    w.cancel_exchange();w.end_shop();w.cancel_vehicle();const auto maximum=w.maximum_damage();
    if(maximum && maximum->kind==1) throw MobTargetingPlayerDeathRequired();
    net::Packet log(scratch.log.begin(),scratch.log.end());w.prepare_game_log(log);
    if(!maximum && type==1) {
        put(log,0,0x303,2);put(log,74,scratch.failed_max_source);log[78]=0;put(log,99,0,2);
        auto* attacker=w.find_user(source);if(attacker) {const auto text=w.user_name(attacker);if(text.size()>20) throw std::invalid_argument("Death player name exceeds original CUser capacity");name(log,text);}
        if(source!=s.id) w.item_drop_by_user(attacker);
        for(unsigned i=0;i<4;++i) put(log,101+i*4,s.kills[i]);
    } else {
        put(log,0,0x301,2);put(log,74,0,4);log[78]=0;
        const auto mob=w.find_mob(maximum?maximum->source:source);
        if(mob) {put(log,74,mob->type,2);name(log,mob->name);}
        log.resize(110);
    }
    w.send_game_log(log);
    finish(s,maximum?maximum->kind:type,maximum?maximum->source:source,w,scratch.notification_opcode);
}
namespace {
template<class T> void replace_raw(T& actor,PlayerResources p) {
    if constexpr(requires(T& a,PlayerResources v) {a.replace_resources_raw(v);}) actor.replace_resources_raw(p);
    else throw std::logic_error("Missing canonical Actor::replace_resources_raw lifecycle API");
}
bool same(PlayerResources a,PlayerResources b) {return a.hp==b.hp && a.mp==b.mp && a.sp==b.sp;}
class ActorBridge final:public MobTargetingOrdinaryDeathServices {
public:
    ActorBridge(Actor& a,const MobTargetingBindingCatalog& c,MobTargetingOrdinaryDeathWorld& w):actor(a),catalog(c),world(w) {read();}
    MobTargetingOrdinaryDeathState state;
    void write() {
        world.death_penalty(actor)=state.penalty;actor.attack_state().mode=state.life;world.soul_state(actor)=state.soul;
        if(!same(actor.resources(),state.current)) {world.replace_resources(actor,state.current);if(!same(actor.resources(),state.current)) throw std::logic_error("Death resource owner did not publish exact raw values");}
        actor.actions().pending_move=state.pending_move;world.rebirth_deadline(actor)=state.rebirth_deadline;world.rebirth_code(actor)=state.rebirth_code;
    }
    void prepare_game_log(net::Packet& p) override {write();write_game_log_main(p,world.subject(actor),world.local_calendar());read();}
    std::optional<std::array<std::uint8_t,32>> npc_name(std::int32_t,std::uint32_t) override {throw std::logic_error("Ordinary Death does not perform NPC lookup");}
    void send_game_log(const net::Packet& p) override {write();world.send_game_log(p);read();}
    void cancel_exchange() override {write();world.cancel_exchange(actor);read();}
    void end_shop() override {write();world.end_shop(actor);read();}
    void cancel_vehicle() override {write();world.cancel_vehicle(actor);read();}
    std::optional<DamageRecord> maximum_damage() override {write();const auto out=actor.damage_ledger().maximum();read();return out;}
    std::optional<MobTargetingDeathMobLog> find_mob(std::uint32_t id) override {
        write();const auto* mob=world.find_mob(actor,id);std::optional<MobTargetingDeathMobLog> out;
        if(mob) {
            const auto* definition=mob->definition?catalog.mob(mob->definition->id):nullptr;
            if(!definition) throw std::logic_error("Death mob lookup requires its retained full catalog binding");
            std::string text;for(unsigned i=2;i<34 && definition->record[i]!=std::byte{0};++i) text+=static_cast<char>(definition->record[i]);
            out=MobTargetingDeathMobLog{mob->type,std::move(text)};
        }
        read();return out;
    }
    Actor* find_user(std::uint32_t id) override {write();auto* a=world.find_user(actor,id);read();return a;}
    std::string user_name(Actor* a) override {return world.player(*a).character.name;}
    void item_drop_by_user(Actor* attacker) override {write();world.item_drop_by_user(actor,attacker);read();}
    std::uint8_t party_kind(PartyRegistryParty* p) const noexcept override {return world.party_kind(p);}
    void pvp_leave_by_death() override {write();world.pvp_leave_by_death(actor);read();}
    void clear_effects() override {write();actor.clear_effects_on_death(world.death_penalty(actor));read();}
    void reset_damage() override {write();world.reset_damage(actor);read();}
    void enter_dead() override {write();world.enter_dead(actor);read();}
    std::uint32_t tick() override {write();const auto t=actor.tick_ms();read();return t;}
    void send_death_view(const net::Packet& p,std::uint32_t exclude) override {write();world.send_death_view(actor,p,exclude);read();}
    void lock_party(PartyRegistryParty* p) override {write();world.lock_party(p);read();}
    // Never flush a stale projection while unwinding a failed transport callback.
    void unlock_party(PartyRegistryParty* p) noexcept override {world.unlock_party(p);read();}
    void send_party(PartyRegistryParty* p,const net::Packet& b) override {write();world.send_party(p,actor,b);read();}
private:
    void read() noexcept {
        state.life=actor.attack_state().mode;state.penalty=world.death_penalty(actor);state.amulet=actor.status().skills.benefits.amulet;state.no_exp_loss=actor.status().skills.benefits.no_exp_loss;
        state.soul=world.soul_state(actor);state.id=actor.id();state.pending_move=actor.actions().pending_move;state.rebirth_deadline=world.rebirth_deadline(actor);state.rebirth_code=world.rebirth_code(actor);
        state.current=actor.resources();state.party=world.party(actor);state.in_zone=actor.status().in_zone;state.kills=world.player(actor).kills;
    }
    Actor& actor;const MobTargetingBindingCatalog& catalog;MobTargetingOrdinaryDeathWorld& world;
};
}
void MobTargetingOrdinaryDeathWorld::replace_resources(Actor& a,PlayerResources p) {replace_raw(a,p);}
void mob_targeting_death_ordinary(Actor& a,std::int32_t type,std::uint32_t id,const MobTargetingBindingCatalog& c,MobTargetingOrdinaryDeathWorld& w) {
    if(a.attack_state().mode==1) return;
    ActorBridge bridge(a,c,w);mob_targeting_death_ordinary(bridge.state,type,id,bridge);bridge.write();
}
}
