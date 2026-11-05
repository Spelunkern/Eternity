#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include "eternity/game/world_lifecycle.hpp"

namespace eternity::game {
// CUser::LeaveWorld 0044aad0 invokes CUser::ReleaseFriend 004757a0.
// The retained catalog, lookup targets and transports outlive these ports;
// the world executor serializes the callback and must retain peers while sending.
inline WorldLifecyclePorts bind_social_world_release(WorldLifecyclePorts ports,const SocialCatalog& social,
    SocialCatalog::Lookup lookup,std::function<void(std::uint32_t,net::Packet)> send_peer) {
    if(!ports.step || !lookup || !send_peer)throw std::invalid_argument("Missing social world release services");
    auto next=std::move(ports.step);
    ports.step=[&social,lookup=std::move(lookup),send_peer=std::move(send_peer),next=std::move(next)](WorldLifecycleStep step) {
        if(step==WorldLifecycleStep::release_friends)social.notify_offline(lookup,send_peer);
        else next(step);
    };
    return ports;
}
}
