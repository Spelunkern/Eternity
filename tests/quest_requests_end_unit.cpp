#include "eternity/game/quest_requests.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
std::vector<std::uint8_t> decode(const std::string& s) {
    std::vector<std::uint8_t> r;
    for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return r;
}
struct World:QuestRequestWorld {
    const QuestDefinition* def=nullptr;RestoredQuest quest;
    bool active=false,found=false;QuestRequestNpc npc;
    std::array<std::array<QuestRequestItem,24>,1> bags{};
    unsigned lookups=0,item_checks=0,database_sends=0,client_sends=0;std::int64_t selection=-1;bool success=false;
    const std::array<std::uint8_t,464>* definition(std::uint16_t) override {return &def->record();}
    RestoredQuest* find_quest(std::uint16_t) override {return active?&quest:nullptr;}
    std::optional<QuestRequestNpc> find_npc(std::uint32_t) override {++lookups;return found?std::optional{npc}:std::nullopt;}
    QuestRequestInventory inventory() override {++item_checks;return {0x12345678,bags};}
    void complete_quest(RestoredQuest& q,std::uint32_t s,bool ok) override {if(&q!=&quest) throw std::runtime_error("wrong quest");selection=s;success=ok;}
    void send_database(const eternity::net::Packet&) override {++database_sends;}
    void send_self(const eternity::net::Packet&) override {++client_sends;}
    void release_item(std::uint8_t,std::uint8_t) override {throw std::runtime_error("unexpected release");}
};
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);std::string line;unsigned rows=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);unsigned id,npc;int count,active,found,nt,ni,items,lookups,consume,success;std::int64_t selection;std::string block,counters;
            row>>id>>count>>npc>>active>>found>>nt>>ni>>items>>block>>counters>>lookups>>consume>>selection>>success;
            const auto definitions=QuestDefinitions::decode(decode(block));World world;world.def=definitions.find(1);world.quest.definition=world.def;
            const auto bytes=decode(counters);std::copy(bytes.begin(),bytes.end(),world.quest.packet.counters.begin());
            world.bags[0][0]={true,1,1,2,0};
            world.active=active!=0;world.found=found!=0;world.npc={nt,ni};
            quest_end_request(static_cast<std::uint16_t>(id),npc,count,world);
            const unsigned consumed=consume==1 && items!=0 ? 1U:0U;
            if(world.database_sends!=consumed || world.client_sends!=consumed || world.bags[0][0].count!=2-consumed) throw std::runtime_error("request consumption flag mismatch");
            if(world.selection!=selection || world.success!=(success!=0) || world.lookups!=static_cast<unsigned>(lookups) || world.item_checks!=static_cast<unsigned>(consume!=-1)) {
                std::cerr<<"case "<<rows<<" selection "<<world.selection<<'/'<<selection<<" lookups "<<world.lookups<<'/'<<lookups<<'\n';throw std::runtime_error("request comparison failed");
            }
            ++rows;
        }
        if(rows!=1024) throw std::runtime_error("missing fixtures");
        std::cout<<rows<<" original quest end request cases passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
