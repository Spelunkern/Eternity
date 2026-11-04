#include "eternity/game/party_runtime.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace eternity::game;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected oracle fixture");
        std::ifstream input(argv[1]);
        if (!input) throw std::runtime_error("Missing fixture");
        std::string line;
        unsigned cases = 0;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream row(line);
            char tag; row >> tag;
            bool matches = false;
            if (tag == 'S') {
                unsigned kind, count; std::uint32_t old, expected;
                row >> kind >> old >> count >> expected;
                std::vector<std::optional<std::uint16_t>> members;
                for (unsigned i=0; i<count; ++i) { int level; row >> level; members.push_back(level < 0 ? std::nullopt : std::optional(static_cast<std::uint16_t>(level))); }
                PartyRuntime party{static_cast<std::uint8_t>(kind), old};
                party.check_experience_level(members);
                matches = party.experience_level == expected;
            } else if (tag == 'F') {
                std::uint32_t maximum, level; double expected;
                row >> maximum >> level >> expected;
                matches = PartyRuntime{0, maximum}.experience_factor(std::bit_cast<std::int32_t>(level)) == expected;
            } else if (tag == 'R') {
                PartyRewardUser candidate, reference;
                std::string x,y,z; int radius, expected;
                row >> candidate.life_status >> candidate.reward_state >> candidate.zone_identity >> candidate.instance >> x >> y >> z >> radius >> expected;
                candidate.x=std::stof(x); candidate.y=std::stof(y); candidate.z=std::stof(z);
                reference.zone_identity=7; reference.instance=2; reference.life_status=1;
                matches = party_reward_in_range(candidate, reference, radius) == (expected != 0);
            }
            if (!row || !matches) throw std::runtime_error("Oracle mismatch: " + line);
            ++cases;
        }
        if (cases != 1508) throw std::runtime_error("Incomplete fixture");
        PartyRuntime normal{0,99};
        const std::optional<std::uint16_t> absent[] = {1, std::nullopt};
        bool rejected=false;
        try { normal.check_experience_level(absent); } catch (const std::invalid_argument&) { rejected=true; }
        if (!rejected || normal.experience_level != 99) throw std::runtime_error("Invalid normal projection accepted");
        std::cout << cases << " original party comparisons passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
