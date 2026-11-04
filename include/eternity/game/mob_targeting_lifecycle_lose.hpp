#pragma once
#include "eternity/game/duel.hpp"
#include <array>
namespace eternity::game {
class Actor;
class GuildMembership;
// Synchronous projections/ports over retained owners. User index 0 is loser,
// index 1 is winner; they may alias. No second user, guild or counter owner.
class MobTargetingLoseDeathServices {
public:
    virtual ~MobTargetingLoseDeathServices()=default;
    virtual void drop_wager(unsigned owner,unsigned recipient)=0;
    virtual void clear_debuffs(unsigned)=0;
    virtual void reset_damage(unsigned)=0;
    virtual void reset_duel_fields(unsigned)=0; // +54cc=0,+54d0=-1,+1528=0 only.
    virtual std::uint32_t counter(unsigned user,unsigned field) const noexcept=0;
    virtual void counter(unsigned user,unsigned field,std::uint32_t) noexcept=0;
    virtual std::uint32_t billing_id(unsigned) const noexcept=0;
    virtual std::uint32_t character_id(unsigned) const noexcept=0;
    virtual std::string name(unsigned) const=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(unsigned,const net::Packet&)=0;
    virtual void prepare_log(unsigned subject,net::Packet&)=0;
    virtual void send_log(const net::Packet&)=0;
    virtual GuildMembership* guild() const noexcept=0; // Pure live query: loser +1758, not guild ID.
    virtual void lock_guild(GuildMembership*)=0;
    virtual void unlock_guild(GuildMembership*) noexcept=0;
    virtual void leave_guild(GuildMembership*,std::int32_t reason)=0;
};
// Complete PvPChkLoseDeath 0047b2c0. 4=duel,5=guild,other raw types return.
// Log buffer is reused between the two sends, retaining original string tails.
void mob_targeting_pvp_lose_death(std::int32_t type,MobTargetingLoseDeathServices&,
    const std::array<std::uint8_t,99>& scratch={});
class MobTargetingLoseDeathWorld:public DuelWorld {
public:
    virtual PlayerCore& player(Actor&) const noexcept=0; // Retained authoritative record; no temporary copies.
    virtual std::uint32_t billing_id(const Actor&) const noexcept=0; // +578c, not account ID.
    virtual void send_database(const net::Packet&)=0;
    virtual GuildMembership* guild(const Actor&) const noexcept=0;
    virtual void lock_guild(GuildMembership*)=0;
    virtual void unlock_guild(GuildMembership*) noexcept=0;
    virtual void leave_guild(GuildMembership*,Actor&,std::int32_t reason)=0;
    virtual void reset_damage(Actor&); // Same Actor ledger, explicit error if API absent.
};
// Existing Actors, PlayerCore counters, wager/effect/ledger owners and transports.
void mob_targeting_pvp_lose_death(Actor& loser,Actor& winner,std::int32_t type,
    MobTargetingLoseDeathWorld&);
}
