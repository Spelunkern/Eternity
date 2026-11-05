#pragma once
#include "eternity/game/guild_duel_broadcast.hpp"
namespace eternity::game {
// Connects original duel sends and cleanup to retained membership registries.
// Owner supplies guild mapping, captured locks, actor/session lookup and the
// exact reset state shared with movement.
class GuildDuelMembershipWorld:public GuildDuelLeaveWorld {
public:
    explicit GuildDuelMembershipWorld(GuildDuelBroadcastServices& services):services_(services){}
    void send_all(GuildDuelState& state,const net::Packet& packet) final {
        guild_duel_broadcast(state,membership(state),packet,GuildDuelAudience::participating,services_);
    }
    void send_members(GuildDuelState& state,const net::Packet& packet) final {
        guild_duel_broadcast(state,membership(state),packet,GuildDuelAudience::ready,services_);
    }
    void reset_all_members(GuildDuelState& state) final {guild_duel_reset_members(membership(state),services_);}
    virtual GuildMembership& membership(GuildDuelState&)=0;
private:GuildDuelBroadcastServices& services_;
};
}
