#include "eternity/game/mob_drops_range.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
MobLootUser user(std::istringstream& row) {
    MobLootUser value;unsigned instance;std::uint32_t x,z;row>>value.state>>value.map>>instance>>value.dead>>x>>z;
    value.instance=static_cast<std::uint16_t>(instance);value.x=std::bit_cast<float>(x);value.z=std::bit_cast<float>(z);return value;
}
struct Zero:RandomSource {unsigned calls=0;std::int32_t next() override {++calls;return 0;}};
struct World:MobLootWorld {
    unsigned grants=0,sequences=0;
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected money composition provider");}
    const MobLootItem* item(std::uint8_t,std::uint8_t) override {unexpected();}
    std::span<const std::uint16_t> end_quests(const MobLootItem&) override {unexpected();}
    bool has_quest(MobLootUser&,std::uint16_t) override {unexpected();}
    const MobLootItem* grade_item(std::uint16_t) override {unexpected();}
    const MobLootItem* sequence_item(std::uint16_t level,std::uint32_t object) override {check(level==1 && object==123,"Wrong sequence composition input");++sequences;return nullptr;}
    void deliver(const MobLootItem&,MobLootUser*,MobLootParty*,bool) override {unexpected();}
    void guild_ranking(MobLootUser&,std::uint16_t) override {unexpected();}
    void log_money(std::int32_t) override {unexpected();}
    void ground_money(std::uint32_t,std::int32_t) override {unexpected();}
    void give_money(MobLootUser& who,std::int32_t amount) override {check(who.id==101 && amount==17,"Wrong unordered party money grant");++grants;}
    bool zone_item(MobLootUser&,MobLootUser*) override {unexpected();}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original eligibility fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read range fixture");unsigned cases=0,unordered=0,dead_zone=0,money_without_credit_gate=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,range,zone,money,distance;std::uint32_t mob_zone;std::int32_t radius;
        row>>scenario>>mob_zone;const auto credited=user(row),candidate=user(row);row>>radius>>range>>zone>>money>>distance;check(!row.fail(),"Malformed eligibility fixture");
        check(mob_drop_zone_enabled(mob_zone,credited,candidate)==(zone!=0),"Original zone gate differs: "+std::to_string(scenario));
        check(mob_drop_range_enabled(mob_zone,credited,candidate,radius)==(range!=0),"Original item range differs: "+std::to_string(scenario));
        check(mob_drop_money_range_enabled(credited,candidate)==(money!=0),"Original money range differs: "+std::to_string(scenario));
        check(distance<=1 && (!range || distance==1),"Invalid original distance observer");
        if(range && std::isnan(candidate.x)) ++unordered;if(zone && candidate.dead==1) ++dead_zone;
        if(money && (credited.state!=4 || credited.map!=mob_zone)) ++money_without_credit_gate;++cases;
    }
    // Use the actual loot coordinator, not only a predicate: the original inline
    // money branch accepts this unordered distance despite credited state three.
    MobLootContext mob;mob.money_base=17;mob.level=1;mob.object_id=123;MobLootUser credit;credit.id=100;credit.state=3;MobLootUser member;member.id=101;member.x=std::bit_cast<float>(0x7fc00000U);
    MobLootParty party;party.members={&member};Zero random;ServerRandom server;World world;mob_loot(mob,&credit,&party,random,server,world);
    check(world.grants==1 && world.sequences==1 && random.calls==1,"Live loot coordinator lost original unordered money behavior");
    check(cases==2048 && unordered>0 && dead_zone>0 && money_without_credit_gate>0,"Incomplete eligibility boundaries");
    std::cout<<cases<<" original item/zone/money gates matched; "<<unordered<<" unordered accepts; "<<dead_zone<<" dead zone accepts; native loot money composition passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
