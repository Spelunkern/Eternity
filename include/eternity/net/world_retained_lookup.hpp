#pragma once
#include "eternity/net/world_server.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>

namespace eternity::net {
// WorldRoot owns its registry by value and only exposes sessions() on the
// world strand. Keep the root alive and defer that access to the strand.
template<class WorldOwner>
RetainedWorldLookup retained_world_owner_lookup(asio::strand<asio::any_io_executor> world,
    std::shared_ptr<WorldOwner> owner) {
    if(!owner) throw std::invalid_argument("Retained lookup requires a world owner");
    return [world=std::move(world),owner=std::move(owner)](std::uint32_t account,std::function<void(std::optional<bool>)> done) {
        if(!done) throw std::invalid_argument("Retained lookup requires completion");
        asio::post(world,[owner,account,done=std::move(done)] {
            std::optional<bool> live;
            try {live=bool(owner->sessions().find_account(account));} catch(...) {}
            done(live);
        });
    };
}
// The registry and its objects live on this world strand. Register each owner
// before allowing replacement and retain it until all account work drains.
// The result is a snapshot, not an atomic reservation. The world executor must
// stay alive until submitted lookups complete or their sockets close.
template<class Session>
RetainedWorldLookup retained_world_registry_lookup(asio::strand<asio::any_io_executor> world,
    std::shared_ptr<game::VerticalSessionRegistry<Session>> owners) {
    if(!owners) throw std::invalid_argument("Retained lookup requires a world session registry");
    return [world=std::move(world),owners=std::move(owners)](std::uint32_t account,std::function<void(std::optional<bool>)> done) {
        if(!done) throw std::invalid_argument("Retained lookup requires completion");
        asio::post(world,[owners,account,done=std::move(done)] {
            std::optional<bool> live;
            try {live=bool(owners->find_account(account));} catch(...) {}
            done(live);
        });
    };
}
}
