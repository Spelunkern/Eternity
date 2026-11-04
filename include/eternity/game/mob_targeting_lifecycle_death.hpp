#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/game_log.hpp"
#include "eternity/game/npcs.hpp"
#include <array>
#include <optional>
namespace eternity::game {
struct PartyRegistryParty;
// Explicit projection of split lifecycle owners, never a second life owner.
struct MobTargetingDeathState {
    std::int32_t life=0,penalty=0,amulet=0,no_exp_loss=0;
    std::uint32_t soul=0,id=0,pending_move=0,rebirth_deadline=0,rebirth_code=0;
    PlayerResources current{}; // Observation only: DeathByNPC never zeroes it.
    PartyRegistryParty* party=nullptr;
    bool in_zone=false;
};
class MobTargetingNpcDeathServices {
public:
    virtual ~MobTargetingNpcDeathServices()=default;
    // Called with 110 bytes; fill the common 74-byte log header. Other bytes
    // retain caller scratch provenance; native Actor wrapper starts them zero.
    virtual void prepare_game_log(net::Packet&)=0;
    virtual std::optional<std::array<std::uint8_t,32>> npc_name(std::int32_t type,std::uint32_t id)=0;
    virtual void send_game_log(const net::Packet&)=0;
    virtual void pvp_leave_by_death()=0;
    virtual void clear_effects()=0;
    virtual void reset_damage()=0;
    virtual void enter_dead()=0;
    virtual std::uint32_t tick()=0;
    virtual void send_death_view(const net::Packet&,std::uint32_t excluded_user)=0;
    // Must retain captured party through unlock; actual recursive exclusion.
    virtual void lock_party(PartyRegistryParty*)=0;
    virtual void unlock_party(PartyRegistryParty*) noexcept=0;
    // This service context retains the ORIGINAL subject for 00445680 exclusion.
    virtual void send_party(PartyRegistryParty*,const net::Packet&)=0;
};
// Complete DeathByNPC 00458fe0. Log scratch seed is explicit because the
// original leaves bytes unwritten on NPC lookup miss. No resource zeroing,
// exchange/shop/vehicle cancellation or PvP rewards are implied here.
void mob_targeting_death_by_npc(MobTargetingDeathState&,std::int32_t npc_type,
    std::uint32_t npc_id,MobTargetingNpcDeathServices&,
    const std::array<std::uint8_t,110>& scratch={});
// Ordered NPC tail. Death shares initial stages but has a DISTINCT raid
// notification branch: do not reuse this NPC-specific 0c06 tail blindly.
// SendDeath 00458470 emits 0504 with the subject excluded, no self send.
void mob_targeting_npc_death_finish(MobTargetingDeathState&,std::int32_t source_type,
    std::uint32_t source_id,MobTargetingNpcDeathServices&);
// Required root projections for existing state not publicly exposed by Actor.
// Accessors are silent, non-reentrant authoritative references; no shadow map.
class MobTargetingNpcDeathWorld {
public:
    virtual ~MobTargetingNpcDeathWorld()=default;
    virtual std::int32_t& death_penalty(Actor&) noexcept=0; // +145c
    virtual std::uint32_t& soul_state(Actor&) noexcept=0; // +1430, NOT +1434/+1448
    virtual std::uint32_t& rebirth_deadline(Actor&) noexcept=0; // +1454
    virtual std::uint32_t& rebirth_code(Actor&) noexcept=0; // +1458
    virtual PartyRegistryParty* party(const Actor&) const noexcept=0;
    virtual GameLogSubject subject(const Actor&) const=0;
    virtual std::chrono::sys_seconds local_calendar()=0;
    virtual void send_game_log(const net::Packet&)=0;
    virtual void pvp_leave_by_death(Actor&)=0;
    // Actual Actor ledger Reset, not end_duel/recover or another ledger.
    virtual void reset_damage(Actor&)=0;
    virtual void enter_dead(Actor&)=0;
    virtual void send_death_view(Actor&,const net::Packet&,std::uint32_t excluded_user)=0;
    virtual void lock_party(PartyRegistryParty*)=0;
    virtual void unlock_party(PartyRegistryParty*) noexcept=0;
    virtual void send_party(PartyRegistryParty*,Actor& excluded_subject,const net::Packet&)=0;
};
// Borrows real Actor, existing immutable NPC catalog and real root services.
// Caller retains Actor/effects/catalog/captured parties through all callbacks.
void mob_targeting_death_by_npc(Actor&,std::int32_t npc_type,std::uint32_t npc_id,
    const NpcDefinitions&,MobTargetingNpcDeathWorld&);
}
