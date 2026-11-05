#pragma once
#include "eternity/game/world_users.hpp"
#include <atomic>
#include <mutex>
#include <span>

namespace eternity::game {
// Stable DB acknowledgment state is owned alongside the actual world user.
struct VerticalDatabaseAckUser {
    WorldUser& user;std::recursive_mutex& mutex;
    std::atomic<std::uint32_t> acknowledgments{0};
    bool socket_open=false;std::uint32_t transfer_state=0;
};
class VerticalDatabaseAckServices {
public:
    virtual ~VerticalDatabaseAckServices()=default;
    // Account-ID lookup equivalent to CWorld::FindUser 0040d570. Both the first
    // and second returned owners/mutexes must stay alive through the callback.
    virtual VerticalDatabaseAckUser* find(std::uint32_t account)=0;
    virtual void close(VerticalDatabaseAckUser&,std::uint32_t reason)=0;
    virtual void leave(VerticalDatabaseAckUser&)=0;
};
// ps_game.exe DB-agent 0302 branch 00408d30..00408dfd. Calls are serialized
// with world/transport state changes, including socket_open and transfer_state.
// The original repeats lookup after taking the first user's recursive lock.
inline bool vertical_database_disconnect_ack(std::span<const std::uint8_t> packet,VerticalDatabaseAckServices& services) {
    if(packet.size()!=6 || packet[0]!=2 || packet[1]!=3)return false;
    std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t(packet[2+i])<<(i*8);
    auto* first=services.find(account);if(!first)return true;
    const std::lock_guard lock(first->mutex);
    auto* current=services.find(account);if(!current)return true;
    if(current->acknowledgments.fetch_add(1,std::memory_order_seq_cst)+1U!=1)return true;
    if(!current->socket_open)services.leave(*current);
    else if(current->transfer_state!=1)services.close(*current,104);
    return true;
}
}
