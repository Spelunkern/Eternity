#include "eternity/game/npc_patrol.hpp"
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
struct Random final:RandomSource {std::int32_t value=0;std::uint32_t calls=0;std::int32_t next() override {++calls;return value;}};
struct Services final:ZoneNpcServices {
    struct Buffer final:NpcBroadcast {std::size_t& deliveries;explicit Buffer(std::size_t& count):deliveries(count) {}void send_cell(CellCoordinates) override {++deliveries;}};
    std::size_t deliveries=0;
    Npc* allocate() override {throw std::logic_error("Unexpected NPC pool allocation during patrol");}
    std::uint32_t next_id() override {throw std::logic_error("Unexpected NPC serial allocation during patrol");}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("Unexpected NPC guard in patrol");}
    void send_cell_view(CellCoordinates,const Bytes&) override {++deliveries;}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const Bytes&,std::optional<std::uint8_t>) override {return std::make_unique<Buffer>(deliveries);}
};
struct Movement final:NpcMovementWorld {
    bool success=false;Bytes moves;ZoneNpcs* zone=nullptr;
    bool move(Npc& npc,UserPosition destination,std::uint8_t mode) override {
        for(auto value:{destination.x,destination.y,destination.z}) number(moves,std::bit_cast<std::uint32_t>(value));number(moves,mode);
        if(success) {if(zone) require(zone->move(npc,destination,mode),"Native zone rejected original valid patrol move");else npc.position=destination;}
        return success;
    }
};
struct State {
    NpcDefinition definition;MapNpc source;Npc npc;Random random;Services services;std::unique_ptr<ZoneNpcs> zone;Movement movement;
    void initialize(std::int32_t users,std::uint32_t deadline,bool present,std::int32_t index,bool forward,std::int32_t distance,std::int32_t speed,UserPosition current,std::vector<UserPosition> path,bool live=false) {
        zone.reset();definition={};source={};npc={};movement={};
        auto put=[&](std::size_t offset,std::uint32_t value){for(unsigned int i=0;i<4;++i) definition.record[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};put(8,static_cast<std::uint32_t>(distance));put(12,static_cast<std::uint32_t>(speed));
        for(const auto& point:path) source.positions.push_back({point,0});npc.position=current;npc.definition=&definition;npc.source=present?&source:nullptr;npc.id=1;
        npc.users_using=users;npc.next_move=deadline;npc.next_position=index;npc.forward=forward;
        if(live) {zone=std::make_unique<ZoneNpcs>(MapGeometry(400),services);require(zone->enter(npc),"Cannot attach live patrol NPC");movement.zone=zone.get();}
    }
    Bytes update(std::uint32_t tick,std::int32_t value,bool success) {
        random.value=value;random.calls=0;movement.success=success;movement.moves.clear();update_npc_patrol(npc,tick,random,movement);
        Bytes result;for(auto coordinate:{npc.position.x,npc.position.y,npc.position.z}) number(result,std::bit_cast<std::uint32_t>(coordinate));
        number(result,npc.forward?1:0,1);number(result,static_cast<std::uint32_t>(npc.next_position));number(result,npc.next_move);number(result,random.calls);number(result,static_cast<std::uint32_t>(movement.moves.size()/16));result.insert(result.end(),movement.moves.begin(),movement.moves.end());return result;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected NPC patrol oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open NPC patrol oracle");State state;std::string line;unsigned int cases=0,paths=0,steps=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            std::uint32_t tick=0;std::int32_t random=0;unsigned int success=0;std::string expected;
            if(kind=='M') {
                std::int32_t users=0,index=0,distance=0,speed=0;std::uint32_t deadline=0;unsigned int present=0,count=0,forward=0;
                row>>users>>deadline>>tick>>present>>count>>index>>forward>>distance>>speed>>random>>success;const auto current=position(row);std::vector<UserPosition> path;
                for(unsigned int i=0;i<count;++i) path.push_back(position(row));row>>expected;state.initialize(users,deadline,present!=0,index,forward!=0,distance,speed,current,std::move(path));
            } else if(kind=='B') {
                std::int32_t distance=0,speed=0;unsigned int count=0;row>>distance>>speed;const auto current=position(row);row>>count;std::vector<UserPosition> path;
                for(unsigned int i=0;i<count;++i) path.push_back(position(row));state.initialize(0,0,true,0,true,distance,speed,current,std::move(path),true);++paths;
            } else if(kind=='T') row>>tick>>random>>success>>expected;
            else require(false,"Unknown NPC patrol vector type");
            if(kind!='B') {
                const auto bytes=state.update(tick,random,success!=0),original=unhex(expected);
                if(bytes!=original) {const auto at=std::mismatch(bytes.begin(),bytes.end(),original.begin(),original.end());throw std::runtime_error("Original NPC patrol differs at case "+std::to_string(cases)+" step "+std::to_string(steps)+" byte "+std::to_string(at.first-bytes.begin()));}
                if(kind=='M') ++cases;else ++steps;
            }
            require(static_cast<bool>(row),"Malformed NPC patrol vector");std::string extra;require(!(row>>extra),"Trailing NPC patrol data");
        }
        require(cases==1536 && paths==12 && steps==576 && state.services.deliveries>0,"NPC patrol coverage changed");
        std::cout<<cases<<" original NPC patrol updates and "<<paths<<" native zone paths with "<<steps<<" ticks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
