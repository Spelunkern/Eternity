#pragma once
#include "eternity/game/world_primary_logout_phase.hpp"
#include <memory>
#include <stdexcept>

namespace eternity::game {
// Implementations bind the actual DB-thread packet owners, world-thread
// lifecycle flag and experience event manager. No successful defaults exist.
class WorldDbUpdateServices {
public:
    virtual ~WorldDbUpdateServices()=default;
    virtual void update_manager_packets()=0;
    virtual void update_database_packets()=0;
    virtual bool world_threads_started() const=0;
    virtual void check_experience_event()=0;
};
enum class WorldDbUpdateResult {completed,interrupted};
// ps_game.exe CWorldDBThread::OnUpdate 00406e20. Caller schedules this on
// the same serialized executor as the root, with the real DWORD clock.
template<class Root> class WorldDbUpdate final {
public:
    WorldDbUpdate(std::shared_ptr<Root> root,std::shared_ptr<WorldDbUpdateServices> services)
        :root_(root),services_(std::move(services)) {
        if(!root || !services_) throw std::invalid_argument("World DB update requires real world and services");
    }
    WorldDbUpdateResult update(std::uint32_t now) {
        services_->update_manager_packets();
        services_->update_database_packets();
        if(!services_->world_threads_started()) return WorldDbUpdateResult::completed;
        auto root=root_.lock();if(!root) throw std::logic_error("World DB update lost its world owner");
        if(root->update_registry_logout_phase(now)==PrimaryLogoutResult::interrupted) return WorldDbUpdateResult::interrupted;
        if(now>=next_experience_) {
            services_->check_experience_event();
            next_experience_=now+60000U;
        }
        return WorldDbUpdateResult::completed;
    }
    std::uint32_t next_experience_deadline() const noexcept {return next_experience_;}
private:
    std::weak_ptr<Root> root_;std::shared_ptr<WorldDbUpdateServices> services_;std::uint32_t next_experience_=0;
};
}
