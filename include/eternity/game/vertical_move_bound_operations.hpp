#pragma once
#include "eternity/game/vertical_move_packet.hpp"
#include "eternity/game/vertical_move_status_stand.hpp"
#include "eternity/game/user_priority_zone_view.hpp"
#include "eternity/net/world_server.hpp"

namespace eternity::game {
// Connects movement gates to the actual admission, shop and connection owners.
// All references, the clock and the channel outlive this object and run on the
// serialized world executor. The speed table must be the shared live table.
class VerticalMoveBoundOperations final:public VerticalMoveWorldOperations {
public:
    using Clock=std::function<std::uint32_t()>;
    VerticalMoveBoundOperations(Actor& actor,ZoneEntryState& entry,VerticalDatabaseAckUser& connection,
        ZoneLeaveShopState& shop,ZoneShopShutdownServices& shop_services,UserPriorityZoneView& view,
        const std::array<float,5>& speeds,Clock clock,net::WorldChannel channel)
        :actor_(actor),entry_(entry),connection_(connection),shop_(shop),shop_services_(shop_services),view_(view),
         speeds_(speeds),clock_(std::move(clock)),channel_(std::move(channel)) {
        if(&connection_.user.actor!=&actor_ || connection_.user.admission.user_id!=actor_.account_id() ||
           !entry_.bound || entry_.character_id!=actor_.id() || !clock_ || !channel_.disconnect_with_reason)
            throw std::invalid_argument("Movement operations require retained actor and live providers");
    }
    void status_stand(Actor& actor) override {require_actor(actor);vertical_move_status_stand(actor,connection_,shop_,shop_services_);}
    std::uint16_t direction(const Actor& actor) override {require_actor(actor);return entry_.direction;}
    void send_view_action(Actor& actor,const net::Packet& packet) override {
        require_actor(actor);
        // CObjectConnection::PSendViewAction 004bd890, PacketPC 0046a32c:
        // radius 80, source ID immediate, extra ID zero, priority 4.
        if(view_.has_zone(actor))view_.send_near(actor,packet,80.0F,actor.id(),0,4);
    }
    std::uint32_t tick_ms() override {return clock_();}
    const std::array<float,5>& movement_speed_table() override {return speeds_;}
    void close(Actor& actor,std::uint32_t reason) override {require_actor(actor);channel_.disconnect_with_reason(reason);}
private:
    void require_actor(const Actor& actor) const {if(&actor!=&actor_)throw std::logic_error("Movement operation belongs to another actor");}
    Actor& actor_;ZoneEntryState& entry_;VerticalDatabaseAckUser& connection_;ZoneLeaveShopState& shop_;
    ZoneShopShutdownServices& shop_services_;UserPriorityZoneView& view_;const std::array<float,5>& speeds_;Clock clock_;net::WorldChannel channel_;
};
}
