#pragma once
#include "eternity/game/actor_load.hpp"
#include "eternity/game/vertical_persistence.hpp"
#include "eternity/game/user_quick_slots.hpp"
#include <stdexcept>
#include <utility>

namespace eternity::game {
// One stable per-session owner for the SQL snapshot, DB-agent replica and
// actor-loading pipeline. Rules, scene and their pools must outlive this owner;
// the actor must be released only to an owner retaining the same dependencies.
// This is an ownership component, not a world session or transport.
class VerticalSessionCharacter final {
public:
    VerticalSessionCharacter(VerticalPersistenceSelection selection,const ActorLoadRules& rules,
        ActorLoadOptions options,ActorScene& scene)
        : selection_(validate(std::move(selection))),quick_slots_(selection_.snapshot.quick_slots),pipeline_(selection_.snapshot,rules,options,scene) {}
    VerticalSessionCharacter(const VerticalSessionCharacter&) = delete;
    VerticalSessionCharacter& operator=(const VerticalSessionCharacter&) = delete;
    VerticalSessionCharacter(VerticalSessionCharacter&&) = delete;
    VerticalSessionCharacter& operator=(VerticalSessionCharacter&&) = delete;
    const PlayerSnapshot& snapshot() const noexcept {return selection_.snapshot;}
    CharacterReplica& replica() noexcept {return *selection_.replica;}
    UserQuickSlots& quick_slots() noexcept {return quick_slots_;}
    ActorLoadPipeline& pipeline() noexcept {return pipeline_;}
private:
    static VerticalPersistenceSelection validate(VerticalPersistenceSelection selection) {
        if(!selection.replica) throw std::invalid_argument("Session character requires a replica");
        const auto core=selection.replica->core();
        if(core.user_id!=selection.snapshot.core.user_id || core.character.id!=selection.snapshot.core.character.id)
            throw std::invalid_argument("Session character replica ownership mismatch");
        return selection;
    }
    VerticalPersistenceSelection selection_;
    UserQuickSlots quick_slots_;
    ActorLoadPipeline pipeline_;
};
}
