#pragma once
#include "eternity/game/mob_targeting_runtime.hpp"
#include "eternity/storage/sql_server.hpp"
#include <vector>
namespace eternity::game {
struct MobTargetingBindingSkill {
    std::uint16_t id=0,detail=0,cooldown_seconds=0;
    std::uint8_t effect=0,level=0;
};
struct MobTargetingBindingAttack {
    std::uint16_t type=0,damage=0,damage_add=0;
    std::uint8_t enabled=0,range=0,attribute=0,special=0;
    std::uint32_t time=0;
    const MobTargetingBindingSkill* skill=nullptr; // Non-owning catalog slot.
};
struct MobTargetingBindingDefinition {
    std::uint16_t id=0;
    std::uint8_t ai=0,chase_range=0;
    std::int32_t country=2;
    std::array<MobTargetingBindingAttack,3> attacks{};
    MobTargetingBindings targeting() const;
};
// Targeting-only projection of original LoadConstMobSkill 004c0be0 and
// LoadConstMob 004c1720. Rows use procedure POSITIONS, irrespective of labels.
// Skills load first; their stable slots own every definition's skill pointer.
// This owner must outlive definitions, mobs and all callbacks that use them.
class MobTargetingBindingCatalog {
public:
    static MobTargetingBindingCatalog load(std::uint16_t mob_max_id,std::uint16_t skill_max_id,
        const storage::ResultSet& mob_rows,const storage::ResultSet& skill_rows);
    MobTargetingBindingCatalog(MobTargetingBindingCatalog&&) noexcept=default;
    MobTargetingBindingCatalog& operator=(MobTargetingBindingCatalog&&) noexcept=default;
    MobTargetingBindingCatalog(const MobTargetingBindingCatalog&)=delete;
    MobTargetingBindingCatalog& operator=(const MobTargetingBindingCatalog&)=delete;
    const MobTargetingBindingDefinition* mob(std::uint32_t id) const noexcept;
    const MobTargetingBindingSkill* skill(std::uint32_t id) const noexcept;
    std::size_t mob_slots() const noexcept {return mobs_.size();}
    std::size_t skill_slots() const noexcept {return skills_.size();}
private:
    MobTargetingBindingCatalog()=default;
    std::vector<MobTargetingBindingSkill> skills_;
    std::vector<MobTargetingBindingDefinition> mobs_;
};
}
