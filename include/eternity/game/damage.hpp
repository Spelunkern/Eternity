#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace eternity::game {
struct DamageRecord {
    std::uint8_t kind=0;
    std::uint32_t source=0;
    std::int32_t amount=0;
    bool operator==(const DamageRecord&) const = default;
};
// Owned by a single actor executor. Entries retain their first insertion order.
class DamageLedger {
public:
    void add(std::int32_t kind,std::uint32_t source,std::int32_t amount);
    void reset() noexcept {entries_.clear();}
    std::optional<DamageRecord> maximum() const noexcept;
    const std::vector<DamageRecord>& entries() const noexcept {return entries_;}
private:
    std::vector<DamageRecord> entries_;
};
// Required operations surrounding scalar HP damage. The actor supplies equipment
// wear, its damage ledger and low-HP passive activation in this exact order.
class DamageObserver {
public:
    virtual ~DamageObserver() = default;
    virtual void defense_wear() = 0;
    virtual void record_damage(std::int32_t kind,std::uint32_t source,std::int32_t amount) = 0;
    virtual void check_hp_ability() = 0;
};
struct ScalarDamageState {
    std::int32_t attack_mode=0;
    std::uint32_t protection_value=0,maximum_hp=0;
};
}
