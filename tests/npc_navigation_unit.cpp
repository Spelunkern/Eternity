#include "eternity/game/npc_navigation.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;using Bytes=eternity::net::Packet;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
UserPosition position(std::istringstream& row) {std::uint32_t x=0,y=0,z=0;row>>x>>y>>z;return {std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)};}
struct Services final:ZoneNpcServices {
    struct Buffer final:NpcBroadcast {std::size_t& deliveries;explicit Buffer(std::size_t& count):deliveries(count) {}void send_cell(CellCoordinates) override {++deliveries;}};
    std::size_t deliveries=0;
    Npc* allocate() override {throw std::logic_error("Unexpected navigation NPC allocation");}
    std::uint32_t next_id() override {throw std::logic_error("Unexpected navigation NPC serial");}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("Unexpected navigation guard preparation");}
    void send_cell_view(CellCoordinates,const Bytes&) override {++deliveries;}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const Bytes&,std::optional<std::uint8_t>) override {return std::make_unique<Buffer>(deliveries);}
};
struct World final:NpcNavigationWorld {
    std::uint32_t clock=0;mutable std::uint32_t clocks=0;bool success=true;Bytes moves;ZoneNpcs* zone=nullptr;
    std::uint32_t tick_ms() const noexcept override {++clocks;return clock;}
    bool move(Npc& npc,UserPosition destination,std::uint8_t mode) override {
        for(auto coordinate:{destination.x,destination.y,destination.z}) number(moves,std::bit_cast<std::uint32_t>(coordinate));number(moves,mode);
        if(success) {if(zone) require(zone->move(npc,destination,mode),"Native zone rejected original guard navigation move");else npc.position=destination;}
        return success;
    }
};
struct State {
    NpcDefinition definition;MapNpc source;Npc npc;World world;Services services;std::unique_ptr<ZoneNpcs> zone;
    void initialize(const MapDefinition& map,UserPosition current,std::int32_t status,std::int32_t kind,std::uint32_t uid,std::uint32_t deadline,std::uint32_t disable,std::int32_t speed,std::int32_t distance,std::int32_t index,unsigned int path_mode,MapTile path_point,const std::vector<MapTile>& trail,bool live=false) {
        zone.reset();npc={};world={};definition={};source={};
        auto put=[&](std::size_t offset,std::uint32_t value){for(unsigned int i=0;i<4;++i) definition.record[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};put(8,static_cast<std::uint32_t>(distance));put(12,static_cast<std::uint32_t>(speed));
        source.positions={{{5,1,5},0},{{10,2,10},0},{{15,3,5},0}};npc.definition=&definition;npc.source=&source;npc.position=current;npc.id=1;npc.next_position=index;
        npc.status=status;npc.target_kind=kind;npc.target_id=uid;npc.next_chase=deadline;npc.disable_time=disable;
        if(path_mode==1) npc.path.set_path(path_point);else if(path_mode==2) npc.path.find(map,{5,5},path_point);
        for(auto point:trail) npc.trail.add(map,point);
        if(live) {zone=std::make_unique<ZoneNpcs>(map.geometry(),services);require(zone->enter(npc),"Cannot attach navigation NPC");world.zone=zone.get();}
    }
    Bytes update(unsigned int operation,UserPosition target,std::int32_t range,std::uint32_t tick,std::uint32_t clock,bool success,const MapDefinition& map) {
        world.moves.clear();world.clocks=0;world.clock=clock;world.success=success;
        const auto result=operation==0?chase_npc(npc,target,range,tick,map,world):return_npc_home(npc,target,tick,map,world);Bytes bytes;number(bytes,result?1:0,1);
        for(auto coordinate:{npc.position.x,npc.position.y,npc.position.z}) number(bytes,std::bit_cast<std::uint32_t>(coordinate));
        for(auto value:{static_cast<std::uint32_t>(npc.status),static_cast<std::uint32_t>(npc.target_kind),npc.target_id,npc.next_chase,npc.disable_time}) number(bytes,value);
        number(bytes,npc.path.found()?1:0,1);const auto bounds=npc.path.bounds();for(auto value:{bounds.minimum_x,bounds.minimum_z,bounds.maximum_x,bounds.maximum_z}) number(bytes,static_cast<std::uint32_t>(value));
        const auto cursor=npc.path.current();number(bytes,cursor?1:0,1);if(cursor) {number(bytes,static_cast<std::uint32_t>(cursor->x));number(bytes,static_cast<std::uint32_t>(cursor->z));}
        const auto path=npc.path.nodes();number(bytes,static_cast<std::uint32_t>(path.size()));for(auto point:path) {number(bytes,static_cast<std::uint32_t>(point.x));number(bytes,static_cast<std::uint32_t>(point.z));}
        const auto trail=npc.trail.nodes();number(bytes,static_cast<std::uint32_t>(trail.size()));for(auto it=trail.rbegin();it!=trail.rend();++it) {number(bytes,static_cast<std::uint32_t>(it->x));number(bytes,static_cast<std::uint32_t>(it->z));}
        number(bytes,world.clocks);number(bytes,static_cast<std::uint32_t>(world.moves.size()/16));bytes.insert(bytes.end(),world.moves.begin(),world.moves.end());return bytes;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected guard navigation oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guard navigation oracle");std::string line;std::map<unsigned int,MapDefinition> maps;State state;unsigned int cases=0,paths=0,steps=0,active=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;unsigned int operation=0,success=0;UserPosition target;std::int32_t range=0;std::uint32_t tick=0,clock=0;std::string expected;
            if(kind=='N') {unsigned int id=0;MapDefinition map;std::string hex;row>>id>>map.size>>hex;map.tiles=unhex(hex);maps.emplace(id,std::move(map));}
            else if(kind=='M') {
                row>>operation>>active;const auto current=position(row);target=position(row);
                std::int32_t speed=0,distance=0,status=0,target_kind=0,index=0;std::uint32_t uid=0,deadline=0,disable=0;unsigned int path_mode=0,trail_count=0;MapTile point;
                row>>range>>tick>>clock>>speed>>distance>>status>>target_kind>>uid>>deadline>>disable>>success>>index>>path_mode>>point.x>>point.z>>trail_count;std::vector<MapTile> trail(trail_count);for(auto& tile:trail) row>>tile.x>>tile.z;row>>expected;
                state.initialize(maps.at(active),current,status,target_kind,uid,deadline,disable,speed,distance,index,path_mode,point,trail);
            } else if(kind=='B') {row>>active;state.initialize(maps.at(active),{5.25F,3,5.75F},1,1,70001,0,0,500,2,0,0,{0,0},{},true);++paths;}
            else if(kind=='T') {row>>operation;target=position(row);row>>range>>tick>>clock>>success>>expected;}
            else require(false,"Unknown guard navigation vector type");
            if(kind=='M' || kind=='T') {
                const auto bytes=state.update(operation,target,range,tick,clock,success!=0,maps.at(active)),original=unhex(expected);
                if(bytes!=original) {const auto at=std::mismatch(bytes.begin(),bytes.end(),original.begin(),original.end());throw std::runtime_error("Original guard navigation differs at case "+std::to_string(cases)+" step "+std::to_string(steps)+" byte "+std::to_string(at.first-bytes.begin()));}
                if(kind=='M') ++cases;else ++steps;
            }
            require(static_cast<bool>(row),"Malformed guard navigation vector");std::string extra;require(!(row>>extra),"Trailing guard navigation data");
        }
        require(maps.size()==4 && cases==1544 && paths==3 && steps==108 && state.services.deliveries>0,"Guard navigation coverage changed");
        std::cout<<cases<<" original guard chase/return states and "<<paths<<" native zone routes with "<<steps<<" updates passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
