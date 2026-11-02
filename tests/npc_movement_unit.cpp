#include "eternity/game/zone_npcs.hpp"
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
    std::uint32_t tick_ms() const noexcept override {return 0;}
    struct Buffer final:NpcBroadcast {
        Services& owner;Bytes packet;std::uint8_t priority;
        Buffer(Services& services,Bytes bytes,std::uint8_t rank):owner(services),packet(std::move(bytes)),priority(rank) {}
        void send_cell(CellCoordinates cell) override {
            number(owner.events,static_cast<std::uint32_t>(cell.x));number(owner.events,static_cast<std::uint32_t>(cell.z));number(owner.events,priority);number(owner.events,static_cast<std::uint32_t>(packet.size()));
            owner.events.insert(owner.events.end(),packet.begin(),packet.end());
            for(auto coordinate:{owner.npc->position.x,owner.npc->position.y,owner.npc->position.z}) number(owner.events,std::bit_cast<std::uint32_t>(coordinate));
            number(owner.events,static_cast<std::uint32_t>(owner.npc->cell->x));number(owner.events,static_cast<std::uint32_t>(owner.npc->cell->z));
        }
    };
    Npc* npc=nullptr;Bytes events;std::uint32_t allocations=0,fail=0;
    Npc* allocate() override {throw std::logic_error("Unexpected NPC allocation in movement");}
    std::uint32_t next_id() override {throw std::logic_error("Unexpected NPC ID in movement");}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("Unexpected NPC guard in movement");}
    void send_cell_view(CellCoordinates,const Bytes&) override {}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const Bytes& packet,std::optional<std::uint8_t> priority) override {
        const auto index=allocations++;if(fail&(1U<<index)) return {};
        return std::make_unique<Buffer>(*this,packet,priority.value_or(255));
    }
};
struct State {
    Services services;Npc npc;std::unique_ptr<ZoneNpcs> zone;
    void initialize(std::int32_t extent,CellCoordinates cell,UserPosition stored) {
        zone.reset();npc={};services.npc=&npc;zone=std::make_unique<ZoneNpcs>(MapGeometry(extent),services);
        npc.id=0xfedcba98;npc.type=257;npc.type_id=70000;npc.angle=-3;npc.position={static_cast<float>(cell.x*40+1),7,static_cast<float>(cell.z*40+1)};
        require(zone->enter(npc),"Cannot attach initial movement NPC");npc.position=stored;
    }
    Bytes move(UserPosition destination,std::uint8_t mode,std::uint32_t fail,bool detached) {
        services.events.clear();services.allocations=0;services.fail=fail;if(detached) npc.zone=nullptr;
        const auto success=zone->move(npc,destination,mode);Bytes result;number(result,success?1:0,1);
        for(auto coordinate:{npc.position.x,npc.position.y,npc.position.z}) number(result,std::bit_cast<std::uint32_t>(coordinate));
        number(result,static_cast<std::uint32_t>(npc.cell->x));number(result,static_cast<std::uint32_t>(npc.cell->z));number(result,services.allocations);result.insert(result.end(),services.events.begin(),services.events.end());
        require(zone->all().size()==1 && zone->find(npc.id)==&npc,"Movement changed zone membership/lookup");
        const auto contents=zone->in_cell(*npc.cell);require(contents.size()==1 && contents[0]==&npc,"Movement did not update NPC cell membership");return result;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected NPC movement oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open NPC movement oracle");std::string line;unsigned int cases=0,paths=0,steps=0;State state;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='M' || kind=='B') {
                std::int32_t extent=0;CellCoordinates cell;row>>extent>>cell.x>>cell.z;const auto stored=position(row);state.initialize(extent,cell,stored);
            }
            if(kind=='B') ++paths;
            else if(kind=='M' || kind=='P') {
                const auto destination=position(row);unsigned int mode=0,fail=0,detached=0;std::string expected;row>>mode>>fail;if(kind=='M') row>>detached;row>>expected;
                const auto bytes=state.move(destination,static_cast<std::uint8_t>(mode),fail,detached!=0);const auto original=unhex(expected);
                if(bytes!=original) {const auto at=std::mismatch(bytes.begin(),bytes.end(),original.begin(),original.end());throw std::runtime_error("Original NPC movement differs at case "+std::to_string(cases)+" step "+std::to_string(steps)+" byte "+std::to_string(at.first-bytes.begin()));}
                if(kind=='M') ++cases;else ++steps;
            } else require(false,"Unknown NPC movement vector type");
            require(static_cast<bool>(row),"Malformed NPC movement vector");std::string extra;require(!(row>>extra),"Trailing NPC movement data");
        }
        require(cases==1536 && paths==12 && steps==144,"NPC movement coverage changed");
        std::cout<<cases<<" original NPC movements and "<<paths<<" paths with "<<steps<<" consecutive steps passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
