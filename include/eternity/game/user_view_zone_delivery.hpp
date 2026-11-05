#pragma once
#include "eternity/game/zones.hpp"
#include "eternity/game/user_view_snapshot_transport.hpp"
#include <stdexcept>

namespace eternity::game {
// Binds both sides of movement visibility to one retained RuntimeZone. The
// serialized world owner must retain the zone, registries and supplied services
// throughout publication, and must prevent their replacement while this exists.
// Recipient services provide real connection retention and delivery; snapshots
// send to the moving user's connection through the supplied snapshot services.
class UserViewZoneDelivery final {
    static RuntimeZone& ready(RuntimeZone& zone) {
        if(!zone.map || !zone.users || !zone.corpses || !zone.mobs || !zone.npcs ||
           !zone.items || !zone.environment)
            throw std::invalid_argument("User view delivery requires a complete retained zone");
        return zone;
    }
public:
    UserViewZoneDelivery(RuntimeZone& zone,UserViewCellServices& recipients,UserCellSnapshotServices& viewer)
        :zone_(ready(zone)),snapshot_(*zone_.users,*zone_.corpses,*zone_.mobs,*zone_.npcs,
            *zone_.items,*zone_.environment,viewer),bridge_(recipients,snapshot_),transport_(*zone_.users,bridge_) {}
    UserViewZoneDelivery(const UserViewZoneDelivery&)=delete;
    UserViewZoneDelivery& operator=(const UserViewZoneDelivery&)=delete;
    UserViewActorTransport& transport() noexcept {return transport_;}
private:
    RuntimeZone& zone_;
    CellSnapshot snapshot_;
    UserViewSnapshotTransport bridge_;
    UserViewCellTransport transport_;
};
}
