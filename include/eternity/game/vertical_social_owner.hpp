#pragma once
#include "eternity/game/social_friend_request.hpp"
#include "eternity/game/zone_leave_requests.hpp"

namespace eternity::game {
// Bind the invitation fields already retained by the zone-leave owner. The
// returned view and state must outlive all serialized social callbacks.
inline SocialFriendUser bind_social_friend_user(SocialCatalog& catalog,ZoneLeaveRequestState& requests,
    std::uint32_t list_id,std::uint32_t wire_id,std::uint8_t country,std::uint8_t family,
    std::uint8_t grow,std::uint8_t job,std::string name) {
    return {catalog,list_id,wire_id,country,family,grow,job,std::move(name),requests.request_5478,requests.deadline_583c};
}
}
