#include "eternity/game/mob_damage.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;using Bytes=std::vector<std::uint8_t>;
void check(bool value) {if(!value) throw std::runtime_error("Original damage pool mismatch");}
void number(Bytes& bytes,std::uint32_t value,unsigned n=4) {for(unsigned i=0;i<n;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
struct Services final:MobDamagePoolServices {
    bool fail=false;std::vector<MobDamageRecord*> objects;
    std::unique_ptr<MobDamageRecord> construct() override {if(fail) return {};auto record=std::make_unique<MobDamageRecord>();objects.push_back(record.get());return record;}
    unsigned id(MobDamageRecord* record) {auto at=std::find(objects.begin(),objects.end(),record);check(at!=objects.end());return static_cast<unsigned>(at-objects.begin());}
};
struct State {
    Services services;MobDamagePool pool{services};MobDamageLedger first{pool},second{pool};
    State() {first.fair_combat=second.fair_combat=false;}
    MobDamageLedger& ledger(unsigned i) {check(i<2);return i?second:first;}
    Bytes snapshot() {
        Bytes bytes;number(bytes,pool.construction_attempts());number(bytes,static_cast<std::uint32_t>(pool.available().size()));number(bytes,static_cast<std::uint32_t>(pool.available().size()),1);
        for(auto at=pool.available().rbegin();at!=pool.available().rend();++at) number(bytes,services.id(*at),1);
        for(auto* owner:{&first,&second}) {number(bytes,owner->fair_combat?1:0,1);number(bytes,static_cast<std::uint32_t>(owner->records().size()));number(bytes,static_cast<std::uint32_t>(owner->records().size()),1);for(auto* record:owner->records()) number(bytes,services.id(record),1);}
        number(bytes,static_cast<std::uint32_t>(services.objects.size()),1);
        for(auto* record:services.objects) {number(bytes,record->type);number(bytes,record->id);number(bytes,static_cast<std::uint32_t>(record->damage));number(bytes,static_cast<std::uint32_t>(record->aggro));number(bytes,record->party);}return bytes;
    }
};
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());std::unique_ptr<State> state;std::string line;unsigned scenarios=0,operations=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='S') {unsigned scenario;row>>scenario;check(scenario==scenarios++);state=std::make_unique<State>();continue;}
        unsigned owner;row>>owner;
        if(tag=='Q') {
            std::string expected;row>>expected;check(static_cast<bool>(row));MobDamageRecord result{0xa5,0xa5a5a5a5U,std::bit_cast<std::int32_t>(0xa5a5a5a5U),std::bit_cast<std::int32_t>(0xa5a5a5a5U),0xa5a5a5a5U};
            const auto found=state->ledger(owner).maximum_aggro(result);Bytes bytes;number(bytes,found?1:0,1);for(unsigned i=0;i<8;++i) number(bytes,0xa5,1);number(bytes,result.type,1);for(unsigned i=0;i<3;++i) number(bytes,0xa5,1);number(bytes,result.id);number(bytes,static_cast<std::uint32_t>(result.damage));number(bytes,static_cast<std::uint32_t>(result.aggro));number(bytes,result.party);
            check(bytes.size()*2==expected.size());for(unsigned i=0;i<bytes.size();++i) check(bytes[i]==std::stoul(expected.substr(i*2,2),nullptr,16));++operations;continue;
        }
        if(tag=='A') {std::int32_t type;std::uint32_t id,damage,aggro,party;unsigned fail;row>>type>>id>>damage>>aggro>>party>>fail;state->services.fail=fail!=0;state->ledger(owner).add(type,id,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),party);}
        else {check(tag=='R');state->ledger(owner).reset();}
        std::string expected;row>>expected;check(static_cast<bool>(row));const auto actual=state->snapshot();check(actual.size()*2==expected.size());for(unsigned i=0;i<actual.size();++i) if(actual[i]!=std::stoul(expected.substr(i*2,2),nullptr,16)) throw std::runtime_error("Damage pool differs at operation "+std::to_string(operations)+" byte "+std::to_string(i));++operations;
    }
    check(scenarios==64 && operations==2496);std::cout<<scenarios<<" original shared damage heaps, "<<operations<<" mutations/queries passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
