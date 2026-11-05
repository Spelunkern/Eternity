#pragma once
#include "eternity/game/world_users.hpp"
#include <mutex>

namespace eternity::game {
enum class VerticalDisconnectResult {deferred,save_requested,finalized};
class VerticalDisconnectServices {
public:
    virtual ~VerticalDisconnectServices()=default;
    virtual std::recursive_mutex& user_mutex(WorldUser&)=0;
    virtual std::recursive_mutex& zone_mutex(RuntimeZone&)=0;
    virtual void dead_disconnect(WorldUser&)=0; // 00459380.
    virtual std::uint32_t tick_ms()=0;
    virtual void prepare_save(WorldUser&)=0; // 0046c980.
    virtual void request_save(WorldUser&)=0; // 0046c660.
    // Full original CZone::LeaveUser 004161b0, including cell broadcasts and
    // all lifecycle effects; not merely ZoneUsers::detach.
    virtual void leave_zone(WorldUser&,RuntimeZone&)=0;
    // Remaining 0040cbd0.. tail, including logging and user ownership.
    virtual void finish_final(WorldUser&)=0;
};
// CWorld::LeaveUser 0040ca20, location-4 branch. final_save corresponds to
// CUser+57bc. Calls are serialized; world, user, zone and services survive
// callbacks. The caller ensures this path has not already finalized.
inline VerticalDisconnectResult vertical_disconnect_in_zone(WorldUsers& users,WorldUser& user,
    bool final_save,VerticalDisconnectServices& services) {
    if(user.admission.location!=4 || users.find(user.actor.id())!=&user)
        throw std::logic_error("Disconnect requires retained in-world user");
    std::unique_lock lock(services.user_mutex(user));
    if(user.actor.attack_state().mode==1)services.dead_disconnect(user);
    if(!final_save) {
        if(user.admission.logout_type==0) {
            user.admission.logout_type=2;user.admission.logout_tick=services.tick_ms()+10000U;
            return VerticalDisconnectResult::deferred;
        }
        services.prepare_save(user);services.request_save(user);return VerticalDisconnectResult::save_requested;
    }
    users.leave(user);
    if(auto* zone=user.admission.zone) {
        lock.unlock();std::unique_lock zone_lock(services.zone_mutex(*zone));lock.lock();
        if(auto* current=user.admission.zone)services.leave_zone(user,*current);
    }
    services.finish_final(user);return VerticalDisconnectResult::finalized;
}
}
