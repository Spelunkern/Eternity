#pragma once
#include "eternity/game/vertical_move_priority_retained.hpp"
#include "eternity/game/vertical_move_retained_interactions.hpp"
#include "eternity/game/vertical_move_retained_session.hpp"
#include "eternity/game/vertical_move_world_services.hpp"
#include "eternity/game/vertical_move_bound_operations.hpp"
#include <optional>

namespace eternity::game {
// The supplied operation owner performs actual status, correction, clock,
// speed-table and close work. Priority and interaction dispatch are bound to
// the retained world services here; no callback is assumed successful.
class VerticalMoveRetainedWorldServices final:public VerticalMoveWorldServices {
public:
    VerticalMoveRetainedWorldServices(UserPriorityViewState& cadence,UserPriorityZoneView& view,
        VerticalNpcInteraction& npc,VerticalMoveInteractionServices& interactions,VerticalMoveWorldOperations& operations)
        :VerticalMoveWorldServices(cadence,view,npc,interactions),operations_(operations) {}
    void status_stand(Actor& actor) override {operations_.status_stand(actor);}
    std::uint16_t direction(const Actor& actor) override {return operations_.direction(actor);}
    void send_view_action(Actor& actor,const net::Packet& packet) override {operations_.send_view_action(actor,packet);}
    std::uint32_t tick_ms() override {return operations_.tick_ms();}
    const std::array<float,5>& movement_speed_table() override {return operations_.movement_speed_table();}
    void close(Actor& actor,std::uint32_t reason) override {operations_.close(actor,reason);}
private:VerticalMoveWorldOperations& operations_;
};

// These references are the actual per-session and world owners; their lifetime
// and serialized mutation must cover the retained movement object.
struct VerticalMoveBoundInputs {
    VerticalDatabaseAckUser& connection;
    ZoneLeaveShopState& shop;
    ZoneShopShutdownServices& shop_services;
    const std::array<float,5>& speed_table;
    VerticalMoveBoundOperations::Clock clock;
    net::WorldChannel channel;
};

// One movement session and its priority/interaction adapters share the same
// retained actor and runtime zone. All dependencies outlive this object and
// are accessed exclusively on the serialized world executor.
template<class Session> class VerticalMoveRetainedWorld final {
    static ZoneUsers& users(RuntimeZone& zone) {
        if(!zone.map || !zone.users)throw std::invalid_argument("Movement requires a retained zone");return *zone.users;
    }
    static std::int32_t cells(RuntimeZone& zone) {
        if(!zone.map)throw std::invalid_argument("Movement requires a map");return zone.map->definition.geometry().cell_count();
    }
public:
    VerticalMoveRetainedWorld(VerticalSessionRegistry<Session>& registry,Session& owner,Actor& actor,RuntimeZone& zone,
        WorldAdmissionUser& user,ZoneEntryState& entry,VerticalMotionState& motion,UserPriorityViewState& cadence,
        VerticalNpcInteraction& npc,const VerticalDuelMovement& duel,VerticalMovePvpServices& pvp,
        typename VerticalMoveRetainedInteractions<Session>::GuildRefresh guild,UserViewSharedBuffers& buffers,
        typename VerticalMovePriorityRetained<Session>::Connection connection,typename VerticalMovePriorityRetained<Session>::Queue queue,
        VerticalMoveWorldOperations& operations,typename VerticalMoveRetainedSession<Session>::Entry entry_of,
        typename VerticalMoveRetainedSession<Session>::Channel channel_of,typename VerticalMoveRetainedSession<Session>::StackBytes bytes)
        :priority_(registry,actor,users(zone),cells(zone),cells(zone),buffers,connection,std::move(queue)),
         interactions_(registry,users(zone),duel,pvp,std::move(guild)),
         services_(cadence,priority_.view(),npc,interactions_,operations),
         movement_(registry,owner,actor,zone,user,entry,motion,services_,std::move(entry_of),std::move(channel_of),std::move(bytes),&buffers,std::move(connection)) {}
    // Construct the concrete operation owner after the shared priority view,
    // so correction and normal movement use the same retained zone traversal.
    VerticalMoveRetainedWorld(VerticalSessionRegistry<Session>& registry,Session& owner,Actor& actor,RuntimeZone& zone,
        WorldAdmissionUser& user,ZoneEntryState& entry,VerticalMotionState& motion,UserPriorityViewState& cadence,
        VerticalNpcInteraction& npc,const VerticalDuelMovement& duel,VerticalMovePvpServices& pvp,
        typename VerticalMoveRetainedInteractions<Session>::GuildRefresh guild,UserViewSharedBuffers& buffers,
        typename VerticalMovePriorityRetained<Session>::Connection connection,typename VerticalMovePriorityRetained<Session>::Queue queue,
        VerticalMoveBoundInputs inputs,typename VerticalMoveRetainedSession<Session>::Entry entry_of,
        typename VerticalMoveRetainedSession<Session>::Channel channel_of,typename VerticalMoveRetainedSession<Session>::StackBytes bytes)
        :priority_(registry,actor,users(zone),cells(zone),cells(zone),buffers,connection,std::move(queue)),
         interactions_(registry,users(zone),duel,pvp,std::move(guild)),
         bound_(std::in_place,actor,entry,inputs.connection,inputs.shop,inputs.shop_services,priority_.view(),
             inputs.speed_table,std::move(inputs.clock),std::move(inputs.channel)),
         services_(cadence,priority_.view(),npc,interactions_,*bound_),
         movement_(registry,owner,actor,zone,user,entry,motion,services_,std::move(entry_of),std::move(channel_of),std::move(bytes),&buffers,std::move(connection)) {
        if(&inputs.connection.user.admission!=&user)throw std::invalid_argument("Movement connection belongs to another admission");
    }
    std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet) {return movement_.receive(packet);}
    VerticalMoveRetainedWorld(const VerticalMoveRetainedWorld&)=delete;
    VerticalMoveRetainedWorld& operator=(const VerticalMoveRetainedWorld&)=delete;
private:
    VerticalMovePriorityRetained<Session> priority_;
    VerticalMoveRetainedInteractions<Session> interactions_;
    std::optional<VerticalMoveBoundOperations> bound_;
    VerticalMoveRetainedWorldServices services_;
    VerticalMoveRetainedSession<Session> movement_;
};
}
