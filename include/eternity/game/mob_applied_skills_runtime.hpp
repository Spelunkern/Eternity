#pragma once
#include "eternity/game/mob_applied_skills_catalog.hpp"
namespace eternity::game {
class MobPoolServices;
class MobAppliedSkillsRuntimeServices {
public:
    virtual ~MobAppliedSkillsRuntimeServices()=default;
    // Retained per-subject ports, not a temporary service object. All ports and
    // their world owners outlive every mob in the bound MobPool.
    virtual MobAppliedSkillsCatalogPorts& ports(MobRuntime&)=0;
};
// Borrow the ONE existing world catalog, skill heap and manager ID counter.
// This context and its services outlive the bound pool. World serialization,
// deferred subject destruction through callbacks and reload quiescence required.
class MobAppliedSkillsRuntime {
public:
    MobAppliedSkillsRuntime(const MobTargetingBindingCatalog& c,MobAppliedSkillPool& p,
        std::atomic<std::uint32_t>& ids,MobAppliedSkillsRuntimeServices& s):catalog_(c),pool_(p),ids_(ids),services_(s){}
private:
    friend class MobPool;
    void bind(MobRuntime&,MobPoolServices&);
    const MobTargetingBindingCatalog& catalog_;MobAppliedSkillPool& pool_;
    std::atomic<std::uint32_t>& ids_;MobAppliedSkillsRuntimeServices& services_;
};
class MobAppliedSkillsRuntimeResetServices {
public:
    virtual ~MobAppliedSkillsRuntimeResetServices()=default;
    // Original 10-byte 0305 current HP view packet; only called with a zone.
    virtual void send_hp(MobRuntime&,const std::array<std::uint8_t,10>&)=0;
    virtual void send_speed(MobRuntime&)=0;
};
// Full ResetAttack 004905c0, separate from InitAttack/reset_mob_attack.
// Restores resources/control before ClearApplySkill, HP view, speed, SetAttack.
void reset_mob_applied_skills_runtime(MobRuntime&,MobAppliedSkillsRuntimeResetServices&);
// Shared FreeMob/destructor coordination. No Lua VM/backend is fabricated.
void release_mob_applied_skills_runtime_ai(MobRuntime&,MobPoolServices&);
}
