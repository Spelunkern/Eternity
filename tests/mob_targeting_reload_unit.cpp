#include "eternity/game/mob_targeting_reload.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity::game;
using Bytes = std::vector<std::byte>;
void check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
Bytes unhex(const std::string &text) {
  Bytes b;
  for (std::size_t i = 0; i < text.size(); i += 2)
    b.push_back(
        static_cast<std::byte>(std::stoul(text.substr(i, 2), nullptr, 16)));
  return b;
}
struct Services final : MobTargetingReloadServices {
  unsigned mode = 0, width = 0;
  int row = -1;
  std::uint32_t changes = 0;
  Bytes record;
  std::vector<eternity::storage::Row> rows;
  std::array<std::optional<MobTargetingReloadLink>, 9> links;
  std::string events;
  void event(std::string e) { events += e + ' '; }
  bool profile() override {
    event("P");
    return mode != 1;
  }
  void parse_connection() override { event("S"); }
  bool connect() override {
    event("C");
    return mode != 2;
  }
  bool allocate() override {
    event("A");
    return mode != 3;
  }
  bool query(MobTargetingReloadKind, std::uint16_t id) override {
    check(id == 7, "Wrong reload request ID");
    event("Q7");
    return mode != 4;
  }
  std::int16_t fetch() override {
    ++row;
    const auto status = (mode == 10 && row == 0 || mode == 11 && row == 1) ? -1
                        : static_cast<unsigned>(row) < rows.size() ? 0
                                                                   : 100;
    event("F" + std::to_string(status));
    return static_cast<std::int16_t>(status);
  }
  bool get_data(unsigned column, unsigned, int type,
                std::span<std::byte> destination) override {
    event("G" + std::to_string(column));
    if (((mode == 7 || mode == 10) && column == 1) ||
        (mode == 8 && column == width / 2) || (mode == 9 && column == width) ||
        (mode == 11 && row == 1) ||
        (mode == 16 && row == 1 && column == width / 2))
      return false;
    const auto &value = rows.at(static_cast<std::size_t>(row)).at(column - 1);
    if (!value)
      return true;
    if (type == 1) {
      destination[0] = std::byte{'R'};
      destination[1] = std::byte{0};
      return true;
    }
    const auto number = static_cast<std::uint32_t>(std::stoll(*value));
    for (std::size_t i = 0; i < destination.size(); ++i)
      destination[i] = static_cast<std::byte>((number >> (8 * i)) & 255U);
    return true;
  }
  void free_database() override { event("B"); }
  void disconnect() override { event("D"); }
  void release_pool() override {
    event("R");
    if (mode == 13 || mode == 17) {
      record.at(38) ^= std::byte{255};
      changes = 97;
    }
  }
  void log_error() override { event("E"); }
  std::optional<MobTargetingReloadLink> skill(std::uint16_t id) const override {
    return id > 0 && id < links.size() ? links[id] : std::nullopt;
  }
  std::span<std::byte> target(MobTargetingReloadKind,
                              std::uint16_t id) override {
    check(id == 1, "Reload must publish RETURNED row ID");
    return record;
  }
};
void projection_check() {
  // Existing skill storage and published definition pointer survive reload.
  MobTargetingBindingSkill skill{1, 41, 65, 2, 100};
  MobTargetingBindingDefinition d;
  d.id = 1;
  d.attacks[0].skill = &skill;
  const auto *address = &skill;
  Bytes raw(160);
  raw[2] = std::byte{100};
  raw[49] = std::byte{3};
  raw[50] = std::byte{42};
  raw[74] = std::byte{255};
  raw[75] = std::byte{255};
  mob_targeting_reload_skill_projection(skill, raw);
  check(d.attacks[0].skill == address &&
            d.targeting().attacks[0].cooldown_seconds == 65535 &&
            skill.id == 1 && skill.effect == 3,
        "Published skill pointer/live cooldown changed identity");
  Bytes mob(268);
  mob[38] = std::byte{3};
  mob[105] = std::byte{60};
  mob[120] = std::byte{1};
  mob[128] = std::byte{1};
  mob[130] = std::byte{255};
  mob[144] = std::byte{1};
  const auto *definition = &d;
  mob_targeting_reload_definition_projection(d, mob, [&](std::uint32_t token) {
    return token == 1 ? address : nullptr;
  });
  check(&d == definition && d.id == 1 && d.attacks[0].enabled == 255 &&
            d.attacks[0].skill == address && d.country == 1,
        "Definition reload projection lost owner/raw enabled");
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Expected reload oracle");
    std::ifstream input(argv[1]);
    check(input.good(), "Cannot open reload oracle");
    std::string line;
    Services s;
    unsigned kind = 0, caseid = 0, count = 0;
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == '#')
        continue;
      std::istringstream row(line);
      char op;
      row >> op;
      if (op == 'S') {
        s = Services{};
        std::string before;
        row >> caseid >> kind >> s.mode >> s.changes >> before;
        s.record = unhex(before);
        s.width = kind ? 82 : 69;
      } else if (op == 'L') {
        unsigned token, id, effect, detail;
        row >> token >> id >> effect >> detail;
        if (id)
          s.links.at(token) =
              MobTargetingReloadLink{token, static_cast<std::uint8_t>(effect),
                                     static_cast<std::uint16_t>(detail)};
      } else if (op == 'V') {
        eternity::storage::Row values;
        std::string value;
        while (row >> value)
          values.push_back(value == "N" ? eternity::storage::Cell{}
                                        : eternity::storage::Cell{value});
        s.rows.push_back(std::move(values));
        continue;
      } else if (op == 'O') {
        unsigned success;
        std::uint32_t changes;
        std::string after;
        row >> success >> changes >> after;
        char delimiter;
        row >> delimiter;
        check(delimiter == '|', "Malformed reload result");
        std::string writes, events;
        std::getline(row, writes, '|');
        std::getline(row, events);
        const auto result = mob_targeting_reload(
            kind ? MobTargetingReloadKind::skill : MobTargetingReloadKind::mob,
            7, s.changes, s);
        std::ostringstream actual_writes;
        for (const auto &w : result.writes)
          actual_writes << w.offset << ':' << unsigned(w.width) << ' ';
        const auto same = [](const std::string &a, const std::string &b) {
          std::istringstream x(a), y(b);
          std::string p, q;
          while (x >> p)
            if (!(y >> q) || p != q)
              return false;
          return !(y >> q);
        };
        if (result.success != (success != 0) || s.changes != changes ||
            s.record != unhex(after) || !same(actual_writes.str(), writes) ||
            !same(s.events, events)) {
          std::size_t at = 0;
          const auto expected = unhex(after);
          while (at < expected.size() && s.record[at] == expected[at])
            ++at;
          throw std::runtime_error(
              "Original reload differs at scenario " + std::to_string(caseid) +
              " byte " + std::to_string(at) + " changes " +
              std::to_string(s.changes) + "/" + std::to_string(changes) +
              " writes " + actual_writes.str());
        }
        ++count;
        continue;
      } else
        throw std::runtime_error("Unknown reload fixture operation");
      check(static_cast<bool>(row), "Malformed reload fixture");
    }
    projection_check();
    check(count == 432, "Reload coverage changed");
    std::cout << count
              << " complete original reload traces, byte publication order and "
                 "counters passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
