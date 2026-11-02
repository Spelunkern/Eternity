#include "eternity/game/corpses.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& bytes) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
net::Packet unhex(const std::string& text) {
    if(text=="-") return {};net::Packet result;require(text.size()%2==0,"Invalid corpse hex input");
    for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;
}
void append(net::Packet& data,std::uint32_t value,unsigned int size) {for(unsigned int i=0;i<size;++i) data.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::string snapshot(const Corpse& corpse) {
    net::Packet data;append(data,corpse.id,4);for(auto value:{corpse.position.x,corpse.position.y,corpse.position.z}) append(data,std::bit_cast<std::uint32_t>(value),4);
    append(data,corpse.direction,2);data.insert(data.end(),corpse.appearance.begin(),corpse.appearance.end());
    data.insert(data.end(),corpse.equipment_type.begin(),corpse.equipment_type.end());data.insert(data.end(),corpse.equipment_id.begin(),corpse.equipment_id.end());data.insert(data.end(),corpse.name.begin(),corpse.name.end());return hex(data);
}
struct Scene final:ActorScene {
    void unexpected() {throw std::runtime_error("Corpse capture performed an unrelated actor operation");}
    void send_self(const net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct Services final:ZoneCorpseServices,CorpseViewer {
    int zone;CorpsePool& pool;WorldCorpses& world;std::vector<Corpse*>& bodies;std::vector<std::string>& events;Corpse*& leaving;
    bool fail=false;std::function<void()> on_send;
    Services(int value,CorpsePool& heap,WorldCorpses& registry,std::vector<Corpse*>& allocated,std::vector<std::string>& trace,Corpse*& current)
        :zone(value),pool(heap),world(registry),bodies(allocated),events(trace),leaving(current) {}
    int index(const Corpse* corpse) const {if(!corpse) return -1;const auto entry=std::find(bodies.begin(),bodies.end(),corpse);require(entry!=bodies.end(),"Untracked corpse pointer");return static_cast<int>(entry-bodies.begin());}
    Corpse* allocate() override {if(fail) return nullptr;auto* body=pool.allocate();if(std::find(bodies.begin(),bodies.end(),body)==bodies.end()) bodies.push_back(body);return body;}
    void release(Corpse& corpse) override {require(!corpse.zone && !corpse.cell,"Corpse freed before detach");events.push_back("F"+std::to_string(index(&corpse)));pool.release(corpse);}
    void diagnostic(CorpseIssue issue,std::uint32_t id) override {events.push_back(std::string(1,issue==CorpseIssue::duplicate?'D':issue==CorpseIssue::missing?'M':'W')+std::to_string(id));}
    void send_cell_view(CellCoordinates cell,const net::Packet& packet) override {
        require(leaving && !world.find(leaving->id) && !leaving->zone && leaving->cell && leaving->cell->x==cell.x && leaving->cell->z==cell.z,"Corpse broadcast order differs from original");
        events.push_back("V"+std::to_string(zone)+","+std::to_string(cell.x)+","+std::to_string(cell.z)+":"+hex(packet));
    }
    void send_self(const net::Packet& packet) override {events.push_back("S"+hex(packet));if(on_send) on_send();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected corpse oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open corpse oracle");
        CorpsePool pool;WorldCorpses world;std::vector<Corpse*> bodies;std::vector<std::string> events;Corpse* leaving=nullptr;
        Services services0(0,pool,world,bodies,events,leaving),services1(1,pool,world,bodies,events,leaving);
        const MapGeometry geometry0(160),geometry1(320);ZoneCorpses zone0(geometry0,world,services0),zone1(geometry1,world,services1);
        const std::array<ZoneCorpses*,2> zones{&zone0,&zone1};const std::array<Services*,2> services{&services0,&services1};
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;Scene scene;
        unsigned int count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;row>>op;events.clear();int result=0;
            if(op=='C') {
                unsigned int zone=0,direction=0,mask=0;std::uint32_t id=0;std::array<std::uint32_t,3> position{};std::array<unsigned int,7> appearance{};std::array<unsigned int,8> types{},ids{};
                row>>zone>>id;for(auto& value:position) row>>value;row>>direction;for(auto& value:appearance) row>>value;row>>mask;
                for(auto& value:types) row>>value;for(auto& value:ids) row>>value;std::string name;row>>name;
                std::vector<PlayerItem> inventory;for(std::uint16_t slot=0;slot<8;++slot) if(mask&(1U<<slot)) {PlayerItem item;item.slot=slot;item.type=static_cast<std::uint8_t>(types[slot]);item.type_id=static_cast<std::uint8_t>(ids[slot]);inventory.push_back(item);}
                PlayerItem packed;packed.bag=1;packed.slot=7;packed.type=99;inventory.push_back(packed);
                Actor actor(id,{}, {},{},scene,{items,enchant,mobs},inventory);auto& identity=actor.appearance().identity;
                identity.country=static_cast<std::uint8_t>(appearance[0]);identity.family=static_cast<std::uint8_t>(appearance[1]);identity.hair=static_cast<std::uint8_t>(appearance[2]);identity.face=static_cast<std::uint8_t>(appearance[3]);
                identity.size=static_cast<std::uint8_t>(appearance[4]);identity.job=static_cast<std::uint8_t>(appearance[5]);identity.sex=static_cast<std::uint8_t>(appearance[6]);
                const auto bytes=unhex(name);identity.name.assign(bytes.begin(),bytes.end());
                const auto* corpse=zones.at(zone)->create(actor,{std::bit_cast<float>(position[0]),std::bit_cast<float>(position[1]),std::bit_cast<float>(position[2])},static_cast<std::uint16_t>(direction));result=services0.index(corpse);
            } else if(op=='L') {
                unsigned int zone=0,index=0;row>>zone>>index;leaving=bodies.at(index);result=zones.at(zone)->leave(*leaving)?1:0;
            } else if(op=='R') {
                std::uint32_t id=0;row>>id;leaving=world.find(id);result=world.leave(id)?1:0;
            } else if(op=='F' || op=='W' || op=='Q') {
                unsigned int zone=0;std::uint32_t id=0;Corpse* corpse=nullptr;
                if(op=='W') {row>>id;corpse=world.find(id);}
                else if(op=='F') {row>>zone>>id;corpse=zones.at(zone)->find(id);}
                else {CellCoordinates cell;row>>zone>>cell.x>>cell.z>>id;corpse=zones.at(zone)->find_cell(cell,id);}
                result=services0.index(corpse);
            } else if(op=='E' || op=='O') {
                unsigned int zone=0;CellCoordinates cell;row>>zone>>cell.x>>cell.z;
                if(op=='E') zones.at(zone)->send_enter(cell,*services.at(zone));else zones.at(zone)->send_leave(cell,*services.at(zone));
            } else throw std::runtime_error("Unknown corpse operation");
            const auto context=" at corpse vector "+std::to_string(count);int original_result=0;std::size_t allocated=0;row>>original_result>>allocated;
            require(result==original_result,"Corpse operation result differs"+context);require(allocated==bodies.size(),"Corpse heap reuse differs"+context);
            for(const auto* body:bodies) {
                int zone=-1,x=0,z=0;std::string expected;row>>zone>>x>>z>>expected;
                const int actual_zone=body->zone==&zone0?0:body->zone==&zone1?1:-1;
                require(zone==actual_zone && x==(body->cell?body->cell->x:0) && z==(body->cell?body->cell->z:0),"Corpse binding differs"+context);
                require(snapshot(*body)==expected,"Corpse snapshot or recycled fields differ"+context);
            }
            std::size_t registered=0;row>>registered;require(registered==world.size(),"Global corpse count differs"+context);
            for(std::size_t i=0;i<registered;++i) {std::uint32_t id=0;int index=0;row>>id>>index;require(services0.index(world.find(id))==index,"Global corpse ownership differs"+context);}
            for(unsigned int zone=0;zone<2;++zone) require(zones[zone]->size()==static_cast<std::size_t>(std::count_if(bodies.begin(),bodies.end(),[&](const auto* body){return body->zone==zones[zone];})),"Zone corpse count differs"+context);
            std::size_t event_count=0;row>>event_count;require(event_count==events.size(),"Corpse notification count differs"+context);
            for(const auto& event:events) {std::string expected;row>>expected;require(event==expected,"Corpse notification differs"+context+": "+event+" versus "+expected);}
            require(static_cast<bool>(row),"Truncated corpse vector");std::string extra;require(!(row>>extra),"Trailing corpse vector data");++count;
        }
        require(count==396,"Corpse oracle coverage changed");
        Actor actor(9999,{}, {},{},scene,{items,enchant,mobs});services0.fail=true;
        require(!zone0.create(actor,{0,0,0},0),"Failed corpse allocation proceeded into capture");services0.fail=false;
        Corpse invalid;invalid.position={-40,0,0};bool rejected=false;try {zone0.enter(invalid);}catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Invalid negative original corpse cell accepted");
        // Send callbacks can unlink/reuse the current body or move the same cell's
        // shared search cursor; traversal must read that live cursor afterwards.
        CorpsePool heap2;WorldCorpses world2;std::vector<Corpse*> allocated2;std::vector<std::string> trace2;Corpse* current2=nullptr;
        Services port2(2,heap2,world2,allocated2,trace2,current2);ZoneCorpses grid2(geometry0,world2,port2);
        for(std::uint32_t id=100;id<103;++id) {auto* body=port2.allocate();body->id=id;require(grid2.enter(*body),"Cannot prepare live corpse cursor test");}
        bool first=true;port2.on_send=[&] {
            if(!first) return;first=false;current2=allocated2[0];require(world2.leave(100),"Cannot unlink currently announced corpse");
            auto* replacement=port2.allocate();require(replacement==current2,"Corpse pool did not reuse current body");replacement->id=103;require(grid2.enter(*replacement),"Cannot append replacement corpse");
        };
        grid2.send_enter({0,0},port2);port2.on_send={};
        const auto sent_ids=[&] {
            std::vector<std::uint32_t> result;for(const auto& event:trace2) if(event[0]=='S') {
                const auto bytes=unhex(event.substr(1));std::uint32_t id=0;for(unsigned int i=0;i<4;++i) id|=static_cast<std::uint32_t>(bytes.at(2+i))<<(8*i);result.push_back(id);
            }return result;
        };
        require(sent_ids()==std::vector<std::uint32_t>{100,101,102,103},"Corpse send did not traverse current removal and tail reuse");
        trace2.clear();first=true;port2.on_send=[&] {if(first) {first=false;require(grid2.find_cell({0,0},102)==allocated2[2],"Cannot move shared corpse cursor");}};
        grid2.send_leave({0,0},port2);port2.on_send={};
        require(sent_ids()==std::vector<std::uint32_t>{101,103} && grid2.size()==3 && world2.size()==3,"Cell visibility removal ignored shared cursor or unlinked bodies");
        current2=allocated2[1];world2.leave(101);current2=allocated2[2];world2.leave(102);
        require(heap2.allocate()==allocated2[2],"Corpse heap did not preserve LIFO order");
        heap2.release(*allocated2[2]);bool double_free=false;try {heap2.release(*allocated2[2]);}catch(const std::invalid_argument&) {double_free=true;}
        require(double_free,"Corpse heap accepted duplicate release");
        std::cout<<count<<" original corpse lifecycle/visibility operations and pooled snapshots passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
