#pragma once
#include "eternity/game/blessing.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/vertical_world.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// The application and send operations must use the real session, blessing
// status and zone transport owners; waiting users are actual admission users.
class WorldKillCountServices {
public:
    virtual ~WorldKillCountServices()=default;
    virtual const BlessingDefinitions& definitions() const=0;
    virtual std::uint32_t count(std::uint8_t country) const=0;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    virtual void apply_actor(Actor&)=0;
    virtual std::shared_ptr<void> retain_waiting(WorldAdmissionUser&)=0;
    virtual std::uint8_t waiting_country(WorldAdmissionUser&)=0;
    virtual void apply_waiting(WorldAdmissionUser&)=0;
    // Deliver to this zone's matching in-zone and waiting users.
    virtual void send_country(RuntimeZone&,std::uint8_t,const net::Packet&)=0;
};
enum class WorldKillCountResult {completed,interrupted};
// CWorldThread::UpdateKillCount 00407c30 and SendAllKillCount 00407e20.
// The caller executes the separate retained obelisk reset immediately after.
class WorldKillCountPhase final {
public:
    explicit WorldKillCountPhase(std::shared_ptr<WorldKillCountServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Kill count requires actual status and transport owners");
    }
    void request_apply(std::uint8_t country) {validate(country);apply_[country]=true;}
    void request_send(std::uint8_t country) {validate(country);send_[country]=true;}
    template<class Root> WorldKillCountResult update(Root& root) {
        auto& world=root.world();if(!world.running())throw std::logic_error("World is not running");
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            for(std::uint8_t country=0;country<2 && complete;++country) {
                if(!apply_[country] && !send_[country])continue;
                if(apply_[country]) {
                    const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
                        if(!current.running()) {complete=false;return;}
                        if(!zone.users)throw std::logic_error("Kill count zone lacks its user owner");
                        zone.users->visit_all([&](Actor& actor){return services_->retain_actor(actor);},[&](Actor& actor){
                            if(current.running() && actor.appearance().identity.country==country)services_->apply_actor(actor);
                        });
                        if(!current.running()){complete=false;return;}
                        root.admission().visit_zone_waiting(zone,[&](WorldAdmissionUser& user){return services_->retain_waiting(user);},[&](WorldAdmissionUser& user){
                            if(current.running() && services_->waiting_country(user)==country)services_->apply_waiting(user);
                        });
                        if(!current.running())complete=false;
                    });
                    if(!visited || !complete) {complete=false;break;}
                }
                const auto& definitions=services_->definitions();const auto size=definitions.size(country);
                if(!size)throw std::logic_error("Kill count country has no retained definitions");
                const auto* maximum=definitions.find(country,static_cast<std::int32_t>(size-1));
                if(!maximum)throw std::logic_error("Kill count maximum is unavailable");
                const auto value=std::min(services_->count(country),maximum->kill_count);
                net::Packet packet{0x0f,0x02,country};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));
                const bool sent=root.visit_active_zones([&](RuntimeZone& zone){if(current.running())services_->send_country(zone,country,packet);else complete=false;});
                if(!sent || !complete) {complete=false;break;}
                apply_[country]=false;send_[country]=false;
            }
        });
        return complete?WorldKillCountResult::completed:WorldKillCountResult::interrupted;
    }
private:
    static void validate(std::uint8_t country){if(country>1)throw std::out_of_range("Invalid kill count country");}
    std::shared_ptr<WorldKillCountServices> services_;std::array<bool,2> apply_{},send_{};
};
}
