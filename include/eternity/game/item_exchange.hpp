#pragma once
#include <array>
#include <cstdint>

namespace eternity::game {
class Actor;
struct ExchangeItemReference {std::uint8_t bag=0,slot=0,count=0;};
// The serialized actor owner must keep borrowed peers alive through callbacks.
struct ItemExchangeState {
    std::int32_t mode=0; // +1568 for trades, +1590 for duel offers.
    Actor* peer=nullptr; // Borrowed +156c pointer; not used for duel lookup.
    std::uint32_t money=0;
    std::array<ExchangeItemReference,8> items{};
    std::uint8_t ready=0; // +158c / +15b4.
};
class ItemExchangeWorld {
public:
    virtual ~ItemExchangeWorld() = default;
    // CZone::FindUser in the requesting actor's current zone, including diagnostics.
    virtual Actor* find_exchange_user(Actor& requester,std::uint32_t id) = 0;
};
}
