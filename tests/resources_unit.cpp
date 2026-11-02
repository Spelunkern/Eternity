#include "eternity/game/resources.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Sink final:ResourceSink {
    bool zone=false;std::optional<bool> party;std::vector<std::string> events;
    bool in_zone() const noexcept override {return zone;}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void record(char route,const net::Packet& packet) {
        static constexpr char hex[]="0123456789abcdef";std::string value(1,route);
        for(auto byte:packet) {value+=hex[byte>>4];value+=hex[byte&15];}events.push_back(value);
    }
    void send_view(const net::Packet& packet) override {record('V',packet);}
    void send_party(const net::Packet& packet) override {record('P',packet);}
    void send_self(const net::Packet& packet) override {record('S',packet);}
    void reset_damage() override {events.emplace_back("D");}
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected resource oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open resource vectors");
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);int op=0,zone=0,party=0,initializing=0,attack=0,recovering=0;
            PlayerResources current{},maximum{},expected{};std::array<std::int32_t,3> amounts{},actual{};std::size_t event_count=0;
            row>>op>>zone>>party>>initializing>>attack>>recovering>>current.hp>>current.mp>>current.sp>>maximum.hp>>maximum.mp>>maximum.sp;
            for(auto& value:amounts) row>>value;
            row>>expected.hp>>expected.mp>>expected.sp;for(auto& value:actual) row>>value;row>>event_count;
            require(static_cast<bool>(row) && op>=0 && op<=5,"Malformed resource vector");
            Sink sink;sink.zone=zone!=0;if(party>=0) sink.party=party!=0;ActorResources actor(9001,current,sink);
            if(op==0) require(actor.heal(maximum,amounts)==actual,"Heal adjusted amounts differ at vector "+std::to_string(count));
            else if(op<=2) actor.recover(maximum,amounts,op==2,attack,recovering!=0);
            else actor.maximum_changed(maximum,static_cast<std::uint8_t>(op-3),initializing!=0);
            const auto& result=actor.current();const auto context=" at vector "+std::to_string(count)+" operation "+std::to_string(op);
            require(result.hp==expected.hp && result.mp==expected.mp && result.sp==expected.sp,"Resources differ"+context);
            require(sink.events.size()==event_count,"Event count differs"+context);
            for(const auto& event:sink.events) {std::string original;row>>original;require(event==original,"Event differs"+context+": "+event+" versus "+original);}
            std::string extra;require(!(row>>extra),"Unexpected trailing vector data");++count;
        }
        require(count==1008,"Resource vector coverage changed");std::cout<<count<<" original resource mutation and ordered notification vectors passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
