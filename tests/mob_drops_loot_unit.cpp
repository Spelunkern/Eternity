#include "eternity/game/mob_drops.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
struct World:MobLootWorld {
    MobLootItem definition;std::array<std::uint16_t,3> quests{1,2,3};std::string events;bool have_item=false,sequence=false,zone=false;unsigned quest_mask=0;
    void event(std::string s) {if(!events.empty()) events+=',';events+=s;}
    const MobLootItem* item(std::uint8_t type,std::uint8_t id) override {check(type==1 && id==1,"Wrong quest item lookup");event("I");return have_item?&definition:nullptr;}
    std::span<const std::uint16_t> end_quests(const MobLootItem&) override {event("Q");return quests;}
    bool has_quest(MobLootUser& user,std::uint16_t quest) override {event("F"+std::to_string(user.id)+":"+std::to_string(quest));return quest_mask&(1U<<((user.id-100+quest)%8));}
    const MobLootItem* grade_item(std::uint16_t grade) override {event("S"+std::to_string(grade));return grade%3?&definition:nullptr;}
    const MobLootItem* sequence_item(std::uint16_t level,std::uint32_t id) override {check(level==30 && id==123,"Wrong sequence lookup");event("T");return sequence?&definition:nullptr;}
    void deliver(const MobLootItem&,MobLootUser*,MobLootParty*,bool seq) override {event(seq?"D1":"D0");}
    void guild_ranking(MobLootUser&,std::uint16_t points) override {event("K"+std::to_string(points));}
    void log_money(std::int32_t amount) override {event("M"+std::to_string(amount));}
    void ground_money(std::uint32_t owner,std::int32_t amount) override {event("E"+std::to_string(owner)+":"+std::to_string(amount));}
    void give_money(MobLootUser& user,std::int32_t amount) override {event("G"+std::to_string(user.id)+":"+std::to_string(static_cast<std::uint32_t>(amount)));}
    bool zone_item(MobLootUser&,MobLootUser* selected) override {event("Z"+std::to_string(selected?selected->id:0));return zone;}
};
int main(int argc,char** argv) {try {
    check(argc==2,"Expected loot fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
        unsigned scenario,present,user,quest_type,start,have_item,quests,quality,money_base,money_random,logged,zone,sequence;std::uint32_t seed,final_seed;int zone_type,mode;std::string expected;
        row>>scenario>>seed>>present>>user>>quest_type>>start>>have_item>>quests>>quality>>money_base>>money_random>>zone_type>>logged>>mode>>zone>>sequence>>expected>>final_seed;check(!row.fail(),"Malformed fixture");
        MobLootContext mob;mob.mob_id=0;mob.level=30;mob.object_id=123;mob.money_base=static_cast<std::uint16_t>(money_base);mob.money_random=static_cast<std::uint16_t>(money_random);mob.zone_type=zone_type;mob.logged=logged;mob.quest_type=static_cast<std::uint8_t>(quest_type);mob.quest_type_id=1;
        for(unsigned i=0;i<10;++i) {mob.slots[i].grade=static_cast<std::uint16_t>((scenario+i)%4==0?i+1:0);const std::array<std::uint32_t,4> rates{0,30,100,1000000};mob.slots[i].rate=rates[(scenario/43+i)%4];}
        std::array<MobLootUser,5> users{};MobLootParty party;party.mode=mode;party.leader=2;
        const std::array<float,5> positions{0,99,100,0,101};const std::array<std::int32_t,5> bonus{0,-20,15,100,0};
        for(unsigned i=0;i<users.size();++i) {users[i].id=100+i;users[i].dead=i==4?1:0;users[i].map=i==3?1:0;users[i].x=positions[i];users[i].money_mode=static_cast<std::int32_t>(i%4);users[i].money_percent=bonus[i];party.members.push_back(&users[i]);}
        MicrosoftRandom random(seed);ServerRandom server(seed);World world;world.have_item=have_item;world.sequence=sequence;world.zone=zone;world.quest_mask=quests;world.definition.quest_start=start;world.definition.quality=static_cast<std::uint16_t>(quality);
        mob_loot(mob,user?&users[0]:nullptr,present?&party:nullptr,random,server,world);
        check((world.events.empty()?"-":world.events)==expected,"Original loot events differ: "+std::to_string(scenario)+" actual="+world.events+" expected="+expected);
        check(random.state()==final_seed,"Original loot random state differs: "+std::to_string(scenario));++cases;
    }
    check(cases==1024,"Incomplete loot coverage");std::cout<<cases<<" original complete loot traces matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
