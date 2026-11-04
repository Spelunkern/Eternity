#include "eternity/game/obelisk_runtime_config.hpp"
#include "eternity/game/obelisk_runtime_portal_config.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
std::vector<std::uint32_t> ordinary(std::string_view text) {
    const auto zones=parse_obelisk_runtime_configuration(text);std::vector<std::uint32_t> words;
    const auto put=[&](auto n){words.push_back(static_cast<std::uint32_t>(n));};
    const auto position=[&](UserPosition p){put(std::bit_cast<std::uint32_t>(p.x));put(std::bit_cast<std::uint32_t>(p.y));put(std::bit_cast<std::uint32_t>(p.z));};
    put(zones.size());for(const auto& z:zones) {
        put(z.map);put(z.records.size());for(const auto& o:z.records) {
            put(o.base_id);put(o.country);position(o.center);for(auto n:o.mobs) put(n);put(o.spawns.size());
            for(const auto& s:o.spawns) {position(s.center);put(std::bit_cast<std::uint32_t>(s.radius));for(auto n:s.mobs) put(n);put(s.count);}
        }
    }
    return words;
}
std::vector<std::uint32_t> portals(std::string_view text) {
    const auto inputs=parse_obelisk_runtime_portal_configuration(text);std::vector<std::uint32_t> words;
    words.push_back(static_cast<std::uint32_t>(inputs.size()));for(const auto& input:inputs) {
        words.push_back(input.base_id);words.push_back(static_cast<std::uint32_t>(input.map));words.push_back(static_cast<std::uint32_t>(input.portal));
        for(const auto& side:input.references) for(const auto& ref:side) {words.push_back(static_cast<std::uint32_t>(ref.map));words.push_back(static_cast<std::uint32_t>(ref.ordinal));}
    }
    return words;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Expected asset oracle fixture and private recovered Obelisk.ini");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing asset oracle fixture");
        std::ifstream asset(argv[2],std::ios::binary);if(!asset) throw std::runtime_error("Missing private recovered Obelisk.ini");
        const std::string text((std::istreambuf_iterator<char>(asset)),std::istreambuf_iterator<char>());
        std::uint64_t fnv=0xcbf29ce484222325ULL;for(unsigned char byte:text) fnv=(fnv^byte)*0x100000001b3ULL;
        std::string line;unsigned count=0;std::array<bool,2> seen{};
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;std::istringstream row(line);char kind;std::uint64_t size,hash;row>>kind>>size>>hash;
            if(size!=text.size()||hash!=fnv) throw std::runtime_error("Recovered asset differs from original fixture input");
            if(kind!='O'&&kind!='P') throw std::runtime_error("Unknown asset comparison kind");
            const auto index=kind=='O'?0U:1U;if(seen[index]) throw std::runtime_error("Duplicate asset comparison");seen[index]=true;
            std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);
            const auto actual=kind=='O'?ordinary(text):portals(text);
            if(actual!=expected) {
                std::size_t i=0;while(i<actual.size()&&i<expected.size()&&actual[i]==expected[i]) ++i;
                std::cerr<<"Asset "<<kind<<" word "<<i<<" native="<<(i<actual.size()?std::to_string(actual[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
                throw std::runtime_error("Complete original asset parser comparison failed");
            }
            std::cout<<kind<<": "<<actual.size()<<" original temporary-table words matched\n";++count;
        }
        if(count!=2) throw std::runtime_error("Incomplete original asset comparison");return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
