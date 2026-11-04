#include "eternity/game/party_runtime.hpp"
#include "eternity/game/level_up_notice.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace eternity::game;
namespace {
void require(bool value) { if (!value) throw std::runtime_error("Party level-up integration mismatch"); }
struct World final : LevelUpNoticeWorld {
    PartyRuntime party;
    std::vector<std::optional<std::uint16_t>> members;
    unsigned membership{};
    bool locked{};
    std::int64_t observed=-1;
    std::string events;
    void rebuild_attack() override {}
    void send_database(const eternity::net::Packet&) override {}
    void send_self(const eternity::net::Packet&) override {}
    bool in_zone() const override { return false; }
    void send_scene(const eternity::net::Packet&, float, std::uint8_t) override { require(false); }
    bool acquire_party() override {
        require(!locked);
        if (!membership) return false;
        events += '[';
        if (membership == 2) { events += ']'; return false; }
        locked=true;
        return true;
    }
    std::uint8_t party_type() const override { require(locked); return party.kind; }
    void update_party_experience() override {
        require(locked); events += 'R'; party.check_experience_level(members);
    }
    void send_party(const eternity::net::Packet& packet) override {
        require(locked);
        observed=party.experience_level;
        events += 'T';
        constexpr char hex[]="0123456789abcdef";
        for (const auto byte : packet) { events += hex[byte >> 4]; events += hex[byte & 15]; }
    }
    void release_party() override { require(locked); locked=false; events += ']'; }
    void energy_all_max() override { require(!locked); }
    void recover_add(PlayerResources) override { require(!locked); }
    void fill_game_log_main(std::span<std::uint8_t>) override {}
    void send_game_log(const eternity::net::Packet&) override {}
    void update_guild_level(std::uint32_t, std::uint16_t) override { require(!locked); }
};
}
int main(int argc, char** argv) {
    try {
        require(argc == 2);
        std::ifstream input(argv[1]); require(input.good());
        eternity::storage::ResultSet rows; rows.columns={"Level", "Grow", "Exp"};
        for (unsigned grow=1; grow<=4; ++grow)
            for (unsigned level=1; level<=60; ++level)
                rows.rows.push_back({std::to_string(level), std::to_string(grow), "1000"});
        const auto curve=Experience::load(rows);
        std::string line; unsigned cases=0;
        while (std::getline(input, line)) {
            if (line.empty() || line[0]=='#') continue;
            std::istringstream row(line); World world;
            unsigned kind, level, count; std::uint32_t actor_id, expected;
            std::int64_t observed; std::string events;
            row >> kind >> world.party.experience_level >> world.membership >> actor_id >> level >> count;
            world.party.kind=static_cast<std::uint8_t>(kind);
            for (unsigned i=0; i<count; ++i) {
                int value; row >> value;
                world.members.push_back(value < 0 ? std::nullopt : std::optional(static_cast<std::uint16_t>(value)));
            }
            row >> expected >> observed >> events; require(static_cast<bool>(row));
            PlayerCore player; player.character.grow=1;
            player.character.level=static_cast<std::uint16_t>(level);
            // All unrelated level-up providers are observed only by their lock
            // invariant. The comparison covers the original party slice.
            notify_level_up(player, actor_id, {}, {}, curve, 0, actor_id, world);
            if (world.events.empty()) world.events="-";
            if (world.party.experience_level != expected || world.observed != observed || world.events != events || world.locked)
                throw std::runtime_error("Party LevelUp original slice differs at case " + std::to_string(cases));
            ++cases;
        }
        require(cases==256);
        std::cout << cases << " original LevelUp party sequences passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
