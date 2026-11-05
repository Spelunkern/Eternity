#pragma once
#include "eternity/game/guild_duel_leave.hpp"
#include "eternity/game/guild_duel_reset_members.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"

namespace eternity::game {
enum class GuildDuelAudience {participating,ready};
class GuildDuelBroadcastServices:public GuildDuelResetServices {
public:
    virtual std::unique_ptr<UserViewBuffer> allocate(const net::Packet&)=0;
    virtual void enqueue(GuildUser&,Actor&,UserViewBuffer&)=0;
};
// 004383c0 selects opposing_guild_id; 00438320 selects byte +54f8.
// Both allocate one shared buffer only if guild status is nonzero. The owner
// serializes guild membership, retains all records/actors through callbacks and
// forbids structural tree mutation until original reentrant traversal is ported.
inline void guild_duel_broadcast(GuildDuelState& state,GuildMembership& membership,
    const net::Packet& packet,GuildDuelAudience audience,GuildDuelBroadcastServices& services) {
    if(state.status==0)return;
    auto buffer=services.allocate(packet);if(!buffer)return;
    for(const auto& [id,record]:membership.online) {
        (void)id;if(!record)break;if(!record->user)continue;
        auto& user=*record->user;auto& actor=services.actor(user);
        if(actor.id()!=user.character_id)throw std::logic_error("Guild broadcast resolved another actor");
        auto pin=services.retain(actor);if(!pin)throw std::logic_error("Cannot retain guild broadcast actor");
        const bool included=audience==GuildDuelAudience::participating?actor.relations().opposing_guild_id!=0:services.movement_state(actor).ready_flag!=0;
        if(included)services.enqueue(user,actor,*buffer);
    }
}
// Supplies the real shared buffer/refcount path; owner resolves the exact
// retained live connection and actor/session for each guild member.
class GuildDuelSharedBroadcast:public GuildDuelBroadcastServices {
public:
    explicit GuildDuelSharedBroadcast(UserViewSharedBuffers& buffers):buffers_(buffers){}
    std::unique_ptr<UserViewBuffer> allocate(const net::Packet& packet) final{return buffers_.allocate(packet);}
    void enqueue(GuildUser&,Actor& actor,UserViewBuffer& buffer) final {
        auto& user=connection(actor);if(user.id!=actor.id())throw std::logic_error("Guild connection belongs to another actor");
        buffers_.enqueue(user,buffer);
    }
    virtual PartyMembershipUser& connection(Actor&)=0;
private:UserViewSharedBuffers& buffers_;
};
}
