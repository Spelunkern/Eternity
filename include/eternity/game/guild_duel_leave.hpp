#pragma once
#include "eternity/game/vertical_move_guild_duel.hpp"
#include "eternity/game/guild_duel_state.hpp"
#include <bit>

namespace eternity::game {
class GuildDuelLeaveWorld {
public:
    virtual ~GuildDuelLeaveWorld()=default;
    virtual GuildDuelState* find_guild(std::uint32_t)=0;
    // Holds the other guild's actual recursive lock through the token lifetime.
    virtual std::shared_ptr<void> lock(GuildDuelState&)=0;
    virtual void send_all(GuildDuelState&,const net::Packet&)=0;
    virtual void send_members(GuildDuelState&,const net::Packet&)=0;
    // Original 00438070 must reset all online members, including effects and
    // damage; provider must iterate the actual live membership, never a no-op.
    virtual void reset_all_members(GuildDuelState&)=0;
};
// CGuild::PvPLeaveGuild 00438160. Caller already holds the source guild lock;
// guilds, actor and services outlive callbacks and all operations are serialized.
inline void guild_duel_leave(GuildDuelState& guild,Actor& actor,VerticalGuildDuelMovement& member,
    std::uint32_t mode,GuildDuelLeaveWorld& world) {
    GuildDuelState* other=guild.opponent_id?world.find_guild(guild.opponent_id):nullptr;
    --guild.participants;
    const auto id=actor.id();const bool leader=mode==1 && id==guild.leader_id;
    const bool terminal=std::bit_cast<std::int32_t>(guild.participants)<=0 || leader;
    net::Packet leave{6,0x25,static_cast<std::uint8_t>(mode)};for(unsigned shift=0;shift<32;shift+=8)leave.push_back(static_cast<std::uint8_t>(id>>shift));
    if(other) {auto lock=world.lock(*other);if(!lock)throw std::logic_error("Cannot lock opponent guild");world.send_all(*other,leave);}
    world.send_all(guild,leave);
    if(!terminal) {
        actor.clear_duel_debuffs();actor.reset_duel_damage();actor.relations().opposing_guild_id=0;
        actor.status().current_target_type=0;actor.relations().guild_duel_ready=0;member.state=-1;return;
    }
    if(other) {
        auto lock=world.lock(*other);if(!lock)throw std::logic_error("Cannot lock opponent guild");
        world.send_members(*other,{5,0x25,1});world.reset_all_members(*other);
        other->status=0;other->stage=0;other->opponent_id=0;other->leader_id=0;
    }
    world.send_members(guild,{5,0x25,2});world.reset_all_members(guild);
    guild.status=0;guild.stage=0;guild.opponent_id=0;guild.leader_id=0;
}
}

namespace eternity::game {
// Binds the movement gate to canonical retained GuildDuelState and member
// records. The owner supplies captured locking/current membership and records.
class GuildDuelMovementAdapter:public VerticalGuildDuelMovementServices {
public:
    explicit GuildDuelMovementAdapter(GuildDuelLeaveWorld& world):world_(world){}
    std::int32_t guild_duel_status(void* guild) final{return duel_state(guild).status;}
    void leave_guild_duel(void* guild,Actor& actor,std::uint8_t mode) final {
        guild_duel_leave(duel_state(guild),actor,movement_state(actor),mode,world_);
    }
    virtual GuildDuelState& duel_state(void* live_guild)=0;
    virtual VerticalGuildDuelMovement& movement_state(Actor&)=0;
private:GuildDuelLeaveWorld& world_;
};
}
