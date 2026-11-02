#include "eternity/game/zone_entry.hpp"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const eternity::net::Packet& packet) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:packet) out<<std::setw(2)<<unsigned(byte);return out.str();}
std::string cell(CellCoordinates value) {return std::to_string(value.x)+' '+std::to_string(value.z);}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original zone entry oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open zone entry oracle");std::string line;std::vector<std::string> events;std::size_t cases=0,next=0;ZoneEntryState state;ZoneEntryMap map;bool returned=false;std::int32_t shape=0;ZoneEntryPorts port;
        port.attach=[&](CellCoordinates c){require(state.bound && state.cell.x==c.x && state.cell.z==c.z && state.map==map.id,"Membership preceded original field assignment");events.push_back("ATTACH "+cell(c));};port.send_view=[&](CellCoordinates c,eternity::net::Packet p){events.push_back("VIEW "+cell(c)+' '+hex(p));};port.shape=[&](){events.push_back("SHAPE");return shape;};port.send_shape=[&](){events.push_back("SEND_SHAPE");};port.send_speed=[&](){events.push_back("SEND_SPEED");};port.cell_info=[&](CellCoordinates c){events.push_back("CELL_INFO "+cell(c));};port.weather=[&](){events.push_back("WEATHER 0");};port.obelisks=[&](){events.push_back("OBELISKS");};port.entered=[&](){events.push_back("ENTER_ZONE");};port.diagnostic=[&](){events.push_back("DIAGNOSTIC");};port.close=[&](std::uint32_t reason){events.push_back("CLOSE "+std::to_string(reason));};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {std::uint32_t x=0,y=0,z=0;unsigned int permission=0;row>>map.size>>x>>y>>z>>map.width>>map.height>>map.id>>map.dungeon>>state.character_id>>state.direction>>permission>>state.vehicle_mode>>state.vehicle_partner>>state.guild_user_level>>shape>>state.bound;state.permission=static_cast<std::uint8_t>(permission);state.position={std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)};state.map=31337;state.cell={-7,-9};events.clear();next=0;returned=enter_zone(state,map,port);++cases;}
            else if(tag=="RETURN") {bool success=false,bound=false;std::uint16_t id=0;CellCoordinates c;std::int32_t mode=0;row>>success>>id>>c.x>>c.z>>mode>>bound;require(returned==success && state.map==id && state.cell.x==c.x && state.cell.z==c.z && state.vehicle_mode==mode && state.bound==bound,"Original zone entry state differs at case "+std::to_string(cases));}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original zone entry order/packet differs at case "+std::to_string(cases)+" effect "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="END") require(next==events.size(),"Native zone entry emitted extra effects");
            else throw std::runtime_error("Unknown zone entry oracle row");
        }
        require(cases==1584,"Zone entry oracle coverage changed");
        std::cout<<cases<<" original zone admission/state/0201/notification cases passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
