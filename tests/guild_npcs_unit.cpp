#include "eternity/game/guild_npcs.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& p) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:p) out<<std::setw(2)<<unsigned(b);return out.str();}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild NPC fixture");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild NPC fixture");GuildNpcReplica db;GuildNpcs game;std::uint32_t id=0;int fault=-1;unsigned int count=0,index=0;std::array<std::int64_t,3> previous{};std::size_t cases=0;bool db_seen=false,client_seen=false;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>id>>fault>>count;for(auto& slot:db.slots) row>>slot.type>>slot.level>>slot.marker;for(auto& slot:game.slots) row>>slot.type>>slot.level;previous={};index=0;db_seen=client_seen=false;++cases;}
            else if(tag=="ROW") {std::array<std::string,3> values;for(auto& value:values) row>>value;
                if(fault!=-2 && (fault<0 || index<static_cast<unsigned int>(fault))) {for(std::size_t i=0;i<3;++i) if(values[i]!="N") previous[i]=std::stoll(values[i]);db.load(static_cast<std::uint8_t>(previous[2]),static_cast<std::uint16_t>(previous[0]),static_cast<std::uint8_t>(previous[1]));}++index;}
            else if(tag=="RETURN") {unsigned int returned=0;row>>returned;require(returned==((fault==-2 || fault>=0)?0U:1U),"Original SQL fault scenario changed");}
            else if(tag=="DBSTATE") {for(const auto& slot:db.slots) {std::uint16_t type=0,level=0,marker=0;row>>type>>level>>marker;require(slot.type==type && slot.level==level && slot.marker==marker,"Original DB NPC partial/retained state differs at case "+std::to_string(cases));}}
            else if(tag=="DBPACKET") {const auto p=db.packet(id);require(p && hex(*p)==line.substr(9),"Original guild DB NPC packet differs");require(game.apply(*p),"Original valid DB NPC update rejected");db_seen=true;}
            else if(tag=="GAMESTATE") {require(db_seen==db.packet(id).has_value(),"Native DB NPC emitted an extra packet");for(const auto& slot:game.slots) {std::uint16_t type=0,level=0;row>>type>>level;require(slot.type==type && slot.level==level,"Original game NPC update changed untouched slots");}}
            else if(tag=="CLIENT") {const auto p=game.client_packet();require(p && hex(*p)==line.substr(7),"Original guild client NPC packet differs");client_seen=true;}
            else if(tag=="END") require(client_seen==game.client_packet().has_value() && index==count,"NPC fixture row count or omitted client packet differs");else throw std::runtime_error("Unknown guild NPC fixture row");
        }
        require(cases==512,"Guild NPC fixture coverage changed");
        std::cout<<cases<<" original SQL NPC/partial-state/DB packet/game update/client packet sequences passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
