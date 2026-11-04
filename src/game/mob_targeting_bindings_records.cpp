#include "eternity/game/mob_targeting_bindings_records.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
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
                   const MobTargetingBindingCatalog &services) {
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
        put(b, at + 16, type,
            4); // Token is the allocated slot, not its mutable ID field.
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

void decode(const storage::Row &row, std::span<std::byte> data,
            std::span<const Column> columns, std::uint32_t *quest) {
  if (row.size() < columns.size())
    throw std::runtime_error("Incomplete positional definition record");
  for (std::size_t i = 0; i < columns.size(); ++i) {
    const auto &c = columns[i];
    const auto &value = row[i];
    if (!value)
      continue;
    if (c.type == 1) {
      const auto n =
          std::min(value->size(), static_cast<std::size_t>(c.length - 1));
      for (std::size_t k = 0; k < n; ++k)
        data[static_cast<std::size_t>(c.offset) + k] =
            static_cast<std::byte>(static_cast<unsigned char>((*value)[k]));
      continue;
    }
    const auto width = c.type == 4 ? 4U : c.type == 5 ? 2U : 1U;
    std::int64_t number = 0;
    const auto &text = *value;
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), number);
    const std::int64_t low = width == 1   ? 0
                             : width == 2 ? INT16_MIN
                                          : INT32_MIN;
    const std::int64_t high = width == 1   ? UINT8_MAX
                              : width == 2 ? UINT16_MAX
                                           : UINT32_MAX;
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        number < low || number > high)
      throw std::runtime_error("Invalid positional definition record value");
    if (c.offset < 0) {
      if (!quest)
        throw std::logic_error("Unexpected external scratch");
      *quest = static_cast<std::uint32_t>(number);
    } else
      put(data, static_cast<unsigned>(c.offset),
          static_cast<std::uint32_t>(number), width);
  }
}
} // namespace
std::array<std::byte, 160>
decode_mob_targeting_skill_record(const storage::Row &row) {
  std::array<std::byte, 160> result{};
  decode(row, result, skill_columns, nullptr);
  result[34] = std::byte{0};
  normalize_skill(result);
  result[2] = std::byte{100};
  return result;
}
std::array<std::byte, 268>
decode_mob_targeting_mob_record(const storage::Row &row,
                                const MobTargetingBindingCatalog &skills,
                                std::uint32_t &quest) {
  std::array<std::byte, 268> result{};
  decode(row, result, mob_columns, &quest);
  result[33] = std::byte{0};
  normalize_mob(result, quest, skills);
  return result;
}
} // namespace eternity::game
