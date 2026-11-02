#pragma once
#include "eternity/game/game_log.hpp"

namespace eternity::game {
class Actor;
struct ItemDefinition;
struct DuelDropItem {
    PlayerItem item;
    const ItemDefinition* definition=nullptr;
    UserPosition position;
    std::uint32_t user_id=0,party_id=0,drop_id=0,money=0;
    std::uint8_t drop_type=0;
};
struct DuelWager {
    std::uint32_t money=0; // CUser+15b8.
    // Borrowed pool allocations, corresponding to CUser+15bc..15d8.
    std::array<DuelDropItem*,8> items{};
};
class DuelWorld {
public:
    virtual ~DuelWorld() = default;
    virtual Actor* find_duel_user(Actor& requester,std::uint32_t id) = 0;
    virtual UserPosition duel_position(const Actor&) = 0;
    virtual std::int32_t drop_random() = 0; // Original CRT rand stream, shared with other consumers.
    virtual DuelDropItem* allocate_drop_item() = 0;
    virtual bool enter_duel_item(Actor& zone_owner,DuelDropItem&,bool notify) = 0;
    virtual GameLogSubject duel_log_subject(const Actor&) = 0;
    virtual std::chrono::sys_seconds local_calendar() = 0;
    virtual void send_duel_log(const net::Packet&) = 0;
};
UserPosition random_drop_position(const Actor&,DuelWorld&);
void drop_duel_wager(Actor& owner,Actor& recipient,DuelWorld&);
}
