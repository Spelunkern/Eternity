#include "eternity/game/mob_targeting_reload.hpp"
#include <algorithm>
#include <array>
#include <bit>
namespace eternity::game {
namespace {
struct Column {
  int offset;
  unsigned length;
  int type;
};
// Original GetData destinations/types from 004c20c0 and 004c50c0. The mob
// quest-item scalar lives four bytes BEFORE the zeroed record buffer.
constexpr std::array<Column, 69> mob_columns{
    {{0, 2, 5},    {2, 32, 1},   {34, 2, 5},   {36, 2, 5},   {38, 1, -6},
     {40, 2, 5},   {42, 2, 5},   {-4, 1, 4},   {48, 4, 4},   {52, 2, 5},
     {54, 2, 5},   {56, 2, 5},   {58, 2, 5},   {60, 2, 5},   {62, 1, -6},
     {63, 1, -6},  {64, 1, -6},  {66, 2, 5},   {68, 2, 5},   {70, 1, -6},
     {71, 1, -6},  {72, 1, -6},  {73, 1, -6},  {74, 1, -6},  {75, 1, -6},
     {76, 1, -6},  {77, 1, -6},  {78, 1, -6},  {79, 1, -6},  {80, 1, -6},
     {81, 1, -6},  {82, 1, -6},  {83, 1, -6},  {84, 1, -6},  {85, 1, -6},
     {86, 1, -6},  {87, 1, -6},  {88, 1, -6},  {89, 1, -6},  {90, 1, -6},
     {92, 4, 4},   {96, 1, -6},  {100, 4, 4},  {104, 1, -6}, {105, 1, -6},
     {128, 2, 5},  {132, 4, 4},  {136, 1, -6}, {138, 2, 5},  {140, 2, 5},
     {142, 1, -6}, {143, 1, -6}, {130, 1, -6}, {148, 2, 5},  {152, 4, 4},
     {156, 1, -6}, {158, 2, 5},  {160, 2, 5},  {162, 1, -6}, {163, 1, -6},
     {150, 1, -6}, {168, 2, 5},  {172, 4, 4},  {176, 1, -6}, {178, 2, 5},
     {180, 2, 5},  {182, 1, -6}, {183, 1, -6}, {170, 1, -6}}};
constexpr std::array<Column, 82> skill_columns{
    {{0, 2, 5},   {2, 1, -6},   {3, 30, 1},   {38, 1, -6},  {39, 1, -6},
     {40, 1, -6}, {41, 1, -6},  {42, 1, -6},  {43, 1, -6},  {44, 1, -6},
     {82, 2, 5},  {36, 2, 5},   {45, 1, -6},  {46, 1, -6},  {47, 1, -6},
     {48, 1, -6}, {49, 1, -6},  {50, 2, 5},   {52, 2, -6},  {53, 2, -6},
     {54, 2, -6}, {55, 2, -6},  {56, 2, -6},  {57, 2, -6},  {58, 2, -6},
     {59, 2, -6}, {60, 2, -6},  {61, 2, -6},  {62, 2, -6},  {63, 2, -6},
     {64, 2, -6}, {65, 2, -6},  {66, 2, -6},  {67, 2, -6},  {70, 2, 5},
     {68, 2, 5},  {72, 1, -6},  {74, 2, 5},   {76, 1, -6},  {77, 1, -6},
     {78, 1, -6}, {80, 2, 5},   {84, 1, -6},  {85, 1, -6},  {86, 1, -6},
     {87, 1, -6}, {88, 1, -6},  {92, 2, 5},   {96, 1, -6},  {97, 1, -6},
     {98, 1, -6}, {99, 1, -6},  {100, 2, 5},  {102, 1, -6}, {104, 2, 5},
     {106, 2, 5}, {108, 2, 5},  {110, 1, -6}, {112, 2, 5},  {114, 2, 5},
     {116, 2, 5}, {118, 2, 5},  {120, 2, 5},  {122, 2, 5},  {124, 1, -6},
     {126, 2, 5}, {128, 1, -6}, {130, 2, 5},  {132, 1, -6}, {134, 2, 5},
     {136, 2, 5}, {138, 2, 5},  {140, 2, 5},  {142, 2, 5},  {144, 2, 5},
     {146, 2, 5}, {148, 1, -6}, {149, 1, -6}, {150, 1, -6}, {151, 1, -6},
     {152, 2, 5}, {154, 2, 5}}};
std::uint32_t read(std::span<const std::byte> b, unsigned offset, unsigned n) {
  std::uint32_t value = 0;
  for (unsigned i = 0; i < n; ++i)
    value |= std::to_integer<std::uint32_t>(b[offset + i]) << (8 * i);
  return value;
}
void put(std::span<std::byte> b, unsigned offset, std::uint32_t value,
         unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    b[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255U);
}
void normalize_skill(std::span<std::byte> b) {
  for (auto at : {96U, 97U})
    if (read(b, at, 1) > 19)
      put(b, at, 19, 1);
  for (auto at : {48U, 148U})
    put(b, at, std::clamp(read(b, at, 1), 1U, 3U) - 1U, 1);
  if (read(b, 78, 1) > 8)
    put(b, 78, 0, 1);
  if (read(b, 77, 1) > 15)
    put(b, 77, 0, 1);
  if (read(b, 76, 1) > 60)
    put(b, 76, 60, 1);
}
void normalize_mob(std::span<std::byte> b, std::uint32_t quest,
                   MobTargetingReloadServices &services) {
  if (quest == 0)
    put(b, 44, 0, 1);
  else {
    const auto signed_quest = std::bit_cast<std::int32_t>(quest);
    const auto type = signed_quest < 1001   ? 27U
                      : signed_quest < 2001 ? 28U
                                            : 29U;
    put(b, 44, type, 1);
    put(b, 45, quest - (type == 27 ? 0U : type == 28 ? 1000U : 2000U), 1);
  }
  for (unsigned i = 0; i < 3; ++i) {
    const auto at = 128 + i * 20;
    const auto type = read(b, at, 2), enabled = read(b, at + 2, 1);
    put(b, at + 16, 0, 4);
    if (type == 0) {
      if (enabled && read(b, at + 8, 1) > 60)
        put(b, at + 8, 60, 1);
    } else {
      put(b, at, 0, 2);
      put(b, at + 2, 0, 1);
      const auto skill = services.skill(static_cast<std::uint16_t>(type));
      if (skill && (skill->effect == 1 || skill->effect == 3 ||
                    (skill->effect == 2 &&
                     (skill->detail == 41 || skill->detail == 42)))) {
        put(b, at, type, 2);
        put(b, at + 2, enabled, 1);
        put(b, at + 16, skill->token, 4);
      }
    }
    if (i != 2 && read(b, at + 14, 1) > 4)
      put(b, at + 14, 0, 1);
  }
  if (read(b, 64, 1) > 4)
    put(b, 64, 0, 1);
  put(b, 63, 0, 1);
  for (auto at : {92U, 100U})
    if (read(b, at, 4) == 0)
      put(b, at, 1, 4);
  auto selector = read(b, 183, 1);
  if (selector > 15) {
    selector = 0;
    put(b, 183, 0, 1);
  }
  constexpr std::array<std::uint32_t, 16> times{
      5000,      30000,   60000,   180000,   300000,   600000,
      900000,    1800000, 3600000, 14400000, 43200000, 86400000,
      259200000, 7200000, 0,       604800000};
  put(b, 108, times[selector], 4);
  put(b, 112, read(b, 172, 4), 4);
  put(b, 116, read(b, 178, 2), 4);
  const auto country = read(b, 182, 1);
  put(b, 120, country == 1 ? 0U : country == 2 ? 1U : 2U, 4);
  put(b, 124, read(b, 180, 2) != 0, 1);
  if (read(b, 48, 4) == 0)
    put(b, 48, 1, 4);
}
std::vector<MobTargetingReloadWrite> fields(MobTargetingReloadKind kind) {
  std::vector<MobTargetingReloadWrite> f;
  const auto add = [&](unsigned at, unsigned n) {
    f.push_back({static_cast<std::uint16_t>(at), static_cast<std::uint8_t>(n)});
  };
  if (kind == MobTargetingReloadKind::skill) {
    add(36, 2);
    for (unsigned i = 38; i <= 49; ++i)
      add(i, 1);
    add(50, 2);
    for (unsigned i = 52; i < 68; ++i)
      add(i, 1);
    for (auto at : {68U, 70U})
      add(at, 2);
    add(72, 1);
    add(74, 2);
    for (auto at : {76U, 77U, 78U})
      add(at, 1);
    add(80, 2);
    add(82, 2);
    for (unsigned i = 84; i <= 88; ++i)
      add(i, 1);
    add(92, 4);
    for (unsigned i = 96; i <= 99; ++i)
      add(i, 1);
    add(100, 2);
    add(102, 1);
    for (auto at : {104U, 106U, 108U})
      add(at, 2);
    add(110, 1);
    for (unsigned i = 112; i <= 122; i += 2)
      add(i, 2);
    add(124, 1);
    add(126, 2);
    add(128, 1);
    add(130, 2);
    add(132, 1);
    for (unsigned i = 134; i <= 146; i += 2)
      add(i, 2);
    for (unsigned i = 148; i <= 151; ++i)
      add(i, 1);
    add(152, 2);
    add(154, 2);
    add(156, 4);
  } else {
    for (auto at : {34U, 36U})
      add(at, 2);
    add(38, 1);
    add(40, 2);
    add(42, 2);
    add(44, 1);
    add(45, 1);
    add(48, 4);
    for (unsigned i = 52; i <= 60; i += 2)
      add(i, 2);
    for (auto at : {62U, 63U, 64U})
      add(at, 1);
    add(66, 2);
    add(68, 2);
    for (unsigned i = 70; i <= 90; ++i)
      add(i, 1);
    add(92, 4);
    add(96, 1);
    add(100, 4);
    add(104, 1);
    add(105, 1);
    for (auto at : {108U, 112U, 116U, 120U})
      add(at, 4);
    add(124, 1);
    for (unsigned i = 0; i < 3; ++i) {
      const auto at = 128 + i * 20;
      add(at, 2);
      add(at + 2, 1);
      add(at + 4, 4);
      add(at + 8, 1);
      add(at + 10, 2);
      add(at + 12, 2);
      add(at + 14, 1);
      add(at + 15, 1);
      add(at + 16, 4);
    }
  }
  return f;
}
} // namespace
MobTargetingReloadResult mob_targeting_reload(MobTargetingReloadKind kind,
                                              std::uint16_t requested,
                                              std::uint32_t &changes,
                                              MobTargetingReloadServices &s) {
  MobTargetingReloadResult result;
  if (!s.profile())
    return result;
  s.parse_connection();
  if (!s.connect()) {
    s.log_error();
    return result;
  }
  if (!s.allocate()) {
    s.log_error();
    return result;
  } // Original does not disconnect/release here.
  const auto cleanup = [&] {
    s.free_database();
    s.disconnect();
    s.release_pool();
  };
  if (!s.query(kind, requested)) {
    s.log_error();
    cleanup();
    return result;
  }
  std::array<std::byte, 272> storage{};
  auto stage = std::span<std::byte>(storage).subspan(
      4, kind == MobTargetingReloadKind::skill ? 160 : 268);
  const auto columns = kind == MobTargetingReloadKind::skill
                           ? std::span<const Column>(skill_columns)
                           : std::span<const Column>(mob_columns);
  unsigned count = 0;
  while (s.fetch() != 100) {
    for (unsigned i = 0; i < columns.size(); ++i) {
      const auto &c = columns[i];
      const auto width = c.type == 4    ? 4U
                         : c.type == 5  ? 2U
                         : c.type == -6 ? 1U
                                        : c.length;
      auto destination = std::span<std::byte>(storage).subspan(
          static_cast<std::size_t>(c.offset + 4), width);
      if (!s.get_data(i + 1, c.length, c.type, destination)) {
        s.log_error();
        cleanup();
        return result;
      }
    }
    if (kind == MobTargetingReloadKind::skill)
      normalize_skill(stage);
    else
      normalize_mob(stage, read(storage, 0, 4), s);
    ++count;
  }
  cleanup();
  if (count != 1) {
    s.log_error();
    return result;
  }
  auto target = s.target(kind, static_cast<std::uint16_t>(read(stage, 0, 2)));
  if (target.size() != stage.size())
    throw std::invalid_argument("Reload requires stable complete owner record");
  const auto publication_fields = fields(kind);
  // Allocate trace storage before the first in-place write. Recording must not
  // introduce allocation failures halfway through original field publication.
  result.writes.reserve(publication_fields.size() + 1);
  for (const auto &f : publication_fields) {
    if (read(target, f.offset, f.width) == read(stage, f.offset, f.width))
      continue;
    put(target, f.offset, read(stage, f.offset, f.width), f.width);
    ++changes;
    result.writes.push_back(f);
  }
  if (kind == MobTargetingReloadKind::skill) {
    put(target, 2, 100, 1);
    result.writes.push_back({2, 1});
  }
  result.success = true;
  return result;
}
void mob_targeting_reload_skill_projection(MobTargetingBindingSkill &s,
                                           std::span<const std::byte> b) {
  if (b.size() != 160)
    throw std::invalid_argument("Expected skill reload record");
  s.effect = static_cast<std::uint8_t>(read(b, 49, 1));
  s.detail = static_cast<std::uint16_t>(read(b, 50, 2));
  s.cooldown_seconds = static_cast<std::uint16_t>(read(b, 74, 2));
  s.level = static_cast<std::uint8_t>(read(b, 2, 1));
}
void mob_targeting_reload_definition_projection(
    MobTargetingBindingDefinition &d, std::span<const std::byte> b,
    const std::function<const MobTargetingBindingSkill *(std::uint32_t)>
        &resolve) {
  if (b.size() != 268 || !resolve)
    throw std::invalid_argument("Expected complete mob reload record/resolver");
  std::array<const MobTargetingBindingSkill *, 3> skills{};
  for (unsigned i = 0; i < 3; ++i)
    if (const auto token = read(b, 144 + i * 20, 4)) {
      skills[i] = resolve(token);
      if (!skills[i])
        throw std::invalid_argument("Unresolved live reload skill token");
    }
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
    a.skill = skills[i];
  }
}
} // namespace eternity::game
