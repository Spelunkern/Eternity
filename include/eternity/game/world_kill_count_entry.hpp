#pragma once
#include "eternity/game/entry_delivery.hpp"
#include "eternity/game/world_kill_count_state.hpp"

namespace eternity::game {
// CUser::SendKCStatus 00481920 reads the same world count/reset deadline as
// CUser::InitKCStatus 0048be90 reads for the selected country grade.
template<class Root> class WorldKillCountEntry final {
public:
    using State=WorldKillCountState<Root>;
    explicit WorldKillCountEntry(std::shared_ptr<State> state):state_(std::move(state)) {
        if(!state_)throw std::invalid_argument("Entry kill-count binding requires actual world state");
    }
    // Resolve immediately before constructing the actor-load pipeline, on the
    // world strand. Keep this binding or the state alive through equipment().
    void bind_actor_load(ActorLoadOptions& options)const {
        if(options.blessing)throw std::logic_error("Actor load already has a blessing owner");
        const auto& country=state_->country(options.country);
        const auto* definition=state_->definitions().find(options.country,static_cast<std::int32_t>(country.index));
        if(!definition)throw std::logic_error("Entry kill-count status is unavailable");
        options.blessing=definition;
    }
    // The closure retains the same state through entry delivery and requires
    // the existing EntryDelivery clock and actual self-channel to be bound.
    void bind_delivery(EntryDeliveryPorts& ports)const {
        if(ports.kill_count)throw std::logic_error("Entry kill-count owner is already bound");
        ports.kill_count=[state=state_](std::uint8_t country){const auto& current=state->country(country);return EntryKillCount{current.count,current.deadlines.reset};};
    }
private:std::shared_ptr<State> state_;
};
}
