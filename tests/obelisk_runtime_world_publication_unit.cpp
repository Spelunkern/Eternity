#include "eternity/game/obelisk_runtime_world_publication.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct Database final:ObeliskRuntimePublication {
    int scenario=0;unsigned sends=0;std::vector<std::vector<ObeliskRuntime>> maps;
    std::vector<ObeliskRuntimePortalLink> portals;std::vector<std::uint32_t> trace;
    void zone_packet(const eternity::net::Packet&) override {throw std::runtime_error("World persistence requested a zone service");}
    void database_packet(const eternity::net::Packet& p) override {
        trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());++sends;
        if(scenario/8%2) {
            for(auto& records:maps) for(auto& o:records) {o.country=(o.country+1)%3;o.last_destroy_country=(o.last_destroy_country+1)%3;o.change_time+=sends;o.base_id^=0xa5a5a5a5U;}
            for(auto& link:portals) link.base_id^=0x55555555U;
        }
    }
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    Database db;db.scenario=scenario;db.maps.resize(static_cast<std::size_t>(scenario/16%5));std::vector<std::span<const ObeliskRuntime>> maps;
    for(std::size_t mi=0;mi<db.maps.size();++mi) {
        auto& records=db.maps[mi];records.resize(std::array<std::size_t,8>{0,1,2,99,100,101,199,200}[(static_cast<std::size_t>(scenario/2)+mi)%8]);
        for(std::size_t row=0;row<records.size();++row) {auto& o=records[row];o.country=(scenario+static_cast<int>(mi+row))%3;o.last_destroy_country=(scenario/3+static_cast<int>(mi+row))%3;o.change_time=0x12340000U+static_cast<std::uint32_t>(mi*256+row);o.base_id=0x00010000U+static_cast<std::uint32_t>(mi*256+row);}
        maps.push_back(records);
    }
    db.portals.resize(std::array<std::size_t,8>{0,1,99,100,101,199,200,201}[static_cast<std::size_t>(scenario%8)]);
    for(std::size_t i=0;i<db.portals.size();++i) db.portals[i].base_id=0x10010000U+static_cast<std::uint32_t>(i);
    publish_world_obelisk_database_lists(db.portals,maps,db);
    if(db.trace!=expected) {
        std::size_t i=0;while(i<db.trace.size()&&i<expected.size()&&db.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<db.trace.size()?std::to_string(db.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original world persistence comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected world persistence fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing world persistence fixture");std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);run(scenario,expected);++count;}
        if(count!=128) throw std::runtime_error("Incomplete world persistence fixture");std::cout<<count<<" original complete world persistence cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
