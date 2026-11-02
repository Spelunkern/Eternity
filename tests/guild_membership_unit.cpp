#include "eternity/game/guild_membership.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& p) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:p) out<<std::setw(2)<<unsigned(b);return out.str();}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild membership oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild membership oracle");
        std::array<GuildUser,3> users;std::array<GuildMembership::Record,384> records;std::unique_ptr<GuildMembership> guild;GuildDirectory directory;
        std::vector<std::string> events;std::size_t next=0,cases=0,operations=0,count=0;bool failed=false,returned=false;std::string line;
        GuildMemberPorts port;port.allocate=[&](const net::Packet& p){events.push_back("ALLOC "+hex(p));return !failed;};port.release=[&]{events.push_back("FREE");};port.send=[&](GuildUser& user,net::Packet p){events.push_back("SEND "+std::to_string(&user-users.data())+' '+hex(p));};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                unsigned int scenario=0;row>>scenario;guild=std::make_unique<GuildMembership>(55,port);directory={};count=std::array<unsigned int,6>{0,1,39,40,41,81}[scenario%6];failed=scenario%5==0;
                for(auto& record:records) record=std::make_shared<GuildMember>();
                for(unsigned int phase=0;phase<3;++phase) for(unsigned int i=0;i<count;++i) {
                    auto record=records[phase*128+i];record->id=(phase+1)*1000+i;record->name=i%3?"N"+std::to_string(record->id):std::string(20,'X');record->job=0x123+static_cast<int>(i);record->level=70000+static_cast<int>(i);record->rank=std::array<std::int32_t,6>{-1,0,1,2,3,255}[i%6];record->user=phase==0 && i%4!=0?&users[i%3]:nullptr;record->billing_id=i%2;record->ranking_entry=77+i;
                    auto& map=phase==0?guild->online:phase==1?guild->offline:guild->applications;map.emplace(record->id,scenario%11==7 && i==1?nullptr:record);
                }
                for(unsigned int i=0;i<3;++i) {users[i]={std::array<std::uint32_t,3>{2000,1000,3000}[i],123,99,100+i,7,nullptr};directory.add(users[i].character_id,scenario%11?guild.get():nullptr);}
                if(scenario%4==1 && count) guild->applications.emplace(2000,records[256]);
                for(unsigned int i=0;i<7;++i) if((scenario%17==16?127:scenario)&(1U<<i)) guild->leaders[i]=records[128+i%std::max<std::size_t>(count,1)];
                events.clear();next=0;++cases;
            } else if(tag=="OP") {
                std::string op;unsigned int i=0;row>>op>>i;events.clear();next=0;
                if(op=="CONNECT") returned=guild->connect(users.at(i));else if(op=="DISCONNECT") returned=guild->disconnect(users.at(i));else if(op=="INIT") directory.initialize(users.at(i));else if(op=="SEND") directory.send(users.at(i));else if(op=="RELEASE") directory.release(users.at(i));else if(op=="LIST") guild->send_users(users.at(i));else if(op=="APPLICATIONS") guild->send_applications(users.at(i));else throw std::runtime_error("Unknown guild operation");++operations;
            } else if(tag=="RETURN") {bool expected=false;row>>expected;require(returned==expected,"Original guild return differs at operation "+std::to_string(operations));}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original guild event differs at case "+std::to_string(cases)+" operation "+std::to_string(operations)+" event "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="STATE") {
                std::ostringstream out;out<<guild->online.size()<<' '<<guild->offline.size()<<' '<<guild->applications.size();
                for(const auto& user:users) out<<' '<<user.guild_id<<' '<<user.rank<<' '<<user.request_id<<' '<<bool(user.guild)<<' '<<user.ranking_entry;
                for(const auto& leader:guild->leaders) out<<' '<<(leader?leader->id:0);
                for(unsigned int phase=0;phase<3;++phase) for(unsigned int i=0;i<count;++i) {const auto& record=records[phase*128+i];out<<' '<<(record->user?static_cast<int>(record->user-users.data())+1:0)<<' '<<record->ranking_entry;}
                require(out.str()==line.substr(6),"Original guild state differs at operation "+std::to_string(operations)+": "+out.str()+" vs "+line.substr(6));
            } else if(tag=="ENDOP") require(next==events.size(),"Extra native guild effects");else if(tag!="END") throw std::runtime_error("Unknown guild fixture row");
        }
        require(cases==72 && operations==648,"Guild membership oracle coverage changed");
        std::cout<<cases<<" original guild sequences, "<<operations<<" membership/list owners, officer slots, packet pool failures and client records passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
