#pragma once
#include "eternity/game/world_kill_count_state.hpp"
#include "eternity/game/world_kill_count_phase.hpp"

namespace eternity::game {
// The user owner must apply the status from the supplied shared world state;
// send_country must reach both actual in-zone and zone-wait users.
template<class Root> class WorldKillCountZonePorts {
public:
    using State=WorldKillCountState<Root>;
    virtual ~WorldKillCountZonePorts()=default;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    virtual void apply_actor(Actor&,const State&)=0;
    virtual std::shared_ptr<void> retain_waiting(WorldAdmissionUser&)=0;
    virtual std::uint8_t waiting_country(WorldAdmissionUser&)=0;
    virtual void apply_waiting(WorldAdmissionUser&,const State&)=0;
    virtual void send_country(RuntimeZone&,std::uint8_t,const net::Packet&)=0;
};
template<class Root> class WorldKillCountBoundZone final:public WorldKillCountServices {
public:
    using State=WorldKillCountState<Root>;using Ports=WorldKillCountZonePorts<Root>;
    WorldKillCountBoundZone(std::shared_ptr<State> state,std::shared_ptr<Ports> ports):state_(std::move(state)),ports_(std::move(ports)) {
        if(!state_ || !ports_)throw std::invalid_argument("Kill-count zone requires retained state and real user services");
    }
    const BlessingDefinitions& definitions()const override{return state_->definitions();}
    std::uint32_t count(std::uint8_t country)const override{return state_->country(country).count;}
    std::shared_ptr<void> retain_actor(Actor& actor) override{return ports_->retain_actor(actor);}
    void apply_actor(Actor& actor) override{ports_->apply_actor(actor,*state_);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override{return ports_->retain_waiting(user);}
    std::uint8_t waiting_country(WorldAdmissionUser& user) override{return ports_->waiting_country(user);}
    void apply_waiting(WorldAdmissionUser& user) override{ports_->apply_waiting(user,*state_);}
    void send_country(RuntimeZone& zone,std::uint8_t country,const net::Packet& packet) override{ports_->send_country(zone,country,packet);}
private:std::shared_ptr<State> state_;std::shared_ptr<Ports> ports_;
};
// CWorld::ApplyKillCount 004139c0, CWorldThread::UpdateKillCount 00407c30
// and CWorld::UpdateKillCount 00413300 share one retained world state.
template<class Root> class WorldKillCountComposition final {
public:
    using State=WorldKillCountState<Root>;using Ports=WorldKillCountZonePorts<Root>;
    struct Inputs {
        std::shared_ptr<const BlessingDefinitions> definitions;std::array<WorldKillCountCountry,2> initial;
        typename State::Clock tick;typename State::Database database;typename State::Disabled disabled;typename State::GameLog game_log;
        std::shared_ptr<Ports> zones;
    };
    // Bind before startup on the world strand. Discard an unstarted root if a
    // later binding throws after an earlier binding has already succeeded.
    static std::shared_ptr<State> bind(const std::shared_ptr<Root>& root,Inputs inputs) {
        if(!root || !inputs.definitions || !inputs.tick || !inputs.database || !inputs.disabled || !inputs.game_log || !inputs.zones)
            throw std::invalid_argument("Kill-count composition requires all actual owners");
        auto state=std::make_shared<State>(root,std::move(inputs.definitions),std::move(inputs.initial),std::move(inputs.tick),
            std::move(inputs.database),std::move(inputs.disabled),std::move(inputs.game_log));
        auto zones=std::make_shared<WorldKillCountBoundZone<Root>>(state,std::move(inputs.zones));
        root->bind_kill_count_phase(std::move(zones));root->bind_world_kill_count_timer(state);return state;
    }
};
}
