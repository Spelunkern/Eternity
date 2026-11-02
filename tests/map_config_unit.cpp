#include "eternity/game/map_config.hpp"
#include "eternity/game/creation.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
using Bytes=std::vector<std::uint8_t>;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
void put(Bytes& bytes,std::uint32_t value,unsigned int width=4) {for(unsigned int i=0;i<width;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void put(Bytes& bytes,std::int32_t value) {put(bytes,std::bit_cast<std::uint32_t>(value));}
void put(Bytes& bytes,const MapLocation& location) {
    put(bytes,location.map);put(bytes,std::bit_cast<std::uint32_t>(location.position.x));put(bytes,std::bit_cast<std::uint32_t>(location.position.y));put(bytes,std::bit_cast<std::uint32_t>(location.position.z));
}
Bytes canonical(const MapConfiguration& config) {
    // Original active start tuples, followed by map ID/type/war/create, counted filename,
    // both rebirth tuples, five weather DWORDs, 21 schedule WORDs and four final DWORDs.
    Bytes result;put(result,static_cast<std::uint32_t>(config.maps.size()));
    for(const auto& starts:config.starts) {put(result,static_cast<std::uint32_t>(starts.size()));for(const auto& position:starts) put(result,position);}
    for(const auto& map:config.maps) {
        put(result,map.id);put(result,static_cast<std::uint32_t>(map.dungeon));put(result,map.war_type);put(result,map.create_type);
        put(result,static_cast<std::uint32_t>(map.filename.size()));result.insert(result.end(),map.filename.begin(),map.filename.end());
        for(const auto& rebirth:map.rebirth) put(result,rebirth);
        put(result,map.weather.state);put(result,map.weather.power);put(result,map.weather.rate);put(result,map.weather.delay_ms);put(result,map.weather.none_delay_ms);
        for(const auto& time:map.open_times) {put(result,static_cast<std::uint32_t>(time.day),2);put(result,static_cast<std::uint32_t>(time.hour),2);put(result,static_cast<std::uint32_t>(time.keep_hours),2);}
        put(result,map.expire_ms);put(result,map.create_minimum_users);put(result,map.enter_maximum_users);put(result,static_cast<std::uint32_t>(map.market));
    }
    return result;
}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
std::uint64_t fingerprint(const Bytes& bytes) {std::uint64_t result=0xcbf29ce484222325ULL;for(auto byte:bytes) result=(result^byte)*0x100000001b3ULL;return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected map-config oracle and optional original data directory");std::ifstream input(argv[1]);require(input.good(),"Cannot open map-config oracle");
        std::string line;unsigned int synthetic=0,actual=0,assets=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='A') {
                std::string sha;std::size_t length=0;std::uint64_t hash=0;row>>sha>>length>>hash;
                if(argc==3) {
                    const std::filesystem::path root=argv[2];const auto config=MapConfiguration::load(root/"Map.ini");const auto records=canonical(config);
                    require(records.size()==length && fingerprint(records)==hash,"Original full map configuration fields differ");++actual;
                    const auto maps=ServerMaps::load(config,root/"Map");require(maps.size()==68 && !maps.find(65535),"Configured map registry differs");
                    for(std::size_t i=0;i<maps.size();++i) {const auto* map=maps.find(static_cast<std::uint16_t>(i));require(map && map->settings.id==static_cast<std::int32_t>(i) && map->definition.size>0 && map->settings.filename==config.maps[i].filename,"Configured map file binding differs");++assets;}
                    const auto starts=read_start_positions(root/"Map.ini");
                    for(std::size_t family=0;family<4;++family) {
                        require(starts[family].size()==config.starts[family].size(),"Character creation start count differs from original config loader");
                        for(std::size_t i=0;i<starts[family].size();++i) {const auto& a=starts[family][i];const auto& b=config.starts[family][i];require(a.map==static_cast<std::uint16_t>(b.map) && a.x==b.position.x && a.y==b.position.y && a.z==b.position.z,"Character creation lost original configured coordinates");}
                    }
                }
            } else if(kind=='S') {
                std::string ini_hex,record_hex;row>>ini_hex>>record_hex;const auto ini=unhex(ini_hex);
                const auto config=MapConfiguration::decode(std::string_view(reinterpret_cast<const char*>(ini.data()),ini.size()));const auto records=canonical(config),expected=unhex(record_hex);
                if(records!=expected) {
                    std::size_t at=0;while(at<std::min(records.size(),expected.size()) && records[at]==expected[at]) ++at;
                    throw std::runtime_error("Original map configuration differs at fixture "+std::to_string(synthetic)+", byte "+std::to_string(at));
                }
                ++synthetic;
            } else throw std::runtime_error("Unknown map configuration oracle row");
            std::string extra;require(!(row>>extra),"Unexpected map configuration oracle data");
        }
        require(synthetic==19 && (argc!=3 || (actual==1 && assets==68)),"Map configuration oracle coverage changed");
        std::cout<<synthetic<<" original map configurations, "<<actual<<" distribution configuration and "<<assets<<" configured map files passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
