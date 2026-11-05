#pragma once
#include "eternity/game/vertical_move_composed_world.hpp"

namespace eternity::game {
// Every referenced owner is the canonical state used by the rest of the world.
// lease must retain those owners and the zone until the binding is destroyed;
// it must not create a session/registry ownership cycle. Access is serialized
// on the world executor, including teardown and queued buffer handling.
template<class Session> struct VerticalMoveGameplayDependencies {
    std::shared_ptr<void> lease;
    UserPriorityViewState& cadence;VerticalNpcInteraction& npc;const VerticalDuelMovement& duel;
    const VerticalGuildDuelMovement& guild_duel;DuelWorld& duel_world;VerticalGuildDuelMovementServices& guild_services;
    typename VerticalMoveRetainedInteractions<Session>::GuildRefresh guild_refresh;
    typename VerticalMoveRetainedPvp<Session>::LogBytes log_bytes;UserViewSharedBuffers& buffers;
    typename VerticalMovePriorityRetained<Session>::Connection connection;
    typename VerticalMovePriorityRetained<Session>::Queue queue;
    typename VerticalMoveShopShared<Session>::Shop shop_of;ZoneLeaveShopState& shop;
    const std::array<float,5>& speeds;VerticalMoveBoundOperations::Clock clock;
    typename VerticalMoveRetainedSession<Session>::StackBytes stack_bytes;
};

// Session exposes the coordinator's world_user(), admission(), zone_entry(),
// motion(), database_ack_user(), channel() and closing() accessors. The result
// can be retained by the gameplay dispatcher; it holds only a weak session
// reference, avoiding a session/dispatcher cycle. The source is pinned for
// each call, and must be stopped before its world owners are removed.
template<class Session> class VerticalMoveGameplayBinding final {
public:
    static std::shared_ptr<VerticalMoveGameplayBinding> create(VerticalSessionRegistry<Session>& registry,
        const std::shared_ptr<Session>& session,RuntimeZone& zone,VerticalMoveGameplayDependencies<Session> deps) {
        if(!session || !deps.lease || session->closing() || !session->world_user() || !session->database_ack_user())
            throw std::invalid_argument("Movement gameplay requires a retained live session and owners");
        auto& actor=session->world_user()->actor;auto& admission=session->admission();auto& entry=session->zone_entry();
        if(&session->world_user()->admission!=&admission || admission.zone!=&zone || admission.location!=4 ||
           !zone.users || zone.users->find_actor(actor.id())!=&actor || registry.retain_actor(actor)!=session ||
           !entry.bound || entry.character_id!=actor.id() || &session->database_ack_user()->user!=session->world_user())
            throw std::logic_error("Movement gameplay lost its admitted session");
        return std::shared_ptr<VerticalMoveGameplayBinding>(new VerticalMoveGameplayBinding(registry,session,actor,zone,std::move(deps)));
    }
    std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet) {
        auto owner=owner_.lock();
        if(!owner || owner->closing() || owner->world_user()!=world_user_ || owner->database_ack_user()!=ack_ ||
           &owner->admission()!=admission_ || &owner->zone_entry()!=entry_ || &owner->motion()!=motion_ ||
           !world_user_ || &world_user_->actor!=actor_ || &world_user_->admission!=admission_ ||
           admission_->zone!=&zone_ || admission_->location!=4 || !zone_.users || zone_.users->find_actor(actor_->id())!=actor_ ||
           registry_.retain_actor(*actor_)!=owner)
            throw std::logic_error("Movement gameplay session is no longer admitted");
        return movement_.receive(packet);
    }
    VerticalMoveGameplayBinding(const VerticalMoveGameplayBinding&)=delete;
    VerticalMoveGameplayBinding& operator=(const VerticalMoveGameplayBinding&)=delete;
private:
    VerticalMoveGameplayBinding(VerticalSessionRegistry<Session>& registry,const std::shared_ptr<Session>& owner,
        Actor& actor,RuntimeZone& zone,VerticalMoveGameplayDependencies<Session> deps)
        :registry_(registry),owner_(owner),actor_(&actor),zone_(zone),world_user_(owner->world_user()),ack_(owner->database_ack_user()),
         admission_(&owner->admission()),entry_(&owner->zone_entry()),motion_(&owner->motion()),lease_(std::move(deps.lease)),
         movement_(registry,*owner,actor,zone,owner->admission(),owner->zone_entry(),owner->motion(),deps.cadence,deps.npc,
            deps.duel,deps.guild_duel,deps.duel_world,deps.guild_services,std::move(deps.guild_refresh),std::move(deps.log_bytes),
            deps.buffers,std::move(deps.connection),std::move(deps.queue),std::move(deps.shop_of),*owner->database_ack_user(),deps.shop,
            deps.speeds,std::move(deps.clock),[](Session& session)->ZoneEntryState&{return session.zone_entry();},
            [](Session& session)->net::WorldChannel&{return session.channel();},std::move(deps.stack_bytes)) {}
    VerticalSessionRegistry<Session>& registry_;std::weak_ptr<Session> owner_;Actor* actor_;RuntimeZone& zone_;
    WorldUser* world_user_;VerticalDatabaseAckUser* ack_;WorldAdmissionUser* admission_;ZoneEntryState* entry_;VerticalMotionState* motion_;std::shared_ptr<void> lease_;
    VerticalMoveComposedWorld<Session> movement_;
};
}
