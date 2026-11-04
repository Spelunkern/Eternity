#include "mob_damage_fixture.hpp"
#include "eternity/game/obelisk_runtime_zone.hpp"
#include "eternity/game/obelisk_runtime_publication.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
using eternity::net::Packet;
namespace {
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
struct Services final:MobPoolServices,ZoneMobServices {
    MobDamageFixture damage_fixture;
    MobDamagePool& damage_pool() override {return damage_fixture.pool;}
    bool capture=false;std::uint32_t tick=0,unix=0,ticks=0;
    ZoneMobs* zone=nullptr;MobPool* pool=nullptr;ObeliskRuntimeMap* map=nullptr;
    std::vector<MobRuntime*> objects;std::vector<std::uint32_t> trace;
    void event(std::initializer_list<std::uint32_t> values) {if(capture) trace.insert(trace.end(),values);}
    std::uint32_t identity(const MobRuntime* mob) const {
        if(!mob) return 255;
        const auto found=std::find(objects.begin(),objects.end(),mob);require(found!=objects.end(),"Unknown pooled actor");
        return static_cast<std::uint32_t>(found-objects.begin());
    }
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected removal service");}
    std::unique_ptr<MobRuntime> construct() override {
        require(!capture,"Removal reuse unexpectedly allocated a new actor");auto object=std::make_unique<MobRuntime>();objects.push_back(object.get());return object;
    }
    std::uint32_t unix_time() override {event({12,unix});return unix;}
    std::uint32_t tick_ms() override {const auto value=tick+17U*ticks++;event({1,value});return value;}
    void release_compiled_ai(MobRuntime& mob) override {
        require(!mob.zone && !mob.cell && zone->find(mob.id)==nullptr,"Recycled actor is still linked");
        event({3,identity(&mob),mob.id,static_cast<std::uint32_t>(zone->size()),static_cast<std::uint32_t>(pool->available()),static_cast<std::uint32_t>(map->reset_send)});
    }
    void release_stack_ai(MobRuntime&) override {unexpected();}
    void close_lua(void*) override {unexpected();}
    void send_cell_view(CellCoordinates cell,const Packet& packet) override {
        if(!capture) return;
        require(packet.size()==6 && packet[0]==2 && packet[1]==6,"Unexpected zone disappearance packet");
        std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=static_cast<std::uint32_t>(packet[2+i])<<(8*i);
        auto found=std::find_if(objects.begin(),objects.end(),[&](auto* mob){return mob->id==id;});require(found!=objects.end(),"Unknown disappearance actor");
        const auto& mob=**found;
        require(!mob.zone && mob.cell && mob.cell->x==cell.x && mob.cell->z==cell.z && zone->find(id)==nullptr,"Forced removal publication order changed");
        trace.push_back(2);trace.push_back(static_cast<std::uint32_t>(packet.size()));trace.insert(trace.end(),packet.begin(),packet.end());
        event({static_cast<std::uint32_t>(zone->size()),static_cast<std::uint32_t>(mob.zone!=nullptr),static_cast<std::uint32_t>(mob.cell->x),static_cast<std::uint32_t>(mob.cell->z)});
    }
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Packet&,std::optional<std::uint8_t>) override {unexpected();}
    void lua_init(MobRuntime&) override {unexpected();}
};
struct OtherOwners final:MobRemovalOwners {
    void destroy_obelisk(MobRuntime&,std::int32_t) override {Services::unexpected();}
    void remove_obelisk_member(MobRuntime&,void*) override {Services::unexpected();}
    void destroy_linked_group(std::int32_t) override {Services::unexpected();}
    void remove_linked_member(MobRuntime&,void*) override {Services::unexpected();}
};
struct World final:ObeliskRuntimeWorld,ObeliskRuntimePublication {
    Services& services;ObeliskRuntimeMap& map;ObeliskRuntime& obelisk;ObeliskRuntimeZoneRemoval* removal=nullptr;
    World(Services& s,ObeliskRuntimeMap& m,ObeliskRuntime& o):services(s),map(m),obelisk(o) {}
    std::string_view mob_name(const MobRuntime&) override {return "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";}
    void remove_by_id(std::uint32_t id) override {static_cast<void>(removal->remove_by_id(id));}
    void send_change(std::size_t index) override {require(index==0,"Wrong obelisk index");publish_obelisk_change(map,obelisk,*this);}
    void packet(std::uint32_t tag,const Packet& packet) {
        services.event({tag,static_cast<std::uint32_t>(packet.size())});services.trace.insert(services.trace.end(),packet.begin(),packet.end());
    }
    void zone_packet(const Packet& bytes) override {packet(4,bytes);services.event({static_cast<std::uint32_t>(map.reset_send)});}
    void database_packet(const Packet& bytes) override {packet(6,bytes);}
    void database(const Packet& bytes) override {database_packet(bytes);}
    void game_log(const Packet& bytes) override {packet(5,bytes);}
    void update_portal_links() override {services.event({7,static_cast<std::uint32_t>(map.reset_send)});}
    void add_kill_count(std::int32_t,std::int32_t) override {Services::unexpected();}
    bool kill_count_status14(std::int32_t) override {Services::unexpected();}
    void subtract_kill_count(std::int32_t,std::int32_t) override {Services::unexpected();}
    void broadcast(const Packet&) override {Services::unexpected();}
    MobRuntime* allocate(bool) override {Services::unexpected();}
    void prepare_companion(MobRuntime&) override {Services::unexpected();}
    void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&) override {Services::unexpected();}
    void enter(MobRuntime&) override {Services::unexpected();}
    std::int32_t random() override {Services::unexpected();}
};
void run(int scenario,const std::vector<std::uint32_t>& expected) {
    Services s;s.unix=123456U+static_cast<std::uint32_t>(scenario);s.tick=std::array<std::uint32_t,4>{0,100,0xfffffff0U,0xffffffffU}[static_cast<std::size_t>(scenario/11%4)];
    auto counter=std::array<std::uint32_t,3>{0,0xfffffffdU,0xffffffffU}[static_cast<std::size_t>(scenario%3)];MobPool pool(counter,s);s.pool=&pool;
    ZoneMobs zone(MapGeometry(81),s);s.zone=&zone;ObeliskRuntimeMap map{true,scenario/7%2!=0,42,81,{}};s.map=&map;
    ObeliskRuntime o;o.id=0x81001234U;o.country=scenario%3;o.base_country=scenario/3%3;o.last_destroy_country=std::array<int,4>{-1,0,1,2}[static_cast<std::size_t>(scenario/9%4)];
    o.change_time=0xfedcba98U;o.base_id=0x01234567;o.respawn.type=0;o.respawn.count=1;o.respawn.current=std::array<int,3>{1,0,-1}[static_cast<std::size_t>(scenario/5%3)];o.respawn.next=123;o.respawn.obelisk=true;
    o.spawns.resize(1);auto& group=o.spawns[0];group.count=2;group.members.resize(2);group.respawn.count=2;
    group.respawn.current=std::array<int,4>{2,1,0,-1}[static_cast<std::size_t>(scenario/7%4)];group.respawn.next=2345;group.respawn.obelisk_source=&group;
    MobRuntimeTemplate definition;definition.id=321;definition.respawn_time=std::array<std::uint32_t,5>{0,1,20,0xffffffffU,0x80000000U}[static_cast<std::size_t>(scenario/3%5)];
    std::array<MobRuntime*,3> objects;std::array<std::uint32_t,3> ids;
    for(std::size_t i=0;i<3;++i) {
        auto* mob=pool.allocate();objects[i]=mob;ids[i]=mob->id;mob->definition=&definition;mob->type=static_cast<std::uint16_t>(654+i);
        mob->position={std::array<float,3>{1.25,41.25,80.25}[i],2.5,3.25};mob->respawn=i==0?&o.respawn:&group.respawn;
        require(zone.enter(*mob),"Initial actor entry failed");
    }
    o.principal=objects[0];
    const auto slots=std::array<std::array<unsigned,2>,5>{{{1,1},{255,2},{1,2},{255,255},{2,1}}}[static_cast<std::size_t>(scenario%5)];
    for(std::size_t i=0;i<2;++i) group.members[i]=slots[i]==255?nullptr:objects[slots[i]];
    World world(s,map,o);OtherOwners other;const std::array<std::int32_t,2> kc{30,-15};
    ObeliskRuntimeZoneRemoval removal(map,std::span<ObeliskRuntime>(&o,1),zone,pool,world,kc,other);world.removal=&removal;s.capture=true;
    auto snapshot=[&] {
        s.event({9,static_cast<std::uint32_t>(zone.size()),static_cast<std::uint32_t>(pool.available()),counter,static_cast<std::uint32_t>(map.reset_send),static_cast<std::uint32_t>(o.principal!=nullptr),
                 static_cast<std::uint32_t>(o.country),static_cast<std::uint32_t>(o.last_destroy_country),static_cast<std::uint32_t>(o.respawn.current),o.respawn.next,
                 static_cast<std::uint32_t>(group.respawn.current),group.respawn.next,s.identity(group.members[0]),s.identity(group.members[1])});
    };
    snapshot();const auto mode=static_cast<unsigned>(scenario/5%2);s.event({10,mode});
    if(mode) {reset_obelisks(map,std::span<ObeliskRuntime>(&o,1),[&]{return s.tick_ms();},world);s.event({11,255});}
    else s.event({11,static_cast<std::uint32_t>(removal.remove_by_id(ids[0]))});
    snapshot();
    for(const auto [op,index]:std::array<std::pair<unsigned,unsigned>,3>{{{2,0},{3,1},{4,2}}}) {
        s.event({10,op});s.event({11,static_cast<std::uint32_t>(removal.remove_by_id(ids[index]))});snapshot();
    }
    for(unsigned op:{5,6}) {
        s.event({10,op});auto* mob=pool.allocate();require(mob!=nullptr,"Pooled reuse failed");
        s.event({8,s.identity(mob),mob->id,static_cast<std::uint32_t>(pool.available()),counter,mob->drop_random.seed(),mob->drop_random.words()[0]});snapshot();
    }
    require(zone.size()==0 && pool.available()==1,"Final removal/pool ownership mismatch");
    if(s.trace!=expected) {
        std::size_t i=0;while(i<s.trace.size()&&i<expected.size()&&s.trace[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" divergence at "<<i<<" native="<<(i<s.trace.size()?std::to_string(s.trace[i]):"end")
                 <<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Original zone/obelisk/pool composition comparison failed");
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected zone composition oracle");std::ifstream input(argv[1]);require(input.good(),"Missing zone composition oracle");
        std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;
            std::vector<std::uint32_t> expected;std::uint32_t word;while(row>>word) expected.push_back(word);
            run(scenario,expected);++cases;
        }
        require(cases==256,"Incomplete zone composition fixture");std::cout<<cases<<" original real-zone/obelisk/pool sequences passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
