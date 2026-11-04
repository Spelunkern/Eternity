#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/mob_targeting_bindings_records.hpp"
#include <bit>
namespace eternity::game {
namespace {
std::uint32_t read(std::span<const std::byte> b, unsigned at,
                   unsigned width) noexcept {
  std::uint32_t n = 0;
  for (unsigned i = 0; i < width; ++i)
    n |= std::to_integer<std::uint32_t>(b[at + i]) << (8 * i);
  return n;
}
} // namespace
std::string_view MobTargetingBindingSkill::name() const noexcept {
  const auto *text = reinterpret_cast<const char *>(record.data() + 3);
  std::size_t n = 0;
  while (n < 32 && text[n] != '\0')
    ++n;
  return {text, n};
}
void MobTargetingBindingSkill::refresh_from_record() noexcept {
  const auto b = std::span<const std::byte>(record);
  const auto byte = [&](unsigned at) {
    return static_cast<std::uint8_t>(read(b, at, 1));
  };
  const auto word = [&](unsigned at) {
    return static_cast<std::uint16_t>(read(b, at, 2));
  };
  id = word(0);
  level = byte(2);
  effect = byte(49);
  detail = word(50);
  cooldown_seconds = word(74);
  SkillDefinition s;
  s.id = id;
  s.level = level;
  s.effect = effect;
  s.detail = detail;
  s.aggro = word(100);
  s.change_type = word(152);
  s.change_level = word(154);
  s.weapon1 = byte(96);
  s.weapon2 = byte(97);
  s.weapon_value = byte(98);
  s.bag = byte(99);
  s.attribute = byte(78);
  s.defense_type = byte(148);
  s.defense_value = byte(149);
  s.apply_range = byte(87);
  s.limit_hp = byte(150);
  s.keep_time_seconds = read(b, 92, 4);
  s.offline_countdown = false;
  s.preserve_on_death = byte(151) != 0;
  for (unsigned i = 0; i < 3; ++i) {
    s.abilities[i] = {byte(124 + i * 4), word(126 + i * 4)};
    s.time_heal[i] = word(142 + i * 2);
    time_damage[i] = word(112 + i * 2);
  }
  time_damage_type = byte(110);
  compatible = s; // Same member address, live retained aliases.
}
MobTargetingBindings MobTargetingBindingDefinition::targeting() const {
  MobTargetingBindings result;
  result.ai = ai;
  for (std::size_t i = 0; i < attacks.size(); ++i) {
    const auto &a = attacks[i];
    result.attacks[i] = {a.enabled != 0, a.skill != nullptr,
                         a.skill ? a.skill->cooldown_seconds
                                 : std::uint16_t{0}};
  }
  return result;
}
MobTargetingBindingCatalog MobTargetingBindingCatalog::load(
    std::uint16_t mob_max_id, std::uint16_t skill_max_id,
    const storage::ResultSet &mob_rows, const storage::ResultSet &skill_rows) {
  MobTargetingBindingCatalog result;
  result.skills_.resize(static_cast<std::size_t>(skill_max_id) + 1);
  result.mobs_.resize(static_cast<std::size_t>(mob_max_id) + 1);
  for (const auto &row : skill_rows.rows) {
    auto record = decode_mob_targeting_skill_record(row);
    const auto id = read(record, 0, 2);
    if (id >= result.skills_.size())
      throw std::runtime_error(
          "Mob skill row exceeds original MaxID allocation");
    result.skills_[id].record = record;
    result.publish_skill_record(id);
  }
  std::uint32_t quest = 0;
  for (const auto &row : mob_rows.rows) {
    auto record = decode_mob_targeting_mob_record(row, result, quest);
    const auto id = read(record, 0, 2);
    if (id >= result.mobs_.size())
      throw std::runtime_error("Mob row exceeds original MaxID allocation");
    result.mobs_[id].record = record;
    result.publish_mob_record(id);
  }
  return result;
}
const MobTargetingBindingDefinition *
MobTargetingBindingCatalog::mob(std::uint32_t id) const noexcept {
  return id < mobs_.size() && mobs_[id].id != 0 ? &mobs_[id] : nullptr;
}
const MobTargetingBindingSkill *
MobTargetingBindingCatalog::skill(std::uint32_t id) const noexcept {
  // Original GetMobSkillInfo permits one-past COUNT=MaxID+1. That undefined
  // allocation-adjacent read cannot manufacture a valid native skill slot.
  return id > 0 && id < skills_.size() && skills_[id].id != 0 ? &skills_[id]
                                                              : nullptr;
}
const SkillDefinition *
MobTargetingBindingCatalog::skill_definition(std::uint32_t id) const noexcept {
  const auto *s = skill(id);
  return s ? &s->compatible : nullptr;
}
const MobTargetingBindingSkill *MobTargetingBindingCatalog::from_definition(
    const SkillDefinition *definition) const noexcept {
  if (!definition)
    return nullptr;
  for (const auto &slot : skills_)
    if (slot.id != 0 && &slot.compatible == definition)
      return &slot;
  return nullptr;
}
std::span<const std::byte>
MobTargetingBindingCatalog::skill_record(std::uint32_t id) const {
  return skills_.at(id).record;
}
std::span<const std::byte>
MobTargetingBindingCatalog::mob_record(std::uint32_t id) const {
  return mobs_.at(id).record;
}
std::span<std::byte>
MobTargetingBindingCatalog::mutable_skill_record(std::uint32_t id) {
  return skills_.at(id).record;
}
std::span<std::byte>
MobTargetingBindingCatalog::mutable_mob_record(std::uint32_t id) {
  return mobs_.at(id).record;
}
void MobTargetingBindingCatalog::publish_skill_record(std::uint32_t id) {
  skills_.at(id).refresh_from_record();
}
void MobTargetingBindingCatalog::publish_mob_record(std::uint32_t id) {
  auto &d = mobs_.at(id);
  const auto b = std::span<const std::byte>(d.record);
  std::array<const MobTargetingBindingSkill *, 3> refs{};
  for (unsigned i = 0; i < 3; ++i)
    if (const auto token = read(b, 144 + i * 20, 4)) {
      refs[i] = skill(token);
      if (!refs[i])
        throw std::runtime_error("Unresolved stable mob skill slot token");
    }
  d.id = static_cast<std::uint16_t>(read(b, 0, 2));
  d.ai = static_cast<std::uint8_t>(read(b, 38, 1));
  d.chase_range = static_cast<std::uint8_t>(read(b, 105, 1));
  d.country = std::bit_cast<std::int32_t>(read(b, 120, 4));
  for (unsigned i = 0; i < 3; ++i) {
    const auto at = 128 + i * 20;
    auto &a = d.attacks[i];
    a.type = static_cast<std::uint16_t>(read(b, at, 2));
    a.enabled = static_cast<std::uint8_t>(read(b, at + 2, 1));
    a.time = read(b, at + 4, 4);
    a.range = static_cast<std::uint8_t>(read(b, at + 8, 1));
    a.damage = static_cast<std::uint16_t>(read(b, at + 10, 2));
    a.damage_add = static_cast<std::uint16_t>(read(b, at + 12, 2));
    a.attribute = static_cast<std::uint8_t>(read(b, at + 14, 1));
    a.special = static_cast<std::uint8_t>(read(b, at + 15, 1));
    a.skill = refs[i];
  }
}
} // namespace eternity::game
