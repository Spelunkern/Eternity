#pragma once
#include "eternity/game/vertical_move_gameplay_binding.hpp"

namespace eternity::game {
// Adapts the composed movement owner to a gameplay dispatcher's create_retained
// contract. The dispatcher and its other-opcode handler are real world owners;
// the dependency lease must retain the supplied state without a session cycle.
// Session/zone/registry and callbacks are serialized on the world executor.
template<class Gameplay,class Strand,class Session>
std::shared_ptr<Gameplay> make_vertical_move_gameplay(Strand strand,VerticalSessionRegistry<Session>& registry,
    const std::shared_ptr<Session>& owner,RuntimeZone& zone,VerticalMoveGameplayDependencies<Session> dependencies,
    typename Gameplay::Other other) {
    if(!owner)throw std::invalid_argument("Movement gameplay requires a session");
    return Gameplay::create_retained(std::move(strand),registry,owner,zone,owner->zone_entry(),
        [&registry,dependencies=std::move(dependencies)](Session& session,Actor& actor,RuntimeZone& selected,
            WorldAdmissionUser& admission,ZoneEntryState& entry) mutable {
            if(!session.world_user() || &session.world_user()->actor!=&actor || &session.admission()!=&admission ||
               &session.zone_entry()!=&entry || admission.zone!=&selected)
                throw std::logic_error("Movement factory received mismatched session owners");
            return VerticalMoveGameplayBinding<Session>::create(registry,session.shared_from_this(),selected,std::move(dependencies));
        },std::move(other));
}
}
