#include "eternity/game/zones.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::string hex(const RuntimeZone& zone) {static constexpr char digits[]="0123456789abcdef";std::string out;for(auto p:zone.instance_portals) for(auto v:{p.id,p.country}) for(unsigned int i=0;i<4;++i) {const auto byte=(static_cast<std::uint32_t>(v)>>(8*i))&255U;out+=digits[byte>>4];out+=digits[byte&15];}return out;}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected portal oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open portal oracle");ConfiguredMap map;RuntimeZone zone;zone.map=&map;std::string line;unsigned int scenarios=0,updates=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op=0;row>>op;
            if(op=='S') {row>>map.settings.create_type;for(auto& p:zone.instance_portals) {std::uint32_t id=0,country=0;row>>id>>country;p={std::bit_cast<std::int32_t>(id),std::bit_cast<std::int32_t>(country)};}++scenarios;}
            else {require(op=='U',"Unknown portal operation");std::int32_t id=0;std::uint32_t country=0,result=0;std::string expected;row>>id>>country>>result>>expected;zone.update_instance_portal_country(id,std::bit_cast<std::int32_t>(country));require(static_cast<std::uint32_t>(zone.instance_portal_country(id))==result,"Original portal country differs");require(hex(zone)==expected,"Original portal table differs");++updates;}
            require(static_cast<bool>(row),"Malformed portal vector");
        }
        require(scenarios==128 && updates==8192,"Portal oracle coverage changed");std::cout<<updates<<" original instance portal updates/queries passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
