#include "eternity/game/maps.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;using Bytes=std::vector<std::uint8_t>;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
Bytes query(const MapDefinition& map,MapTile start,MapTile end) {
    MapTile previous{-777,888};const auto result=map.enable_line(start,end,&previous);Bytes bytes;number(bytes,result?1:0,1);number(bytes,static_cast<std::uint32_t>(previous.x));number(bytes,static_cast<std::uint32_t>(previous.z));number(bytes,map.enable_line(start,end)?1:0,1);number(bytes,map.blocked(start)?1:0,1);number(bytes,map.blocked(end)?1:0,1);return bytes;
}
std::pair<MapTile,MapTile> endpoints(std::int32_t size,std::int32_t scenario) {
    const std::array<std::int32_t,7> values{-1,0,1,size-1,size,INT32_MIN,INT32_MAX};
    if(scenario<32) return {{values[static_cast<std::size_t>(scenario%7)],values[static_cast<std::size_t>((scenario/3)%7)]},{values[static_cast<std::size_t>((scenario/5)%7)],values[static_cast<std::size_t>((scenario+1)%7)]}};
    if(size==0) return {{0,0},{0,0}};
    MapTile start{(scenario*31+7)%size,(scenario*17+3)%size},end{(scenario*13+1)%size,(scenario*41+5)%size};
    if(scenario%7==0) end=start;else if(scenario%7==1) end.x=start.x;else if(scenario%7==2) end.z=start.z;return {start,end};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected map line oracle and optional original map directory");std::ifstream input(argv[1]);require(input.good(),"Cannot open map line oracle");
        std::map<unsigned int,MapDefinition> maps;std::string line;unsigned int cases=0,distribution=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='N') {
                unsigned int id=0;std::int32_t size=0;std::string hex;row>>id>>size>>hex;MapDefinition map;map.size=size;if(hex!="-") map.tiles=unhex(hex);maps.emplace(id,std::move(map));
            } else if(kind=='L') {
                unsigned int id=0;MapTile start,end;std::string hex;row>>id>>start.x>>start.z>>end.x>>end.z>>hex;
                if(query(maps.at(id),start,end)!=unhex(hex)) throw std::runtime_error("Original map line differs at case "+std::to_string(cases));++cases;
            } else if(kind=='A') {
                std::string filename,sha;std::size_t size=0;std::uint64_t expected_hash=0;row>>filename>>sha>>size>>expected_hash;
                if(argc==3) {
                    const auto map=MapDefinition::load(std::filesystem::path(argv[2])/filename);std::uint64_t hash=0xcbf29ce484222325ULL;std::size_t bytes=0;
                    for(std::int32_t scenario=0;scenario<128;++scenario) {const auto [start,end]=endpoints(map.size,scenario);for(auto byte:query(map,start,end)) {hash=(hash^byte)*0x100000001b3ULL;++bytes;}}
                    if(bytes!=size || hash!=expected_hash) throw std::runtime_error("Original distribution map line differs for "+filename);++distribution;
                }
            } else require(false,"Unknown map line oracle type");
            require(static_cast<bool>(row),"Malformed map line vector");std::string extra;require(!(row>>extra),"Trailing map line data");
        }
        require(maps.size()==16 && cases==4096 && distribution==(argc==3?77U:0U),"Map line oracle coverage changed");
        std::cout<<cases<<" original map collision/line cases and "<<distribution<<" maps with 128 queries each passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
