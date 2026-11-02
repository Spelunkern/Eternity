#pragma once
#include <cstdint>
#include <span>
#include <memory>
#include <vector>
namespace eternity::game {
struct MobDamageRecord {
    std::uint8_t type=0;std::uint32_t id=0;
    std::int32_t damage=0,aggro=0;std::uint32_t party=0;
};
// ps_game.exe CDamageMob::GetMaxDamage 004b8ef0 / GetMaxAggro 004b9000.
// Preserve untouched output fields and first-in-list ties. Input order is live
// record order; these queries do not substitute damage accumulation ownership.
bool maximum_mob_damage(std::span<const MobDamageRecord>,MobDamageRecord& output);
bool maximum_mob_aggro(std::span<const MobDamageRecord>,MobDamageRecord& output);
class MobDamagePoolServices {
public:
    virtual ~MobDamagePoolServices()=default;
    virtual std::unique_ptr<MobDamageRecord> construct()=0;
};
class MobDamageLedger;
// Shared serialized damage heap. Services and pool outlive all ledgers.
class MobDamagePool {
public:
    explicit MobDamagePool(MobDamagePoolServices& services):services_(services) {}
    std::uint32_t construction_attempts() const noexcept {return attempts_;}
    std::span<MobDamageRecord* const> available() const noexcept {return free_;}
private:
    friend class MobDamageLedger;
    MobDamageRecord* allocate();void release(MobDamageRecord*);
    MobDamagePoolServices& services_;std::uint32_t attempts_=0;
    std::vector<std::unique_ptr<MobDamageRecord>> objects_;std::vector<MobDamageRecord*> free_;
};
class MobDamageLedger {
public:
    explicit MobDamageLedger(MobDamagePool& pool):pool_(pool) {}
    ~MobDamageLedger();
    MobDamageLedger(const MobDamageLedger&)=delete;MobDamageLedger& operator=(const MobDamageLedger&)=delete;
    bool fair_combat=true;
    void add(std::int32_t type,std::uint32_t id,std::int32_t damage,std::int32_t aggro,std::uint32_t party);
    void reset();
    bool maximum_damage(MobDamageRecord& output) const;
    bool maximum_aggro(MobDamageRecord& output) const;
    std::span<MobDamageRecord* const> records() const noexcept {return records_;}
private:
    MobDamagePool& pool_;std::vector<MobDamageRecord*> records_;
};

}
