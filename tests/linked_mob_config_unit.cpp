#include "eternity/game/linked_mob_config.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;using Bytes=std::vector<std::uint8_t>;
void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
void number(Bytes& b,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& h) {Bytes b;if(h=="-") return b;for(std::size_t i=0;i<h.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(h.substr(i,2),nullptr,16)));return b;}
std::vector<ConfiguredMap> maps(std::size_t count) {std::vector<ConfiguredMap> result(count);for(auto& map:result) for(int id:{1010,1010,1011}) {MapPortal p;p.id=id;map.definition.portals.push_back(p);}return result;}
MobRuntimeTemplates definitions() {eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};for(unsigned int i=1;i<4096;++i) rows.rows.push_back({std::to_string(i),i%7==0?"0":"1","0","1","0","0","0","0","0","0","0","0","0","0"});for(auto& row:rows.rows) row.emplace_back("0");return MobRuntimeTemplates::load(rows);}
Bytes canonical(const LinkedMobConfiguration& config,const std::vector<ConfiguredMap>& maps) {
    Bytes b;number(b,static_cast<std::uint32_t>(maps.size()));
    for(std::size_t m=0;m<maps.size();++m) {const auto groups=config.for_map(static_cast<std::uint16_t>(m));number(b,static_cast<std::uint32_t>(groups.size()));for(const auto& g:groups) {
        std::uint32_t portal=0;for(std::size_t i=0;i<maps.size();++i) for(std::size_t j=0;j<maps[i].definition.portals.size();++j) if(g.portal==&maps[i].definition.portals[j]) portal=static_cast<std::uint32_t>(i*3+j+1);
        number(b,g.id);number(b,static_cast<std::uint32_t>(g.change_count));number(b,g.respawn_time);number(b,static_cast<std::uint32_t>(g.portal_map));number(b,static_cast<std::uint32_t>(g.portal_id));number(b,portal);number(b,static_cast<std::uint32_t>(g.spawn_count));
        for(int i=-1;i<g.spawn_count;++i) {const auto& s=i<0?g.leader:g.spawns.at(static_cast<std::size_t>(i));for(auto f:{s.center.x,s.center.y,s.center.z,s.radius}) number(b,std::bit_cast<std::uint32_t>(f));number(b,static_cast<std::uint32_t>(s.radius_twice));number(b,static_cast<std::uint32_t>(s.count));for(auto id:s.type_ids) number(b,static_cast<std::uint32_t>(id));for(auto* d:s.definitions) number(b,d?d->id:0);const auto& r=s.respawn;number(b,static_cast<std::uint32_t>(r.type));number(b,static_cast<std::uint32_t>(r.count));number(b,static_cast<std::uint32_t>(r.current));number(b,r.next);number(b,r.definition?r.definition->id:0);number(b,r.obelisk?1:0);number(b,r.obelisk_source?1:0);number(b,r.both?1:0);number(b,r.both_source==&s?1:0);number(b,static_cast<std::uint32_t>(s.randomized));number(b,s.minimum_minutes);number(b,s.random_minutes);for(auto pos:s.random_centers) for(auto f:{pos.x,pos.y,pos.z}) number(b,std::bit_cast<std::uint32_t>(f));}
        number(b,static_cast<std::uint32_t>(g.current_index));
    }}return b;
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected linked configuration oracle and optional Obelisk.ini");std::ifstream input(argv[1]);require(input.good(),"Cannot open linked config oracle");const auto defs=definitions();std::string line;unsigned int scenarios=0,assets=0;
        while(std::getline(input,line)) {if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;std::size_t count=0;row>>kind>>count;auto world=maps(count);
            if(kind=='S') {std::string text,hex;row>>text>>hex;const auto bytes=unhex(text);const std::string data(bytes.begin(),bytes.end());const auto config=LinkedMobConfiguration::decode(data,world,defs);const auto actual=canonical(config,world),expected=unhex(hex);if(actual!=expected) {const auto at=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());throw std::runtime_error("Original linked configuration differs at case "+std::to_string(scenarios)+" byte "+std::to_string(at.first-actual.begin())+" sizes "+std::to_string(actual.size())+"/"+std::to_string(expected.size()));}++scenarios;}
            else {require(kind=='A',"Unknown linked configuration vector");std::string sha;std::size_t length=0;std::uint64_t expected=0;row>>sha>>length>>expected;if(argc==3) {const auto config=LinkedMobConfiguration::load(argv[2],world,defs);const auto data=canonical(config,world);std::uint64_t hash=0xcbf29ce484222325;for(auto byte:data) hash=(hash^byte)*0x100000001b3ULL;require(data.size()==length && hash==expected,"Original linked configuration distribution differs");++assets;}}
            require(static_cast<bool>(row),"Malformed linked config vector");
        }
        require(scenarios==44,"Linked config fixture coverage changed");std::cout<<scenarios<<" original linked configurations and "<<assets<<" full distributions passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
