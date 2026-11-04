#include "eternity/game/mob_damage.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Original damage destruction/reuse mismatch");}
struct Services final:MobDamagePoolServices {
    unsigned constructed=0;std::map<MobDamageRecord*,unsigned> identities;
    std::unique_ptr<MobDamageRecord> construct() override {auto record=std::make_unique<MobDamageRecord>();identities[record.get()]=constructed++;return record;}
};
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned n,reset,attempts,free,deleted,identity;row>>n>>reset>>attempts>>free>>deleted>>identity;check(static_cast<bool>(row));
        Services services;MobDamagePool pool(services);auto first=std::make_unique<MobDamageLedger>(pool);MobDamageLedger second(pool);
        for(unsigned i=0;i<n;++i) first->add(1,i,1,1,0);
        if(reset) first->reset();
        first.reset();second.add(1,999,1,1,0);
        check(pool.construction_attempts()==attempts && pool.available().size()==free && services.constructed==attempts && second.records().size()==1 && services.identities.at(second.records()[0])==identity);
        ++cases;
    }
    check(cases==8);std::cout<<cases<<" original damage destruction/reset/reuse scenarios passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
