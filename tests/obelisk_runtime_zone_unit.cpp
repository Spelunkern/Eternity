#include "mob_damage_fixture.hpp"
#include "eternity/game/obelisk_runtime_death.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/linked_mobs.hpp"
#include "eternity/game/obelisk_runtime_publication.hpp"
#include "eternity/game/obelisk_runtime_portal.hpp"
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
    bool capture=false,combat=false,natural=false;int scenario=0;std::uint32_t tick=0,unix=0,ticks=0;
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
        require(mob.cell && mob.cell->x==cell.x && mob.cell->z==cell.z,"Removal cell coordinates changed");
        if(natural) require(mob.zone==zone && zone->find(id)==&mob,"Corpse must publish before unlink");
        else require(!mob.zone && zone->find(id)==nullptr,"Forced removal must publish after unlink");
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
struct World final:ObeliskRuntimeWorld,ObeliskRuntimePublication,MobDeathWorld,ObeliskRuntimePortalWorld {
    Services& services;ObeliskRuntimeMap& map;ObeliskRuntime& obelisk;ObeliskRuntimeZoneRemoval* removal=nullptr;ObeliskRuntimeDeathOwnership* combat_owner=nullptr;MobDeathUser user;
    bool portals=false,portal_mutated=false;
    std::array<ObeliskRuntimePortalLink,4> portal_links;
    std::array<std::int32_t,4> portal_countries{};
    std::array<ObeliskRuntime,3> portal_records;
    World(Services& s,ObeliskRuntimeMap& m,ObeliskRuntime& o):services(s),map(m),obelisk(o),user{100,1,static_cast<std::uint8_t>(s.scenario/15%3)} {}
    std::string_view mob_name(const MobRuntime&) override {return "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";}
    void remove_by_id(std::uint32_t id) override {static_cast<void>(removal->remove_by_id(id));}
    void send_change(std::size_t index) override {require(index==0,"Wrong obelisk index");publish_obelisk_change(map,obelisk,*this);}
    void packet(std::uint32_t tag,const Packet& packet) {
        services.event({tag,static_cast<std::uint32_t>(packet.size())});services.trace.insert(services.trace.end(),packet.begin(),packet.end());
    }
    void zone_packet(const Packet& bytes) override {packet(4,bytes);services.event({static_cast<std::uint32_t>(map.reset_send)});}
    void database_packet(const Packet& bytes) override {
        packet(6,bytes);
        std::uint32_t id=0;if(bytes.size()>=6) for(unsigned i=0;i<4;++i) id|=static_cast<std::uint32_t>(bytes[2+i])<<(i*8);
        if(portals&&bytes.size()==11&&id>=0x10001000U&&services.scenario%17==0&&!portal_mutated) {
            portal_mutated=true;obelisk.last_destroy_country=(obelisk.last_destroy_country+1)%3;
            for(std::size_t i=0;i<4;++i) portal_countries[i]=(services.scenario+static_cast<int>(i))%4;
        }
    }
    void database(const Packet& bytes) override {database_packet(bytes);}
    void game_log(const Packet& bytes) override {packet(5,bytes);}
    void update_portal_links() override {
        services.event({7,static_cast<std::uint32_t>(map.reset_send)});
        if(portals) update_obelisk_portal_links(portal_links,*this);
    }
    void map_broadcast(std::size_t,const Packet& bytes) override {zone_packet(bytes);}
    void add_kill_count(std::int32_t country,std::int32_t amount) override {services.event({20,static_cast<std::uint32_t>(country),static_cast<std::uint32_t>(amount)});}
    bool kill_count_status14(std::int32_t) override {return services.scenario/4%2!=0;}
    void subtract_kill_count(std::int32_t country,std::int32_t amount) override {services.event({21,static_cast<std::uint32_t>(country),static_cast<std::uint32_t>(amount)});}
    void broadcast(const Packet& bytes) override {packet(23,bytes);}
    MobRuntime* allocate(bool) override {Services::unexpected();}
    void prepare_companion(MobRuntime&) override {Services::unexpected();}
    void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&) override {Services::unexpected();}
    void enter(MobRuntime&) override {Services::unexpected();}
    std::int32_t random() override {Services::unexpected();}
    void send_death(MobRuntime&,const Packet& bytes,std::uint8_t priority) override {packet(24,bytes);services.event({priority,static_cast<std::uint32_t>(services.zone->size())});}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t id) override {
        services.event({15,id});const auto credit=services.scenario/3%5;return credit==1 || credit==4?&user:nullptr;
    }
    MobDeathParty* party(MobDeathUser&) override {return nullptr;}
    void lock_party(MobDeathParty&) override {Services::unexpected();}
    void unlock_party(MobDeathParty&) noexcept override {std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t) override {Services::unexpected();}
    void add_experience(MobDeathUser& credited,std::uint32_t source,std::int32_t amount) override {services.event({16,credited.id,source,static_cast<std::uint32_t>(amount)});}
    void drop_items(MobRuntime&,MobDeathUser* credited,MobDeathParty*) override {
        services.event({13,credited?credited->id:0});if(services.scenario%13==0) obelisk.country=(obelisk.country+1)%3;
    }
    void quest_kill(MobDeathUser& credited,MobRuntime&) override {services.event({14,credited.id});if(services.scenario%11==0) credited.country=static_cast<std::uint8_t>((credited.country+1)%3);}
    std::optional<std::int32_t> obelisk_country(MobRuntime& mob,std::int32_t type) override {return combat_owner->country(mob,type);}
    void destroy_obelisk(MobRuntime& mob,std::int32_t country,std::int32_t type) override {combat_owner->destroyed(mob,country,type);}
    void remove_obelisk_member(MobRuntime& mob,void* source) override {combat_owner->remove_member(mob,source);}
    void destroy_linked_group(MobRuntime&,std::int32_t) override {Services::unexpected();}
    void remove_linked_member(MobRuntime&,void*) override {Services::unexpected();}
    void log_death(MobRuntime&,MobDeathUser*,bool) override {Services::unexpected();}
    void clear_applied_skills(MobRuntime& mob) override {
        services.event({25,static_cast<std::uint32_t>(obelisk.respawn.current),obelisk.respawn.next,static_cast<std::uint32_t>(mob.damage->records().empty()),
            static_cast<std::uint32_t>(mob.damage->fair_combat),static_cast<std::uint32_t>(services.zone->size()),static_cast<std::uint32_t>(services.damage_fixture.pool.available().size())});
    }
};
void run(int scenario,const std::vector<std::uint32_t>& expected,bool combat,bool portals) {
    Services s;s.scenario=scenario;s.combat=combat;s.unix=123456U+static_cast<std::uint32_t>(scenario);s.tick=std::array<std::uint32_t,4>{0,100,0xfffffff0U,0xffffffffU}[static_cast<std::size_t>(scenario/11%4)];
    auto counter=std::array<std::uint32_t,3>{0,0xfffffffdU,0xffffffffU}[static_cast<std::size_t>(scenario%3)];MobPool pool(counter,s);s.pool=&pool;
    ZoneMobs zone(MapGeometry(81),s);s.zone=&zone;ObeliskRuntimeMap map{true,scenario/7%2!=0,42,81,{}};s.map=&map;
    ObeliskRuntime o;o.id=0x81001234U;o.country=scenario%3;o.base_country=scenario/3%3;o.last_destroy_country=std::array<int,4>{-1,0,1,2}[static_cast<std::size_t>(scenario/9%4)];
    o.change_time=0xfedcba98U;o.base_id=0x01234567;o.respawn.type=0;o.respawn.count=1;o.respawn.current=std::array<int,3>{1,0,-1}[static_cast<std::size_t>(scenario/5%3)];o.respawn.next=123;o.respawn.obelisk=true;
    o.spawns.resize(1);auto& group=o.spawns[0];group.count=2;group.members.resize(2);group.respawn.count=2;
    group.respawn.current=std::array<int,4>{2,1,0,-1}[static_cast<std::size_t>(scenario/7%4)];group.respawn.next=2345;group.respawn.obelisk_source=&group;
    MobRuntimeTemplate definition;definition.id=321;definition.level=1;definition.respawn_time=std::array<std::uint32_t,5>{0,1,20,0xffffffffU,0x80000000U}[static_cast<std::size_t>(scenario/3%5)];
    std::array<MobRuntime*,3> objects;std::array<std::uint32_t,3> ids;
    for(std::size_t i=0;i<3;++i) {
        auto* mob=pool.allocate();objects[i]=mob;ids[i]=mob->id;mob->definition=&definition;mob->type=static_cast<std::uint16_t>(654+i);
        mob->position={std::array<float,3>{1.25,41.25,80.25}[i],2.5,3.25};mob->respawn=i==0?&o.respawn:&group.respawn;
        require(zone.enter(*mob),"Initial actor entry failed");
    }
    o.principal=objects[0];
    const auto slots=std::array<std::array<unsigned,2>,5>{{{1,1},{255,2},{1,2},{255,255},{2,1}}}[static_cast<std::size_t>(scenario%5)];
    for(std::size_t i=0;i<2;++i) group.members[i]=slots[i]==255?nullptr:objects[slots[i]];
    World world(s,map,o);OtherOwners other;const std::array<std::int32_t,2> kc{combat && scenario/6%2?0:30,combat?(scenario/7%2?-15:0):-15};
    ObeliskRuntimeZoneRemoval removal(map,std::span<ObeliskRuntime>(&o,1),zone,pool,world,kc,other);world.removal=&removal;
    ObeliskRuntimeDeathOwnership combat_owner(map,std::span<ObeliskRuntime>(&o,1),zone,world,kc,removal);world.combat_owner=&combat_owner;
    if(combat) {
        const auto credit=scenario/3%5;
        if(credit) objects[0]->damage->add(credit==3?2:1,100,41,0,0);
        objects[0]->damage->fair_combat=scenario/7%2!=0;
    }
    world.portals=portals;
    if(portals) {
        for(std::size_t j=0;j<3;++j) world.portal_records[j].last_destroy_country=(scenario/7+static_cast<int>(j))%3;
        for(std::size_t i=0;i<4;++i) {
            auto& link=world.portal_links[i];const auto n=static_cast<int>(i);
            link.id=0x81002000U+static_cast<std::uint32_t>(i);link.base_id=0x10001000U+static_cast<std::uint32_t>(i);link.portal_id=static_cast<std::uint16_t>(1000+i);
            world.portal_countries[i]=(scenario/3+n)%5-1;link.country=(scenario/13+n)%6==0?nullptr:&world.portal_countries[i];
            link.zone_present=(scenario/3+n)%3!=0;link.open={(scenario+n)%2!=0,(scenario/2+n)%2!=0};
            const auto kind=(scenario/5+n)%3;link.counts={kind==1?0:2,kind==2?0:2};
            for(std::size_t j=0;j<8;++j) {
                const auto* ref=j==0||j==4?&o:&world.portal_records[(j+i)%3];
                link.links[j]=(scenario/11+n+static_cast<int>(j))%7==0?nullptr:ref;
            }
        }
    }
    s.capture=true;
    auto snapshot=[&] {
        s.event({9,static_cast<std::uint32_t>(zone.size()),static_cast<std::uint32_t>(pool.available()),counter,static_cast<std::uint32_t>(map.reset_send),static_cast<std::uint32_t>(o.principal!=nullptr),
                 static_cast<std::uint32_t>(o.country),static_cast<std::uint32_t>(o.last_destroy_country),static_cast<std::uint32_t>(o.respawn.current),o.respawn.next,
                 static_cast<std::uint32_t>(group.respawn.current),group.respawn.next,s.identity(group.members[0]),s.identity(group.members[1])});
        if(portals) for(std::size_t i=0;i<4;++i) s.event({26,static_cast<std::uint32_t>(world.portal_countries[i]),static_cast<std::uint32_t>(world.portal_links[i].open[0]),static_cast<std::uint32_t>(world.portal_links[i].open[1])});
    };
    snapshot();const auto mode=combat?7U:static_cast<unsigned>(scenario/5%2);s.event({10,mode});
    if(combat) {MobDeathExperience rules;mob_death(*objects[0],2,0xfedcba98U,rules,world);s.event({11,255});}
    else if(mode) {reset_obelisks(map,std::span<ObeliskRuntime>(&o,1),[&]{return s.tick_ms();},world);s.event({11,255});}
    else s.event({11,static_cast<std::uint32_t>(removal.remove_by_id(ids[0]))});
    snapshot();
    if(combat) {
        require(objects[0]->zone==&zone && objects[0]->status==0,"Death must retain zone and caller-owned FSM state");
        objects[0]->status=2;objects[0]->death_time=10;LinkedMobPopulation linked{std::span<const LinkedMobGroup>{}};MicrosoftRandom random;
        s.natural=true;
        for(const auto [op,tick]:std::array<std::pair<unsigned,std::uint32_t>,2>{{{8,10},{9,11}}}) {
            s.event({10,op});zone.update_deaths(pool,random,tick,linked);s.event({11,255});snapshot();
        }
        s.natural=false;
    }
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
        require(argc==2 || argc==3,"Expected zone composition oracle and optional death mode");const bool portals=argc==3 && std::string_view(argv[2])=="death-portals";const bool combat=portals||(argc==3 && std::string_view(argv[2])=="death");std::ifstream input(argv[1]);require(input.good(),"Missing zone composition oracle");
        std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;
            std::vector<std::uint32_t> expected;std::uint32_t word;while(row>>word) expected.push_back(word);
            run(scenario,expected,combat,portals);++cases;
        }
        require(cases==256,"Incomplete zone composition fixture");std::cout<<cases<<(combat?" original Death/obelisk/corpse/pool sequences passed\n":" original real-zone/obelisk/pool sequences passed\n");return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
