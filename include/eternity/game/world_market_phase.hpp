#pragma once
#include "eternity/game/vertical_world.hpp"
#include <memory>
#include <stdexcept>

namespace eternity::game {
class WorldMarketServices {
public:
    virtual ~WorldMarketServices()=default;
    virtual bool market_open() const=0;
    virtual void check_item_end_dates(std::uint32_t tick)=0;
};
enum class WorldMarketResult {skipped,completed,interrupted};
// CWorldThread::Update 0040797b..004079e1. The caller runs this after the
// rank pass and before kill-count, world sends and instance cleanup.
class WorldMarketPhase final {
public:
    explicit WorldMarketPhase(std::shared_ptr<WorldMarketServices> services):services_(std::move(services)) {
        if(!services_)throw std::invalid_argument("Market phase requires the actual market manager");
    }
    template<class Root> WorldMarketResult update(Root& root,std::uint32_t now) {
        auto& world=root.world();if(!world.running())throw std::logic_error("World is not running");
        if(now<next_)return WorldMarketResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running()){complete=false;return;}
            if(!zone.map)throw std::logic_error("Market zone lost its map");
            if(zone.map->settings.market && services_->market_open())services_->check_item_end_dates(now);
            if(!world.running())complete=false;
        });
        if(!visited || !complete)return WorldMarketResult::interrupted;
        next_=now+60000U;return WorldMarketResult::completed;
    }
    std::uint32_t next_deadline() const noexcept{return next_;}
private:
    std::shared_ptr<WorldMarketServices> services_;std::uint32_t next_=0;
};
}
