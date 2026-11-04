#pragma once
#include "eternity/game/mob_combat_scheduler_actor.hpp"
#include <array>
#include <memory>
#include <span>
namespace eternity::game {
// Borrowed projection from the real user definition owner, NOT a catalog.
// Both the definition and original name storage outlive all runtimes/poll leases.
struct UserSkillDefinition {const SkillDefinition& effect;std::string_view name;};
struct UserSkillRuntimeSkill:MobCombatSchedulerSkillBinding,std::enable_shared_from_this<UserSkillRuntimeSkill> {
    std::uint32_t instance_id=0,skill_id=0,level=0,number=0;
    std::uint32_t attack_tick=0,reset_tick=0,end_tick=0,previous_tick=0,attacker_type=0,attacker_id=0;
    const UserSkillDefinition* definition=nullptr;
};
using UserSkillHandle=std::shared_ptr<UserSkillRuntimeSkill>;
class UserSkillRuntimeServices {
public:
    virtual ~UserSkillRuntimeServices()=default;
    virtual const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)=0;
    // Actual shared CObjectMgr::CreateSkill, 0040a780: pooled stale fields and
    // retained ID; assign from manager's shared counter ONLY when ID was zero.
    virtual UserSkillHandle allocate()=0;
    // Original ~CUser deletes learned objects, rather than FreeSkill recycling.
    // A captured action lease defers physical destruction until its return.
    virtual void destroy(UserSkillHandle) noexcept=0;
    // Original overwrites/outside-count objects are unreachable, not recycled.
    // Actual manager must retain/account them; never silently recycle them.
    virtual void strand(UserSkillHandle) noexcept=0;
    virtual void invalid_definition(std::uint16_t,std::uint8_t)=0;
    virtual void disconnect(std::uint32_t error,std::uint32_t detail)=0;
};
enum class UserSkillLoadResult {loaded,disconnect_requested};
class UserSkillRuntime {
public:
    UserSkillRuntime(std::shared_ptr<Actor>,UserSkillRuntimeServices&);
    ~UserSkillRuntime();
    UserSkillRuntime(const UserSkillRuntime&)=delete;UserSkillRuntime& operator=(const UserSkillRuntime&)=delete;
    // Complete 0504 wire packet: opcode, DWORD account, BYTE count, six-byte
    // {WORD ID,BYTE level,BYTE number,WORD delay_seconds} records.
    UserSkillLoadResult load(std::span<const std::uint8_t>,std::uint32_t admission_tick);
    Actor& actor() const noexcept{return *actor_;}
    std::int32_t learned_count() const noexcept{return count_;}
    std::uint32_t& selection() noexcept{return selection_;}
    std::uint32_t selection()const noexcept{return selection_;}
    UserSkillHandle slot(std::uint32_t) const; // Poll ignores learned count.
    UserSkillHandle selected() const;
    UserSkillHandle request_slot(std::uint8_t) const; // PacketPC signed count gate.
    // ResetCharacter 0044a050 clears count/index BEFORE learned free loop;
    // slots remain. Deliberately not an eager clear/recycle operation.
    void reset_character() noexcept;
private:
    std::shared_ptr<Actor> actor_;UserSkillRuntimeServices& services_;
    std::int32_t count_=0;std::uint32_t selection_=0;
    std::array<UserSkillHandle,256> slots_{};
};
// Bound alongside the same Actor, no global side map. Real world subclasses
// implement actions using the captured actual runtime, never a SkillID lookup.
class UserSkillRuntimeWorld:public MobCombatSchedulerActorWorld {
public:
    explicit UserSkillRuntimeWorld(UserSkillRuntime& owner):owner_(owner){}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&)const final;
    void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t)final;
    void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t)final;
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)final;
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t)final;
    virtual void use_runtime_mob(Actor&,MobRuntime*,UserSkillHandle,std::uint32_t)=0;
    virtual void attack_runtime_mob(Actor&,MobRuntime*,UserSkillHandle,std::uint32_t)=0;
    virtual void use_runtime_user(Actor&,Actor&,UserSkillHandle,std::uint32_t)=0;
    virtual void attack_runtime_user(Actor&,Actor&,UserSkillHandle,std::uint32_t)=0;
private:
    UserSkillHandle capture(Actor&,const MobCombatSchedulerSkillBinding&)const;
    UserSkillRuntime& owner_;
};
}
