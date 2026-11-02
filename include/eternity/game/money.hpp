#pragma once
#include "eternity/game/game_log.hpp"

namespace eternity::game {
class Actor;
bool money_addable(std::uint32_t current,std::uint32_t amount) noexcept;
bool money_sufficient(std::uint32_t current,std::uint32_t amount) noexcept;
void add_money(std::uint32_t& current,std::uint32_t amount) noexcept;
void add_received_money(std::uint32_t& current,std::uint32_t& amount) noexcept;
net::Packet money_database_packet(std::uint32_t account_id,std::uint32_t amount);
net::Packet money_received_packet(std::uint32_t accepted,std::uint8_t type_id=0,std::uint8_t count=1);
// Owned DB-agent status field; full character snapshot saving remains separate.
class MoneyReplica {
public:
    explicit MoneyReplica(std::uint32_t account_id=0,std::uint32_t money=0):account_id_(account_id),money_(money) {}
    bool apply(std::span<const std::uint8_t> packet);
    std::uint32_t money() const noexcept {return money_;}
private:
    std::uint32_t account_id_,money_;
};
class MoneyWorld {
public:
    virtual ~MoneyWorld() = default;
    // The original 0603 updates DB-agent's owned character status replica.
    // SQL snapshot saving is separate; this port must queue that update in order.
    virtual void send_money_database(const net::Packet&) = 0;
    virtual GameLogSubject money_log_subject(const Actor&) = 0;
    virtual std::chrono::sys_seconds money_local_calendar() = 0;
    virtual void send_money_log(const net::Packet&) = 0;
};
}
