#pragma once
#include "eternity/game/vertical_world_inventory_pickup.hpp"
#include "eternity/game/vertical_world_inventory_party.hpp"
#include "eternity/game/vertical_world_inventory_exchange.hpp"
#include "eternity/game/vertical_world_session_runtime.hpp"
#include "eternity/game/vertical_world_inventory_settlement.hpp"

namespace eternity::game {
// Use inside the runtime's gameplay factory. All referenced owners are the
// actual zone/session services and survive the returned gameplay owner. The
// router must be the sole long-lived gameplay owner and destroy it on the world
// strand before zone or save teardown, including on zone invalidation.
template<class Resource,class Store,class MovementFactory>
std::shared_ptr<VerticalWorldSessionGameplay> vertical_world_inventory_gameplay(
    VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
    VerticalWorldSessionRuntime<Resource,Store>& runtime,std::shared_ptr<VerticalWorldSession> session,
    RuntimeZone& zone,MovementFactory movement,std::shared_ptr<ItemPickupWorld> services,
    std::shared_ptr<VerticalInventoryActions> actions,std::shared_ptr<const ItemQuestLinks> links,
    std::shared_ptr<GroundItemPool> pool,VerticalWorldSessionGameplay::Other next) {
    if(!session)throw std::invalid_argument("Missing inventory gameplay session");
    auto binding=runtime.save_binding(*session);
    if(!binding)throw std::logic_error("Inventory gameplay has no session save binding");
    using Pickup=VerticalWorldInventoryPickup<Store>;
    auto pickup=Pickup::create(strand,registry,session,zone,std::move(binding),std::move(services),
        std::move(actions),std::move(links),std::move(pool),std::move(next));
    struct Lifetime {
        std::shared_ptr<Pickup> pickup;
        explicit Lifetime(std::shared_ptr<Pickup> value):pickup(std::move(value)) {}
        ~Lifetime() noexcept {try {pickup->stop();}catch(...) {std::terminate();}}
    };
    auto lifetime=std::make_shared<Lifetime>(std::move(pickup));
    VerticalWorldSessionGameplay::Other other=[lifetime](VerticalWorldSession& owner,const net::Packet& packet) {
        lifetime->pickup->dispatch(owner,packet);
    };
    return VerticalWorldSessionGameplay::create_retained(std::move(strand),registry,session,zone,session->zone_entry(),std::move(movement),std::move(other));
}
// Composes the retained party/lock/transport owner into the same gameplay
// lifetime as pickup. Its shared party runtime cannot shut down until gameplay
// and its callbacks and captured locks have drained on the world strand.
template<class Resource,class Store,class MovementFactory>
std::shared_ptr<VerticalWorldSessionGameplay> vertical_world_inventory_party_gameplay(
    VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
    VerticalWorldSessionRuntime<Resource,Store>& runtime,std::shared_ptr<VerticalWorldSession> session,
    RuntimeZone& zone,std::shared_ptr<VerticalWorldPartyRuntime> parties,MovementFactory movement,
    std::shared_ptr<ItemPickupWorld> services,std::shared_ptr<VerticalInventoryActions> actions,
    std::shared_ptr<const ItemQuestLinks> links,std::shared_ptr<GroundItemPool> pool,
    VerticalWorldSessionGameplay::Other next) {
    auto party=std::make_shared<VerticalWorldInventoryParty>(strand,registry,session,std::move(parties),std::move(services));
    return vertical_world_inventory_gameplay(std::move(strand),registry,runtime,std::move(session),zone,
        std::move(movement),std::move(party),std::move(actions),std::move(links),std::move(pool),std::move(next));
}

// Adds retained exchange invitation and response routing to the pickup chain.
// Ready uses the supplied settlement owner, or remains with the supplied next.
template<class Resource,class Store,class MovementFactory>
std::shared_ptr<VerticalWorldSessionGameplay> vertical_world_inventory_party_exchange_gameplay(
    VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
    VerticalWorldSessionRuntime<Resource,Store>& runtime,std::shared_ptr<VerticalWorldSession> session,
    RuntimeZone& zone,std::shared_ptr<VerticalWorldPartyRuntime> parties,MovementFactory movement,
    std::shared_ptr<ItemPickupWorld> services,std::shared_ptr<VerticalInventoryActions> actions,
    std::shared_ptr<const ItemQuestLinks> links,std::shared_ptr<GroundItemPool> pool,
    VerticalWorldSessionGameplay::Other next,std::weak_ptr<VerticalWorldInventoryExchange>* expiry_owner=nullptr,
    std::shared_ptr<VerticalExchangeSettlement> settlement={}) {
    if(expiry_owner)expiry_owner->reset();
    auto exchange=VerticalWorldInventoryExchange::create(strand,registry,session,zone,actions,std::move(next),std::move(settlement));
    auto gameplay=vertical_world_inventory_party_gameplay(std::move(strand),registry,runtime,std::move(session),zone,
        std::move(parties),std::move(movement),std::move(services),std::move(actions),std::move(links),std::move(pool),
        VerticalWorldInventoryExchange::bind(exchange));
    if(expiry_owner)*expiry_owner=exchange;
    return gameplay;
}

// Complete retained routing composition. All provided services must be the
// actual shared owners; failures propagate and may leave partial effects.
template<class Resource,class Store,class MovementFactory>
std::shared_ptr<VerticalWorldSessionGameplay> vertical_world_inventory_party_settlement_gameplay(
    VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
    VerticalWorldSessionRuntime<Resource,Store>& runtime,std::shared_ptr<VerticalWorldSession> session,
    RuntimeZone& zone,std::shared_ptr<VerticalWorldPartyRuntime> parties,MovementFactory movement,
    std::shared_ptr<ItemPickupWorld> pickup_services,std::shared_ptr<VerticalInventoryActions> actions,
    std::shared_ptr<const ItemQuestLinks> links,std::shared_ptr<GroundItemPool> pool,
    std::shared_ptr<ItemIds> ids,std::shared_ptr<ItemIdClock> clock,std::shared_ptr<InventoryWorld> settlement_services,
    std::shared_ptr<VerticalWorldInventorySettlementScratch> scratch,VerticalWorldSessionGameplay::Other next,
    std::weak_ptr<VerticalWorldInventoryExchange>* expiry_owner=nullptr) {
    if(expiry_owner)expiry_owner->reset();
    auto settlement=std::make_shared<VerticalWorldInventorySettlement<Resource,Store>>(strand,registry,runtime,session,zone,
        pool,std::move(ids),std::move(clock),std::move(settlement_services),std::move(scratch),vertical_world_inventory_connection_ipv4);
    return vertical_world_inventory_party_exchange_gameplay(std::move(strand),registry,runtime,std::move(session),zone,
        std::move(parties),std::move(movement),std::move(pickup_services),std::move(actions),std::move(links),std::move(pool),
        std::move(next),expiry_owner,std::move(settlement));
}

}
