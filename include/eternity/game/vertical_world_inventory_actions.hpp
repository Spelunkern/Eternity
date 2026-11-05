#pragma once
#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_move_session_resources.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"

namespace eternity::game {
// Uses the exact movement/shop state and the actual shop shutdown service.
// The shop service and its zone/view owners survive the gameplay binding.
// Cancellation and the live pickup-check setting are required real owners.
class VerticalWorldInventoryActions final:public VerticalInventoryActions {
public:
    using Cancel=std::function<void(VerticalWorldSession&,Actor&)>;
    VerticalWorldInventoryActions(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& sessions,
        const std::shared_ptr<VerticalWorldSession>& session,RuntimeZone& zone,
        std::shared_ptr<VerticalMoveSessionResources> movement,std::shared_ptr<ZoneShopShutdownServices> shop,
        std::shared_ptr<const std::uint8_t> pickup_checks,Cancel cancel)
        :strand_(std::move(strand)),sessions_(sessions),session_(session),zone_(zone),movement_(std::move(movement)),
         shop_(std::move(shop)),pickup_checks_(std::move(pickup_checks)),cancel_(std::move(cancel)) {
        if(!strand_.running_in_this_thread() || !session || !movement_ || !shop_ || !pickup_checks_ || !cancel_)
            throw std::invalid_argument("Incomplete retained inventory action owners");
    }
    void cancel_action_exchange(Actor& actor) override {
        auto owner=checked(actor);(void)movement_->shop();cancel_(*owner,actor);checked(actor);(void)movement_->shop();
    }
    void end_shop(Actor& actor) override {
        auto owner=checked(actor);auto& state=movement_->shop();auto& actual=shop_->shop_state(actor);
        checked(actor);if(&actual!=&state)throw std::logic_error("Inventory shop owner differs from movement state");
        zone_shutdown_shop(actor,state,*shop_);checked(actor);
        if(&movement_->shop()!=&state)throw std::logic_error("Inventory shop state changed");
    }
    bool pickup_checks() override {
        auto owner=session_.lock();if(!owner || !owner->world_user())throw std::logic_error("Inventory actions lost session");
        checked(owner->world_user()->actor);(void)movement_->shop();return *pickup_checks_!=0;
    }
private:
    std::shared_ptr<VerticalWorldSession> checked(const Actor& actor) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Inventory actions outside world strand");
        auto owner=session_.lock();
        if(!owner || owner->closing() || !owner->world_user() || &owner->world_user()->actor!=&actor ||
           owner->admission().location!=4 || owner->admission().zone!=&zone_ || !zone_.users ||
           zone_.users->find_actor(actor.id())!=&actor || sessions_.retain_actor(actor)!=owner)
            throw std::logic_error("Inventory actions lost admitted actor");
        if(&movement_->connection()!=&owner->party_user())throw std::logic_error("Inventory actions have foreign movement connection");
        return owner;
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& sessions_;std::weak_ptr<VerticalWorldSession> session_;
    RuntimeZone& zone_;std::shared_ptr<VerticalMoveSessionResources> movement_;std::shared_ptr<ZoneShopShutdownServices> shop_;
    std::shared_ptr<const std::uint8_t> pickup_checks_;Cancel cancel_;
};
}
