#include "eternity/game/character_entry.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const eternity::net::Packet& packet) {std::ostringstream text;text<<std::hex<<std::setfill('0');for(auto byte:packet) text<<std::setw(2)<<unsigned(byte);return text.str();}
eternity::net::Packet unhex(const std::string& value) {eternity::net::Packet packet;for(std::size_t i=0;i<value.size();i+=2) packet.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return packet;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original character entry oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open entry oracle");std::string line;std::vector<std::string> events;std::size_t selections=0,entries=0,next=0;CharacterSelectionState selection;CharacterEntryState state;unsigned int guild_mode=0;std::uint32_t day=0,time=0;
        CharacterSelectionPorts gate;gate.send_db=[&](eternity::net::Packet p){events.push_back("DB "+hex(p));};gate.close=[&](std::uint32_t reason){events.push_back("CLOSE "+std::to_string(reason));};CharacterEntryPorts port;port.send_client=[&](eternity::net::Packet p){events.push_back("CLIENT "+hex(p));};port.send_manager=[&](eternity::net::Packet p){events.push_back("MANAGER "+hex(p));};port.world_day=[&](){events.push_back("WORLD_DAY");return day;};port.system_time=[&](){events.push_back("SYSTEM_TIME");return time;};port.step=[&](CharacterEntryStep step){const auto n=static_cast<unsigned int>(step);events.push_back("STEP "+std::to_string(n)+(step==CharacterEntryStep::flush?" "+std::to_string(state.ready):""));};port.lock_guild=[&](){if(!state.guild_present) return false;events.push_back("GUILD_LOCK");if(guild_mode==2){state.guild_present=false;events.push_back("GUILD_UNLOCK");return false;}return true;};port.unlock_guild=[&](){events.push_back("GUILD_UNLOCK");};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="SELECT") {unsigned int country=0;std::string bytes;row>>selection.user_id>>country>>selection.enabled>>bytes;selection.country=static_cast<std::uint8_t>(country);events.clear();next=0;const auto result=request_character_selection(selection,unhex(bytes),gate);require(result==(events.empty()?CharacterSelectionResult::ignored:events[0].starts_with("DB ")?CharacterSelectionResult::requested:CharacterSelectionResult::closed),"Selection gate result disagrees with its effects");++selections;}
            else if(tag=="SELECTED") {bool enabled=false;row>>enabled;require(selection.enabled==enabled,"Original selection latch differs");}
            else if(tag=="ENTRY") {state={};unsigned int motion=0,a=0,b=0;row>>state.user_id>>state.character_id>>state.auto_character_id>>state.level>>guild_mode>>motion>>a>>b>>day>>time;for(auto& value:state.current) row>>value;for(auto& value:state.maximum) row>>value;state.runtime_id=0x87654321;state.motion=static_cast<std::uint8_t>(motion);state.flag_584c=static_cast<std::uint8_t>(a);state.flag_584d=static_cast<std::uint8_t>(b);state.guild_present=guild_mode!=0;events.clear();next=0;complete_character_selection(state,port);++entries;}
            else if(tag=="READY") {std::uint32_t id=0;std::array<std::int32_t,3> current{};bool ready=false,guild=false;row>>id;for(auto& value:current) row>>value;row>>ready>>guild;require(state.runtime_id==id && state.current==current && state.ready==ready && state.guild_present==guild,"Original entry state/clamps differ");}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original entry order/packet differs at selection "+std::to_string(selections)+" entry "+std::to_string(entries)+" effect "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="END") require(next==events.size(),"Native character entry emitted extra effects");
            else throw std::runtime_error("Unknown entry oracle row");
        }
        require(selections==144 && entries==864,"Entry oracle coverage changed");
        for(std::size_t length=0;length<6;++length) {selection={530001,0,true};events.clear();require(request_character_selection(selection,eternity::net::Packet(length),gate)==CharacterSelectionResult::closed && events==std::vector<std::string>{"CLOSE 9"} && selection.enabled,"Truncated select request read beyond its frame or consumed the latch");}
        // Native exceptional cleanup must release the captured guild lock even
        // if a required callback violates its no-throw boundary contract.
        state={};state.guild_present=true;guild_mode=1;events.clear();const auto step=port.step;port.step=[&](CharacterEntryStep op){if(op==CharacterEntryStep::guild_bank) throw std::runtime_error("Guild send fixture fault");step(op);};bool failed=false;try{complete_character_selection(state,port);}catch(const std::runtime_error&){failed=true;}require(failed && !state.ready && events.back()=="GUILD_UNLOCK","Exceptional entry leaked its captured guild lock or marked readiness");
        std::cout<<selections<<" original selection gates and "<<entries<<" complete 0407 initialization/packet sequences passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
