#include "eternity/game/mob_damage.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Original mob damage selection mismatch");}
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());std::string line;std::vector<MobDamageRecord> records;unsigned scenarios=0,queries=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='S') {unsigned scenario;row>>scenario;check(scenario==scenarios++);records.clear();}
        else if(tag=='N') {unsigned type;std::uint32_t id,damage,aggro,party;row>>type>>id>>damage>>aggro>>party;records.push_back({static_cast<std::uint8_t>(type),id,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),party});}
        else {
            unsigned returned;std::string expected;row>>returned>>expected;check(tag=='D' || tag=='A');
            MobDamageRecord output{0xa5,0xa5a5a5a5U,std::bit_cast<std::int32_t>(0xa5a5a5a5U),std::bit_cast<std::int32_t>(0xa5a5a5a5U),0xa5a5a5a5U};
            const auto result=tag=='D'?maximum_mob_damage(records,output):maximum_mob_aggro(records,output);check(result==(returned!=0));
            std::array<std::uint8_t,28> bytes;bytes.fill(0xa5);bytes[8]=output.type;
            const auto number=[&](unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
            number(12,output.id);number(16,static_cast<std::uint32_t>(output.damage));number(20,static_cast<std::uint32_t>(output.aggro));number(24,output.party);
            check(expected.size()==56);for(unsigned i=0;i<28;++i) check(bytes[i]==std::stoul(expected.substr(i*2,2),nullptr,16));++queries;
        }
        check(static_cast<bool>(row));
    }
    check(scenarios==128 && queries==256);std::cout<<scenarios<<" original damage lists and "<<queries<<" winner queries passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
