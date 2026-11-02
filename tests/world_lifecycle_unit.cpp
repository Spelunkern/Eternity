#include "eternity/game/world_lifecycle.hpp"
#include "eternity/game/entry_catalogs.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& packet) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:packet) out<<std::setw(2)<<unsigned(byte);return out.str();}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original world lifecycle oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open world lifecycle oracle");
        std::string line;std::size_t cases=0,operations=0,next=0;WorldLifecycleState state;WorldLifecyclePorts port;OfflineMoney cache;std::vector<std::string> events;bool admin=false;
        const auto calendar=std::chrono::sys_days{std::chrono::year{2024}/5/8}+std::chrono::hours{16}+std::chrono::minutes{46}+std::chrono::seconds{28};
        port.admin_enter=[&]{events.push_back("ADMIN");return admin;};port.step=[&](WorldLifecycleStep step){events.push_back("STEP "+std::to_string(static_cast<unsigned int>(step)));};port.close=[&](std::uint32_t reason){events.push_back("CLOSE "+std::to_string(reason));};port.local_calendar=[&]{events.push_back("CALENDAR");return calendar;};
        port.game_log=[&](net::Packet packet){events.push_back("LOG "+hex(packet));};port.manager=[&](net::Packet packet){events.push_back("MANAGER "+hex(packet));};
        port.take_money=[&](std::uint32_t id){const auto value=cache.take(id);events.push_back("TAKE "+std::to_string(id)+' '+(value?std::to_string(*value):"-1"));return value;};port.save_money=[&](std::uint32_t id,std::uint32_t value){cache.save(id,value);events.push_back("SAVE "+std::to_string(id)+' '+std::to_string(value));};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                state={};cache={};unsigned int permission=0,party=0,billing=0;std::int64_t old=-1;
                row>>permission>>admin>>party>>state.vehicle_mode>>state.auto_character>>state.reconnect_ready>>state.entry_type>>state.admin_question>>state.money>>state.initial_money>>state.cash_points>>state.ip>>state.disconnected_seconds>>billing>>old>>state.skill_points>>state.stat_points;
                for(auto& value:state.stats) row>>value;state.permission=static_cast<std::uint8_t>(permission);state.billing_mode=static_cast<std::uint8_t>(billing);state.party=party?std::optional{party==2}:std::nullopt;state.bank_money=0xaabbccdd;
                state.subject={42,9001,0x12345678,"account","Hero",30,20,{20.5F,-16,-32}};
                if(old>=0) cache.save(9001,static_cast<std::uint32_t>(old));++cases;
            } else if(tag=="OP") {std::string op;row>>op;events.clear();next=0;if(op=="ENTER") enter_world(state,port);else leave_world(state,port);++operations;}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original world lifecycle differs at case "+std::to_string(cases)+" operation "+std::to_string(operations)+" event "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="ENDOP") require(next==events.size(),"Extra native world lifecycle events");else if(tag!="END") throw std::runtime_error("Unknown world lifecycle oracle row");
        }
        require(cases==1024 && operations==3072,"World lifecycle oracle coverage changed");
        cache.save(0,1);cache.save(0,0);require(cache.take(0)==0 && !cache.take(0),"Offline money overwrite/consume semantics changed");cache.save(0xffffffff,0xffffffff);require(cache.take(0xffffffff)==0xffffffff,"Offline money cache lost DWORD values");
        SocialCatalog social(42,9001),peer(43,7);social.add_friend({0});social.add_friend({7});social.add_friend({8});social.add_friend({7});std::vector<std::uint32_t> looked,sent;
        social.notify_offline([&](std::uint32_t id)->const SocialCatalog* {looked.push_back(id);return id==7?&peer:nullptr;},[&](std::uint32_t id,net::Packet packet){require(packet==net::Packet({7,0x22,0x29,0x23,0,0,0}),"Offline friend packet changed");sent.push_back(id);});
        require(looked==std::vector<std::uint32_t>{7,8,7} && sent==std::vector<std::uint32_t>{7,7} && social.friends().size()==4,"Offline notifications filtered duplicates/one-way friends or cleared the cache");
        std::cout<<cases<<" original world lifecycle sequences, "<<operations<<" owners, log/manager packets, offline money and social cleanup passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
