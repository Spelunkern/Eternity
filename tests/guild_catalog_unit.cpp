#include "eternity/game/guild_catalog.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(std::string_view s) {if(s.empty()) return "-";std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char c:s) out<<std::setw(2)<<unsigned(c);return out.str();}
net::Packet packet(const std::string& raw) {net::Packet out;for(std::size_t i=0;i<raw.size();i+=2) out.push_back(static_cast<std::uint8_t>(std::stoul(raw.substr(i,2),nullptr,16)));return out;}
std::vector<std::string> snapshot(const GuildCatalog& catalog) {
    std::vector<std::string> out{"MAX "+std::to_string(catalog.maximum_id())};
    for(const auto& [id,unused]:catalog.listings()) {
        static_cast<void>(unused);const auto* guild=catalog.find(id);const auto& m=guild->metadata;const auto& v=m.listing;std::ostringstream line;
        line<<"GUILD "<<id<<' '<<hex(v.name)<<' '<<hex(v.master)<<' '<<v.country<<' '<<v.points<<' '<<unsigned(v.rank)<<' '<<m.etin<<' '<<unsigned(m.use_house)<<' '<<hex(v.notice)<<' '<<unsigned(m.buy_house)<<' '<<m.keep_etin<<' '<<m.return_count<<' '<<m.officers;out.push_back(line.str());
        for(unsigned int phase=0;phase<2;++phase) for(const auto& [key,record]:(phase?guild->membership.applications:guild->membership.offline)) {
            std::ostringstream row;row<<"MEMBER "<<id<<' '<<(phase?"APPLICATION":"OFFLINE")<<' '<<key<<' '<<record->id<<' '<<record->level<<' '<<record->job<<' '<<record->rank<<' '<<hex(record->name)<<' '<<record->billing_id<<' '<<record->ranking_entry;out.push_back(row.str());
        }
    }
    return out;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild catalog oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild catalog oracle");std::unique_ptr<GuildCatalog> catalog;std::vector<std::string> events,state;std::size_t next=0,field=0,cases=0,packets=0;unsigned int scenario=0,ga=0,ma=0;std::string line;
        GuildCatalogPorts port;port.transport={[](const net::Packet&)->bool{throw std::runtime_error("Load broadcast unexpectedly");},[](){throw std::runtime_error("Load release unexpectedly");},[](GuildUser&,net::Packet){throw std::runtime_error("Load sent client packet unexpectedly");}};
        port.allocate_guild=[&]{const auto ok=!(scenario%3==0 && ++ga%3==0);if(scenario%3!=0) ++ga;events.push_back("GALLOC "+std::to_string(ok));return ok;};port.allocate_member=[&]{const auto ok=!(scenario%4==0 && ++ma%4==0);if(scenario%4!=0) ++ma;events.push_back("MALLOC "+std::to_string(ok));return ok;};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>scenario;catalog=std::make_unique<GuildCatalog>(port);ga=ma=0;++cases;}
            else if(tag=="PACKET") {std::string raw;row>>raw;events.clear();next=field=0;require(catalog->apply(packet(raw))==EntryCatalogUpdate::applied,"Original valid guild load rejected");state=snapshot(*catalog);++packets;}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Guild catalog allocation order differs at packet "+std::to_string(packets));++next;}
            else if(tag=="MAX" || tag=="GUILD" || tag=="MEMBER") {require(field<state.size() && state[field]==line,"Original guild catalog state differs at packet "+std::to_string(packets)+": "+(field<state.size()?state[field]:"<none>")+" vs "+line);++field;}
            else if(tag=="ENDSTATE") require(next==events.size() && field==state.size(),"Extra native guild catalog effects/state");else if(tag!="END") throw std::runtime_error("Unknown guild catalog oracle row");
        }
        require(cases==96 && packets==768,"Guild catalog oracle coverage changed");
        std::cout<<cases<<" original guild catalog sequences, "<<packets<<" DB packets, first-key indexes, master correction and membership classification passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
