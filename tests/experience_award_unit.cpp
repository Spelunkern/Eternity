#include "eternity/game/experience_award.hpp"
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;using Bytes=eternity::net::Packet;
void check(bool value) {if(!value) throw std::runtime_error("Original experience award mismatch");}
void number(Bytes& bytes,std::uint32_t value,unsigned n=4) {for(unsigned i=0;i<n;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& hex) {Bytes result;if(hex=="-") return result;for(unsigned i=0;i<hex.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));return result;}
struct Ports final:ExperienceAwardPorts {
    Bytes events;
    void packet(char tag,const Bytes& packet) {events.push_back(static_cast<std::uint8_t>(tag));number(events,static_cast<std::uint32_t>(packet.size()));events.insert(events.end(),packet.begin(),packet.end());}
    void send_database(const Bytes& p) override {packet('B',p);}
    void send_self(const Bytes& p) override {packet('C',p);}
    void level_up(PlayerCore& p,BaseStatus&,std::uint32_t amount,std::uint32_t source) override {events.push_back('L');number(events,amount);number(events,source);number(events,p.experience);number(events,p.character.level);}
};
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());eternity::storage::ResultSet exp;exp.columns={"Level","Grow","Exp"};for(unsigned grow=1;grow<=4;++grow) for(unsigned level=1;level<=60;++level) exp.rows.push_back({std::to_string(level),std::to_string(grow),"1000"});const auto curve=Experience::load(exp);
    eternity::storage::ResultSet table;table.columns={"Job","Level","HP","MP","SP"};const auto rules=StatusRules::load(table);std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,grow,level,bypass,doubled,half,party;std::uint32_t old,amount;std::int32_t bonus;std::string events,expected;row>>scenario>>grow>>level>>old>>amount>>bypass>>doubled>>half>>bonus>>party>>events>>expected;check(static_cast<bool>(row));
        PlayerCore player;player.user_id=123;player.character.id=321;player.character.grow=static_cast<std::uint8_t>(grow);player.character.level=static_cast<std::uint16_t>(level);player.experience=old;BaseStatus status{};Ports ports;
        award_experience(player,status,rules,curve,777,std::bit_cast<std::int32_t>(amount),{bypass!=0,doubled!=0,half!=0,party!=0,bonus},ports);
        Bytes actual;number(actual,player.character.level,2);number(actual,player.stat_points,2);number(actual,player.skill_points,2);number(actual,player.experience);
        const auto& c=player.character;for(auto value:{c.strength,c.dexterity,c.intelligence,c.wisdom,c.recovery,c.luck}) number(actual,value,2);for(auto value:{status.maximum.hp,status.maximum.mp,status.maximum.sp}) number(actual,value);const auto& a=status.attributes;for(auto value:{a.strength,a.dexterity,a.intelligence,a.wisdom,a.recovery,a.luck}) number(actual,static_cast<std::uint32_t>(value));
        if(actual!=unhex(expected) || ports.events!=unhex(events)) throw std::runtime_error("Original experience award differs at scenario "+std::to_string(scenario));++cases;
    }
    check(cases==512);std::cout<<cases<<" original experience awards, packets and level transitions passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
