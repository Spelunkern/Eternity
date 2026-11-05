#pragma once
#include "eternity/game/vertical_move_duel_world.hpp"
#include "eternity/game/vertical_move_retained_pvp.hpp"
#include "eternity/game/vertical_move_shop_shared.hpp"
#include "eternity/game/vertical_move_retained_world.hpp"

namespace eternity::game {
// Composes the existing world owners; it does not own session/zone state, pool,
// guild/duel backends or queue scheduling. All supplied references and callback
// results survive the object and are serialized on the owning world executor.
template<class Session> class VerticalMoveComposedWorld final {
    using Retained=VerticalMoveRetainedWorld<Session>;
    static ZoneUsers& users(RuntimeZone& zone) {
        if(!zone.users)throw std::invalid_argument("Movement requires retained zone users");return *zone.users;
    }
    static net::WorldChannel channel(Session& owner,const typename VerticalMoveRetainedSession<Session>::Channel& accessor) {
        if(!accessor)throw std::invalid_argument("Movement requires a session channel");return accessor(owner);
    }
public:
    VerticalMoveComposedWorld(VerticalSessionRegistry<Session>& registry,Session& owner,Actor& actor,RuntimeZone& zone,
        WorldAdmissionUser& admission,ZoneEntryState& entry,VerticalMotionState& motion,UserPriorityViewState& cadence,
        VerticalNpcInteraction& npc,const VerticalDuelMovement& duel,const VerticalGuildDuelMovement& guild_duel,
        DuelWorld& duel_backend,VerticalGuildDuelMovementServices& guild_services,
        typename VerticalMoveRetainedInteractions<Session>::GuildRefresh guild_refresh,
        typename VerticalMoveRetainedPvp<Session>::LogBytes log_bytes,UserViewSharedBuffers& buffers,
        typename VerticalMovePriorityRetained<Session>::Connection connection,typename VerticalMovePriorityRetained<Session>::Queue queue,
        typename VerticalMoveShopShared<Session>::Shop shop_of,VerticalDatabaseAckUser& ack,ZoneLeaveShopState& shop,
        const std::array<float,5>& speeds,VerticalMoveBoundOperations::Clock clock,
        typename VerticalMoveRetainedSession<Session>::Entry entry_of,typename VerticalMoveRetainedSession<Session>::Channel channel_of,
        typename VerticalMoveRetainedSession<Session>::StackBytes stack_bytes)
        :duel_world_(users(zone),registry,duel_backend),pvp_(registry,duel_world_,guild_duel,guild_services,std::move(log_bytes)),
         shop_services_(actor,zone,registry,buffers,connection,shop_of),
         movement_(registry,owner,actor,zone,admission,entry,motion,cadence,npc,duel,pvp_,std::move(guild_refresh),buffers,
             std::move(connection),std::move(queue),VerticalMoveBoundInputs{ack,shop,shop_services_,speeds,std::move(clock),channel(owner,channel_of)},
             std::move(entry_of),channel_of,std::move(stack_bytes)) {
        if(!shop_of || &shop_of(owner)!=&shop)throw std::invalid_argument("Movement shop belongs to another session");
    }
    std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet) {return movement_.receive(packet);}
    VerticalMoveComposedWorld(const VerticalMoveComposedWorld&)=delete;
    VerticalMoveComposedWorld& operator=(const VerticalMoveComposedWorld&)=delete;
private:
    VerticalMoveDuelWorld<Session> duel_world_;VerticalMoveRetainedPvp<Session> pvp_;
    VerticalMoveShopShared<Session> shop_services_;Retained movement_;
};
}
