#pragma once
#include "eternity/game/zone_actor_leave.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"
#include "eternity/game/zone_send_cell_view.hpp"
#include "eternity/game/zone_leave_requests.hpp"

namespace eternity::game {
// The world owner retains these per-actor states and scene/session leases; the
// supplied transport must refer to the same live zone and actual connections.
class ZoneLeaveBoundOwners {
public:
    virtual ~ZoneLeaveBoundOwners()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual ZoneLeaveShopState& shop(Actor&)=0;
    virtual std::uint32_t& exchange_timer(Actor&)=0;
};
// Binds the recovered NPC/shop/exchange prefix to canonical ZoneUsers lookup
// and shared view transport. Later LeaveZone branches remain the caller's job.
class ZoneLeaveBound final:public ZoneLeaveInteractionServices,public ZoneShopShutdownServices,public ZoneLeaveRequestServices {
public:
    ZoneLeaveBound(RuntimeZone& zone,WorldUser& user,ZoneEntryState& entry,
        ZoneLeaveBoundOwners& owners,UserViewActorTransport& view)
        :zone_(zone),user_(user),entry_(entry),owners_(owners),view_(view) {
        if(!zone_.map || !zone_.users || user_.admission.zone!=&zone_ || entry_.character_id!=user_.actor.id())
            throw std::invalid_argument("Zone leave binding requires canonical zone and user");
    }
    void prefix(VerticalNpcInteraction& npc) {zone_leave_interactions(user_.actor,npc,owners_.shop(user_.actor),*this);}
    void requests(PartyRequestState& party,ZoneLeaveRequestState& state) {zone_leave_requests(user_.actor,party,state,*this);}
    Actor* find_in_zone(Actor& requester,std::uint32_t id) override {validate(requester);return zone_.users->find_actor(id);}
    Actor* find_customer(Actor& requester,std::uint32_t id) override {return find_in_zone(requester,id);}
    void shutdown_shop(Actor& actor,ZoneLeaveShopState& state) override {validate(actor);zone_shutdown_shop(actor,state,*this);}
    std::shared_ptr<void> retain(Actor& actor) override{return owners_.retain(actor);}
    ZoneLeaveShopState& shop_state(Actor& actor) override{return owners_.shop(actor);}
    std::uint32_t& exchange_timer(Actor& actor) override{return owners_.exchange_timer(actor);}
    std::optional<CellCoordinates> cell(Actor& actor) override {validate(actor);return user_.admission.zone?std::optional{entry_.cell}:std::nullopt;}
    void send_view(CellCoordinates cell,const net::Packet& packet) override {
        const auto geometry=zone_.map->definition.geometry();zone_send_cell_view(geometry.cell_count(),geometry.cell_count(),cell,packet,view_);
    }
private:
    void validate(const Actor& actor) const {if(&actor!=&user_.actor || user_.admission.zone!=&zone_)throw std::logic_error("Zone leave subject mismatch");}
    RuntimeZone& zone_;WorldUser& user_;ZoneEntryState& entry_;ZoneLeaveBoundOwners& owners_;UserViewActorTransport& view_;
};
}
