#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/zone_npcs.hpp"
#include <bit>
#include <cmath>

namespace eternity::game {
// CUser+1564, retained by the owning session and serialized with NPC removal.
struct VerticalNpcInteraction {Npc* npc=nullptr;};
class VerticalMoveInteractionServices {
public:
    virtual ~VerticalMoveInteractionServices()=default;
    virtual UserPosition position(const Actor&)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    // Actual locked guild-mark refresh from 0046a55f; required when wide=true.
    virtual void refresh_guild_mark(Actor&)=0;
    // Actual 0047bef0 PvP/duel movement checks, always invoked last.
    virtual void check_pvp(Actor&)=0;
};
// PacketPC 0046a55f..0046a64d. The world serializes and retains actor, NPC,
// exchange peer and zone through callbacks. NPC and peer may be absent.
inline void vertical_move_interactions(Actor& actor,VerticalNpcInteraction& interaction,bool wide,
    VerticalMoveInteractionServices& services) {
    if(wide)services.refresh_guild_mark(actor);
    const auto far=[](UserPosition a,UserPosition b) {
        const float dx=static_cast<float>(static_cast<double>(a.x)-b.x);
        const float dz=static_cast<float>(static_cast<double>(a.z)-b.z);
        const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
        return distance>10.0F; // Unordered comparisons do not cancel.
    };
    if(auto* npc=interaction.npc; npc && far(services.position(actor),npc->position)) {
        npc->users_using=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(npc->users_using)-1U);interaction.npc=nullptr;
    }
    auto& exchange=actor.exchange();
    if(exchange.mode!=0 && exchange.peer) {
        Actor* peer=exchange.peer;auto pin=services.retain(*peer);if(!pin)throw std::logic_error("Cannot retain exchange peer");
        if(far(services.position(actor),services.position(*peer))) {
            exchange={};peer->exchange()={};actor.send_self({4,10,2});peer->send_self({4,10,3});
        }
    }
    services.check_pvp(actor);
}
}
