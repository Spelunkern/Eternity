#pragma once
#include "eternity/game/user_priority_view.hpp"
#include "eternity/game/user_priority_area.hpp"

namespace eternity::game {
// Binds PSendView to one actual zone and its required live recipient services.
// The origin service must resolve this same source actor's current position.
// Actor/zone/connection owners outlive all callbacks; one world executor
// serializes them and preserves original buffer and queue contracts.
class UserPriorityZoneView final:public UserPriorityViewServices {
public:
    UserPriorityZoneView(ZoneUsers& zone,std::int32_t width,std::int32_t height,UserPriorityAreaServices& services)
        :zone_(zone),width_(width),height_(height),services_(services) {
        if(width<=0 || height<=0)throw std::invalid_argument("Invalid priority zone dimensions");
    }
    bool has_zone(const Actor& actor) override{return zone_.find_actor(actor.id())==&actor;}
    void send_near(Actor& actor,const net::Packet& packet,float radius,std::uint32_t source,std::uint32_t extra,std::uint8_t priority) override {
        require_actor(actor);user_priority_area(zone_,width_,height_,packet,0,radius,false,source,extra,priority,services_);
    }
    void send_ring(Actor& actor,const net::Packet& packet,float inner,float outer,std::uint32_t source,std::uint32_t extra,std::uint8_t priority) override {
        require_actor(actor);user_priority_area(zone_,width_,height_,packet,inner,outer,true,source,extra,priority,services_);
    }
private:
    void require_actor(Actor& actor) {if(!has_zone(actor))throw std::logic_error("Priority source left its retained zone");}
    ZoneUsers& zone_;std::int32_t width_,height_;UserPriorityAreaServices& services_;
};
}
