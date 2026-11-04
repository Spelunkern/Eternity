#include "eternity/game/obelisk_runtime_publication.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
using eternity::net::Packet;
namespace {
struct Transport final:ObeliskRuntimePublication {
    int scenario=0,operation=0;std::size_t count=0,sends=0;
    std::vector<ObeliskRuntime> records;
    std::vector<std::uint32_t> trace;
    void packet(std::uint32_t tag,const Packet& packet) {
        trace.push_back(tag);trace.push_back(static_cast<std::uint32_t>(packet.size()));trace.insert(trace.end(),packet.begin(),packet.end());++sends;
        if(operation==2 && scenario/36%2 && sends==1) {
            for(std::size_t row=100;row<count;++row) {
                auto& o=records[row];o.country=2;o.last_destroy_country=-1;o.base_id^=0xa5a5a5a5U;o.change_time+=99U;
            }
        }
    }
    void zone_packet(const Packet& bytes) override {packet(1,bytes);}
    void database_packet(const Packet& bytes) override {packet(2,bytes);}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    Transport transport;transport.scenario=scenario;transport.operation=scenario/12%3;
    transport.count=std::array<std::size_t,12>{0,1,2,99,100,101,199,200,201,255,256,301}[static_cast<std::size_t>(scenario%12)];
    transport.records.resize(std::max(transport.count,std::size_t{1}));
    for(std::size_t row=0;row<transport.records.size();++row) {
        auto& o=transport.records[row];const auto r=static_cast<std::uint32_t>(row);
        o.id=0x81000000U+r*65537U+static_cast<std::uint32_t>(scenario);
        o.country=std::array<std::int32_t,7>{-1,0,1,2,15,INT32_MAX,INT32_MIN}[(static_cast<std::size_t>(scenario)+row)%7];
        o.last_destroy_country=std::array<std::int32_t,8>{-1,0,1,2,15,31,INT32_MAX,INT32_MIN}[(static_cast<std::size_t>(scenario/2)+row)%8];
        o.change_time=0xffffffffU-(static_cast<std::uint32_t>(scenario)*65539U+r*7U);
        o.base_id=0xf1230000U+r*37U+static_cast<std::uint32_t>(scenario);
    }
    ObeliskRuntimeMap map;map.zone_present=scenario%7!=0;map.reset_send=scenario/3%2!=0;
    const auto index=static_cast<std::size_t>(scenario/5)%transport.records.size();
    if(transport.operation==0) publish_obelisk_change(map,transport.records[index],transport);
    else if(transport.operation==1) publish_obelisk_database_change(transport.records[index],transport);
    else publish_obelisk_database_list(std::span<const ObeliskRuntime>(transport.records).first(transport.count),transport);
    if(transport.trace!=expected) {
        std::size_t i=0;while(i<transport.trace.size()&&i<expected.size()&&transport.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<transport.trace.size()?std::to_string(transport.trace[i]):"end")
                 <<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original publication comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected publication oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing publication oracle fixture");
        std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t word;
            while(row>>word) expected.push_back(word);
            run(scenario,expected);++cases;
        }
        if(cases!=216) throw std::runtime_error("Incomplete publication fixture");
        std::cout<<cases<<" original publication/chunking scenarios passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
