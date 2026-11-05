#pragma once
#include "eternity/game/character_replica.hpp"
#include <mutex>
#include <span>

namespace eternity::game {
// Synchronous cohosted DB-agent delivery. The caller owns packet ordering and
// failure/disconnect policy. Use the SAME mutex as CharacterSaveStore, and
// retain both mutex and replica until all callers have stopped.
class CharacterReplicaDelivery final {
public:
    CharacterReplicaDelivery(CharacterReplica& replica,std::recursive_mutex& mutex):replica_(replica),mutex_(mutex) {}
    InventoryReplicaResult apply(std::span<const std::uint8_t> packet) {
        std::lock_guard lock(mutex_);
        return replica_.apply(packet);
    }
private:
    CharacterReplica& replica_;
    std::recursive_mutex& mutex_;
};
}
