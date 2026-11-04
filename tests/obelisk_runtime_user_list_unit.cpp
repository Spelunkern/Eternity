#include "eternity/game/obelisk_runtime_user_list.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct User final:ObeliskRuntimeUserPublication {
    int scenario=0,map=0,other=0;unsigned sends=0;
    std::vector<ObeliskRuntime> ordinary;std::vector<ObeliskRuntimePortalLink> links;
    std::vector<UserPosition> positions;std::vector<std::int32_t> countries;
    std::vector<ObeliskRuntimePortalScene> portals;std::vector<std::uint32_t> trace;
    void send_user(const eternity::net::Packet& p) override {
        trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());
        if(++sends==1&&scenario/48%2) {
            for(std::size_t i=100;i<ordinary.size();++i) {auto& o=ordinary[i];o.id^=0xa5a5a5a5U;o.country=2;o.center={static_cast<float>(i)+99.5F,12345.5,-static_cast<float>(i)-99.75F};}
            for(std::size_t i=0;i<links.size();++i) {
                links[i].id^=0x55555555U;portals[i].map=(static_cast<std::size_t>(scenario)+i)%3?&map:&other;
                countries[i]=(scenario+static_cast<int>(i))%6-1;positions[i]={static_cast<float>(i)+200.25F,12345.5,-static_cast<float>(i)-200.5F};
            }
        }
    }
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    User user;user.scenario=scenario;user.ordinary.resize(std::array<std::size_t,8>{0,1,2,99,100,101,199,200}[static_cast<std::size_t>(scenario%8)]);
    const auto count=std::array<std::size_t,6>{0,1,2,12,100,101}[static_cast<std::size_t>(scenario/8%6)];user.links.resize(count);user.positions.resize(count);user.countries.resize(count);user.portals.resize(count);
    const std::array<std::int32_t,7> values{-1,0,1,2,3,INT32_MIN,INT32_MAX};
    for(std::size_t i=0;i<user.ordinary.size();++i) {auto& o=user.ordinary[i];o.id=0xf1230000U+static_cast<std::uint32_t>(i);o.country=values[(static_cast<std::size_t>(scenario)+i)%7];o.center={static_cast<float>(i)+0.25F,12345.5,-static_cast<float>(i)-0.5F};}
    for(std::size_t i=0;i<count;++i) {
        user.links[i].id=0x81230000U+static_cast<std::uint32_t>(i);user.links[i].country=&user.countries[i];user.countries[i]=values[(static_cast<std::size_t>(scenario)+i)%7];
        user.positions[i]={static_cast<float>(i)+20.25F,12345.5,-static_cast<float>(i)-20.5F};
        user.portals[i]={scenario/96%2==0||i%2?&user.map:&user.other,&user.links[i],&user.positions[i]};
    }
    publish_obelisk_user_list(&user.map,user.ordinary,user.portals,user);
    if(user.trace!=expected) {
        std::size_t i=0;while(i<user.trace.size()&&i<expected.size()&&user.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<user.trace.size()?std::to_string(user.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original user scene list comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected user list fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing user list fixture");std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);run(scenario,expected);++count;}
        if(count!=192) throw std::runtime_error("Incomplete user list fixture");
        User user;const std::array<ObeliskRuntimePortalScene,1> irrelevant{{{&user.other,nullptr,nullptr}}};publish_obelisk_user_list(&user.map,{},irrelevant,user);
        if(!user.trace.empty()) throw std::runtime_error("Nonmatching portal or empty scene was published");
        std::cout<<count<<" original mixed user scene list cases passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
