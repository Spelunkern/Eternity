#include "eternity/game/pathfinding.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;using Bytes=std::vector<std::uint8_t>;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
Bytes snapshot(const AStarPath& path,bool result) {
    Bytes bytes;number(bytes,result?1:0,1);number(bytes,path.found()?1:0,1);number(bytes,path.last()?1:0,1);
    const auto bounds=path.bounds();for(auto value:{bounds.minimum_x,bounds.minimum_z,bounds.maximum_x,bounds.maximum_z}) number(bytes,static_cast<std::uint32_t>(value));
    const auto current=path.current();number(bytes,current?1:0,1);if(current) {number(bytes,static_cast<std::uint32_t>(current->x));number(bytes,static_cast<std::uint32_t>(current->z));}
    const auto nodes=path.nodes();number(bytes,static_cast<std::uint32_t>(nodes.size()));for(auto node:nodes) {number(bytes,static_cast<std::uint32_t>(node.x));number(bytes,static_cast<std::uint32_t>(node.z));}return bytes;
}
void compare(const Bytes& bytes,const std::string& hex,const std::string& context) {
    const auto expected=unhex(hex);if(bytes!=expected) throw std::runtime_error("Original A* state/path differs at "+context);
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected A* oracle and optional original map directory");std::ifstream input(argv[1]);require(input.good(),"Cannot open A* oracle");
        std::map<unsigned int,MapDefinition> maps;std::unique_ptr<AStarPath> path;unsigned int active=0,cases=0,sequences=0,operations=0,distribution=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='N') {unsigned int id=0;MapDefinition map;std::string hex;row>>id>>map.size>>hex;map.tiles=unhex(hex);maps.emplace(id,std::move(map));}
            else if(kind=='F' || kind=='Q') {
                unsigned int id=active;if(kind=='F') row>>id;MapTile start,goal;std::string hex;row>>start.x>>start.z>>goal.x>>goal.z>>hex;
                if(kind=='F') {AStarPath search;const auto result=search.find(maps.at(id),start,goal);compare(snapshot(search,result),hex,"case "+std::to_string(cases++));}
                else {require(path!=nullptr,"A* sequence operation without state");const auto result=path->find(maps.at(id),start,goal);compare(snapshot(*path,result),hex,"sequence operation "+std::to_string(operations++));}
            } else if(kind=='B') {row>>active;path=std::make_unique<AStarPath>();++sequences;}
            else if(kind=='C' || kind=='R' || kind=='P' || kind=='S') {
                require(path!=nullptr,"A* cursor operation without state");
                if(kind=='C') path->advance();else if(kind=='R') path->reset();else if(kind=='P') path->reset_path();else {MapTile point;row>>point.x>>point.z;path->set_path(point);}
                std::string hex;row>>hex;compare(snapshot(*path,false),hex,"cursor operation "+std::to_string(operations++));
            } else if(kind=='A') {
                std::string filename,sha;std::size_t size=0;std::uint64_t expected_hash=0;row>>filename>>sha>>size>>expected_hash;
                if(argc==3) {
                    const auto map=MapDefinition::load(std::filesystem::path(argv[2])/filename);std::uint64_t hash=0xcbf29ce484222325ULL;std::size_t bytes=0;
                    for(std::int32_t scenario=0;scenario<8;++scenario) {
                        AStarPath search;const MapTile start{(map.size/2+scenario*7)%map.size,(map.size/2+scenario*13)%map.size};const MapTile goal{std::min(start.x+(scenario%3)*3,map.size-1),std::min(start.z+(scenario%4)*2,map.size-1)};
                        const auto result=search.find(map,start,goal);for(auto byte:snapshot(search,result)) {hash=(hash^byte)*0x100000001b3ULL;++bytes;}
                    }
                    if(bytes!=size || hash!=expected_hash) throw std::runtime_error("Original distribution A* differs for "+filename);++distribution;
                }
            } else require(false,"Unknown A* oracle vector type");
            require(static_cast<bool>(row),"Malformed A* vector");std::string extra;require(!(row>>extra),"Trailing A* vector data");
        }
        require(maps.size()==16 && cases==1536 && sequences==16 && operations==416 && distribution==(argc==3?77U:0U),"A* oracle coverage changed");
        std::cout<<cases<<" original A* searches, "<<operations<<" persistent/cursor operations and "<<distribution<<" maps with 8 searches each passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
