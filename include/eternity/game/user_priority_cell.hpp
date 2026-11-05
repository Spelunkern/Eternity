#pragma once
#include "eternity/game/user_view_publication.hpp"
#include "eternity/game/zone_users.hpp"
#include "eternity/game/actor.hpp"
#include <cmath>

namespace eternity::game {
class UserPriorityCellServices {
public:
    virtual ~UserPriorityCellServices()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    // Returns current source XYZ; the original reads its live position for
    // each recipient, so callbacks may affect subsequent distance checks.
    virtual UserPosition origin()=0;
    virtual void immediate(Actor&,UserViewBuffer&)=0;
    // Provider implements original socket/2048 queue checks, refcount and
    // distance-derived ordering key in CUser::PSend 004bd6f0.
    virtual void priority(Actor&,UserViewBuffer&,std::uint8_t,float distance)=0;
};
// CCell::PSendRange 00424780; caller owns one retained shared packet buffer.
// Cell, origin owner and actors survive callbacks; retain pins the recipient.
template<class Predicate>
inline void user_priority_cell_filtered(ZoneUsers& zone,CellCoordinates cell,UserViewBuffer& buffer,
    std::uint32_t source,std::uint32_t extra,std::uint8_t priority,UserPriorityCellServices& services,Predicate&& within) {
    zone.broadcast_cell(cell.x,cell.z,[&](Actor& actor){return services.retain(actor);},[&](Actor& actor){
        if(actor.id()==source || actor.id()==extra) {services.immediate(actor,buffer);return;}
        const auto recipient=zone.position(actor.id());if(!recipient)throw std::logic_error("Recipient lost cell position");
        const auto origin=services.origin();
        const float dx=static_cast<float>(static_cast<double>(origin.x)-recipient->x);
        const float dz=static_cast<float>(static_cast<double>(origin.z)-recipient->z);
        const float distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
        if(within(distance))services.priority(actor,buffer,priority,distance);
    });
}
inline void user_priority_cell(ZoneUsers& zone,CellCoordinates cell,UserViewBuffer& buffer,
    float radius,std::uint32_t source,std::uint32_t extra,std::uint8_t priority,UserPriorityCellServices& services) {
    user_priority_cell_filtered(zone,cell,buffer,source,extra,priority,services,[&](float distance){return distance<=radius;});
}
// CCell::PSendRing 00424880 includes both boundaries and immediately sends
// both special IDs, including when they are outside the ring.
inline void user_priority_cell_ring(ZoneUsers& zone,CellCoordinates cell,UserViewBuffer& buffer,
    float inner,float outer,std::uint32_t source,std::uint32_t extra,std::uint8_t priority,UserPriorityCellServices& services) {
    user_priority_cell_filtered(zone,cell,buffer,source,extra,priority,services,[&](float distance){return distance>=inner && distance<=outer;});
}
}
