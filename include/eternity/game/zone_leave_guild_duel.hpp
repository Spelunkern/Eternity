#pragma once
#include "eternity/game/guild_duel_leave.hpp"

namespace eternity::game {
class ZoneLeaveGuildDuelServices:public VerticalGuildDuelMovementServices {
public:
    virtual GuildDuelState& duel_state(void*)=0;
    virtual VerticalGuildDuelMovement& movement_state(Actor&)=0;
    virtual Actor* find_in_zone(Actor&,std::uint32_t)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
};
// Guild half of ps_game.exe PvPChkLeaveByZone 0047bafb..0047bcad. Current
// guilds are reread after callbacks while the originally captured locks remain
// held. The original assumes a peer guild exists in the negotiation branches;
// the native owner rejects that invalid state instead of dereferencing null.
inline void zone_leave_guild_duel(Actor& actor,ZoneLeaveGuildDuelServices& services) {
    if(!actor.relations().opposing_guild_id)return;
    auto source_lock=services.capture_guild_lock(actor);
    auto* guild=services.current_guild(actor);if(!guild)return;
    auto* peer=services.find_in_zone(actor,static_cast<std::uint32_t>(services.duel_state(guild).stage));if(!peer)return;
    auto peer_lease=services.retain(*peer);if(!peer_lease)throw std::logic_error("Cannot retain guild duel peer");
    guild=services.current_guild(actor);if(!guild)throw std::logic_error("Guild vanished during duel lookup");
    auto& state=services.duel_state(guild);const auto status=state.status;
    if(status==3 || status==4){services.leave_guild_duel(guild,actor,1);return;}
    if((status!=1 && status!=2) || state.leader_id!=actor.id())return;
    auto peer_lock=services.capture_guild_lock(*peer);
    auto reset_user=[&](Actor& value){
        value.clear_duel_debuffs();value.reset_duel_damage();value.relations().opposing_guild_id=0;
        services.movement_state(value).state=-1;value.status().current_target_type=0;value.relations().guild_duel_ready=0;
    };
    reset_user(actor);reset_user(*peer);
    guild=services.current_guild(actor);auto* other=services.current_guild(*peer);
    if(!guild || !other)throw std::logic_error("Missing guild during negotiation reset");
    auto reset_guild=[&](void* value){auto& s=services.duel_state(value);s.status=0;s.stage=0;s.opponent_id=0;s.leader_id=0;};
    reset_guild(guild);reset_guild(other);
    const auto id=status==1?peer->id():actor.id();net::Packet packet{2,0x25,3};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(id>>(i*8)));
    peer->send_self(packet);actor.send_self(packet);
}
}
