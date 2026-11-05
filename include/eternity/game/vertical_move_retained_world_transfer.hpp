#pragma once
#include "eternity/game/vertical_move_zone_leases.hpp"
#include "eternity/game/vertical_move_zone_transfer_world.hpp"

namespace eternity::game {
// The world owner owns this operation until completion or disconnect recovery.
// Its dependencies and both actual zones stay retained through the wait.
class VerticalMoveRetainedWorldTransfer final {
public:
    using Transfer=VerticalMoveZoneTransfer<VerticalWorldSession>;
    VerticalMoveRetainedWorldTransfer(VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> owner,
        std::shared_ptr<VerticalMoveZoneLeases> zones,RuntimeZone& source,RuntimeZone& destination,
        std::shared_ptr<VerticalMoveSessionResources> resources,Transfer::Ports ports)
        :zones_(std::move(zones)),source_(source),destination_(destination),
         transfer_(registry,std::move(owner),zones_,std::move(resources),std::move(ports)) {
        if(!zones_)throw std::invalid_argument("Zone transfer requires actual zone leases");
        zones_->validate(source_,destination_);
    }
    void begin() {zones_->validate(source_,destination_);transfer_.begin(source_,destination_);}
    void complete() {zones_->validate(source_,destination_);transfer_.complete();}
private:
    std::shared_ptr<VerticalMoveZoneLeases> zones_;RuntimeZone& source_;RuntimeZone& destination_;Transfer transfer_;
};
}
