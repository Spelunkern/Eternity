#include "eternity/game/npcs.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
using Bytes=std::vector<std::uint8_t>;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
Bytes canonical(const NpcDefinitions& definitions) {
    Bytes result;
    for(int type=1;type<=13;++type) {
        number(result,static_cast<std::uint32_t>(definitions.count(type)));
        for(std::size_t i=1;i<=definitions.count(type);++i) {
            const auto* npc=definitions.find(type,static_cast<std::int32_t>(i));require(npc,"Missing valid NPC");
            result.insert(result.end(),npc->record.begin(),npc->record.end());
            for(const auto* links:{&npc->start_quests,&npc->end_quests}) {number(result,static_cast<std::uint32_t>(links->size()));for(auto id:*links) number(result,id,2);}
            if(type==1) {number(result,npc->shop_type,1);number(result,static_cast<std::uint32_t>(npc->shop_items.size()));for(const auto& item:npc->shop_items) result.insert(result.end(),item.begin(),item.end());}
            else if(type==2) for(const auto& gate:npc->gates) {
                number(result,gate.map_id,2);number(result,0,2);
                for(auto coordinate:{gate.position.x,gate.position.y,gate.position.z}) number(result,std::bit_cast<std::uint32_t>(coordinate));
                result.insert(result.end(),gate.name.begin(),gate.name.end());number(result,gate.money);
            }
        }
        require(!definitions.find(type,0) && !definitions.find(type,-1) && !definitions.find(type,static_cast<std::int32_t>(definitions.count(type)+1)),"Invalid positional NPC index accepted");
    }
    for(int type:{-1,0,14,15}) require(!definitions.find(type,1) && definitions.count(type)==0,"Invalid NPC type accepted");
    return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected NPC oracle and optional original NpcQuest.SData");std::ifstream input(argv[1]);require(input.good(),"Cannot open NPC oracle");
        unsigned int synthetic=0,actual=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='A') {
                std::string sha;std::size_t consumed=0,size=0;std::uint64_t expected_hash=0;row>>sha>>consumed>>size>>expected_hash;
                if(argc==3) {
                    std::ifstream file(argv[2],std::ios::binary);require(file.good(),"Cannot open original NPC distribution");Bytes data((std::istreambuf_iterator<char>(file)),{});
                    const auto definitions=NpcDefinitions::decode(data);const auto bytes=canonical(definitions);std::uint64_t hash=0xcbf29ce484222325ULL;for(auto byte:bytes) hash=(hash^byte)*0x100000001b3ULL;
                    require(definitions.consumed()==consumed && bytes.size()==size && hash==expected_hash,"Original complete NPC distribution differs");++actual;
                }
            } else if(kind=='S') {
                std::string hex,expected;std::size_t consumed=0;row>>hex>>consumed>>expected;auto data=unhex(hex);const auto definitions=NpcDefinitions::decode(data);
                require(definitions.consumed()==consumed && canonical(definitions)==unhex(expected),"Original synthetic NPC definition differs");
                for(int type=1;type<=13;++type) if(const auto* npc=definitions.find(type,1)) {
                    require(npc->type()==256-type && npc->type_id()==500 && npc->model()==-500 && npc->move_distance()==static_cast<std::int32_t>(synthetic) && npc->move_speed()==2147483647,"NPC typed fields differ");
                    require(npc->country()==(synthetic==1?0:synthetic==2?1:2) && npc->name().size()<=30,"NPC country/name normalization differs");
                }
                data.pop_back();bool rejected=false;try{NpcDefinitions::decode(data);}catch(const std::runtime_error&){rejected=true;}require(rejected,"Truncated NPC definitions accepted");++synthetic;
            } else require(false,"Unknown NPC vector type");
            require(static_cast<bool>(row),"Malformed NPC vector");std::string extra;require(!(row>>extra),"Trailing NPC vector data");
        }
        require(synthetic==6 && actual==(argc==3?1U:0U),"NPC oracle coverage changed");std::cout<<synthetic<<" original NPC loads/lookups and "<<actual<<" full distribution checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
