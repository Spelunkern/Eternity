#pragma once
#include "eternity/game/vertical_move_interactions.hpp"
#include "eternity/game/vertical_move_pvp.hpp"

namespace eternity::game {
// Bridges PacketPC's final interaction step to the retained per-user duel state.
// The owner still provides live actor positions, retention and guild-mark update.
class VerticalMovePvpInteractionServices:public VerticalMoveInteractionServices {
public:
    VerticalMovePvpInteractionServices(const VerticalDuelMovement& duel,VerticalMovePvpServices& pvp):duel_(duel),pvp_(pvp){}
    void check_pvp(Actor& actor) final{vertical_move_pvp(actor,duel_,pvp_);}
private:const VerticalDuelMovement& duel_;VerticalMovePvpServices& pvp_;
};
}
