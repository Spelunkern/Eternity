#pragma once
#include "eternity/game/mob_ai_states_configured.hpp"
#include <span>
#include <string_view>
namespace eternity::game {
enum class MobAiCompiledFunction {move_random,move_random_run,move_selected,move_run_away,battle,debug};
class MobAiCompiledServices {
public:
    virtual ~MobAiCompiledServices()=default;
    virtual void invoke(MobAiCompiledFunction,MobRuntime&,MobAiConfiguredFsm&)=0;
};
struct MobAiCompiledStateNames {std::string state,enter,update,exit;};
// Original signed-char DJB2 arithmetic with DWORD wrapping and C-string end.
std::uint32_t mob_ai_name_code(std::string_view) noexcept;
// CMob::ConnectToFSM 00491db0 / AddStateFunctions 004954e0.
// Services and subject must outlive every retained callback. Calls on an owner
// must share its normal scheduling exclusion; the original global binding lock
// alone does not protect concurrent state execution or registry access.
void connect_mob_ai_functions(MobRuntime&,MobAiConfiguredFsm&,
    std::span<const MobAiCompiledStateNames>,MobAiCompiledServices&);
}
