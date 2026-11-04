#include "eternity/game/obelisk_runtime_config.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
std::string unhex(std::string_view hex) {
    std::string result;if(hex=="-") return result;
    for(std::size_t i=0;i<hex.size();i+=2) result.push_back(static_cast<char>(std::stoul(std::string(hex.substr(i,2)),nullptr,16)));
    return result;
}
std::vector<std::uint32_t> canonical(const std::vector<ObeliskRuntimeZoneInput>& zones) {
    std::vector<std::uint32_t> words;const auto put=[&](auto n){words.push_back(static_cast<std::uint32_t>(n));};
    const auto position=[&](UserPosition p){put(std::bit_cast<std::uint32_t>(p.x));put(std::bit_cast<std::uint32_t>(p.y));put(std::bit_cast<std::uint32_t>(p.z));};
    put(zones.size());for(const auto& z:zones) {
        put(z.map);put(z.records.size());for(const auto& o:z.records) {
            put(o.base_id);put(o.country);position(o.center);for(auto id:o.mobs) put(id);put(o.spawns.size());
            for(const auto& s:o.spawns) {position(s.center);put(std::bit_cast<std::uint32_t>(s.radius));for(auto id:s.mobs) put(id);put(s.count);}
        }
    }
    return words;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected parser oracle fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing parser fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;std::istringstream row(line);unsigned scenario;std::string hex;row>>scenario>>hex;std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);
            auto actual=canonical(parse_obelisk_runtime_configuration(unhex(hex)));
            if(actual!=expected) {
                std::size_t i=0;while(i<actual.size()&&i<expected.size()&&actual[i]==expected[i]) ++i;
                std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<actual.size()?std::to_string(actual[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
                throw std::runtime_error("Original obelisk parser comparison failed");
            }
            ++count;
        }
        if(count!=45) throw std::runtime_error("Incomplete parser fixture");
        std::string over="[zone_1]\n[Obelisk_1]=(1,2,3),(1,2,3),0\n";for(unsigned i=0;i<65;++i) over+="[Mob]=(1,2,3,1),(1,2,3),1\n";
        bool rejected=false;try {static_cast<void>(parse_obelisk_runtime_configuration(over));} catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Unsupported original slot overflow was accepted");
        std::cout<<count<<" original obelisk parser cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
