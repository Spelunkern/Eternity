#pragma once
#include "eternity/game/vertical_world.hpp"
#include <memory>
#include <optional>
#include <stdexcept>

namespace eternity::game {
struct WorldRankNotice {std::uint16_t map;std::uint8_t type;};
class WorldRankServices {
public:
    virtual ~WorldRankServices()=default;
    virtual ZoneLocalTime local_time()=0;
    virtual std::uint32_t tick_ms()=0;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    // Must perform original GetBindPos and schedule the actual user's move,
    // including its existing move-state guard and 200 ms deadline.
    virtual void move_to_bind(RuntimeZone&,Actor&,std::uint32_t)=0;
    virtual std::optional<WorldRankNotice> notice() const=0;
    virtual void broadcast(RuntimeZone&,const net::Packet&)=0;
};
enum class WorldRankResult {skipped,completed,interrupted};
// CWorldThread::Update 0040788a..0040797b; two separate zone passes.
class WorldRankPhase final {
public:
    explicit WorldRankPhase(std::shared_ptr<WorldRankServices> services):services_(std::move(services)) {
        if(!services_)throw std::invalid_argument("Rank phase requires actual movement, clock and transport owners");
    }
    template<class Root> WorldRankResult update(Root& root,std::uint32_t now) {
        auto& world=root.world();if(!world.running())throw std::logic_error("World is not running");
        if(now<next_)return WorldRankResult::skipped;
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            const bool moved=root.visit_active_zones([&](RuntimeZone& zone){
                if(!current.running()){complete=false;return;}
                if(!zone.map || !zone.users)throw std::logic_error("Rank zone lacks its map or user owner");
                if(zone.map->settings.create_type!=5 || zone.users->size()==0 || zone.owner==0xffffffffU)return;
                if(guild_rank_window(zone.map->settings,services_->local_time()))return;
                const auto tick=services_->tick_ms();
                zone.users->visit_all([&](Actor& actor){return services_->retain_actor(actor);},[&](Actor& actor){
                    if(current.running())services_->move_to_bind(zone,actor,tick);
                });
                if(!current.running())complete=false;
            });
            if(!moved || !complete){complete=false;return;}
            const auto notice=services_->notice();
            if(notice) {
                const net::Packet packet{0x26,0x0d,static_cast<std::uint8_t>(notice->map),static_cast<std::uint8_t>(notice->map>>8),notice->type};
                const bool sent=root.visit_active_zones([&](RuntimeZone& zone){if(current.running())services_->broadcast(zone,packet);else complete=false;});
                if(!sent)complete=false;
            }
        });
        if(!complete)return WorldRankResult::interrupted;
        next_=now+60000U;return WorldRankResult::completed;
    }
    std::uint32_t next_deadline() const noexcept{return next_;}
private:
    std::shared_ptr<WorldRankServices> services_;std::uint32_t next_=0;
};
}
