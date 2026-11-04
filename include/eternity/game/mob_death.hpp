#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/net/framing.hpp"

namespace eternity::game {
// Live projections supplied by the serialized user/party owners. Reward callbacks
// may update levels and party experience state; retain the original member order.
struct MobDeathUser {
    std::uint32_t id=0;std::uint16_t level=0;std::uint8_t country=0;
};
struct MobDeathParty {
    std::uint8_t type=0;std::int32_t maximum_level=0;
    std::vector<MobDeathUser*> members;
};
struct MobDeathExperience {
    MobDeathExperience();
    std::int32_t admin_mode=0,event_active=0;double multiplier=1;
    // Original static CGameData globals at 005423a8 / 005423e8. Each lookup
    // accepts counts through 30, despite overlapping tables. Defaults preserve
    // the image; explicit overrides can project changed runtime globals.
    std::array<double,31> party{},raid{};
};
class MobDeathWorld {
public:
    virtual ~MobDeathWorld()=default;
    virtual void send_death(MobRuntime&,const net::Packet&,std::uint8_t priority)=0;
    virtual MobDeathUser* find_user(MobRuntime&,std::uint32_t)=0;
    virtual MobDeathParty* party(MobDeathUser&)=0;
    virtual void lock_party(MobDeathParty&)=0;
    virtual void unlock_party(MobDeathParty&) noexcept=0;
    // CUser::EnableApplyRange: relative to the credited user, radius 100.
    virtual bool in_reward_range(MobDeathUser& credited,MobDeathUser& member,std::int32_t radius)=0;
    // Required live actor owner calls Actor::gain_experience, with bypass=false.
    virtual void add_experience(MobDeathUser&,std::uint32_t source,std::int32_t amount)=0;
    virtual void drop_items(MobRuntime&,MobDeathUser*,MobDeathParty*)=0;
    virtual void quest_kill(MobDeathUser&,MobRuntime&)=0;
    virtual std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t type)=0;
    virtual void destroy_obelisk(MobRuntime&,std::int32_t country,std::int32_t type)=0;
    virtual void destroy_linked_group(MobRuntime&,std::int32_t type)=0;
    virtual void remove_obelisk_member(MobRuntime&,void* source)=0;
    virtual void remove_linked_member(MobRuntime&,void* source)=0;
    // SendLogBothMob event 4, previous attack skill and pre-reset fair flag.
    virtual void log_death(MobRuntime&,MobDeathUser* credited,bool fair)=0;
    virtual void clear_applied_skills(MobRuntime&)=0;
};
// ps_game.exe CMob::Death 004910f0. Coordinates rewards and ownership; it does
// not set FSM death state, unlink/recycle the corpse or supply missing services.
// Callbacks must retain the subject, damage owner and credited user/party storage.
void mob_death(MobRuntime&,std::int32_t attacker_type,std::uint32_t attacker_id,const MobDeathExperience&,MobDeathWorld&);
}
