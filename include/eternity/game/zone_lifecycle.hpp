#pragma once
#include "eternity/game/zone_users.hpp"
#include <array>
#include "eternity/net/framing.hpp"
#include <functional>

namespace eternity::game {
struct ZoneMovementCheck {
    std::uint32_t back_time=0,check_time=0,previous_time=0;
    UserPosition back,previous;
    float total_distance=0,maximum_speed=0;
};
struct ZoneLifecycleState {
    std::uint8_t motion=0; // CUser+1434, also used by SendUserShape.
    std::uint32_t target_kind=0;
    UserPosition position;
    ZoneMovementCheck movement;
};
struct NProtectState {
    bool present=false,received=false;
    std::uint32_t receive_time=0;
};
struct NProtectPorts {
    // The external CCSAuth2 provider fills the four query DWORDs. Its algorithm
    // is not emulated here. Nonzero return codes preserve state and close 111.
    std::function<std::uint32_t(std::array<std::uint32_t,4>&)> query;
    std::function<std::uint32_t()> tick;
    std::function<void(net::Packet)> send;
    std::function<void(std::uint32_t)> diagnostic,close;
};
// ps_game.exe CUser::NProtectCSASend 0044b700.
bool send_nprotect_query(bool enabled,NProtectState&,const NProtectPorts&);
struct ZoneLifecyclePorts {
    std::function<std::uint32_t()> tick;
    std::function<void(std::uint32_t)> reset_keep_tick;
    std::function<void()> check_map,nprotect;
};
// ps_game.exe CUser::EnterZone 0044ae30. Two separate clock samples, followed
// by ResetKeepTick, ChkApplySkillByMap and NProtectCSASend. No world admission.
void enter_zone_lifecycle(ZoneLifecycleState&,const ZoneLifecyclePorts&);
}
