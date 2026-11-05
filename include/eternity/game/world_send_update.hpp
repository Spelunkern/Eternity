#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace eternity::game {
// Implementations must bind the real CWorld and guild manager owners.
class WorldSendUpdateServices {
public:
    virtual ~WorldSendUpdateServices()=default;
    virtual void update_packets()=0;
    virtual void update_net_send()=0;
    virtual void update_session_lost(std::uint32_t now)=0;
    virtual void update_kill_count(std::uint32_t now)=0;
    virtual void check_guild_rank_notices()=0;
    virtual bool check_guild_rank_status()=0;
    virtual void update_guild_point_rank()=0;
};
// ps_game.exe CWorldSendThread::Run 00406f90, especially 00407003..004070bc.
// The caller runs this on the root's strand at the real send-thread cadence.
template<class Root> class WorldSendUpdate final {
public:
    WorldSendUpdate(std::shared_ptr<Root> root,std::shared_ptr<WorldSendUpdateServices> services,
        const std::function<std::uint32_t()>& tick):root_(root),services_(std::move(services)) {
        if(!root || !services_ || !tick)throw std::invalid_argument("World send update requires actual world, services and clock");
        session_lost_=tick()+30000U;rank_notices_=tick()+59000U;
        instances_=tick()+300U;rank_status_=tick()+60000U;
    }
    bool update(std::uint32_t now) {
        auto root=root_.lock();if(!root)throw std::logic_error("World send update lost its world owner");
        if(!root->world_send_running())throw std::logic_error("World send update requires a running world");
        services_->update_packets();if(!root->world_send_running())return false;
        services_->update_net_send();if(!root->world_send_running())return false;
        if(session_lost_<now) {services_->update_session_lost(now);session_lost_=now+30000U;}
        if(!root->world_send_running())return false;
        services_->update_kill_count(now);if(!root->world_send_running())return false;
        if(rank_notices_<now) {services_->check_guild_rank_notices();rank_notices_=now+59000U;}
        if(!root->world_send_running())return false;
        if(rank_status_<now) {
            const bool active=services_->check_guild_rank_status();
            if(!active && rank_active_)rank_points_=now+1800000U;
            rank_active_=active;rank_status_=now+60000U;
        }
        if(!root->world_send_running())return false;
        if(rank_points_<now) {services_->update_guild_point_rank();rank_points_=0xffffffffU;}
        if(!root->world_send_running())return false;
        if(instances_<now) {root->distribute_instance_broadcasts();instances_=now+300U;}
        return root->world_send_running();
    }
    std::uint32_t next_instance_deadline()const noexcept{return instances_;}
private:
    std::weak_ptr<Root> root_;std::shared_ptr<WorldSendUpdateServices> services_;
    std::uint32_t session_lost_,rank_notices_,instances_,rank_status_,rank_points_=0xffffffffU;
    bool rank_active_=false;
};
}
