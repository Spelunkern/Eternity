#include "eternity/game/mob_drops.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
struct World:MobLootDeliveryWorld {
    MobLootObject object;std::string events;bool allocation=false,zone=false;unsigned eligible=0;
    void event(std::string s) {if(!events.empty()) events+=',';events+=s;}
    MobLootObject* create(const MobLootItem& item,std::uint32_t owner,std::uint32_t party) override {event("A");if(!allocation) return nullptr;object={&item,owner,party};return &object;}
    void sequence_log(MobLootObject&,MobLootUser*) override {event("V");}
    bool zone_item(MobLootUser&,MobLootUser* selected) override {event("Z"+std::to_string(selected?selected->id:0));return zone;}
    bool range_item(MobLootUser& selected,MobLootUser*,std::int32_t radius) override {check(radius==100,"Wrong item radius");event("R"+std::to_string(selected.id));return (eligible&(1U<<(selected.id-100)))!=0;}
    void log_item(MobLootObject&,MobLootUser* selected) override {event("L"+std::to_string(selected?selected->id:0));}
    void give_item(MobLootUser& selected,MobLootObject&) override {event("G"+std::to_string(selected.id));}
    void enter_item(MobLootObject&,std::uint8_t priority) override {check(priority==2,"Wrong item priority");event("E");}
};
int main(int argc,char** argv) {try {
    check(argc==2,"Expected delivery fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
        unsigned scenario,present,user,dex,sequence,logged,zone,eligible,nulls,allocation,have_item;std::uint32_t seed,owner,party_id,final_cursor,final_seed;int mode,kind,cursor,real;std::string expected;
        row>>scenario>>seed>>mode>>kind>>cursor>>present>>user>>dex>>real>>sequence>>logged>>zone>>eligible>>nulls>>allocation>>have_item>>expected>>owner>>party_id>>final_cursor>>final_seed;
        check(!row.fail(),"Malformed fixture");MobLootItem item;item.id=42;item.dex=static_cast<std::uint16_t>(dex);item.real_type=real;
        std::array<MobLootUser,5> users{};MobLootParty party;party.id=77;party.mode=mode;party.type=static_cast<std::uint8_t>(kind);party.cursor=cursor;party.leader=2;
        for(unsigned i=0;i<users.size();++i) {users[i].id=100+i;party.members.push_back(nulls&(1U<<i)?nullptr:&users[i]);}
        MicrosoftRandom random(seed);World world;world.allocation=allocation;world.zone=zone;world.eligible=eligible;
        mob_deliver_item(have_item?&item:nullptr,user?&users[0]:nullptr,present?&party:nullptr,sequence,logged,random,world);
        check((world.events.empty()?"-":world.events)==expected,"Original delivery events differ: "+std::to_string(scenario)+" actual="+world.events+" expected="+expected);
        check(world.object.owner==owner && world.object.party==party_id && static_cast<std::uint32_t>(party.cursor)==final_cursor && random.state()==final_seed,"Original ownership/cursor differs: "+std::to_string(scenario));++cases;
    }
    check(cases==1024,"Incomplete delivery coverage");std::cout<<cases<<" original complete delivery traces matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
