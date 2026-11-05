#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_scene.hpp"

namespace eternity::game {
// The world owner serializes zone transitions and keeps the admitted zone
// alive while its actor remains attached. Do not retain a raw zone after leave.
inline std::function<ZoneUsers*()> vertical_world_actor_zone(std::weak_ptr<VerticalWorldSession> session) {
    return [session=std::move(session)]() -> ZoneUsers* {
        auto owner=session.lock();if(!owner)throw std::logic_error("Scene lost its world session");
        auto* user=owner->world_user();if(!user || !user->actor.status().in_zone)return nullptr;
        auto* zone=owner->admission().zone;if(!zone || !zone->users)throw std::logic_error("Attached session lost its zone owner");
        return zone->users.get();
    };
}
// The supplied real scene owns no session; its zone, party, transport and
// effect services must outlive the retained session and its actor.
inline VerticalWorldActorScene vertical_world_actor_scene(std::shared_ptr<VerticalScene> scene) {
    if(!scene)throw std::invalid_argument("Missing vertical actor scene");
    return {scene,[scene](Actor& actor){scene->bind(actor);},[scene]{scene->unbind();}};
}
}
