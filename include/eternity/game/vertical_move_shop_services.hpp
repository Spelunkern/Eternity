#pragma once
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"
#include "eternity/game/zone_send_cell_view.hpp"
#include "eternity/game/zones.hpp"

namespace eternity::game {
// The zone, user registry, actor/session owners, shop state and view transport
// are retained and serialized for the duration of shutdown and its callbacks.
// The view transport must use this zone's actual cells and connections.
template<class Session> class VerticalMoveShopServices final:public ZoneShopShutdownServices {
public:
    using Shop=std::function<ZoneLeaveShopState&(Session&)>;
    VerticalMoveShopServices(Actor& owner,RuntimeZone& zone,VerticalSessionRegistry<Session>& registry,
        UserViewActorTransport& transport,Shop shop)
        :owner_(owner),zone_(zone),registry_(registry),transport_(transport),shop_(std::move(shop)) {
        if(!zone_.map || !zone_.users || !shop_)throw std::invalid_argument("Shop shutdown requires retained zone and state");
    }
    Actor* find_customer(Actor& owner,std::uint32_t id) override {
        require_owner(owner);return zone_.users->find_actor(id);
    }
    std::shared_ptr<void> retain(Actor& actor) override {return pin(actor);}
    ZoneLeaveShopState& shop_state(Actor& actor) override {auto owner=pin(actor);return shop_(*owner);}
    std::optional<CellCoordinates> cell(Actor& owner) override {
        require_owner(owner);const auto cell=zone_.users->cell(owner.id());
        if(!cell)return {};return CellCoordinates{cell->x,cell->z};
    }
    void send_view(CellCoordinates cell,const net::Packet& packet) override {
        auto owner=pin(owner_);const auto count=zone_.map->definition.geometry().cell_count();
        zone_send_cell_view(count,count,cell,packet,transport_);
    }
private:
    std::shared_ptr<Session> pin(Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner)throw std::logic_error("Shop actor has no retained session");return owner;
    }
    void require_owner(Actor& actor) {
        if(&actor!=&owner_ || zone_.users->find_actor(actor.id())!=&actor)
            throw std::logic_error("Shop owner is not in its retained zone");
    }
    Actor& owner_;RuntimeZone& zone_;VerticalSessionRegistry<Session>& registry_;UserViewActorTransport& transport_;Shop shop_;
};
}
