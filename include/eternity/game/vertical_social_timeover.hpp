#pragma once
#include "eternity/game/vertical_world_session_runtime.hpp"

namespace eternity::game {
// Compose this as PrimaryEffectServices::update_timeovers. The real prefix
// must execute CUser::UpdateTimeover through 0048e3be (exchange and party);
// the real suffix starts at 0048e402 (vehicle and later timers). All run on
// the world strand, and the scheduler/registry/zone outlive this synchronous call.
template<class Resource,class Store>
void vertical_social_timeover(VerticalWorldSessionRuntime<Resource,Store>& runtime,
    VerticalWorldSession::Registry& registry,RuntimeZone& zone,Actor& actor,std::uint32_t tick,
    const std::function<void(VerticalWorldSession&,Actor&,std::uint32_t)>& prefix,
    const std::function<void(VerticalWorldSession&,Actor&,std::uint32_t)>& suffix) {
    if(!prefix || !suffix)throw std::invalid_argument("Missing non-social timeout phase");
    auto owner=registry.retain_actor(actor);
    auto valid=[&] {
        return owner && registry.retain_actor(actor)==owner && owner->world_user() &&
            &owner->world_user()->actor==&actor && owner->admission().location==4 &&
            owner->admission().zone==&zone && !owner->closing();
    };
    if(!valid())throw std::logic_error("Social timeout lost zone session");
    prefix(*owner,actor,tick);
    // Prefix callbacks can reenter disconnect or move the user to another zone.
    if(!valid())throw std::logic_error("Social timeout owner changed during prefix");
    runtime.update_social_timeout(*owner,tick);
    if(!valid())throw std::logic_error("Social timeout owner changed during update");
    suffix(*owner,actor,tick);
}
}
