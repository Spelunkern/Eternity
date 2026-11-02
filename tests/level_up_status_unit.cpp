#include "eternity/game/stats.hpp"
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Original level-up status mismatch");}
std::vector<std::uint8_t> bytes(const std::string& hex) {std::vector<std::uint8_t> result;for(unsigned i=0;i<hex.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));return result;}
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());eternity::storage::ResultSet table;table.columns={"Job","Level","HP","MP","SP"};
    for(unsigned job=0;job<6;++job) for(unsigned level=0;level<=60;++level) {const auto i=job*61+level;const auto resource=[&](unsigned j){return std::to_string((i*i*17+j*123+i*91+1000)&65535);};table.rows.push_back({std::to_string(job),std::to_string(level),resource(0),resource(2),resource(1)});}
    const auto rules=StatusRules::load(table);std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,job,grow;std::string initial,expected;row>>scenario>>job>>grow>>initial>>expected;check(static_cast<bool>(row));const auto raw=bytes(initial);unsigned offset=0;
        const auto read=[&](unsigned n){std::uint32_t result=0;for(unsigned i=0;i<n;++i) result|=static_cast<std::uint32_t>(raw.at(offset++))<<(i*8);return result;};
        PlayerCore player;auto& c=player.character;c.job=static_cast<std::uint8_t>(job);c.grow=static_cast<std::uint8_t>(grow);c.level=static_cast<std::uint16_t>(read(2));player.stat_points=static_cast<std::uint16_t>(read(2));player.skill_points=static_cast<std::uint16_t>(read(2));
        const std::array<std::uint16_t*,6> stored={&c.strength,&c.dexterity,&c.intelligence,&c.wisdom,&c.recovery,&c.luck};for(auto* value:stored) *value=static_cast<std::uint16_t>(read(2));
        BaseStatus status;status.maximum.hp=read(4);status.maximum.mp=read(4);status.maximum.sp=read(4);const std::array<std::int32_t*,6> live={&status.attributes.strength,&status.attributes.dexterity,&status.attributes.intelligence,&status.attributes.wisdom,&status.attributes.recovery,&status.attributes.luck};for(auto* value:live) *value=std::bit_cast<std::int32_t>(read(4));check(offset==raw.size());
        rules.level_up(player,status);std::vector<std::uint8_t> actual;const auto number=[&](std::uint32_t value,unsigned n){for(unsigned i=0;i<n;++i) actual.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
        number(c.level,2);number(player.stat_points,2);number(player.skill_points,2);for(auto* value:stored) number(*value,2);for(auto value:{status.maximum.hp,status.maximum.mp,status.maximum.sp}) number(value,4);for(auto* value:live) number(static_cast<std::uint32_t>(*value),4);
        if(actual!=bytes(expected)) throw std::runtime_error("Original level-up differs at scenario "+std::to_string(scenario));++cases;
    }
    check(cases==384);std::cout<<cases<<" original complete level-up status cases passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
