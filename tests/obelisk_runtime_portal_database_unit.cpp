#include "eternity/game/obelisk_runtime_portal.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct World final:ObeliskRuntimePortalWorld {
    int scenario=0;unsigned sends=0;std::vector<ObeliskRuntimePortalLink> links;std::vector<std::uint32_t> trace;
    void database(const eternity::net::Packet& p) override {
        trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());
        if(++sends==1&&scenario/12%2) for(std::size_t i=100;i<links.size();++i) {links[i].base_id^=0xa5a5a5a5U;links[i].country=nullptr;links[i].open={true,true};}
    }
    void map_broadcast(std::size_t,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected map service during initial DB list");}
    void broadcast(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected world service during initial DB list");}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    World world;world.scenario=scenario;world.links.resize(std::array<std::size_t,12>{0,1,2,12,99,100,101,199,200,201,255,301}[static_cast<std::size_t>(scenario%12)]);
    std::int32_t country=INT32_MIN;
    for(std::size_t i=0;i<world.links.size();++i) {
        auto& link=world.links[i];link.id=0xf1230000U+static_cast<std::uint32_t>(i);link.base_id=(static_cast<std::uint32_t>(i)*0x10203U)^0x10000000U^static_cast<std::uint32_t>(scenario);
        link.country=(static_cast<std::size_t>(scenario/12)+i)%3?&country:nullptr;link.open={(static_cast<std::size_t>(scenario)+i)%2!=0,(static_cast<std::size_t>(scenario/2)+i)%2!=0};
    }
    publish_obelisk_portal_database_list(world.links,world);
    if(world.trace!=expected) {
        std::size_t i=0;while(i<world.trace.size()&&i<expected.size()&&world.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<world.trace.size()?std::to_string(world.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original initial portal DB list comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected portal DB oracle fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing portal DB fixture");std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t word;while(row>>word) expected.push_back(word);run(scenario,expected);++count;}
        if(count!=96) throw std::runtime_error("Incomplete portal DB fixture");std::cout<<count<<" original initial portal DB list cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
