#include "eternity/game/obelisk_runtime_ownership.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
struct Clock final:ObeliskRuntimeOwnershipClock {
    int scenario=0;std::uint32_t seconds=0;
    std::array<std::vector<ObeliskRuntime>,3> records;
    std::vector<std::uint32_t> trace;
    void event(std::initializer_list<std::uint32_t> values) {trace.insert(trace.end(),values);}
    std::uint32_t packed_now() override {
        const auto now=0xa1b2c3d4U+static_cast<std::uint32_t>(scenario);event({1,now});
        if(scenario%11==0) for(auto& map:records) for(auto& o:map) {o.base_id^=0x00400000U;o.country=-5;}
        return now;
    }
    std::uint32_t seconds_between(std::uint32_t now,std::uint32_t change) override {event({2,now,change});return seconds;}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    Clock clock;clock.scenario=scenario;
    clock.seconds=std::array<std::uint32_t,8>{0,1,86399,86400,86401,0x7fffffffU,0x80000000U,0xffffffffU}[static_cast<std::size_t>(scenario/2%8)];
    const auto packed=scenario%3==0?
        static_cast<std::uint32_t>(std::array<std::int32_t,8>{-1,0,1,2,3,100,INT32_MIN,INT32_MAX}[static_cast<std::size_t>(scenario/3%8)]):
        (static_cast<std::uint32_t>(scenario)*73U)^0xdead0000U;
    const auto requested=(scenario%3==0?0x10010203U:0x10203U)+static_cast<std::uint32_t>(scenario/7%3);
    const auto change=scenario%5==0?0U:0xf2345678U+static_cast<std::uint32_t>(scenario);
    std::array<std::span<ObeliskRuntime>,3> maps;
    for(std::size_t mi=0;mi<maps.size();++mi) {
        auto& map=clock.records[mi];map.resize((static_cast<std::size_t>(scenario/3)+mi)%4);
        for(std::size_t row=0;row<map.size();++row) {
            auto& o=map[row];o.country=static_cast<std::int32_t>(70+mi*10+row);o.last_destroy_country=-30-static_cast<std::int32_t>(mi*10+row);
            o.reset_time=static_cast<std::uint32_t>(0x11110000U+mi*16+row);o.change_time=static_cast<std::uint32_t>(0x22220000U+mi*16+row);
            o.base_id=static_cast<std::uint32_t>(0x10203U+row%2);o.respawn.current=9;o.respawn.next=3334;
        }
        maps[mi]=map;
    }
    std::array<std::int32_t,3> country{};std::array<std::array<bool,2>,3> open{};
    std::array<ObeliskRuntimePortalOwnership,3> portals;
    for(std::size_t i=0;i<portals.size();++i) {
        country[i]=-7+static_cast<std::int32_t>(i);open[i]={(static_cast<std::size_t>(scenario)+i)%2!=0,(static_cast<std::size_t>(scenario/2)+i)%2!=0};
        portals[i]={static_cast<std::uint32_t>(0x10010203U+i%2),(static_cast<std::size_t>(scenario/5)+i)%3?&country[i]:nullptr,&open[i]};
    }
    restore_obelisk_ownership(maps,portals,requested,packed,change,clock);
    for(const auto& map:clock.records) for(const auto& o:map)
        clock.event({99,static_cast<std::uint32_t>(o.country),static_cast<std::uint32_t>(o.last_destroy_country),o.reset_time,o.change_time,o.base_id,
                     static_cast<std::uint32_t>(o.respawn.current),o.respawn.next});
    for(std::size_t i=0;i<portals.size();++i) clock.event({98,static_cast<std::uint32_t>(country[i]),static_cast<std::uint32_t>(open[i][0]),static_cast<std::uint32_t>(open[i][1])});
    if(clock.trace!=expected) {
        std::size_t i=0;while(i<clock.trace.size()&&i<expected.size()&&clock.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<clock.trace.size()?std::to_string(clock.trace[i]):"end")
                 <<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original ownership restoration comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected ownership oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing ownership oracle fixture");
        std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t word;
            while(row>>word) expected.push_back(word);
            run(scenario,expected);++cases;
        }
        if(cases!=512) throw std::runtime_error("Incomplete ownership fixture");
        std::cout<<cases<<" original persisted ownership/portal restoration scenarios passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
