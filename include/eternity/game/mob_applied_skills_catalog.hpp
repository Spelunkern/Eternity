#pragma once
#include "eternity/game/mob_applied_skills_application.hpp"
#include "eternity/game/mob_applied_skills_expiration.hpp"
#include "eternity/game/mob_applied_skills_time_damage.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
namespace eternity::game {
// Actual world ports; no clock/combat/death/transport/log/rebuild defaults.
class MobAppliedSkillsCatalogPorts {
public:
    virtual ~MobAppliedSkillsCatalogPorts()=default;
    virtual std::uint32_t tick() noexcept=0;
    virtual void log(const MobAppliedSkill&,std::string_view live_name,std::uint8_t action) noexcept=0;
    virtual void rebuild_attack(MobRuntime&) noexcept=0;
    virtual void send_speed(MobRuntime&) noexcept=0;
    virtual bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&) noexcept=0;
    virtual bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t) noexcept=0;
    virtual void death(MobRuntime&,std::uint32_t,std::uint32_t) noexcept=0;
    virtual void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t) noexcept=0;
};
// Per-mob retained bridge. Catalog, pool, subject, shared ID counter and ports
// outlive this owner (and catalog outlives pooled nodes). Serialize all subject
// operations and ordered raw reload + publish under external world quiescence.
// Never replace/move-assign the catalog after publishing references. Callbacks
// may recurse into this owner, but must not reload its catalog mid-operation.
class MobAppliedSkillsCatalog final:private MobAppliedSkillLogServices,
    private MobAppliedSkillApplicationServices,private MobAppliedSkillEffectServices,
    private MobAppliedSkillDamageServices,private MobAppliedSkillRemovalServices {
public:
    MobAppliedSkillsCatalog(const MobTargetingBindingCatalog&,MobAppliedSkillPool&,MobRuntime&,
                            std::atomic<std::uint32_t>&,MobAppliedSkillsCatalogPorts&);
    // False means absent catalog slot; true delegates admission/filter/refresh
    // decisions to the original owner, including successful rejection paths.
    bool apply(std::uint32_t allocated_slot,std::uint32_t attacker_type,std::uint32_t attacker_id);
    void keep(std::uint32_t now);
    void expire(std::uint32_t now);
    void clear();
    // For synchronous Death/Clear coordination only; do not admit foreign definitions.
    MobAppliedSkills& owner() noexcept{return owner_;}
    std::string_view name(const SkillDefinition&)const noexcept;
private:
    const MobTargetingBindingSkill& slot(const SkillDefinition&)const noexcept;
    void synchronize_names();
    std::uint8_t base_attribute()const noexcept;
    std::uint32_t tick()noexcept override;
    void removed(const MobAppliedSkill&,std::uint8_t)noexcept override;
    void added(const MobAppliedSkill&,std::uint8_t)noexcept override;
    void rebuild_attack(MobRuntime&)noexcept override;
    void send_speed(MobRuntime&)noexcept override;
    void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&)noexcept override;
    MobAppliedSkillDamage time_damage(MobRuntime&,const SkillDefinition&,std::uint32_t)noexcept override;
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override;
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override;
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override;
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override;
    void remove_effects(MobAppliedSkill&)noexcept override;
    const MobTargetingBindingCatalog& catalog_;MobRuntime& mob_;
    std::atomic<std::uint32_t>& counter_;
    MobAppliedSkillsCatalogPorts& ports_;MobAppliedSkills owner_;
};
}
