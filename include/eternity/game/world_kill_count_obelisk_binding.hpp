#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace eternity::game {
// Installs this one retained state into the existing
// MobAppliedSkillsObeliskMapsServices kill-count ports. Original obelisk
// destruction calls AddKillCount/SubKillCount and checks reset status 0xe.
template<class State,class Ports> void bind_world_kill_count_obelisks(std::shared_ptr<State> state,Ports& ports) {
    if(!state || ports.add_kill_count || ports.subtract_kill_count || ports.kill_count_status14)
        throw std::invalid_argument("Obelisk kill-count binding requires one retained state and unbound ports");
    auto country=[](std::int32_t value){if(value<0 || value>1)throw std::out_of_range("Invalid obelisk country");return static_cast<std::uint8_t>(value);};
    std::function<void(std::int32_t,std::int32_t)> add=[state,country](auto c,auto amount){state->add_count(country(c),amount);};
    std::function<void(std::int32_t,std::int32_t)> subtract=[state,country](auto c,auto amount){state->subtract_count(country(c),amount);};
    std::function<bool(std::int32_t)> status=[state,country](auto c){return state->status14(country(c));};
    ports.add_kill_count=std::move(add);ports.subtract_kill_count=std::move(subtract);ports.kill_count_status14=std::move(status);
}
}
