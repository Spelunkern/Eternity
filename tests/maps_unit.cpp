#include "eternity/game/maps.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
using Bytes=std::vector<std::uint8_t>;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
void put(Bytes& bytes,std::uint32_t value) {for(unsigned int i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void put(Bytes& bytes,std::int32_t value) {put(bytes,std::bit_cast<std::uint32_t>(value));}
void put(Bytes& bytes,float value) {put(bytes,std::bit_cast<std::uint32_t>(value));}
void put(Bytes& bytes,UserPosition position) {put(bytes,position.x);put(bytes,position.y);put(bytes,position.z);}
void put(Bytes& bytes,const MapBox& box) {put(bytes,box.minimum);put(bytes,box.maximum);}
void count(Bytes& bytes,std::size_t value) {put(bytes,static_cast<std::uint32_t>(value));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
std::uint64_t fingerprint(const Bytes& bytes) {std::uint64_t hash=0xcbf29ce484222325ULL;for(auto byte:bytes) hash=(hash^byte)*0x100000001b3ULL;return hash;}
Bytes canonical(const MapDefinition& map) {
    // Original memory layout with pointer/runtime-only fields excluded. Each list starts
    // with its DWORD count; original numeric fields retain their runtime DWORD/float bits.
    Bytes bytes;put(bytes,map.size);count(bytes,map.tiles.size());bytes.insert(bytes.end(),map.tiles.begin(),map.tiles.end());
    count(bytes,map.doors.size());for(const auto& door:map.doors) {put(bytes,door.position);put(bytes,door.auto_close);put(bytes,door.close_time);}
    count(bytes,map.ladders.size());for(const auto& position:map.ladders) put(bytes,position);
    count(bytes,map.mobs.size());for(const auto& area:map.mobs) {
        put(bytes,area.box);put(bytes,area.delta_x);put(bytes,area.delta_z);count(bytes,area.spawns.size());
        for(const auto& spawn:area.spawns) {put(bytes,spawn.mob_id);put(bytes,spawn.count);}
    }
    count(bytes,map.npcs.size());for(const auto& npc:map.npcs) {
        put(bytes,npc.type);put(bytes,npc.type_id);count(bytes,npc.positions.size());
        for(const auto& position:npc.positions) {put(bytes,position.position);put(bytes,position.angle);}
    }
    count(bytes,map.portals.size());for(const auto& portal:map.portals) {
        put(bytes,portal.id);put(bytes,portal.position);put(bytes,portal.base_country);put(bytes,portal.country);
        put(bytes,portal.minimum_level);put(bytes,portal.maximum_level);put(bytes,portal.map_id);put(bytes,portal.destination);
    }
    count(bytes,map.binds.size());for(const auto& bind:map.binds) {
        count(bytes,bind.boxes.size());put(bytes,bind.country);put(bytes,bind.attribute);for(const auto& box:bind.boxes) put(bytes,box);
    }
    count(bytes,map.others.size());for(const auto& other:map.others) {
        put(bytes,other.box);put(bytes,other.type);put(bytes,static_cast<std::uint32_t>(other.attribute1));put(bytes,static_cast<std::uint32_t>(other.attribute2));put(bytes,static_cast<std::uint32_t>(other.attribute3));
    }
    return bytes;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3 || argc==4,"Expected map/geometry oracle paths and optional original map directory");
        std::ifstream input(argv[1]);require(input.good(),"Cannot open map oracle");std::string line;unsigned int distributions=0,synthetic=0,actual=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='A') {
                std::string file,sha;std::size_t consumed=0,size=0;std::uint64_t hash=0;row>>file>>sha>>consumed>>size>>hash;++distributions;
                if(argc==4) {
                    const auto map=MapDefinition::load(std::filesystem::path(argv[3])/file);const auto records=canonical(map);
                    require(map.consumed==consumed && records.size()==size && fingerprint(records)==hash,"Original complete map distribution differs: "+file);++actual;
                }
            } else if(kind=='S') {
                std::string hex,record_hex;std::size_t consumed=0;row>>hex>>consumed>>record_hex;
                auto bytes=unhex(hex);const auto map=MapDefinition::decode(bytes);
                require(map.consumed==consumed && canonical(map)==unhex(record_hex),"Original synthetic map fields differ at case "+std::to_string(synthetic));
                bytes.pop_back();bool rejected=false;try {static_cast<void>(MapDefinition::decode(bytes));}catch(const std::runtime_error&) {rejected=true;}
                require(rejected,"Truncated original map was accepted");++synthetic;
            } else throw std::runtime_error("Unknown map oracle record");
            std::string extra;require(!(row>>extra),"Unexpected map oracle data");
        }
        require(distributions==77 && synthetic==4 && (argc!=4 || actual==77),"Map oracle coverage changed");
        input.close();input.open(argv[2]);require(input.good(),"Cannot open map geometry oracle");unsigned int grids=0,positions=0,views=0,movements=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;std::int32_t size=0;row>>kind>>size;const MapGeometry geometry(size);
            if(kind=='G') {
                std::int32_t width=0,height=0;std::uint64_t expected=0;row>>width>>height>>expected;Bytes records;
                require(geometry.cell_count()==width && geometry.cell_count()==height,"Original zone cell dimensions differ");
                for(std::int32_t z=0;z<height;++z) for(std::int32_t x=0;x<width;++x) {
                    const auto bounds=geometry.view({x,z});put(records,bounds.minimum_x);put(records,bounds.maximum_x);put(records,bounds.minimum_z);put(records,bounds.maximum_z);
                }
                require(fingerprint(records)==expected,"Original zone view windows differ");++grids;
            } else if(kind=='P') {
                std::uint32_t xb=0,zb=0;bool safe=false;std::int32_t cx=0,cz=0;row>>xb>>zb>>safe>>cx>>cz;
                const auto x=std::bit_cast<float>(xb),z=std::bit_cast<float>(zb);const auto cell=geometry.cell(x,z);
                require(geometry.safe_position(x,z)==safe && (cell?cell->x:-1)==cx && (cell?cell->z:-1)==cz,"Original map/entry position differs at case "+std::to_string(positions));++positions;
            } else if(kind=='C') {
                std::uint32_t xb=0,zb=0;CellCoordinates cell;bool contains=false;row>>cell.x>>cell.z>>xb>>zb>>contains;
                require(geometry.view(cell).contains(std::bit_cast<float>(xb),std::bit_cast<float>(zb))==contains,"Original cell-view containment differs");++views;
            } else if(kind=='M') {
                CellCoordinates previous,expected;std::uint32_t xb=0,zb=0;bool success=false;
                row>>previous.x>>previous.z>>xb>>zb>>success>>expected.x>>expected.z;
                const auto next=geometry.cell_after_move(previous,std::bit_cast<float>(xb),std::bit_cast<float>(zb));
                require(next.has_value()==success && (next?next->x:previous.x)==expected.x && (next?next->z:previous.z)==expected.z,"Original movement cell retention differs at case "+std::to_string(movements));++movements;
            } else throw std::runtime_error("Unknown geometry oracle record");
            std::string extra;require(!(row>>extra),"Unexpected map geometry data");
        }
        require(grids==9 && positions==1521 && views==375 && movements==2535,"Map geometry oracle coverage changed");
        std::cout<<synthetic<<" original map loads, "<<actual<<" distribution maps and "<<grids+positions+views+movements<<" original geometry checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
