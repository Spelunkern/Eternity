#include "eternity/game/quests.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& text) {
    net::Packet bytes;for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;
}
void put(net::Packet& bytes,std::size_t offset,std::uint32_t value) {for(unsigned int i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected quest oracle and optional original server data path");
        std::ifstream input(argv[1]);require(input.good(),"Cannot open quest oracle");std::string line;net::Packet template_block;
        unsigned int synthetic=0,restorations=0,actual=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='A') {
                std::string sha;std::size_t offset=0,count=0,consumed=0;std::uint64_t expected_hash=0;row>>sha>>offset>>count>>consumed>>expected_hash;
                if(argc==3) {
                    std::ifstream file(argv[2],std::ios::binary);require(file.good(),"Cannot open original NpcQuest.SData");net::Packet bytes((std::istreambuf_iterator<char>(file)),{});
                    const auto definitions=QuestDefinitions::npc_quest(bytes);
                    require(definitions.size()==count && definitions.consumed()==consumed,"Original complete quest distribution shape changed");
                    std::uint64_t hash=0xcbf29ce484222325ULL;
                    for(std::size_t i=1;i<=count;++i) for(auto byte:definitions.find(static_cast<std::uint16_t>(i))->record()) hash=(hash^byte)*0x100000001b3ULL;
                    require(hash==expected_hash,"Original complete quest distribution fields differ");++actual;
                }
            } else if(kind=='S') {
                std::string hex;std::size_t count=0,consumed=0;row>>hex>>count>>consumed;auto bytes=unhex(hex);if(template_block.empty()) template_block=bytes;
                const auto definitions=QuestDefinitions::decode(bytes);require(definitions.size()==count && definitions.consumed()==consumed,"Synthetic quest definition shape differs");
                for(std::size_t i=1;i<=count;++i) {row>>hex;const auto expected=unhex(hex);const auto* definition=definitions.find(static_cast<std::uint16_t>(i));require(definition && std::equal(expected.begin(),expected.end(),definition->record().begin(),definition->record().end()),"Original decoded quest record differs");}
                require(!definitions.find(0) && !definitions.find(static_cast<std::uint16_t>(count+1)),"Invalid positional quest index accepted");
                bytes.pop_back();bool rejected=false;try{QuestDefinitions::decode(bytes);}catch(const std::runtime_error&){rejected=true;}require(rejected,"Truncated quest record accepted");++synthetic;
            } else if(kind=='R') {
                std::uint32_t maximum=0,minimum=0,delay=0,tick=0,end_type=0,start=0,end=0,success=0,mob=0,pc=0;
                row>>maximum>>minimum>>delay>>tick>>end_type>>start>>end>>success>>mob>>pc;
                require(!template_block.empty(),"Restoration precedes synthetic definition template");auto data=template_block;
                put(data,48,minimum);put(data,52,maximum);data[83]=static_cast<std::uint8_t>(end_type);
                const auto definitions=QuestDefinitions::decode(data);require(definitions.find(1)->id()==77,"Quest lookup used embedded ID instead of position");
                require(definitions.find(1)->minimum_minutes()==minimum && definitions.find(1)->maximum_minutes()==maximum,"Quest time accessors changed");
                PlayerSnapshot player;player.quests={{1,static_cast<std::uint16_t>(delay),{3,4,5}},{0,0,{}},{2,0,{}}};player.finished_quests={{1,7},{0,8},{2,9}};
                const auto restored=restore_quests(player,definitions,tick);
                require(restored.active.size()==1 && restored.invalid_active==std::vector<std::uint16_t>{0,2} && restored.finished.size()==1 && restored.finished[0].id==1 && restored.finished[0].success==7,"Quest restoration filtering differs");
                const auto& quest=restored.active[0];
                require(quest.start_tick==start && quest.packet.end_tick==end && quest.success_tick==success && quest.packet.timed==(maximum!=0) && quest.packet.counters==std::array<std::uint8_t,3>{3,4,5} && quest.success==0 && restored.mob_kill_quests==mob && restored.player_kill_quests==pc,"Original quest restoration differs");++restorations;
            } else require(false,"Unknown quest vector kind");
            require(static_cast<bool>(row),"Malformed quest vector");std::string extra;require(!(row>>extra),"Trailing quest vector data");
        }
        require(synthetic==3 && restorations==432 && actual==(argc==3?1U:0U),"Quest oracle coverage changed");
        std::cout<<synthetic<<" original quest loads, "<<restorations<<" runtime restorations and "<<actual<<" full distribution checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
