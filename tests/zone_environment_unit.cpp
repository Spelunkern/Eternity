#include "eternity/game/zone_environment.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
using Bytes=std::vector<std::uint8_t>;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
void put(Bytes& bytes,std::uint32_t value,unsigned int width=4) {for(unsigned int i=0;i<width;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void put(Bytes& bytes,std::int32_t value) {put(bytes,std::bit_cast<std::uint32_t>(value));}
void put(Bytes& bytes,UserPosition position) {put(bytes,std::bit_cast<std::uint32_t>(position.x));put(bytes,std::bit_cast<std::uint32_t>(position.y));put(bytes,std::bit_cast<std::uint32_t>(position.z));}
Bytes canonical(const ZoneEnvironment& zone) {
    Bytes result;const auto& w=zone.weather();put(result,w.definition.state);put(result,w.definition.power);put(result,w.definition.rate);put(result,w.definition.delay_ms);put(result,w.definition.none_delay_ms);put(result,w.active,1);put(result,w.next_update);
    put(result,static_cast<std::uint32_t>(zone.doors().size()));for(const auto& door:zone.doors()) {
        put(result,door.position);put(result,door.id);require(door.cell.has_value(),"Prepared door has no cell");put(result,door.cell->x);put(result,door.cell->z);
        put(result,static_cast<std::uint32_t>(door.open),1);put(result,door.next_close);put(result,door.next_use);put(result,door.auto_close);put(result,door.close_delay_ms);
    }
    put(result,static_cast<std::uint32_t>(zone.regions().size()));for(const auto& region:zone.regions()) {
        put(result,region.position);put(result,region.id);put(result,region.cell.x);put(result,region.cell.z);put(result,region.box.minimum);put(result,region.box.maximum);put(result,region.type);for(auto value:region.values) put(result,value);
    }
    return result;
}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
std::uint64_t fingerprint(const Bytes& bytes) {std::uint64_t result=0xcbf29ce484222325ULL;for(auto byte:bytes) result=(result^byte)*0x100000001b3ULL;return result;}
std::string hex(const Bytes& bytes) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
struct Sink final:ZoneEnvironmentSink {
    std::vector<std::string> events;std::vector<CellCoordinates> cells;
    void send_cell_view(CellCoordinates cell,const net::Packet& packet) override {cells.push_back(cell);events.push_back("C"+hex(packet));}
    void send_all(const net::Packet& packet) override {events.push_back("A"+hex(packet));}
};
struct Random final:RandomSource {std::int32_t value=0;unsigned int calls=0;std::int32_t next() override {++calls;return value;}};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected environment oracle and optional original map directory");std::ifstream input(argv[1]);require(input.good(),"Cannot read environment oracle");
        unsigned int distributions=0,actual=0,synthetic=0,doors=0,weather=0,randoms=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='A') {
                std::string file,sha;std::size_t size=0;std::uint64_t hash=0;row>>file>>sha>>size>>hash;++distributions;
                if(argc==3) {
                    ConfiguredMap map;map.definition=MapDefinition::load(std::filesystem::path(argv[2])/file);const auto records=canonical(ZoneEnvironment::prepare(map));
                    require(records.size()==size && fingerprint(records)==hash,"Original distribution environment differs: "+file);++actual;
                }
            } else if(kind=='S') {
                std::string map_hex,record_hex;row>>map_hex>>record_hex;ConfiguredMap map;map.definition=MapDefinition::decode(unhex(map_hex));
                require(canonical(ZoneEnvironment::prepare(map))==unhex(record_hex),"Original synthetic door/region preparation differs at case "+std::to_string(synthetic));++synthetic;
            } else if(kind=='D') {
                unsigned int op=0,s=0,open=0;std::uint32_t next_close=0,next_use=0;std::size_t events=0;row>>op>>s>>open>>next_close>>next_use>>events;
                ZoneEnvironment zone;ZoneDoor door;door.id=(s*7)&511;if(s&1) door.cell=CellCoordinates{17,23};door.open=(s&2)!=0;
                door.next_close=std::array<std::uint32_t,4>{0,100,2000,UINT32_MAX}[(s/3)%4];door.next_use=std::array<std::uint32_t,4>{0,100,2000,UINT32_MAX}[(s/5)%4];
                door.auto_close=std::array<std::int32_t,3>{0,1,-1}[(s/7)%3];door.close_delay_ms=std::array<std::uint32_t,4>{0,1,2000,UINT32_MAX}[(s/11)%4];zone.doors().push_back(door);
                const auto tick=std::array<std::uint32_t,6>{0,99,100,1999,2000,UINT32_MAX}[s%6];Sink sink;
                if(op==0) zone.use_door(0,tick,sink);else if(op==1) zone.auto_close_door(0,tick,sink);else zone.update_doors(tick,sink);
                const auto& final=zone.doors()[0];const auto context=" at door vector "+std::to_string(doors);
                require(final.open==(open!=0) && final.next_close==next_close && final.next_use==next_use,"Original door state differs"+context);
                require(sink.events.size()==events,"Original door event count differs"+context);for(const auto& event:sink.events) {std::string expected;row>>expected;require(event==expected,"Original door event differs"+context);}
                for(const auto& cell:sink.cells) require(cell.x==17 && cell.z==23,"Door event used the wrong cell");++doors;
            } else if(kind=='W') {
                unsigned int s=0,active=0,calls=0;std::uint32_t following=0;std::size_t events=0;row>>s>>active>>following>>calls>>events;ZoneEnvironment zone;auto& w=zone.weather();
                w.definition.state=std::array<std::int32_t,5>{0,1,2,-1,255}[s%5];w.definition.power=std::array<std::int32_t,4>{0,2,257,-1}[(s/3)%4];
                w.definition.rate=std::array<std::int32_t,7>{-1,0,1,30,99,100,101}[(s/5)%7];w.definition.delay_ms=std::array<std::uint32_t,4>{0,1,1000,UINT32_MAX}[(s/11)%4];
                w.definition.none_delay_ms=std::array<std::uint32_t,3>{0,5000,UINT32_MAX}[(s/13)%3];w.active=static_cast<std::uint8_t>((s/7)%3);w.next_update=std::array<std::uint32_t,3>{0,100,UINT32_MAX}[(s/17)%3];
                const auto tick=std::array<std::uint32_t,6>{0,99,100,1999,2000,UINT32_MAX}[s%6];Random random;random.value=std::array<std::int32_t,7>{0,1,29,30,99,100,32767}[s%7];Sink sink;
                zone.update_weather(tick,random,sink);sink.events.push_back("S"+hex(zone.weather_packet(std::array<std::uint8_t,3>{0,1,255}[s%3])));
                const auto context=" at weather vector "+std::to_string(weather);require(w.active==active && w.next_update==following && random.calls==calls,"Original weather state/random use differs"+context);
                require(sink.events.size()==events,"Original weather event count differs"+context);for(const auto& event:sink.events) {std::string expected;row>>expected;require(event==expected,"Original weather packet differs"+context);}++weather;
            } else if(kind=='R') {
                std::uint32_t seed=0;row>>seed;MicrosoftRandom random(seed);
                for(unsigned int i=0;i<32;++i) {std::int32_t value=0;std::uint32_t state=0;row>>value>>state;require(random.next()==value && random.state()==state,"Original MSVCRT random sequence differs");}++randoms;
            } else throw std::runtime_error("Unknown environment oracle row");
            std::string extra;require(!(row>>extra),"Unexpected environment oracle data");
        }
        require(distributions==77 && synthetic==4 && doors==384 && weather==512 && randoms==5 && (argc!=3 || actual==77),"Environment oracle coverage changed");
        ConfiguredMap map;map.definition.size=40;map.definition.doors={{{0,0,0},1,1},{{40,0,0},1,0}};map.settings.weather={2,3,100,10,100};
        auto zone=ZoneEnvironment::prepare(map);Sink sink;require(zone.use_door(0,100,sink) && zone.use_door(1,100,sink),"Prepared door did not open");zone.update_doors(100,sink);
        require(!zone.use_door(1,2099,sink),"Door cooldown ended early");zone.update_doors(1100,sink);MicrosoftRandom random(123);zone.update_weather(0,random,sink);zone.update_weather(10,random,sink);
        require(sink.events==std::vector<std::string>{"C03040001","C03040101","C03040100","C03040000","A5104010203"},"Combined door/weather operation order changed");
        require(sink.cells.size()==4 && sink.cells[0].x==0 && sink.cells[1].x==1 && sink.cells[2].x==1 && sink.cells[3].x==0,"Prepared door used the wrong view cell");
        ZoneEnvironment snapshot_zone;ZoneDoor a,b;a.id=0x12;b.id=0x13;a.cell=b.cell=CellCoordinates{1,1};b.open=true;
        snapshot_zone.doors().push_back(a);snapshot_zone.doors().push_back(b);std::vector<unsigned> seen;bool nested=true;
        std::function<void(const eternity::net::Packet&)> observe=[&](const auto& packet){seen.push_back(packet.at(2));if(nested){nested=false;snapshot_zone.send_cell_doors({1,1},observe);}};
        snapshot_zone.send_cell_doors({1,1},observe);require(seen==std::vector<unsigned>{0x12,0x12,0x13,0x12,0x13},"Original nested door cursor differs");
        std::cout<<synthetic<<" original preparations, "<<actual<<" map environments, "<<doors<<" door and "<<weather<<" weather cases plus "<<randoms<<" random sequences passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
