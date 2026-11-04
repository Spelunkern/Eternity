#pragma once
#include "eternity/game/mob_targeting_lifecycle_death.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
#include <stdexcept>
namespace eternity::game {
struct MobTargetingOrdinaryDeathState:MobTargetingDeathState {
    std::array<std::uint32_t,4> kills{}; // Existing PlayerCore +14c..158.
};
struct MobTargetingDeathMobLog {std::uint16_t type=0;std::string name;};
struct MobTargetingOrdinaryDeathScratch {
    std::array<std::uint8_t,117> log{};
    // Original failed GetMax leaves its output source slot untouched. This is
    // scratch provenance for LOGGING only, not an authoritative attacker ID.
    std::uint32_t failed_max_source=0;
    std::uint16_t notification_opcode=0; // Unwritten for corrupt party kind !=0/1.
};
class MobTargetingPlayerDeathRequired:public std::logic_error {
public:
    MobTargetingPlayerDeathRequired():std::logic_error("Player maximum requires the unrecovered PvP reward coordinator") {}
};
class MobTargetingOrdinaryDeathServices:public MobTargetingNpcDeathServices {
public:
    virtual void cancel_exchange()=0;
    virtual void end_shop()=0;
    virtual void cancel_vehicle()=0;
    virtual std::optional<DamageRecord> maximum_damage()=0;
    virtual std::optional<MobTargetingDeathMobLog> find_mob(std::uint32_t)=0;
    virtual Actor* find_user(std::uint32_t)=0;
    virtual std::string user_name(Actor*)=0; // Pure retained owner query.
    virtual void item_drop_by_user(Actor* attacker)=0; // nullptr is NOT skipped.
    virtual std::uint8_t party_kind(PartyRegistryParty*) const noexcept=0;
};
// Complete ordinary/fallback routes inside Death 004586a0. A player maximum
// fails explicitly after original entry/cancellations/GetMax, before any fake
// PvP success. This is NOT an alias claiming complete general Death support.
void mob_targeting_death_ordinary(MobTargetingOrdinaryDeathState&,std::int32_t kind,
    std::uint32_t source,MobTargetingOrdinaryDeathServices&,
    const MobTargetingOrdinaryDeathScratch& scratch={});
class MobTargetingOrdinaryDeathWorld:public MobTargetingNpcDeathWorld {
public:
    virtual void replace_resources(Actor&,PlayerResources); // Actual Actor raw API, or explicit missing-interface error.
    virtual const PlayerCore& player(const Actor&) const noexcept=0;
    virtual void cancel_exchange(Actor&)=0;
    virtual void end_shop(Actor&)=0;
    virtual void cancel_vehicle(Actor&)=0;
    virtual MobRuntime* find_mob(Actor&,std::uint32_t)=0;
    virtual Actor* find_user(Actor&,std::uint32_t)=0;
    virtual void item_drop_by_user(Actor& victim,Actor* attacker)=0;
    virtual std::uint8_t party_kind(PartyRegistryParty*) const noexcept=0;
};
// Uses actual Actor damage maximum and retained full catalog for mob names;
// missing resource/metadata/manager/transport owners remain required ports.
void mob_targeting_death_ordinary(Actor&,std::int32_t kind,std::uint32_t source,
    const MobTargetingBindingCatalog&,MobTargetingOrdinaryDeathWorld&);
}
