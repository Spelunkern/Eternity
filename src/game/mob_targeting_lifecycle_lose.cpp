#include "eternity/game/mob_targeting_lifecycle_lose.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
namespace {
void put(net::Packet& p,unsigned at,std::uint32_t n,unsigned size=4) {
    for(unsigned i=0;i<size;++i)p.at(at+i)=static_cast<std::uint8_t>(n>>(8*i));
}
void guild_loss(MobTargetingLoseDeathServices& s) {
    auto* held=s.guild();
    if(held) {
        s.lock_guild(held);
        if(!s.guild()) {s.unlock_guild(held);held=nullptr;}
    }
    try {
        if(auto* live=s.guild())s.leave_guild(live,3);
    } catch(...) {if(held)s.unlock_guild(held);throw;}
    if(held)s.unlock_guild(held);
}
template<class T> void reset(T& actor) {
    if constexpr(requires {actor.reset_damage_ledger();})actor.reset_damage_ledger();
    else throw std::logic_error("Missing canonical Actor::reset_damage_ledger lifecycle API");
}
}
void mob_targeting_pvp_lose_death(std::int32_t type,MobTargetingLoseDeathServices& s,
    const std::array<std::uint8_t,99>& scratch) {
    if(type==5) {guild_loss(s);return;}
    if(type!=4)return;
    s.drop_wager(1,1);s.drop_wager(0,1);
    for(unsigned user=0;user<2;++user) {
        s.clear_debuffs(user);s.reset_damage(user);s.reset_duel_fields(user);
    }
    for(unsigned user=0;user<2;++user) {
        const unsigned field=user==0?3:2;
        const auto old=s.counter(user,field);
        if(old<65535) {
            const auto billing=s.billing_id(user);
            s.counter(user,field,old+1);
            net::Packet packet(11);put(packet,0,0x060c,2);put(packet,2,billing);
            packet[6]=static_cast<std::uint8_t>(user==0?3:2);put(packet,7,old+1);
            s.send_database(packet);
        }
    }
    s.send_self(1,{5,0x24,1});s.send_self(0,{5,0x24,2});
    net::Packet log(scratch.begin(),scratch.end());
    for(unsigned subject:{1U,0U}) {
        s.prepare_log(subject,log);
        put(log,0,subject==1?0x0e01:0x0e02,2);
        const auto other=1-subject;
        put(log,74,s.character_id(other));
        const auto name=s.name(other);
        if(name.size()>20 || name.find('\0')!=std::string::npos)
            throw std::invalid_argument("Invalid PvP death log name");
        std::copy(name.begin(),name.end(),log.begin()+78);log[78+name.size()]=0;
        s.send_log(log);
    }
}
void MobTargetingLoseDeathWorld::reset_damage(Actor& actor) {reset(actor);}
void mob_targeting_pvp_lose_death(Actor& loser,Actor& winner,std::int32_t type,
    MobTargetingLoseDeathWorld& world) {
    struct Bridge final:MobTargetingLoseDeathServices {
        std::array<Actor*,2> users;MobTargetingLoseDeathWorld& w;
        Bridge(Actor& a,Actor& b,MobTargetingLoseDeathWorld& x):users{&a,&b},w(x) {}
        Actor& actor(unsigned i) const {return *users.at(i);}
        void drop_wager(unsigned a,unsigned b) override {drop_duel_wager(actor(a),actor(b),w);}
        void clear_debuffs(unsigned i) override {actor(i).clear_duel_debuffs();}
        void reset_damage(unsigned i) override {w.reset_damage(actor(i));}
        void reset_duel_fields(unsigned i) override {
            auto& a=actor(i);a.relations().duel_state=0;a.relations().duel_type=-1;
            a.status().current_target_type=0; // Target ID/opponent ID survive.
        }
        std::uint32_t counter(unsigned i,unsigned f) const noexcept override {return w.player(actor(i)).kills[f];}
        void counter(unsigned i,unsigned f,std::uint32_t n) noexcept override {w.player(actor(i)).kills[f]=n;}
        std::uint32_t billing_id(unsigned i) const noexcept override {return w.billing_id(actor(i));}
        std::uint32_t character_id(unsigned i) const noexcept override {return w.player(actor(i)).character.id;}
        std::string name(unsigned i) const override {return w.player(actor(i)).character.name;}
        void send_database(const net::Packet& p) override {w.send_database(p);}
        void send_self(unsigned i,const net::Packet& p) override {actor(i).send_self(p);}
        void prepare_log(unsigned i,net::Packet& p) override {write_game_log_main(p,w.duel_log_subject(actor(i)),w.local_calendar());}
        void send_log(const net::Packet& p) override {w.send_duel_log(p);}
        GuildMembership* guild() const noexcept override {return w.guild(actor(0));}
        void lock_guild(GuildMembership* g) override {w.lock_guild(g);}
        void unlock_guild(GuildMembership* g) noexcept override {w.unlock_guild(g);}
        void leave_guild(GuildMembership* g,std::int32_t reason) override {w.leave_guild(g,actor(0),reason);}
    } bridge(loser,winner,world);
    mob_targeting_pvp_lose_death(type,bridge);
}
}
