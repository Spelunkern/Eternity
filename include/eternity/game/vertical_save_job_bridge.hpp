#pragma once
#include "eternity/game/character_save_lifecycle.hpp"
#include "eternity/game/vertical_tracked_db_jobs.hpp"

namespace eternity::game {
// Binds real CharacterSaveLifecycle callbacks to the tracked worker boundary.
// The same recursive mutex serializes worker execution and synchronous replica
// updates. The owner supplies thread-safe social/leave services and must retain
// this bridge, store, mutex and callbacks through all jobs and replies.
template<class Store=CharacterSaveStore> class VerticalSaveJobBridge {
public:
    using Retry=std::function<void(std::uint32_t)>;
    VerticalSaveJobBridge(UserLifecycleState& state,CharacterReplica& replica,Store& store,std::recursive_mutex& mutex,
        std::string username,CharacterSaveLifecyclePorts ports)
        :mutex_(mutex),ports_(std::move(ports)),lifecycle_(state,replica,store,mutex,std::move(username),bind()) {
        if(!ports_.post || !ports_.send)throw std::invalid_argument("Missing save job transport ports");
    }
    InventoryReplicaResult apply(std::span<const std::uint8_t> packet) {return lifecycle_.apply(packet);}
    bool disconnect(std::span<const std::uint8_t> packet) {return lifecycle_.disconnect(packet);}
    bool dispatch(std::uint32_t job) {return lifecycle_.dispatch(job);}
    void character_loaded(std::uint32_t id,std::uint32_t last) {lifecycle_.character_loaded(id,last);}
    bool update_save(std::uint32_t now,std::uint32_t& last) {return lifecycle_.update_save(now,last);}
    void logout_select() {lifecycle_.logout_select();}
    // Called synchronously by VerticalTrackedDbJobs' work provider. Its retry
    // callback cannot escape this call. All replies return to the world strand.
    std::vector<net::Packet> dispatch_collect(std::uint32_t job,const Retry& retry) {
        if(!retry)throw std::invalid_argument("Missing database retry callback");
        std::lock_guard lock(mutex_);if(context_)throw std::logic_error("Nested save job dispatch");
        std::vector<net::Packet> packets;Context current{retry,packets};context_=&current;
        try {if(!lifecycle_.dispatch(job))throw std::invalid_argument("Unsupported save lifecycle job");}
        catch(...){context_=nullptr;throw;}
        context_=nullptr;return packets;
    }
private:
    struct Context {const Retry& retry;std::vector<net::Packet>& packets;};
    CharacterSaveLifecyclePorts bind() {
        auto result=ports_;
        result.post=[this](std::uint32_t job){if(context_)context_->retry(job);else ports_.post(job);};
        result.send=[this](net::Packet packet){if(context_)context_->packets.push_back(std::move(packet));else ports_.send(std::move(packet));};
        return result;
    }
    std::recursive_mutex& mutex_;CharacterSaveLifecyclePorts ports_;Context* context_=nullptr;CharacterSaveLifecycle<Store> lifecycle_;
};
}
