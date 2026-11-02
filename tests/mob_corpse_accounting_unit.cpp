#include "eternity/game/mobs.hpp"
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
struct Random final:RandomSource {std::int32_t value=0;unsigned calls=0;std::int32_t next() override {++calls;return value;}};
void check(bool value) {if(!value) throw std::runtime_error("Original corpse accounting mismatch");}
int main(int argc,char** argv) {try {
    check(argc==3);std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t current,count,old,duration,selector,now,value,expected_current,expected_next,calls;
        row>>current>>count>>old>>duration>>selector>>now>>value>>expected_current>>expected_next>>calls;check(static_cast<bool>(row));
        MobRespawn respawn;respawn.current=std::bit_cast<std::int32_t>(current);respawn.count=std::bit_cast<std::int32_t>(count);respawn.next=old;
        MobRuntimeTemplate definition;definition.respawn_time=duration;definition.respawn_selector=static_cast<std::uint8_t>(selector);Random random;random.value=static_cast<std::int32_t>(value);
        account_regular_mob_corpse(respawn,definition,now,random);
        check(static_cast<std::uint32_t>(respawn.current)==expected_current && respawn.next==expected_next && random.calls==calls);++cases;
    }
    check(cases==540);std::ifstream obelisks(argv[2]);check(obelisks.good());unsigned obelisk_cases=0;
    while(std::getline(obelisks,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t old,now,delay,count,expected_next,expected_count;
        row>>old>>now>>delay>>count>>expected_next>>expected_count;check(static_cast<bool>(row));
        MobRespawn respawn;respawn.obelisk=true;respawn.next=old;respawn.current=static_cast<std::int32_t>(count);MobRuntimeTemplate definition;definition.respawn_time=delay;
        account_obelisk_mob_corpse(respawn,definition,now);
        check(respawn.next==expected_next && static_cast<std::uint32_t>(respawn.current)==expected_count);++obelisk_cases;
    }
    check(obelisk_cases==60);
std::cout<<cases<<" original regular and 60 obelisk corpse accounting cases passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
