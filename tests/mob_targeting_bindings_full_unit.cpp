#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/mob_targeting_bindings_records.hpp"
#include "eternity/game/mob_targeting_reload.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
using namespace eternity::game;
void check(bool b, const char *why) {
  if (!b)
    throw std::runtime_error(why);
}
std::vector<std::byte> unhex(const std::string &s) {
  std::vector<std::byte> b;
  for (std::size_t i = 0; i < s.size(); i += 2)
    b.push_back(
        static_cast<std::byte>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return b;
}
eternity::storage::Row row_values(std::istringstream &in) {
  eternity::storage::Row r;
  std::string v;
  while (in >> v) {
    if (v == "N")
      r.push_back(std::nullopt);
    else if (v.starts_with("H")) {
      const auto b = unhex(v.substr(1));
      r.push_back(
          std::string(reinterpret_cast<const char *>(b.data()), b.size()));
    } else
      r.push_back(v);
  }
  return r;
}
std::uint32_t number(std::span<const std::byte> b, unsigned at, unsigned n) {
  std::uint32_t x = 0;
  for (unsigned i = 0; i < n; ++i)
    x |= std::to_integer<std::uint32_t>(b[at + i]) << (8 * i);
  return x;
}
void compatible(const MobTargetingBindingCatalog &catalog, unsigned id) {
  const auto *slot = catalog.skill(id);
  if (!slot) {
    check(!catalog.skill_definition(id), "Empty compatible slot exposed");
    return;
  }
  const auto *d = catalog.skill_definition(id);
  const auto raw = catalog.skill_record(id);
  check(d == &slot->compatible && catalog.from_definition(d) == slot,
        "Compatible pointer not owned by stable slot");
  check(d->id == number(raw, 0, 2) && d->level == 100 &&
            d->effect == number(raw, 49, 1) && d->detail == number(raw, 50, 2),
        "Compatible raw level/effect differs");
  check(d->keep_time_seconds == number(raw, 92, 4) && !d->offline_countdown &&
            d->preserve_on_death == (number(raw, 151, 1) != 0),
        "Mob keep seconds wrongly normalized as user skill");
  check(slot->time_damage_type == number(raw, 110, 1),
        "Time damage type differs");
  for (unsigned i = 0; i < 3; ++i)
    check(d->abilities[i].type == number(raw, 124 + i * 4, 1) &&
              d->abilities[i].value == number(raw, 126 + i * 4, 2) &&
              slot->time_damage[i] == number(raw, 112 + i * 2, 2) &&
              d->time_heal[i] == number(raw, 142 + i * 2, 2),
          "Ability/time damage/time heal mapping differs");
  const auto *text = reinterpret_cast<const char *>(raw.data() + 3);
  std::size_t n = 0;
  while (n < 32 && text[n])
    ++n;
  check(slot->name() == std::string_view(text, n), "Stable name view differs");
}
// Test SQL/pool boundary with observed lifecycle; actual mutation is the
// existing recovered reload coordinator against the SAME catalog record
// buffers.
struct Reload final : MobTargetingReloadServices {
  MobTargetingBindingCatalog &owner;
  eternity::storage::Row row;
  int cursor = -1;
  unsigned calls = 0;
  Reload(MobTargetingBindingCatalog &o, eternity::storage::Row r)
      : owner(o), row(std::move(r)) {}
  bool profile() override {
    ++calls;
    return true;
  }
  void parse_connection() override { ++calls; }
  bool connect() override {
    ++calls;
    return true;
  }
  bool allocate() override {
    ++calls;
    return true;
  }
  bool query(MobTargetingReloadKind, std::uint16_t id) override {
    check(id == 1, "Wrong owner reload ID");
    ++calls;
    return true;
  }
  std::int16_t fetch() override { return ++cursor == 0 ? 0 : 100; }
  bool get_data(unsigned column, unsigned, int type,
                std::span<std::byte> out) override {
    const auto &v = row.at(column - 1);
    if (!v)
      return true;
    if (type == 1) {
      const auto n = std::min(v->size(), out.size() - 1);
      for (std::size_t i = 0; i < n; ++i)
        out[i] = static_cast<std::byte>(static_cast<unsigned char>((*v)[i]));
      out[n] = std::byte{0};
    } else {
      auto n = static_cast<std::uint32_t>(std::stoll(*v));
      for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<std::byte>((n >> (8 * i)) & 255U);
    }
    return true;
  }
  void free_database() override { ++calls; }
  void disconnect() override { ++calls; }
  void release_pool() override { ++calls; }
  void log_error() override {
    throw std::runtime_error("Unexpected owner reload error");
  }
  std::optional<MobTargetingReloadLink> skill(std::uint16_t id) const override {
    const auto *s = owner.skill(id);
    if (!s)
      return {};
    return MobTargetingReloadLink{id, s->effect, s->detail};
  }
  std::span<std::byte> target(MobTargetingReloadKind kind,
                              std::uint16_t id) override {
    return kind == MobTargetingReloadKind::skill
               ? owner.mutable_skill_record(id)
               : owner.mutable_mob_record(id);
  }
};
void live_owner() {
  eternity::storage::ResultSet skills, mobs;
  skills.rows.push_back(eternity::storage::Row(82, std::string("0")));
  mobs.rows.push_back(eternity::storage::Row(69, std::string("0")));
  auto &s = skills.rows[0];
  s[0] = "1";
  s[1] = "3";
  s[2] = "Old name";
  s[16] = "1";
  s[17] = "41";
  s[47] = "6";
  s[64] = "1";
  s[79] = "1";
  auto &m = mobs.rows[0];
  m[0] = "1";
  m[2] = "1";
  m[8] = "1";
  m[45] = "1";
  m[52] = "255";
  auto owner = MobTargetingBindingCatalog::load(2, 2, mobs, skills);
  const auto *slot = owner.skill(1);
  const auto *def = owner.skill_definition(1);
  const auto *mob = owner.mob(1);
  const auto *raw = owner.skill_record(1).data();
  check(def->abilities[0].value == 0 && def->keep_time_seconds == 6 &&
            !def->offline_countdown,
        "User-only ability/hour normalization leaked into mob catalog");
  auto changed = s;
  changed[2] = "Replacement ignored";
  changed[16] = "4";
  changed[47] = "99";
  changed[65] = "65535";
  changed[57] = "3";
  changed[58] = "600";
  changed[59] = "0";
  changed[60] = "65535";
  Reload reload(owner, changed);
  std::uint32_t count = 0;
  check(mob_targeting_reload(MobTargetingReloadKind::skill, 1, count, reload)
                .success &&
            reload.calls == 8,
        "Reload lifecycle/publication failed");
  owner.publish_skill_record(1);
  check(owner.skill(1) == slot && owner.skill_definition(1) == def &&
            owner.skill_record(1).data() == raw && def->effect == 4 &&
            def->keep_time_seconds == 99 && def->abilities[0].value == 65535 &&
            slot->time_damage == std::array<std::uint16_t, 3>{600, 0, 65535},
        "Live full-skill aliases lost updates or identity");
  check(slot->name() == "Old name" && mob->attacks[0].skill == slot,
        "Reload changed preserved name or cleared an existing mob link");
  Reload mob_reload(owner, m);
  check(mob_targeting_reload(MobTargetingReloadKind::mob, 1, count, mob_reload)
            .success,
        "Mob owner reload failed");
  owner.publish_mob_record(1);
  check(owner.mob(1) == mob && !mob->attacks[0].skill &&
            mob->attacks[0].enabled == 0,
        "Subsequent mob reload did not re-evaluate unsupported skill");
  SkillDefinition impostor = *def;
  check(!owner.from_definition(&impostor),
        "User/temporary definition confused with stable mob skill");
  auto moved = std::move(owner);
  check(moved.skill_definition(1) == def &&
            moved.from_definition(def) == slot &&
            moved.skill_record(1).data() == raw,
        "Owner move invalidated published views");
  // GetMobSkillInfo guards nonzero stored ID but returns the indexed slot.
  // Opaque pointer tokens must never be taken from mutable metadata IDs.
  auto altered = moved.mutable_skill_record(1);
  altered[0] = std::byte{42};
  altered[49] = std::byte{1};
  moved.publish_skill_record(1);
  std::uint32_t quest = 0;
  const auto decoded = decode_mob_targeting_mob_record(m, moved, quest);
  check(number(decoded, 144, 4) == 1 && moved.skill(1)->id == 42,
        "Skill slot identity confused with stored ID");
  check(moved.mutable_skill_record(0).size() == 160 &&
            moved.mutable_skill_record(2).size() == 160 &&
            !moved.skill_definition(2),
        "Mutable allocated slots incorrectly require admission");
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Expected full-skill oracle");
    std::ifstream input(argv[1]);
    check(input.good(), "Cannot read full-skill oracle");
    eternity::storage::ResultSet skills, mobs;
    std::optional<MobTargetingBindingCatalog> catalog;
    unsigned sm = 0, mm = 0, cases = 0, records = 0;
    std::string line;
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      std::istringstream in(line);
      char op;
      in >> op;
      if (op == 'S') {
        unsigned id;
        in >> id >> mm >> sm;
        skills = {};
        mobs = {};
        catalog.reset();
        ++cases;
      } else if (op == 'K' || op == 'M') {
        (op == 'K' ? skills : mobs).rows.push_back(row_values(in));
      } else if (op == 'T' || op == 'U') {
        if (!catalog)
          catalog.emplace(MobTargetingBindingCatalog::load(
              static_cast<std::uint16_t>(mm), static_cast<std::uint16_t>(sm),
              mobs, skills));
        unsigned id;
        std::string text;
        in >> id >> text;
        const auto expected = unhex(text);
        const auto actual =
            op == 'T' ? catalog->skill_record(id) : catalog->mob_record(id);
        if (actual.size() != expected.size() ||
            !std::equal(actual.begin(), actual.end(), expected.begin()))
          throw std::runtime_error(
              "Complete original record differs at scenario " +
              std::to_string(cases - 1) + " kind " + op + " slot " +
              std::to_string(id));
        if (op == 'T')
          compatible(*catalog, id);
        ++records;
      }
    }
    live_owner();
    check(cases == 256 && records == 3584, "Full record coverage changed");
    std::cout << cases << " original full loads, " << records
              << " complete records and stable reload aliases passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
