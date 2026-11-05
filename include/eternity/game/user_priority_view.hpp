#pragma once
#include "eternity/game/actor.hpp"
#include <bit>

namespace eternity::game {
// CUser+f8, retained per user and shared by every invocation of PSendView.
struct UserPriorityViewState {std::uint32_t counter=0;};
class UserPriorityViewServices {
public:
    virtual ~UserPriorityViewServices()=default;
    virtual bool has_zone(const Actor&)=0;
    // Required actual 00421220 and 004213d0 sends, including shared-buffer
    // allocation, source/extra immediate IDs, radius and priority queues.
    virtual void send_near(Actor&,const net::Packet&,float radius,std::uint32_t source,std::uint32_t extra,std::uint8_t priority)=0;
    virtual void send_ring(Actor&,const net::Packet&,float inner,float outer,std::uint32_t source,std::uint32_t extra,std::uint8_t priority)=0;
};
// CUser::PSendView 004bd950. The result means the periodic wide pass ran,
// not whether buffer allocation or the near send succeeded. Zone, actor and
// packet survive callbacks and all uses of this counter share one executor.
inline bool user_priority_view(Actor& actor,UserPriorityViewState& state,const net::Packet& packet,
    std::uint32_t extra,std::uint8_t priority,UserPriorityViewServices& services) {
    if(!services.has_zone(actor))return false;
    services.send_near(actor,packet,80.0F,actor.id(),extra,priority);
    ++state.counter;
    if(std::bit_cast<std::int32_t>(state.counter)<4)return false;
    state.counter=0;
    services.send_ring(actor,packet,80.0F,120.0F,actor.id(),extra,priority);
    return true;
}
}
