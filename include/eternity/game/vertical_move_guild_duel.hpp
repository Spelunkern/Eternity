#pragma once
#include "eternity/game/vertical_move_active_duel.hpp"

namespace eternity::game {
// CUser+54e8/+54f4; owned by the real guild-duel session state.
struct VerticalGuildDuelMovement {UserPosition anchor;std::int32_t state=-1;std::uint8_t ready_flag=0;};
class VerticalGuildDuelMovementServices {
public:
    virtual ~VerticalGuildDuelMovementServices()=default;
    // Implements CGuildLock 0042b800: capture the current guild, lock it,
    // then release and return empty if membership disappeared. The returned
    // lifetime object releases the captured lock on destruction.
    virtual std::shared_ptr<void> capture_guild_lock(Actor&)=0;
    // Must reread the actor's current guild after acquiring the captured lock.
    virtual void* current_guild(Actor&)=0;
    virtual std::int32_t guild_duel_status(void*)=0;
    // Actual CGuild::PvPLeaveGuild 00438160, mode 1, including its callbacks.
    virtual void leave_guild_duel(void*,Actor&,std::uint8_t mode)=0;
};
inline void vertical_move_guild_duel(Actor& actor,UserPosition position,const VerticalGuildDuelMovement& state,
    VerticalGuildDuelMovementServices& services) {
    if(actor.relations().opposing_guild_id==0 || state.state!=-1)return;
    const float dx=static_cast<float>(static_cast<double>(state.anchor.x)-position.x);
    const float dz=static_cast<float>(static_cast<double>(state.anchor.z)-position.z);
    const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
    if(!(distance>40.0F))return;
    auto lock=services.capture_guild_lock(actor);
    if(auto* guild=services.current_guild(actor)) {
        const auto status=services.guild_duel_status(guild);
        if(status==3 || status==4)services.leave_guild_duel(guild,actor,1);
    }
}
// Binds the final guild check to the same DuelWorld position source; the owner
// implements actor retention and original raw log-stack policy.
class VerticalMoveGuildPvpServices:public VerticalMovePvpWorldServices {
public:
    VerticalMoveGuildPvpServices(DuelWorld& world,const VerticalGuildDuelMovement& state,VerticalGuildDuelMovementServices& guild)
        :VerticalMovePvpWorldServices(world),state_(state),guild_(guild){}
    void check_guild_duel(Actor& actor) final {
        // Original reads position only after enemy/state gates.
        if(actor.relations().opposing_guild_id && state_.state==-1)
            vertical_move_guild_duel(actor,position(actor),state_,guild_);
    }
private:const VerticalGuildDuelMovement& state_;VerticalGuildDuelMovementServices& guild_;
};
}
