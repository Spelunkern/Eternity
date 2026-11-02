#include "eternity/game/user_lifecycle.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity::game;
namespace {void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original lifecycle oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open lifecycle oracle");std::string line;std::vector<std::string> events;std::size_t cases=0,next=0,clocks=0;UserLifecycleState state;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                std::string kind;std::uint32_t job=0;bool success=false;row>>kind>>job>>state.status>>state.connected>>success>>state.user_id;state.next_save_tick=0xf0000000;events.clear();next=0;
                UserLifecyclePorts ports;ports.post=[&](std::uint32_t n){events.push_back("POST "+std::to_string(n));};ports.send=[&](eternity::net::Packet p){std::ostringstream text;text<<"SEND "<<std::hex<<std::setfill('0');for(auto byte:p) text<<std::setw(2)<<unsigned(byte);events.push_back(text.str());};ports.leave=[&](){events.push_back("LEAVE "+std::to_string(state.user_id));};ports.reset=[&](){events.push_back("RESET");};ports.roster=[&](bool country){events.push_back("ROSTER "+std::to_string(country));};ports.save=[&](bool periodic,bool quick){events.push_back("SAVE "+std::to_string(periodic)+' '+std::to_string(quick));return success;};ports.save_exit=[&](bool shutdown){events.push_back("SAVE_EXIT "+std::to_string(shutdown));};UserLifecycle lifecycle(state,std::move(ports));
                if(kind=="EXIT") lifecycle.exit();else if(kind=="DISCONNECT") lifecycle.disconnect_by_game_server();else if(kind=="LOGOUT") lifecycle.logout_select();else {require(lifecycle.dispatch_save_job(job),"Recovered lifecycle job was rejected");require(!lifecycle.dispatch_save_job(26),"Unreconstructed DB job reported success");}++cases;
            }
            else if(tag=="LOAD_CLOCK") {std::uint32_t time_id=0,last=0,expected_status=0,expected_tick=0;row>>time_id>>last>>expected_status>>expected_tick;UserLifecyclePorts p;p.post=[](std::uint32_t){};p.send=[](eternity::net::Packet){};p.leave=p.reset=[](){};p.roster=[](bool){};p.save=[](bool,bool){return false;};p.save_exit=[](bool){};UserLifecycle life(state,std::move(p));life.character_loaded(time_id,last);require(state.status==expected_status && state.next_save_tick==expected_tick,"Original load save-clock staggering differs");++clocks;}
            else if(tag=="STATE") {std::uint32_t status=0,tick=0;row>>status>>tick;require(state.status==status && state.next_save_tick==tick,"Original lifecycle state differs at case "+std::to_string(cases));}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original lifecycle effect differs at case "+std::to_string(cases)+" effect "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="END") require(next==events.size(),"Extra native lifecycle effects");
            else throw std::runtime_error("Unknown lifecycle oracle row");
        }
        require(cases==560 && clocks==54,"Lifecycle oracle coverage changed");
        std::cout<<cases<<" original exit/disconnect/selection/save-job cases and "<<clocks<<" load clocks passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
