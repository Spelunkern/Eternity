#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/game_log.hpp"
#include <optional>
namespace eternity::game {
// Explicit live input projection for fields split across existing Actor,
// PlayerCore, resource and world admission owners. Not another user owner.
struct MobTargetingRebirthState {
    std::int32_t life=0,recovery=1;
    std::uint8_t motion=0;
    std::uint32_t id=0,experience=0;
    std::uint16_t map=0,instance=0;
    UserPosition position;
    PlayerResources current{},maximum{};
    bool in_zone=false;
};
class MobTargetingRebirthServices {
public:
    virtual ~MobTargetingRebirthServices()=default;
    virtual std::optional<std::uint32_t> find_dead(std::uint32_t user)=0;
    virtual void leave_dead(std::uint32_t dead)=0;
    virtual bool move_user(std::uint16_t instance,const UserPosition&)=0;
    // Original signed target < map_count, then computed map pointer != null.
    // Preserve that contract; do not silently turn an absent provider into true.
    virtual bool map_present(std::int32_t target)=0;
    virtual void zone_leave_user_move(std::int32_t target,const UserPosition&)=0;
    virtual void send_cell_view(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    virtual void prepare_game_log(net::Packet& header)=0;
    virtual void send_game_log(const net::Packet&)=0;
    virtual void send_recover_change(PlayerResources)=0; // HP/MP/SP, not healing.
};
// ps_game.exe CUser::Rebirth 00459ec0, complete coordination. No success result:
// admitted life==1 publishes life=0 even without corpse or after MoveUser false.
// Callbacks may reenter/mutate live state; all storage must survive them.
void mob_targeting_rebirth(MobTargetingRebirthState&,const UserPosition& destination,
                          std::int32_t kind,std::int32_t map,bool restore,
                          MobTargetingRebirthServices&);
// Required actual root owners; no success/no-op defaults. Actor publicly lacks
// silent exact resource replacement and raw location/admission access. These
// ports must delegate to those real owners, not an auxiliary resource cache.
// subject/instance/map_present are side-effect-free owner queries; action ports
// may reenter. replace_resources/presentation_motion setters are exact silent
// writes, without callbacks. They must publish or fail explicitly.
class MobTargetingLifecycleWorld {
public:
    virtual ~MobTargetingLifecycleWorld()=default;
    virtual GameLogSubject subject(const Actor&) const=0;
    virtual std::uint16_t instance(const Actor&) const=0;
    // CUser+1434 presentation motion, NOT scheduler attack motion +1448.
    // Canonical Actor currently conflates these; require the actual owner.
    virtual std::uint8_t presentation_motion(const Actor&) const=0;
    virtual void presentation_motion(Actor&,std::uint8_t)=0;
    virtual void replace_resources(Actor&,PlayerResources)=0; // No heal/clamp/send.
    virtual std::optional<std::uint32_t> find_dead(Actor&,std::uint32_t)=0;
    virtual void leave_dead(Actor&,std::uint32_t)=0;
    virtual bool move_user(Actor&,std::uint16_t,const UserPosition&)=0;
    virtual bool map_present(std::int32_t)=0;
    virtual void zone_leave_user_move(Actor&,std::int32_t,const UserPosition&)=0;
    virtual void send_cell_view(Actor&,const net::Packet&)=0;
    virtual void send_recover_change(Actor&,PlayerResources)=0;
    virtual std::chrono::sys_seconds local_calendar()=0;
    virtual void send_game_log(const net::Packet&)=0;
};
// Borrows existing Actor and world; retains no user/resource/list state.
// Same serialized executor and deferred subject destruction as targeting.
void mob_targeting_rebirth(Actor&,const UserPosition&,std::int32_t kind,
                          std::int32_t map,bool restore,MobTargetingLifecycleWorld&);
}
