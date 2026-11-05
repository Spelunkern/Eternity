#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/zone_logout_update.hpp"

namespace eternity::game {
class PrimaryLogoutServices {
public:
    virtual ~PrimaryLogoutServices()=default;
    // Pins the actual session through LogoutUser. Neither retention method may
    // change list membership; callbacks may unlink their current user.
    virtual std::shared_ptr<void> retain_waiting(WorldAdmissionUser&)=0;
    virtual void logout_waiting(WorldAdmissionUser&)=0;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    virtual WorldAdmissionUser& admission(Actor&)=0;
    virtual void logout_actor(Actor&,WorldAdmissionUser&)=0;
};
enum class PrimaryLogoutResult {skipped,completed,interrupted};
// CWorldDBThread::OnUpdate 00406e20 gates CWorld::UpdateLogout 00412e10 at
// 1000 ms; CZone::UpdateLogout 00421830 visits waiting users, then zone users.
// Invoke on the common serialized world executor only while world threads are
// active. This is a DB-thread pass, not part of CWorldThread::Update. The
// registry alternative includes pending and active instance zones.
class WorldPrimaryLogoutPhase final {
public:
    explicit WorldPrimaryLogoutPhase(std::shared_ptr<PrimaryLogoutServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary logout phase requires real session and logout owners");
    }
    PrimaryLogoutResult update(VerticalWorld& world,WorldAdmission& admission,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryLogoutResult::skipped;
        auto result=PrimaryLogoutResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!dispatch(current,admission,zone,now)) result=PrimaryLogoutResult::interrupted;
            });
        });
        if(result==PrimaryLogoutResult::completed) next_=now+1000U;
        return result;
    }
    template<class Root> PrimaryLogoutResult update_registry(Root& root,std::uint32_t now) {
        auto& world=root.world();if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryLogoutResult::skipped;
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            current.zones()->visit_logout_zones([&](RuntimeZone& zone){
                if(!dispatch(current,root.admission(),zone,now)) complete=false;
            });
        });
        if(!complete) return PrimaryLogoutResult::interrupted;
        next_=now+1000U;return PrimaryLogoutResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    bool dispatch(VerticalWorld& world,WorldAdmission& admission,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) return false;
        if(!zone.users) throw std::logic_error("Logout zone has no retained user owner");
        admission.update_zone_wait_logout_deadlines(zone,now,
            [&](WorldAdmissionUser& user){return services_->retain_waiting(user);},
            [&](WorldAdmissionUser& user){services_->logout_waiting(user);});
        if(!world.running()) return false;
        zone_update_logout_deadlines(*zone.users,now,
            [&](Actor& actor){return services_->retain_actor(actor);},
            [&](Actor& actor)->WorldAdmissionUser& {return services_->admission(actor);},
            [&](Actor& actor,WorldAdmissionUser& user){services_->logout_actor(actor,user);});
        return world.running();
    }
    std::shared_ptr<PrimaryLogoutServices> services_;std::uint32_t next_=0;
};
}
