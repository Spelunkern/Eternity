#include "eternity/game/zone_npcs.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;using Bytes=eternity::net::Packet;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
Bytes read_file(const std::filesystem::path& path) {std::ifstream file(path,std::ios::binary);require(file.good(),"Cannot open NPC input");return Bytes((std::istreambuf_iterator<char>(file)),{});}
struct Services final:ZoneNpcServices {
    bool synthetic=false,leaving=false;std::uint32_t attempts=0,guards=0,serial=100;NpcPool pool;std::vector<Npc*> allocated;Bytes events;
    Npc* allocate() override {
        const auto attempt=attempts++;if(synthetic && attempt%5==4) return nullptr;
        auto* npc=pool.allocate();npc->id=synthetic && attempt%3==1?70000+attempt:0;npc->disable_time=synthetic?0x12345678U:0;
        allocated.push_back(npc);return npc;
    }
    std::uint32_t next_id() override {return ++serial;}
    void invalid_guard_position(const MapNpc&) override {++guards;}
    void send_cell_view(CellCoordinates cell,const Bytes& packet) override {
        number(events,static_cast<std::uint32_t>(cell.x));number(events,static_cast<std::uint32_t>(cell.z));number(events,static_cast<std::uint32_t>(packet.size()));events.insert(events.end(),packet.begin(),packet.end());
        if(leaving) {
            const auto id=static_cast<std::uint32_t>(packet[2])|(static_cast<std::uint32_t>(packet[3])<<8)|(static_cast<std::uint32_t>(packet[4])<<16)|(static_cast<std::uint32_t>(packet[5])<<24);
            const auto found=std::find_if(allocated.begin(),allocated.end(),[&](auto* npc){return npc->id==id && npc->zone==nullptr;});
            require(found!=allocated.end() && (*found)->cell.has_value(),"NPC leave notification ordering differs");
        }
    }
};
Bytes prepare(const MapDefinition& map,const NpcDefinitions& definitions,bool synthetic) {
    Services services;services.synthetic=synthetic;services.serial=synthetic?0xfffffffcU:100;ZoneNpcs zone(map.geometry(),services);zone.initialize(map,definitions);
    Bytes result;number(result,services.attempts);number(result,services.guards);number(result,services.serial);number(result,static_cast<std::uint32_t>(services.allocated.size()));
    require(zone.all().size()==services.allocated.size(),"Initialized NPC missing from zone registry");
    for(auto* npc:services.allocated) {
        require(npc->source && npc->definition==definitions.find(npc->type,npc->type_id) && npc->zone==&zone && npc->cell,"NPC initialization binding differs");
        number(result,static_cast<std::uint32_t>(npc->source-map.npcs.data()));
        for(auto coordinate:{npc->position.x,npc->position.y,npc->position.z}) number(result,std::bit_cast<std::uint32_t>(coordinate));
        number(result,npc->id);number(result,static_cast<std::uint32_t>(npc->type));number(result,static_cast<std::uint32_t>(npc->type_id));number(result,static_cast<std::uint32_t>(npc->angle));number(result,npc->disable_time);
        number(result,static_cast<std::uint32_t>(npc->cell->x));number(result,static_cast<std::uint32_t>(npc->cell->z));
        const auto contents=zone.in_cell(*npc->cell);require(std::find(contents.begin(),contents.end(),npc)!=contents.end(),"NPC missing from cell list");
    }
    number(result,static_cast<std::uint32_t>(services.events.size()));result.insert(result.end(),services.events.begin(),services.events.end());services.events.clear();services.leaving=true;
    for(auto it=services.allocated.rbegin();it!=services.allocated.rend();++it) {
        const auto cell=*(*it)->cell;require(zone.leave(**it) && !(*it)->zone && !(*it)->cell,"NPC leave did not clear ownership");
        const auto contents=zone.in_cell(cell);require(std::find(contents.begin(),contents.end(),*it)==contents.end(),"NPC left in cell list");
    }
    require(zone.all().empty(),"NPC registry not emptied");number(result,static_cast<std::uint32_t>(services.events.size()));result.insert(result.end(),services.events.begin(),services.events.end());return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==4,"Expected NPC runtime oracle and optional map directory/NpcQuest.SData");std::ifstream input(argv[1]);require(input.good(),"Cannot open NPC runtime oracle");
        std::optional<NpcDefinitions> minimal,actual;if(argc==4) actual=NpcDefinitions::decode(read_file(argv[3]));unsigned int synthetic=0,distribution=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='N') {std::string hex;row>>hex;minimal=NpcDefinitions::decode(unhex(hex));}
            else if(kind=='A') {
                std::string filename,sha;std::size_t size=0;std::uint64_t expected_hash=0;row>>filename>>sha>>size>>expected_hash;
                if(actual) {const auto map=MapDefinition::load(std::filesystem::path(argv[2])/filename);const auto bytes=prepare(map,*actual,false);std::uint64_t hash=0xcbf29ce484222325ULL;for(auto byte:bytes) hash=(hash^byte)*0x100000001b3ULL;
                    require(bytes.size()==size && hash==expected_hash,"Original distribution NPC runtime differs");++distribution;}
            } else if(kind=='S') {
                std::string hex,expected;row>>hex>>expected;require(minimal.has_value(),"Missing positional NPC definitions");const auto map=MapDefinition::decode(unhex(hex));
                require(prepare(map,*minimal,true)==unhex(expected),"Original synthetic NPC runtime differs");++synthetic;
            } else require(false,"Unknown NPC runtime oracle type");
            require(static_cast<bool>(row),"Malformed NPC runtime vector");std::string extra;require(!(row>>extra),"Trailing NPC runtime vector data");
        }
        require(synthetic==4 && distribution==(argc==4?77U:0U),"NPC runtime oracle coverage changed");
        // Allocated IDs and fields survive pool reuse, following the original LIFO list.
        NpcPool pool;auto* a=pool.allocate();auto* b=pool.allocate();a->id=42;a->disable_time=99;pool.release(*a);pool.release(*b);
        require(pool.allocate()==b && pool.allocate()==a && a->id==42 && a->disable_time==99,"NPC pool reuse differs");
        bool rejected=false;try{Npc foreign;pool.release(foreign);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Foreign NPC accepted into pool");
        // Entry uses cell bounds and accepts the last extra cell beyond nominal map size.
        Services services;ZoneNpcs zone(MapGeometry(81),services);Npc npc;npc.id=90000;npc.type=257;npc.type_id=70000;npc.angle=-3;npc.position={119,3,0};
        require(zone.enter(npc) && zone.find(npc.id)==&npc && npc.cell->x==2,"NPC extra-cell entry differs");require(zone.leave(npc) && !zone.find(npc.id),"NPC lookup survived removal");
        std::cout<<synthetic<<" original NPC initialization/entry/leave cases and "<<distribution<<" complete map populations passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
