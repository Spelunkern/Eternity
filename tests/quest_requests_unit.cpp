#include "eternity/game/quest_requests.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
std::vector<std::uint8_t> hex(const std::string& s) {
    std::vector<std::uint8_t> result;
    if(s=="-") return result;
    for(std::size_t i=0;i<s.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return result;
}
struct World:QuestRequestItems {
    std::vector<std::uint8_t> events;
    void send_database(const eternity::net::Packet& p) override {events.push_back('B');events.insert(events.end(),p.begin(),p.end());}
    void send_self(const eternity::net::Packet& p) override {events.push_back('C');events.insert(events.end(),p.begin(),p.end());}
    void release_item(std::uint8_t,std::uint8_t) override {}
};
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);std::string line;unsigned rows=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int mode,bags,consume,type,id,result;std::string ds,is,cs,ocs,ois,es;
            row>>mode>>bags>>consume>>type>>id>>ds>>is>>cs>>result>>ocs>>ois>>es;
            auto dbytes=hex(ds),ibytes=hex(is),cbytes=hex(cs),expected=hex(ois);
            std::array<std::uint8_t,464> d{};std::copy(dbytes.begin(),dbytes.end(),d.begin());
            std::array<std::uint8_t,3> counters{};std::copy(cbytes.begin(),cbytes.end(),counters.begin());
            std::vector<std::array<QuestRequestItem,24>> inventory(static_cast<std::size_t>(bags));
            for(std::size_t i=0;i<ibytes.size()/5;++i) inventory[i/24][i%24]={ibytes[5*i]!=0,ibytes[5*i+1],ibytes[5*i+2],ibytes[5*i+3],ibytes[5*i+4]};
            World world;QuestRequestInventory projection{0x12345678,inventory};
            const bool actual=mode==0?quest_find_item(projection,type,id,consume!=0,world):quest_find_items(projection,d,counters,consume!=0,world);
            std::vector<std::uint8_t> final;
            for(const auto& bag:inventory) for(const auto& item:bag) final.insert(final.end(),{static_cast<std::uint8_t>(item.present),item.type,item.id,item.count,static_cast<std::uint8_t>(item.effect)});
            if(actual!=(result!=0) || final!=expected || std::vector<std::uint8_t>(counters.begin(),counters.end())!=hex(ocs) || world.events!=hex(es)) {
                std::cerr<<"case "<<rows<<" mode "<<mode<<" result "<<actual<<"/"<<result<<" inventory "<<(final==expected)<<" counters "<<(std::vector<std::uint8_t>(counters.begin(),counters.end())==hex(ocs))<<" events "<<(world.events==hex(es))<<'\n';
                throw std::runtime_error("original comparison failed");
            }
            ++rows;
        }
        if(rows!=768) throw std::runtime_error("missing fixtures");
        std::cout<<rows<<" original quest item cases passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
