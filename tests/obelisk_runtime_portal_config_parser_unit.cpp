#include "eternity/game/obelisk_runtime_portal_config.hpp"
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
std::vector<std::uint32_t> canonical(const std::vector<ObeliskRuntimePortalInput>& portals) {
    std::vector<std::uint32_t> words;const auto put=[&](auto n){words.push_back(static_cast<std::uint32_t>(n));};
    put(portals.size());for(const auto& p:portals) {
        put(p.base_id);put(p.map);put(p.portal);
        for(const auto& side:p.references) for(const auto& ref:side) {put(ref.map);put(ref.ordinal);}
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
            auto actual=canonical(parse_obelisk_runtime_portal_configuration(unhex(hex)));
            if(actual!=expected) {
                std::size_t i=0;while(i<actual.size()&&i<expected.size()&&actual[i]==expected[i]) ++i;
                std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<actual.size()?std::to_string(actual[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
                throw std::runtime_error("Original portal parser comparison failed");
            }
            ++count;
        }
        if(count!=50) throw std::runtime_error("Incomplete parser fixture");
        std::string over;for(unsigned i=0;i<65;++i) over+="[Portal_1,0,1001]\n";
        bool rejected=false;try {static_cast<void>(parse_obelisk_runtime_portal_configuration(over));} catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Unsupported original portal row overflow was accepted");
        std::cout<<count<<" original portal parser cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
