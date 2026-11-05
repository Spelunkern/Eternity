#pragma once
#include "eternity/game/character_replica_delivery.hpp"
#include "eternity/game/vertical_database_disconnect.hpp"
#include <type_traits>

namespace eternity::game {
// The caller supplies the executor and owns this object until all queued jobs
// and callbacks have completed. The store must use this same recursive mutex.
// Callbacks must not throw or destroy this object; post must enqueue work.
struct CharacterSaveLifecyclePorts {
    std::function<void(std::uint32_t)> post;
    std::function<void(net::Packet)> send;
    std::function<void()> leave,reset,save_friends,save_blocks;
    std::function<void(bool)> roster;
    std::function<void(std::string_view)> diagnostic;
};
// Store is normally CharacterSaveStore; a substitute permits isolated tests of
// the scheduling contract without starting SQL Server.
template<class Store = CharacterSaveStore>
class CharacterSaveLifecycle {
public:
    CharacterSaveLifecycle(UserLifecycleState& state,CharacterReplica& replica,Store& store,
        std::recursive_mutex& mutex,std::string username,CharacterSaveLifecyclePorts ports)
        :state_(state),replica_(replica),store_(store),mutex_(mutex),username_(std::move(username)),
         ports_(std::move(ports)),delivery_(replica,mutex),lifecycle_(state,make_ports()) {
        if(!ports_.post || !ports_.send || !ports_.leave || !ports_.reset || !ports_.roster || !ports_.save_friends || !ports_.save_blocks)
            throw std::invalid_argument("Incomplete character save lifecycle ports");
        if constexpr(std::is_same_v<Store,CharacterSaveStore>) {
            if(!store_.uses_cache_mutex(mutex_)) throw std::invalid_argument("Character save store and lifecycle need the same mutex");
        }
        std::lock_guard lock(mutex_);
        if(state_.user_id!=replica_.core().user_id) throw std::invalid_argument("Character save lifecycle account mismatch");
    }
    CharacterSaveLifecycle(const CharacterSaveLifecycle&)=delete;
    CharacterSaveLifecycle& operator=(const CharacterSaveLifecycle&)=delete;
    InventoryReplicaResult apply(std::span<const std::uint8_t> packet) {return delivery_.apply(packet);}
    bool disconnect(std::span<const std::uint8_t> packet) {
        std::lock_guard lock(mutex_);return vertical_database_disconnect_receive(packet,state_,lifecycle_);
    }
    bool dispatch(std::uint32_t job) {std::lock_guard lock(mutex_);return lifecycle_.dispatch_save_job(job);}
    void character_loaded(std::uint32_t id,std::uint32_t last) {std::lock_guard lock(mutex_);lifecycle_.character_loaded(id,last);}
    bool update_save(std::uint32_t now,std::uint32_t& last) {std::lock_guard lock(mutex_);return lifecycle_.update_save(now,last);}
    void logout_select() {std::lock_guard lock(mutex_);lifecycle_.logout_select();}
private:
    CharacterSaveCallbacks save_callbacks() {
        return {ports_.save_friends,ports_.save_blocks,[this]{ports_.post(9);},[this]{lifecycle_.exit();}};
    }
    UserLifecyclePorts make_ports() {
        return {[this](std::uint32_t job){ports_.post(job);},
            [this](net::Packet packet){ports_.send(std::move(packet));},
            [this]{ports_.leave();},[this]{ports_.reset();},[this](bool country){ports_.roster(country);},
            [this](bool periodic,bool quick){return store_.save(replica_,periodic,quick,true,username_,save_callbacks(),ports_.diagnostic).legacy_success;},
            [this](bool shutdown){(void)store_.save_exit(replica_,shutdown,username_,save_callbacks(),ports_.diagnostic);}};
    }
    UserLifecycleState& state_;CharacterReplica& replica_;Store& store_;std::recursive_mutex& mutex_;
    std::string username_;CharacterSaveLifecyclePorts ports_;CharacterReplicaDelivery delivery_;UserLifecycle lifecycle_;
};
}
