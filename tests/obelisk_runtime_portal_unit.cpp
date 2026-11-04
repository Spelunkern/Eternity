#include "eternity/game/obelisk_runtime_portal.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct World final:ObeliskRuntimePortalWorld {
    int scenario=0;unsigned sends=0;
    std::vector<ObeliskRuntimePortalLink> links;
    std::array<ObeliskRuntime,8> obs;
    std::array<std::int32_t,12> countries{};
    std::vector<std::uint32_t> trace;
    void send(std::uint32_t tag,const eternity::net::Packet& p) {
        trace.push_back(tag);trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());
        if(++sends==1&&scenario%7==0) {
            for(std::size_t i=0;i<links.size();++i) countries[i]=(static_cast<int>(i)+scenario)%5-1;
            for(std::size_t i=0;i<obs.size();++i) obs[i].last_destroy_country=static_cast<std::int32_t>((i+1)%3);
        }
    }
    void database(const eternity::net::Packet& p) override {send(1,p);}
    void map_broadcast(std::size_t,const eternity::net::Packet& p) override {send(2,p);}
    void broadcast(const eternity::net::Packet& p) override {send(3,p);}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    World world;world.scenario=scenario;
    world.links.resize(std::array<std::size_t,8>{0,1,2,3,4,6,11,12}[static_cast<std::size_t>(scenario%8)]);
    for(std::size_t j=0;j<8;++j) {world.obs[j].country=99;world.obs[j].last_destroy_country=(scenario/8+static_cast<int>(j))%4-1;}
    for(std::size_t i=0;i<world.links.size();++i) {
        auto& link=world.links[i];const auto n=static_cast<int>(i);
        link.id=0xf1230000U+static_cast<std::uint32_t>(i);link.base_id=0x10000000U+static_cast<std::uint32_t>(i);link.portal_id=static_cast<std::uint16_t>(i);
        world.countries[i]=(scenario/3+n)%6-1;link.country=(scenario/9+n)%7==0?nullptr:&world.countries[i];
        link.zone_present=(scenario/5+n)%3!=0;link.open={(scenario/2+n)%2!=0,(scenario/4+n)%2!=0};
        link.counts={(scenario/11+n)%6-1,(scenario/13+n)%6-1};
        for(std::size_t j=0;j<8;++j) {
            const auto mode=scenario/17%5;const auto idx=(j+i)%8;
            if(mode==1||mode==2) world.obs[idx].last_destroy_country=mode-1;
            link.links[j]=(scenario/19+n+static_cast<int>(j))%7==0?nullptr:&world.obs[idx];
        }
    }
    const auto forced=scenario/64%4;
    if(forced==2||forced==3) {
        const auto side=forced==3?0:1;
        for(auto& ob:world.obs) ob.last_destroy_country=side;
        for(std::size_t i=0;i<world.links.size();++i) {
            auto& link=world.links[i];link.country=&world.countries[i];world.countries[i]=3;link.open={false,false};
            link.counts={side==0?2:0,side==1?2:0};
            for(std::size_t j=0;j<8;++j) link.links[j]=&world.obs[j];
        }
    }
    update_obelisk_portal_links(world.links,world);
    for(std::size_t i=0;i<world.links.size();++i) {
        world.trace.push_back(99);world.trace.push_back(static_cast<std::uint32_t>(world.countries[i]));
        world.trace.push_back(world.links[i].open[0]);world.trace.push_back(world.links[i].open[1]);
    }
    if(world.trace!=expected) {
        std::size_t i=0;while(i<world.trace.size()&&i<expected.size()&&world.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<world.trace.size()?std::to_string(world.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Portal coordination comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected portal fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing portal fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int scenario;row>>scenario;std::uint32_t word;std::vector<std::uint32_t> expected;
            while(row>>word) expected.push_back(word);
            run(scenario,expected);++count;
        }
        if(count!=1024) throw std::runtime_error("Incomplete portal fixture");
        World world;world.links.resize(13);bool rejected=false;
        try {update_obelisk_portal_links(world.links,world);} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected||!world.trace.empty()) throw std::runtime_error("Unsafe original stack size not rejected before publication");
        std::cout<<count<<" original portal coordination/publication scenarios passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
