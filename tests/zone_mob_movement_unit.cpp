#include "eternity/game/zone_mobs.hpp"
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
struct Services final:ZoneMobServices {
    std::uint32_t clock=0,clocks=0;
    std::uint32_t tick_ms() override {++clocks;return clock;}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua entry in movement fixture");}
    struct Buffer final:MobBroadcast {
        Services& owner;Bytes packet;std::uint8_t priority;
        Buffer(Services& services,Bytes bytes,std::uint8_t rank):owner(services),packet(std::move(bytes)),priority(rank) {}
        void send_cell(CellCoordinates cell) override {
            number(owner.events,static_cast<std::uint32_t>(cell.x));number(owner.events,static_cast<std::uint32_t>(cell.z));number(owner.events,priority);number(owner.events,static_cast<std::uint32_t>(packet.size()));
            owner.events.insert(owner.events.end(),packet.begin(),packet.end());
            for(auto coordinate:{owner.mob->position.x,owner.mob->position.y,owner.mob->position.z}) number(owner.events,std::bit_cast<std::uint32_t>(coordinate));
            number(owner.events,static_cast<std::uint32_t>(owner.mob->cell->x));number(owner.events,static_cast<std::uint32_t>(owner.mob->cell->z));
        }
    };
    MobRuntime* mob=nullptr;Bytes events;std::uint32_t allocations=0,fail=0;
    void send_cell_view(CellCoordinates,const Bytes&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Bytes& packet,std::optional<std::uint8_t> priority) override {
        const auto index=allocations++;if(fail&(1U<<index)) return {};
        return std::make_unique<Buffer>(*this,packet,priority.value_or(255));
    }
};
struct State {
    Services services;MobRuntime mob;std::unique_ptr<ZoneMobs> zone;
    void initialize(std::int32_t extent,CellCoordinates cell,UserPosition stored) {
        zone.reset();mob={};services.mob=&mob;zone=std::make_unique<ZoneMobs>(MapGeometry(extent),services);
        mob.id=0xfedcba98;mob.type=60000;mob.position={static_cast<float>(cell.x*40+1),7,static_cast<float>(cell.z*40+1)};
        require(zone->enter(mob),"Cannot attach initial movement mob");mob.position=stored;
    }
    Bytes move(UserPosition destination,std::uint8_t mode,std::uint32_t fail,bool detached,std::uint32_t tick) {
        services.events.clear();services.allocations=0;services.fail=fail;services.clock=tick;services.clocks=0;if(detached) mob.zone=nullptr;
        const auto success=zone->move(mob,destination,mode);Bytes result;number(result,success?1:0,1);
        for(auto coordinate:{mob.position.x,mob.position.y,mob.position.z}) number(result,std::bit_cast<std::uint32_t>(coordinate));
        number(result,static_cast<std::uint32_t>(mob.cell->x));number(result,static_cast<std::uint32_t>(mob.cell->z));for(auto v:{mob.previous_position,mob.move_direction}) for(auto f:{v.x,v.y,v.z}) number(result,std::bit_cast<std::uint32_t>(f));number(result,mob.previous_move_time);number(result,std::bit_cast<std::uint32_t>(mob.move_range));for(auto f:{mob.respawn_position.x,mob.respawn_position.y,mob.respawn_position.z}) number(result,std::bit_cast<std::uint32_t>(f));number(result,services.allocations);number(result,services.clocks);result.insert(result.end(),services.events.begin(),services.events.end());
        require(zone->all().size()==1 && zone->find(mob.id)==&mob,"Movement changed zone membership/lookup");
        const auto contents=zone->in_cell(*mob.cell);require(contents.size()==1 && contents[0]==&mob,"Movement did not update mob cell membership");return result;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected mob movement oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open mob movement oracle");std::string line;unsigned int cases=0,paths=0,steps=0;State state;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='M' || kind=='B') {
                std::int32_t extent=0;CellCoordinates cell;row>>extent>>cell.x>>cell.z;const auto stored=position(row);state.initialize(extent,cell,stored);
            }
            if(kind=='B') ++paths;
            else if(kind=='M' || kind=='P') {
                const auto destination=position(row);unsigned int mode=0,fail=0,detached=0;std::uint32_t tick=0;std::string expected;row>>mode>>fail;if(kind=='M') row>>detached;row>>tick>>expected;
                const auto bytes=state.move(destination,static_cast<std::uint8_t>(mode),fail,detached!=0,tick);const auto original=unhex(expected);
                if(bytes!=original) {const auto at=std::mismatch(bytes.begin(),bytes.end(),original.begin(),original.end());throw std::runtime_error("Original mob movement differs at case "+std::to_string(cases)+" step "+std::to_string(steps)+" byte "+std::to_string(at.first-bytes.begin()));}
                if(kind=='M') ++cases;else ++steps;
            } else require(false,"Unknown mob movement vector type");
            require(static_cast<bool>(row),"Malformed mob movement vector");std::string extra;require(!(row>>extra),"Trailing mob movement data");
        }
        require(cases==1536 && paths==12 && steps==144,"mob movement coverage changed");
        std::cout<<cases<<" original mob movements and "<<paths<<" paths with "<<steps<<" consecutive steps passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
