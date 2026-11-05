#pragma once
#include "eternity/game/vertical_move_transfer_directory.hpp"
#include <exception>

namespace eternity::game {
// Native continuation after the real admission owner publishes location 4.
// Failure leaves the admitted actor and transfer retained for real disconnect.
inline void vertical_move_transfer_zone_entered(VerticalWorldSession::Registry& registry,
    VerticalMoveTransferDirectory& transfers,WorldAdmissionUser& user,RuntimeZone& zone) {
    auto owner=registry.find_account(user.user_id);
    if(!owner || &owner->admission()!=&user || user.location!=4 || user.zone!=&zone)
        throw std::logic_error("Zone entry observer lost its registered session");
    if(!transfers.contains(*owner))return;
    try {transfers.complete(*owner);}
    catch(...) {auto error=std::current_exception();owner->report_dispatch_failure(error);std::rethrow_exception(error);}
}
}
