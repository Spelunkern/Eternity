#include "eternity/game/mob_drops_range.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
struct World:MobLootDeliveryWorld {
    std::uint32_t zone=0;bool allocation=false;MobLootObject object;std::string events;
    void event(std::string value) {if(!events.empty()) events+=',';events+=value;}
    MobLootObject* create(const MobLootItem& definition,std::uint32_t owner,std::uint32_t party) override {event("A");if(!allocation) return nullptr;object={&definition,owner,party};return &object;}
    void sequence_log(MobLootObject&,MobLootUser*) override {throw std::runtime_error("Unexpected sequence log");}
    bool zone_item(MobLootUser& credited,MobLootUser* selected) override {check(selected,"Missing retained original leader");return mob_drop_zone_enabled(zone,credited,*selected);}
    bool range_item(MobLootUser& selected,MobLootUser* credited,std::int32_t radius) override {check(credited && radius==100,"Missing original range context");return mob_drop_range_enabled(zone,*credited,selected,radius);}
    void log_item(MobLootObject&,MobLootUser* selected) override {event("L"+std::to_string(selected?selected->id:0));}
    void give_item(MobLootUser& selected,MobLootObject&) override {event("G"+std::to_string(selected.id));}
    void enter_item(MobLootObject&,std::uint8_t appearance) override {check(appearance==2,"Wrong ground appearance");event("E");}
};
int main(int argc,char** argv) {try {
    check(argc==2,"Expected real-range delivery fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,kind,present,dex,logged,nulls,allocation,have_item;std::uint32_t seed,zone,owner,party_id,final_cursor,final_seed;int mode,cursor,real;
        row>>scenario>>seed>>mode>>kind>>cursor>>present>>dex>>real>>logged>>nulls>>allocation>>have_item>>zone;
        std::array<MobLootUser,5> users{};MobLootParty party;party.id=77;party.leader=2;party.mode=mode;party.type=static_cast<std::uint8_t>(kind);party.cursor=cursor;
        for(unsigned i=0;i<users.size();++i) {unsigned instance;std::uint32_t x,z;auto& user=users[i];user.id=100+i;row>>user.state>>user.map>>instance>>user.dead>>x>>z;user.instance=static_cast<std::uint16_t>(instance);user.x=std::bit_cast<float>(x);user.z=std::bit_cast<float>(z);party.members.push_back(nulls&(1U<<i)?nullptr:&user);}
        std::string expected;row>>expected>>owner>>party_id>>final_cursor>>final_seed;check(!row.fail(),"Malformed real range delivery");
        MobLootItem definition;definition.id=42;definition.dex=static_cast<std::uint16_t>(dex);definition.real_type=real;World world;world.zone=zone;world.allocation=allocation;MicrosoftRandom random(seed);
        mob_deliver_item(have_item?&definition:nullptr,&users[0],present?&party:nullptr,false,logged,random,world);
        check((world.events.empty()?"-":world.events)==expected,"Original real range delivery differs: "+std::to_string(scenario)+" actual="+world.events+" expected="+expected);
        check(world.object.owner==owner && world.object.party==party_id && static_cast<std::uint32_t>(party.cursor)==final_cursor && random.state()==final_seed,"Original real range ownership/cursor differs: "+std::to_string(scenario));++cases;
    }
    check(cases==1024,"Incomplete real range delivery coverage");std::cout<<cases<<" complete original delivery traces with actual zone/range bodies matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
