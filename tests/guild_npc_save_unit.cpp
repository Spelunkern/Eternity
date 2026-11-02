#include "eternity/game/guild_sql.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild NPC save oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild NPC save oracle");GuildDbMetadata cache;
        unsigned int mode=0,fail_at=0,turn=0,attempt=0;std::uint32_t guild=0,returned=0;std::size_t cases=0,passes=0,next=0;std::vector<std::string> trace;std::string line;
        GuildNpcSavePorts ports{[&]{const bool ok=turn!=0 || mode!=1;trace.push_back("ACQUIRE "+std::to_string(ok));return ok;},[&]{trace.push_back("RELEASE");},[&]{trace.push_back("PREPARE");},[&](const GuildNpcSaveOperation& operation){const bool ok=turn!=0 || (mode!=3 && (mode!=2 || attempt!=fail_at));++attempt;std::string text="EXEC exec "+operation.procedure+' ';for(std::size_t i=0;i<4;++i) text+=(i?",":"")+std::to_string(operation.parameters[i]);trace.push_back(text+" | "+std::to_string(ok));return ok;},[&]{trace.push_back("ERROR");}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {cache=GuildDbMetadata{};row>>guild>>mode>>fail_at>>cache.metadata.etin>>cache.metadata.keep_etin>>cache.save_type;for(auto& npc:cache.npcs.slots) row>>npc.type>>npc.level>>npc.marker;++cases;}
            else if(tag=="UPGRADE") {unsigned int slot=0,type=0,level=0;std::uint32_t etin=0,keep=0;row>>slot>>type>>level>>etin>>keep;require(cache.upgrade_npc(static_cast<std::uint8_t>(slot),static_cast<std::uint16_t>(type),static_cast<std::uint8_t>(level),etin,keep),"Original NPC upgrade rejected");}
            else if(tag=="RESET") {std::uint32_t keep=0;row>>keep;cache.reset_npcs(keep);}
            else if(tag=="PASS") {row>>turn;attempt=0;next=0;trace.clear();returned=save_guild_npcs(guild,cache.npcs,ports);++passes;}
            else if(tag=="TRACE") {require(next<trace.size() && trace[next]==line.substr(6),"Original guild NPC save trace differs at case "+std::to_string(cases)+": "+(next<trace.size()?trace[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="RETURN") {std::uint32_t expected=0;row>>expected;require(expected==returned,"Original NPC save error count differs");}
            else if(tag=="STATE") {
                std::uint32_t etin=0,keep=0,marker=0;row>>etin>>keep>>marker;require(cache.metadata.etin==etin && cache.metadata.keep_etin==keep && static_cast<std::uint32_t>(cache.save_type)==marker,"Original NPC upgrade/reset metadata differs");
                for(const auto& npc:cache.npcs.slots) {unsigned int type=0,level=0,save=0;row>>type>>level>>save;require(npc.type==type && npc.level==level && npc.marker==save,"Original NPC slot state differs at case "+std::to_string(cases));}
            } else if(tag=="ENDPASS") require(next==trace.size(),"Extra native NPC persistence operation");else if(tag!="END") throw std::runtime_error("Unknown NPC save fixture row");
        }
        require(cases==256 && passes==512,"Original NPC save coverage changed");std::cout<<cases<<" original NPC upgrade/reset scenarios and "<<passes<<" save passes matched slot, metadata, SQL, error and marker behavior\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
