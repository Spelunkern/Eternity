#pragma once
#include "eternity/game/mob_targeting_runtime.hpp"
#include "eternity/game/skills.hpp"
#include "eternity/storage/sql_server.hpp"
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
namespace eternity::game {
struct MobTargetingBindingSkill {
  std::uint16_t id = 0, detail = 0, cooldown_seconds = 0;
  std::uint8_t effect = 0, level = 0;
  std::array<std::byte, 160> record{};
  SkillDefinition
      compatible; // Embedded stable view, never a temporary/map entry.
  std::uint8_t time_damage_type = 0;
  std::array<std::uint16_t, 3> time_damage{};
  std::string_view name() const noexcept;
  void refresh_from_record() noexcept;
};
struct MobTargetingBindingAttack {
  std::uint16_t type = 0, damage = 0, damage_add = 0;
  std::uint8_t enabled = 0, range = 0, attribute = 0, special = 0;
  std::uint32_t time = 0;
  const MobTargetingBindingSkill *skill = nullptr; // Non-owning catalog slot.
};
struct MobTargetingBindingDefinition {
  std::uint16_t id = 0;
  std::uint8_t ai = 0, chase_range = 0;
  std::int32_t country = 2;
  std::array<MobTargetingBindingAttack, 3> attacks{};
  std::array<std::byte, 268> record{};
  MobTargetingBindings targeting() const;
};
// Complete normalized records and stable targeting/applied-skill projections of
// original LoadConstMobSkill 004c0be0 and LoadConstMob 004c1720. Rows use
// procedure POSITIONS, irrespective of labels. Skills load first; their stable
// slots own every definition's skill pointer. This owner must outlive
// definitions, mobs and all callbacks that use them.
class MobTargetingBindingCatalog {
public:
  static MobTargetingBindingCatalog load(std::uint16_t mob_max_id,
                                         std::uint16_t skill_max_id,
                                         const storage::ResultSet &mob_rows,
                                         const storage::ResultSet &skill_rows);
  MobTargetingBindingCatalog(MobTargetingBindingCatalog &&) noexcept = default;
  MobTargetingBindingCatalog &
  operator=(MobTargetingBindingCatalog &&) noexcept = default;
  MobTargetingBindingCatalog(const MobTargetingBindingCatalog &) = delete;
  MobTargetingBindingCatalog &
  operator=(const MobTargetingBindingCatalog &) = delete;
  const MobTargetingBindingDefinition *mob(std::uint32_t id) const noexcept;
  const MobTargetingBindingSkill *skill(std::uint32_t id) const noexcept;
  const SkillDefinition *skill_definition(std::uint32_t id) const noexcept;
  const MobTargetingBindingSkill *
  from_definition(const SkillDefinition *) const noexcept;
  std::span<const std::byte> skill_record(std::uint32_t allocated_slot) const;
  std::span<const std::byte> mob_record(std::uint32_t allocated_slot) const;
  // Serialized owner/admin access, including allocated ID-zero/empty slots.
  // Never resize/replace this owner while published references exist. After
  // ordered raw reload succeeds, publish synchronizes existing embedded views.
  std::span<std::byte> mutable_skill_record(std::uint32_t allocated_slot);
  std::span<std::byte> mutable_mob_record(std::uint32_t allocated_slot);
  void publish_skill_record(std::uint32_t allocated_slot);
  void publish_mob_record(std::uint32_t allocated_slot);
  std::size_t mob_slots() const noexcept { return mobs_.size(); }
  std::size_t skill_slots() const noexcept { return skills_.size(); }

private:
  MobTargetingBindingCatalog() = default;
  std::vector<MobTargetingBindingSkill> skills_;
  std::vector<MobTargetingBindingDefinition> mobs_;
};
} // namespace eternity::game
