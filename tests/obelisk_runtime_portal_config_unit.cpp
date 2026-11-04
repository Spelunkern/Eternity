#include "eternity/game/obelisk_runtime_portal_config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    std::array<std::array<ObeliskRuntime,2>,3> records;
    std::array<std::array<std::int32_t,3>,3> countries{};
    std::array<std::array<ObeliskRuntimePortalTarget,3>,3> portals;
    std::array<ObeliskRuntimePortalMap,3> maps;
    for(std::size_t mi=0;mi<3;++mi) {
        for(std::size_t row=0;row<2;++row) records[mi][row].base_id=static_cast<std::uint32_t>(mi*65536+100+row);
        for(std::size_t j=0;j<3;++j) {countries[mi][j]=static_cast<int>(mi*10+j)-2;portals[mi][j]={j==2?1002:1001,&countries[mi][j]};}
        maps[mi]={portals[mi],records[mi],(scenario+static_cast<int>(mi))%2==0};
    }
    std::vector<ObeliskRuntimePortalInput> inputs;
    for(int i=0;i<2+scenario%4;++i) {
        ObeliskRuntimePortalInput input;input.base_id=0x10000000U+static_cast<std::uint32_t>(10+i);input.map=(scenario/3+i)%4;
        input.portal=std::array<int,3>{1001,1002,999}[static_cast<std::size_t>((scenario+i)%3)];
        for(std::size_t side=0;side<2;++side) for(int j=0;j<4;++j) input.references[side][static_cast<std::size_t>(j)]=scenario%7==0?ObeliskRuntimePortalReference{3,0}:ObeliskRuntimePortalReference{(scenario/5+i+j)%4,std::array<int,4>{0,1,2,4}[(static_cast<std::size_t>(scenario+i+j)+side)%4]};
        inputs.push_back(input);
    }
    // The full original loader first consumed six ordinary obelisk IDs.
    std::uint32_t next=(scenario%2?0xfffffffcU:100U)+6U;
    auto configuration=ObeliskRuntimePortalConfiguration::bind(inputs,maps,next);
    auto moved=std::move(configuration);std::vector<std::uint32_t> actual{next,static_cast<std::uint32_t>(moved.links().size())};
    for(std::size_t i=0;i<moved.links().size();++i) {
        const auto& link=moved.links()[i];
        for(auto n:{link.id,link.base_id,static_cast<std::uint32_t>(moved.target_map(i)),static_cast<std::uint32_t>(link.portal_id),static_cast<std::uint32_t>(*link.country),static_cast<std::uint32_t>(link.zone_present),static_cast<std::uint32_t>(link.open[0]),static_cast<std::uint32_t>(link.open[1]),static_cast<std::uint32_t>(link.counts[0]),static_cast<std::uint32_t>(link.counts[1])}) actual.push_back(n);
        for(auto* ref:link.links) {
            std::uint32_t key=0;for(std::size_t mi=0;mi<3;++mi) for(std::size_t row=0;row<2;++row) if(ref==&records[mi][row]) key=static_cast<std::uint32_t>(mi*10+row+1);
            if(ref&&!key) throw std::runtime_error("Unbound native portal reference");actual.push_back(key);
        }
    }
    if(actual!=expected) {
        std::size_t i=0;while(i<actual.size()&&i<expected.size()&&actual[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<actual.size()?std::to_string(actual[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original portal configuration mismatch");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected portal configuration fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing fixture");std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> words;std::uint32_t n;while(row>>n) words.push_back(n);run(scenario,words);++count;}
        std::int32_t country=2;std::array<ObeliskRuntime,1> records;std::array<ObeliskRuntimePortalTarget,1> targets{{{1,&country}}};
        std::array<ObeliskRuntimePortalMap,1> maps{{{targets,records,true}}};std::array<ObeliskRuntimePortalInput,1> invalid;invalid[0].portal=1;invalid[0].references[0][0]={0,2};
        std::uint32_t next=0;bool rejected=false;try {static_cast<void>(ObeliskRuntimePortalConfiguration::bind(invalid,maps,next));} catch(const std::out_of_range&) {rejected=true;}
        if(!rejected||next!=1) throw std::runtime_error("Unsafe one-past reference handling changed");
        if(count!=64) throw std::runtime_error("Incomplete portal configuration fixture");std::cout<<count<<" original portal binding cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
