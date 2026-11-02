#pragma once
#include "eternity/game/character_save.hpp"

namespace eternity::game {
struct UserLifecycleState {
    std::uint32_t user_id=0,status=1,next_save_tick=0;
    bool connected=false;
};
// Transport/ownership callbacks must not throw or destroy this object during a
// call. leave removes the registry owner after the ordered exit notifications.
struct UserLifecyclePorts {
    std::function<void(std::uint32_t)> post;
    std::function<void(net::Packet)> send;
    std::function<void()> leave,reset;
    std::function<void(bool with_country)> roster;
    std::function<bool(bool periodic,bool quick)> save;
    std::function<void(bool shutdown)> save_exit;
};
// ps_dbAgent.exe CUser lifecycle, not the public game-client wire protocol.
// Caller serializes all access, including callbacks that enqueue later work.
class UserLifecycle {
public:
    UserLifecycle(UserLifecycleState& state,UserLifecyclePorts ports);
    // Call only after successful full character loading/initial replication.
    void character_loaded(std::uint32_t save_time_id,std::uint32_t last);
    bool update_save(std::uint32_t now,std::uint32_t& last);
    void exit();
    void disconnect_by_game_server();
    void logout_select();
    // Implements save/lifecycle jobs 8,9,10,22. Other DB jobs remain explicit
    // integration work and return false without claiming successful execution.
    bool dispatch_save_job(std::uint32_t job);
private:
    void notify(std::uint16_t opcode);
    UserLifecycleState& state_;UserLifecyclePorts ports_;
};
}
