#pragma once
#include <array>
#include <cstdint>
namespace eternity::game {
// ps_game.exe CMob::SetTarget 0049fad0 / SetTargetEx 004b65e0.
// Explicit projection until the production FSM/scene owner is integrated.
struct MobTargetingAttack { bool enabled=false, has_skill=false; std::uint16_t cooldown_seconds=0; };
struct MobTargetingState {
    bool fsm_enabled=false;
    // scene_status is the current top scene status, or zero for an empty stack.
    std::int32_t scene_status=0, status=0, move_mode=0;
    std::uint32_t vision_fixed_user=0, target_type=0, target_id=0;
    std::uint32_t combat_timeout=0, next_move_time=0;
    std::uint8_t ai=0;
    std::array<MobTargetingAttack,3> attacks{};
    std::array<std::uint32_t,3> attack_times{};
    std::int32_t effective_status() const { return fsm_enabled?scene_status:status; }
};
struct MobTargetingUser { std::uint32_t id=0; std::int32_t life_status=0; };
struct MobTargetingServices {
    virtual ~MobTargetingServices()=default;
    // Original MSVCRT rand(): required shared random provider, range 0..32767.
    virtual std::int32_t random()=0;
    virtual std::uint32_t tick_ms()=0;
    // Help kinds: 2 FindHelpNormal 0049ff70 (mode 0), 3 FindHelpAttack
    // 004a0470, 5 FindHelpNormal2 004a0200. Providers may mutate the projection.
    virtual void help(MobTargetingState&, const MobTargetingUser&, std::uint8_t kind)=0;
    // DispatchEvent 004925f0: event 1, argument is CURRENT target ID.
    virtual void dispatch(MobTargetingState&, std::uint32_t event, std::uint32_t argument)=0;
};
void mob_set_target_ex(MobTargetingState&,const MobTargetingUser&,MobTargetingServices&);
void mob_set_target(MobTargetingState&,const MobTargetingUser&,bool initialize,MobTargetingServices&);
}
