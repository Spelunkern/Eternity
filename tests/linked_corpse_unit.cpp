#include "eternity/game/linked_mobs.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Original linked corpse timer mismatch");}
struct Random final:RandomSource {std::int32_t value=0;unsigned calls=0;std::int32_t next() override {++calls;return value;}};
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);LinkedMobGroup group;MobRuntimeTemplate definition;std::uint32_t old,now,seed,value,expected,current,calls,index,word;
        row>>group.current_index>>group.leader.randomized>>group.leader.minimum_minutes>>group.leader.random_minutes>>group.respawn_time>>definition.respawn_time>>old>>now>>seed>>value>>expected>>current>>calls>>index>>word;check(static_cast<bool>(row));
        LinkedMobPopulation population(std::span<const LinkedMobGroup>(&group,1));MobRespawn respawn;respawn.both=true;respawn.current=7;respawn.next=old;MobRuntime mob;mob.definition=&definition;mob.respawn=&respawn;mob.drop_random.seed(seed);Random random;random.value=static_cast<std::int32_t>(value);
        population.account_principal_corpse(mob,now,random);
        if(respawn.next!=expected || mob.drop_random.index()!=index || mob.drop_random.words()[0]!=word) std::cerr<<"case "<<cases<<" next "<<respawn.next<<"/"<<expected<<" index "<<mob.drop_random.index()<<"/"<<index<<" word "<<mob.drop_random.words()[0]<<"/"<<word<<"\n";
        check(respawn.next==expected && static_cast<std::uint32_t>(respawn.current)==current && random.calls==calls && mob.drop_random.index()==index && mob.drop_random.words()[0]==word);++cases;
    }
    check(cases==256);std::cout<<cases<<" original linked principal corpse timers passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
