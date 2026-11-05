#pragma once
#include "eternity/game/vertical_move_guild_duel.hpp"
#include "eternity/game/vertical_session_registry.hpp"

namespace eternity::game {
// The serialized world owner supplies the actual duel world, guild owner,
// session registry, per-user guild-duel state and original log-byte policy.
// All those owners and any actors selected by DuelWorld outlive each callback.
template<class Session> class VerticalMoveRetainedPvp final:public VerticalMoveGuildPvpServices {
public:
    using LogBytes=std::function<std::array<std::uint8_t,100>()>;
    VerticalMoveRetainedPvp(VerticalSessionRegistry<Session>& registry,DuelWorld& world,
        const VerticalGuildDuelMovement& state,VerticalGuildDuelMovementServices& guild,LogBytes log_bytes)
        :VerticalMoveGuildPvpServices(world,state,guild),registry_(registry),log_bytes_(std::move(log_bytes)) {
        if(!log_bytes_)throw std::invalid_argument("Movement duel requires a log-byte provider");
    }
    std::shared_ptr<void> retain(Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        if(!owner)throw std::logic_error("Duel actor has no retained session");
        return owner;
    }
    std::array<std::uint8_t,100> log_stack_bytes() override{return log_bytes_();}
private:VerticalSessionRegistry<Session>& registry_;LogBytes log_bytes_;
};
}
