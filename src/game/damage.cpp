#include "eternity/game/damage.hpp"
#include <bit>

namespace eternity::game {
void DamageLedger::add(std::int32_t kind,std::uint32_t source,std::int32_t amount) {
    // ps_game.exe CDamage::Add 004b8b60 compares the stored BYTE against the
    // full input int, then wraps DWORD addition. Out-of-byte kinds never merge.
    for(auto& entry:entries_) {
        if(entry.kind==kind && entry.source==source) {
            entry.amount=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(entry.amount)+static_cast<std::uint32_t>(amount));
            return;
        }
    }
    entries_.push_back({static_cast<std::uint8_t>(kind),source,amount});
}
std::optional<DamageRecord> DamageLedger::maximum() const noexcept {
    // CDamage::GetMax 004b8c40: signed comparison; ties retain the oldest entry.
    if(entries_.empty()) return std::nullopt;
    auto result=entries_.front();
    for(const auto& entry:entries_) if(entry.amount>result.amount) result=entry;
    return result;
}
}
