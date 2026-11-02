#include "eternity/game/ground_items.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& data) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:data) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
struct Services final:ZoneItemServices,ItemViewer {
    unsigned int zone;std::array<GroundItem,256>& items;std::vector<std::string>& events;std::set<int>& released;
    ZoneItems* owner=nullptr;std::uint32_t tick=0;std::function<void()> on_send,on_release;
    Services(unsigned int i,std::array<GroundItem,256>& objects,std::vector<std::string>& trace,std::set<int>& freed):zone(i),items(objects),events(trace),released(freed) {}
    int index(const GroundItem* item) const {return item?static_cast<int>(item-items.data()):-1;}
    std::uint32_t tick_ms() override {events.push_back("T");return tick;}
    void send_cell_view(CellCoordinates cell,const net::Packet& packet) override {
        const auto id=static_cast<std::uint32_t>(packet.at(2)) | (static_cast<std::uint32_t>(packet.at(3))<<8) | (static_cast<std::uint32_t>(packet.at(4))<<16) | (static_cast<std::uint32_t>(packet.at(5))<<24);
        const auto bound=packet.size()==22;require(std::any_of(items.begin(),items.end(),[&](const auto& item) {return item.id==id && static_cast<bool>(item.zone)==bound && item.cell && item.cell->x==cell.x && item.cell->z==cell.z;}),"Item broadcast membership ordering differs");
        events.push_back("V"+std::to_string(zone)+","+std::to_string(cell.x)+","+std::to_string(cell.z)+","+std::to_string(bound)+","+std::to_string(index(owner->find(id)))+":"+hex(packet));
    }
    void release(GroundItem& item) override {require(!item.zone && !item.cell,"Expired item released before detach");released.insert(index(&item));events.push_back("F"+std::to_string(index(&item)));if(on_release) on_release();}
    void send_self(const net::Packet& packet) override {events.push_back("S"+hex(packet));if(on_send) on_send();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ground item oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open ground item oracle");
        std::array<GroundItem,256> items{};std::vector<std::string> events;std::set<int> released;
        Services services0(0,items,events,released),services1(1,items,events,released);const MapGeometry geometry0(160),geometry1(320);
        ZoneItems zone0(geometry0,services0),zone1(geometry1,services1);services0.owner=&zone0;services1.owner=&zone1;
        const std::array<ZoneItems*,2> zones{&zone0,&zone1};const std::array<Services*,2> services{&services0,&services1};
        std::string line;unsigned int count=0,seen=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;unsigned int zone=0;int result=0;row>>op>>zone;events.clear();
            if(op=='E') {
                unsigned int index=0,kind=0,type_id=0,amount=0,flag=0,custom=0;std::uint32_t identity=0,tick=0,duration=0;std::array<std::uint32_t,3> position{};
                row>>index>>identity;for(auto& value:position) row>>value;row>>kind>>type_id>>amount>>flag>>tick>>custom>>duration;
                if(index==seen) ++seen;require(index<seen,"Skipped native item handle");auto& item=items.at(index);item.id=identity;
                item.position={std::bit_cast<float>(position[0]),std::bit_cast<float>(position[1]),std::bit_cast<float>(position[2])};item.item.type=static_cast<std::uint8_t>(kind);item.item.type_id=static_cast<std::uint8_t>(type_id);item.item.count=static_cast<std::uint8_t>(amount);
                services.at(zone)->tick=tick;result=zones.at(zone)->enter(item,static_cast<std::uint8_t>(flag),custom?duration:60000)?1:0;
            } else if(op=='L') {unsigned int index=0;row>>index;result=zones.at(zone)->leave(items.at(index))?1:0;}
            else if(op=='U') {std::uint32_t tick=0;row>>tick;zones.at(zone)->update(tick);}
            else if(op=='F') {std::uint32_t id=0;row>>id;result=services0.index(zones.at(zone)->find(id));}
            else {
                CellCoordinates cell;row>>cell.x>>cell.z;
                if(op=='Q') {std::uint32_t id=0;row>>id;result=services0.index(zones.at(zone)->find_cell(cell,id));}
                else if(op=='I') zones.at(zone)->send_enter(cell,*services.at(zone));
                else {require(op=='O',"Unknown ground item operation");zones.at(zone)->send_leave(cell,*services.at(zone));}
            }
            const auto context=" at ground item vector "+std::to_string(count);int original_result=0;unsigned int original_seen=0;row>>original_result>>original_seen;
            require(result==original_result && seen==original_seen,"Ground item result differs"+context);
            std::array<unsigned int,2> members{};
            for(unsigned int i=0;i<seen;++i) {
                const auto& item=items[i];const int owner=item.zone?(item.zone==&zone0?0:1):-1;if(owner>=0) ++members[static_cast<std::size_t>(owner)];
                int original_owner=0;row>>original_owner;require(owner==original_owner,"Ground item owner differs"+context);
                const auto cell=item.cell.value_or(CellCoordinates{});
                const std::array<std::uint32_t,11> values{static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.z),item.expires_at,item.id,
                    std::bit_cast<std::uint32_t>(item.position.x),std::bit_cast<std::uint32_t>(item.position.y),std::bit_cast<std::uint32_t>(item.position.z),item.item.type,item.item.type_id,item.item.count,static_cast<std::uint32_t>(released.contains(static_cast<int>(i)))};
                for(std::size_t field=0;field<values.size();++field) {std::uint32_t original=0;row>>original;require(values[field]==original,"Ground item state differs"+context+" handle "+std::to_string(i)+" field "+std::to_string(field));}
            }
            for(unsigned int i=0;i<2;++i) {
                require(zones[i]->size()==members[i],"Ground item live count differs"+context);unsigned int entries=0;row>>entries;std::map<std::uint32_t,int> expected;
                for(unsigned int j=0;j<entries;++j) {std::uint32_t id=0;int index=0;row>>id>>index;expected.emplace(id,index);require(services0.index(zones[i]->find(id))==index,"Ground item index differs"+context);}
                for(unsigned int j=0;j<seen;++j) {const auto id=items[j].id;require(services0.index(zones[i]->find(id))==(expected.contains(id)?expected.at(id):-1),"Ground item index retains an erased entry"+context);}
            }
            std::size_t event_count=0;row>>event_count;require(events.size()==event_count,"Ground item event count differs"+context);
            for(const auto& event:events) {std::string original;row>>original;require(event==original,"Ground item event differs"+context+": "+event+" versus "+original);}
            require(static_cast<bool>(row),"Truncated ground item vector");std::string extra;require(!(row>>extra),"Trailing ground item data");++count;
        }
        require(count==640,"Ground item coverage changed");
        // Live cursors: deleting the current and next item, then inserting another,
        // changes the rest of the same stream. Nested find changes its shared cursor.
        std::array<GroundItem,256> live{};std::vector<std::string> trace;std::set<int> freed;Services service(0,live,trace,freed);ZoneItems zone(geometry0,service);service.owner=&zone;
        for(unsigned int i=0;i<4;++i) {live[i].id=100+i;live[i].position={10,20,10};}
        zone.enter(live[0],1);zone.enter(live[1],1);zone.enter(live[2],1);trace.clear();bool changed=false;
        service.on_send=[&] {if(!changed) {changed=true;zone.leave(live[0]);zone.leave(live[1]);zone.enter(live[3],1);}};
        zone.send_enter({0,0},service);require(trace.size()==7 && trace[0]=="S"+hex(ground_item_appearance(live[0],0)) && trace[5]=="S"+hex(ground_item_appearance(live[2],0)) && trace[6]=="S"+hex(ground_item_appearance(live[3],0)),"Live item stream did not follow removals/insertion");
        bool searched=false;service.on_send=[&] {if(!searched) {searched=true;(void)zone.find_cell({0,0},999);}};trace.clear();zone.send_leave({0,0},service);require(trace.size()==3,"Nested shared item cursor did not restart after exhausted lookup");
        service.on_send={};
        // Expiry is strict and release callbacks can remove the next live node.
        live[2].expires_at=10;live[3].expires_at=10;service.on_release=[&] {if(live[3].zone) zone.leave(live[3]);};trace.clear();zone.update(10);require(trace.empty(),"Equal expiry prematurely freed items");zone.update(11);
        require(freed.size()==1 && freed.contains(2) && zone.size()==0,"Item update did not follow a release-time removal");
        bool invalid=false;live[4].position={-40,0,0};try {zone.enter(live[4],0);}catch(const std::invalid_argument&) {invalid=true;}require(invalid,"Invalid negative item index was accepted");
        std::cout<<count<<" original zone/cell item cases and live-cursor mutations passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
