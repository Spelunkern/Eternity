#pragma once
#include "eternity/game/world_users.hpp"
#include "eternity/game/zone_users.hpp"
#include <stdexcept>

namespace eternity::game {
// ps_game.exe CZone update at 004218ba..00421926. The caller serializes the
// zone/world and keeps both alive; retain pins the actor and its session through
// close_expired, which may detach the current user. lookup must not mutate the
// zone and returns the admission state belonging to that exact actor. The
// separate zone-waiting-list pass is WorldAdmission::update_zone_wait_logout_deadlines.
inline void zone_update_logout_deadlines(ZoneUsers& zone,std::uint32_t tick,
    const ZoneUsers::RetainActor& retain,
    const std::function<WorldAdmissionUser&(Actor&)>& lookup,
    const std::function<void(Actor&,WorldAdmissionUser&)>& close_expired) {
    if(!retain || !lookup || !close_expired) throw std::invalid_argument("Logout update requires retained users and services");
    zone.visit_all(retain,[&](Actor& actor) {
        auto& admission=lookup(actor);
        const auto deadline=admission.logout_tick;
        if(deadline!=0 && deadline<=tick) {
            admission.logout_tick=0;
            close_expired(actor,admission); // CWorld::LogoutUser 0040c860.
        }
    });
}
}
