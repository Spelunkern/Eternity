#pragma once
#include "eternity/game/game_log.hpp"
#include <functional>
#include <map>

namespace eternity::game {
enum class WorldLifecycleStep {
    guild_list,leave_party,leave_raid,release_guild,release_friends,leave_dead,
    admin_end,admin_not_answered,admin_leave,cancel_vehicle,cancel_vehicle_pair
};
struct WorldLifecycleState {
    GameLogSubject subject;
    std::uint8_t permission=255,billing_mode=0;
    std::optional<bool> party;
    std::uint32_t admin_question=0,vehicle_mode=0,auto_character=0,reconnect_ready=0;
    std::int32_t entry_type=4; // CUser+5814, signed comparison to 3.
    std::uint32_t money=0,initial_money=0,bank_money=0,cash_points=0,disconnected_seconds=0,ip=0;
    std::uint16_t skill_points=0,stat_points=0;
    std::array<std::uint16_t,6> stats{}; // CUser+170..17a, source order.
};
// Owned by the serialized world executor; take consumes the previous value.
class OfflineMoney {
public:
    void save(std::uint32_t character,std::uint32_t money);
    std::optional<std::uint32_t> take(std::uint32_t character);
private:
    std::map<std::uint32_t,std::uint32_t> money_;
};
struct WorldLifecyclePorts {
    std::function<bool()> admin_enter;
    std::function<void(WorldLifecycleStep)> step;
    std::function<void(std::uint32_t)> close;
    std::function<std::chrono::sys_seconds()> local_calendar;
    std::function<void(net::Packet)> game_log,manager;
    std::function<std::optional<std::uint32_t>(std::uint32_t)> take_money;
    std::function<void(std::uint32_t,std::uint32_t)> save_money;
};
// ps_game.exe CUser::EnterWorld 0044a820 / LeaveWorld 0044aad0. Callbacks
// implement required gameplay/admin services and must not throw. They may update
// this live state; later conditions read it again, in original call order.
bool enter_world(WorldLifecycleState&,const WorldLifecyclePorts&);
void leave_world(WorldLifecycleState&,const WorldLifecyclePorts&);
}
