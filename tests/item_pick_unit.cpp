#include "equipment_fixture.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/item_pick.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& packet) {static constexpr char digits[]="0123456789abcdef";std::string value;for(auto byte:packet) {value+=digits[byte>>4];value+=digits[byte&15];}return value;}
void append(net::Packet& bytes,std::uint32_t value,unsigned int length) {for(unsigned int i=0;i<length;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint64_t canonical_hash(const ItemQuestLinks& links) {
    std::uint64_t hash=0xcbf29ce484222325ULL;
    const auto add=[&](std::uint32_t value,unsigned int length) {for(unsigned int i=0;i<length;++i) hash=(hash^static_cast<std::uint8_t>(value>>(8*i)))*0x100000001b3ULL;};
    for(const auto& link:links.records()) for(const auto* group:{&link.start,&link.end}) {add(static_cast<std::uint32_t>(group->size()),4);for(auto value:*group) add(value,2);}return hash;
}
void check_links(const ItemQuestLinks& links,std::istringstream& row) {
    std::size_t consumed=0;std::uint64_t expected=0;row>>consumed>>expected;require(links.consumed()==consumed && canonical_hash(links)==expected,"Original item quest link records differ");
    auto definitions=ItemDefinitions::load(item_definitions());definitions.initialize_quest_flags(links);
    for(const auto [type,id]:std::array<std::pair<std::uint8_t,std::uint8_t>,8>{{{5,1},{16,1},{21,1},{45,1},{30,1},{30,2},{30,3},{30,4}}}) {unsigned int flag=0;row>>flag;require(definitions.find(type,id)->quest_start==static_cast<bool>(flag),"Original quest-start flag differs");}
    for(std::int32_t type:{-1,0,1,16,27,30,255,256}) for(std::int32_t id:{-1,0,1,3,255,256}) require(static_cast<bool>(links.find(type,id))==(type>0 && type<256 && id>0 && id<256),"Item quest link lookup bounds differ");
}
struct Scene final:ActorScene {
    void unexpected() {throw std::runtime_error("Pickup performed an unrelated actor operation");}
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
struct World final:ItemPickWorld,ZoneItemServices {
    std::uint32_t tick=0,party=0;UserPosition position;std::vector<std::uint16_t> active;std::vector<std::string> events;
    ZoneItems* zone=nullptr;GroundItem* item=nullptr;
    std::uint32_t pick_tick_ms() override {events.push_back("T");return tick;}
    std::uint32_t pick_party_id(Actor&) override {events.push_back("P");return party;}
    UserPosition pick_position(const Actor&) override {events.push_back("D");return position;}
    bool pick_quest_active(Actor&,std::uint16_t id) override {events.push_back("Q"+std::to_string(id));return std::find(active.begin(),active.end(),id)!=active.end();}
    std::uint32_t tick_ms() override {return 0;}
    void release(GroundItem&) override {throw std::runtime_error("Pickup freed an item instead of returning it");}
    void send_cell_view(CellCoordinates cell,const net::Packet& packet) override {
        require(cell.x==0 && cell.z==0,"Pickup used the wrong cell");
        if(packet[0]==2) require(!item->zone && item->cell && !zone->find(item->id),"Pickup broadcast preceded detach");events.push_back("V"+hex(packet));
    }
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected item pickup oracle and optional NpcQuest.SData");std::ifstream input(argv[1]);require(input.good(),"Cannot open item pickup oracle");
        ItemQuestLinks links;const MapGeometry geometry(160);const ItemDefinitions no_items;const EnchantValues enchant;const MobAttackTemplates mobs;Scene scene;
        std::string line;unsigned int count=0,loads=0,actual=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;row>>op;
            if(op=='A') {
                std::string sha;std::size_t prefix=0;row>>sha>>prefix;
                if(argc==3) {std::ifstream file(argv[2],std::ios::binary);require(file.good(),"Cannot open original NPC/quest data");const net::Packet bytes((std::istreambuf_iterator<char>(file)),{});const auto original=ItemQuestLinks::npc_quest(bytes);check_links(original,row);++actual;}
                else continue;
            } else if(op=='L') {
                std::size_t entries=0;row>>entries;std::map<std::pair<unsigned int,unsigned int>,ItemQuestLink> fixture;
                for(std::size_t i=0;i<entries;++i) {unsigned int type=0,id=0;row>>type>>id;auto& link=fixture[{type,id}];for(auto* group:{&link.start,&link.end}) {std::size_t length=0;row>>length;group->resize(length);for(auto& value:*group) row>>value;}}
                net::Packet bytes;for(unsigned int type=0;type<256;++type) for(unsigned int id=0;id<256;++id) {const auto entry=fixture.find({type,id});const ItemQuestLink empty;const auto& link=entry==fixture.end()?empty:entry->second;for(const auto* group:{&link.start,&link.end}) {append(bytes,static_cast<std::uint32_t>(group->size()),4);for(auto value:*group) append(bytes,value,2);}}
                links=ItemQuestLinks::decode(bytes);check_links(links,row);++loads;
                bytes.pop_back();bool invalid=false;try {(void)ItemQuestLinks::decode(bytes);}catch(const std::runtime_error&) {invalid=true;}require(invalid,"Truncated item quest links accepted");
            } else {
                require(op=='P' && loads==1,"Unknown pickup vector or missing links");unsigned int method=0,checks=0,present=0,type=0,has_definition=0,real_type=0,quest_start=0,def_type=0,def_id=0;std::int32_t error=0;World world;GroundItem item;ItemDefinition definition;
                row>>method>>checks>>present>>world.tick>>world.party>>item.user_id>>item.party_id>>item.expires_at>>type>>has_definition>>real_type>>quest_start>>def_type>>def_id>>error;
                std::array<std::uint32_t,6> coordinates{};for(auto& value:coordinates) row>>value;const UserPosition position{std::bit_cast<float>(coordinates[0]),std::bit_cast<float>(coordinates[1]),std::bit_cast<float>(coordinates[2])};world.position={std::bit_cast<float>(coordinates[3]),std::bit_cast<float>(coordinates[4]),std::bit_cast<float>(coordinates[5])};
                std::size_t active=0;row>>active;world.active.resize(active);for(auto& value:world.active) row>>value;
                ZoneItems zone(geometry,world);world.zone=&zone;world.item=&item;item.id=9009;item.item.type=static_cast<std::uint8_t>(type);item.position={10,20,10};const auto expiry=item.expires_at;
                if(present) require(zone.enter(item,0),"Cannot attach pickup fixture");item.position=position;item.expires_at=expiry;
                definition.type=static_cast<std::uint8_t>(def_type);definition.type_id=static_cast<std::uint8_t>(def_id);definition.real_type=static_cast<std::uint8_t>(real_type);definition.quest_start=quest_start!=0;item.definition=has_definition?&definition:nullptr;
                Actor actor(9001,{}, {},{},scene,{no_items,enchant,mobs});world.events.clear();
                const bool result=method==0?ground_item_pick_enabled(item,actor.id(),world.party,checks!=0,world):zone.pick(actor,item.id,error,checks!=0,links,world)!=nullptr;
                const auto context=" at item pickup vector "+std::to_string(count);
                const std::array<std::uint32_t,6> values{static_cast<std::uint32_t>(result),static_cast<std::uint32_t>(error),static_cast<std::uint32_t>(item.zone!=nullptr),static_cast<std::uint32_t>(zone.find(item.id)!=nullptr),static_cast<std::uint32_t>(zone.find_cell({0,0},item.id)!=nullptr),static_cast<std::uint32_t>(zone.size())};
                for(std::size_t i=0;i<values.size();++i) {std::uint32_t original=0;row>>original;require(values[i]==original,"Pickup result/state differs"+context+" field "+std::to_string(i));}
                require(item.expires_at==expiry && item.item.type==type && item.definition==(has_definition?&definition:nullptr),"Pickup altered retained item payload");
                std::size_t events=0;row>>events;require(world.events.size()==events,"Pickup event count differs"+context);for(const auto& event:world.events) {std::string original;row>>original;require(event==original,"Pickup event differs"+context+": "+event+" versus "+original);}
                ++count;
            }
            require(static_cast<bool>(row),"Truncated item pickup vector");std::string extra;require(!(row>>extra),"Trailing item pickup data");
        }
        require(loads==1 && count==1536 && actual==(argc==3?1U:0U),"Item pickup coverage changed");
        std::cout<<count<<" original pickup permission/zone cases, "<<loads<<" item-link/flag fixture and "<<actual<<" full distributions passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
