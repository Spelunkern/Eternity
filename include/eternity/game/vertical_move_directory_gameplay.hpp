#pragma once
#include "eternity/game/vertical_move_gameplay_factory.hpp"
#include "eternity/game/vertical_move_session_directory.hpp"

namespace eternity::game {
// The world owner supplies actual services and a lease retaining zone, pool,
// speed table, callbacks and every referenced provider. Resources retain their
// own lease; neither lease may own the session or directory. All work is on
// the same serialized world executor.
template<class Session> struct VerticalMoveDirectoryServices {
    std::shared_ptr<void> lease;
    DuelWorld& duel_world;VerticalGuildDuelMovementServices& guild_services;
    typename VerticalMoveRetainedInteractions<Session>::GuildRefresh guild_refresh;
    typename VerticalMoveRetainedPvp<Session>::LogBytes log_bytes;
    UserViewSharedBuffers& buffers;
    typename VerticalMovePriorityRetained<Session>::Connection connection;
    typename VerticalMovePriorityRetained<Session>::Queue queue;
    typename VerticalMoveShopShared<Session>::Shop shop;
    const std::array<float,5>& speeds;VerticalMoveBoundOperations::Clock clock;
    typename VerticalMoveRetainedSession<Session>::StackBytes stack_bytes;
};

template<class Gameplay,class Strand,class Session>
std::shared_ptr<Gameplay> make_vertical_move_directory_gameplay(Strand strand,VerticalSessionRegistry<Session>& registry,
    VerticalMoveSessionDirectory<Session>& directory,const std::shared_ptr<Session>& owner,RuntimeZone& zone,
    VerticalMoveDirectoryServices<Session> services,typename Gameplay::Other other) {
    if(!owner || !owner->world_user() || !services.lease || !services.connection || !services.queue || !services.shop)
        throw std::invalid_argument("Movement directory gameplay requires live owners");
    auto resources=directory.find(owner->world_user()->actor);
    if(!resources || &resources->connection()!=&services.connection(*owner) ||
       &resources->queue()!=&services.queue(*owner) || &resources->shop()!=&services.shop(*owner))
        throw std::logic_error("Movement directory gameplay has mismatched session resources");
    struct Lease {std::shared_ptr<void> world;std::shared_ptr<VerticalMoveSessionResources> resources;};
    auto lease=std::make_shared<Lease>(Lease{std::move(services.lease),resources});
    VerticalMoveGameplayDependencies<Session> dependencies{std::move(lease),resources->cadence(),resources->npc(),resources->duel(),
        resources->guild_duel(),services.duel_world,services.guild_services,std::move(services.guild_refresh),std::move(services.log_bytes),
        services.buffers,std::move(services.connection),std::move(services.queue),std::move(services.shop),resources->shop(),services.speeds,
        std::move(services.clock),std::move(services.stack_bytes)};
    return make_vertical_move_gameplay<Gameplay>(std::move(strand),registry,owner,zone,std::move(dependencies),std::move(other));
}
}
