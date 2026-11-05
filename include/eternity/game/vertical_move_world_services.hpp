#pragma once
#include "eternity/game/vertical_move_priority_services.hpp"
#include "eternity/game/vertical_move_interactions.hpp"

namespace eternity::game {
// Binds the remaining recovered movement interactions to the actual actor's
// shared priority-view state. The world owner still supplies the real guild,
// PvP, live positions, actor retention, speed table, clock and close services.
class VerticalMoveWorldServices:public VerticalMovePriorityServices {
public:
    VerticalMoveWorldServices(UserPriorityViewState& state,UserPriorityZoneView& view,
        VerticalNpcInteraction& npc,VerticalMoveInteractionServices& interactions)
        :VerticalMovePriorityServices(state,view),npc_(npc),interactions_(interactions){}
    void finish_move(Actor& actor,bool wide_pass) final {vertical_move_interactions(actor,npc_,wide_pass,interactions_);}
private:VerticalNpcInteraction& npc_;VerticalMoveInteractionServices& interactions_;
};
}
