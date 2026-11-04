#include "eternity/game/obelisk_runtime_maps.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using eternity::net::Packet;
namespace {
void require(bool b,const char* message) {if(!b) throw std::runtime_error(message);}
void put(std::vector<std::uint8_t>& out,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i) out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void real(std::vector<std::uint8_t>& out,float f) {put(out,std::bit_cast<std::uint32_t>(f));}
void write_maps(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::vector<std::uint8_t> bytes;put(bytes,81);bytes.resize(bytes.size()+(81*81+7)/8);
    for(unsigned i=0;i<4;++i) put(bytes,0);put(bytes,3);
    for(unsigned j=0;j<3;++j) {real(bytes,20.25F+static_cast<float>(j)*10);real(bytes,99);real(bytes,30.5F+static_cast<float>(j)*10);put(bytes,j==2?1002:1001);put(bytes,0,2);put(bytes,60,2);put(bytes,0);for(unsigned i=0;i<3;++i) real(bytes,0);}
    put(bytes,0);put(bytes,0);
    for(unsigned i=0;i<2;++i) {std::ofstream out(root/(std::to_string(i)+".svmap"),std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
}
MobRuntimeTemplates templates() {
    eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};
    for(unsigned i=1;i<4096;++i) rows.rows.push_back({std::to_string(i),i%7==0?"0":"1","0","1","0","0","0","0","0","0","0","0","0","0","0"});return MobRuntimeTemplates::load(rows);
}
MobTargetingBindingCatalog catalog() {
    eternity::storage::ResultSet rows,skills;rows.columns.resize(69);skills.columns.resize(82);
    for(unsigned i=1;i<4096;++i) {eternity::storage::Row row(69,std::string("0"));row[0]=std::to_string(i);row[1]="Fixture";row[2]=i%7==0?"0":"1";row[8]="1";rows.rows.push_back(std::move(row));}
    return MobTargetingBindingCatalog::load(4095,0,rows,skills);
}
struct ZoneServices final:ZoneMobServices {
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected actor service in map ownership fixture");}
    std::uint32_t tick_ms() override {unexpected();}
    void send_cell_view(CellCoordinates,const Packet&) override {unexpected();}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Packet&,std::optional<std::uint8_t>) override {unexpected();}
    void lua_init(MobRuntime&) override {unexpected();}
};
struct Services final:ObeliskRuntimeMapsServices {
    ServerMaps& maps;ZoneServices zone_services;std::vector<std::unique_ptr<ZoneMobs>> zones;
    int scenario=0;bool resetting=false,dropped=false;std::vector<std::uint32_t> trace;
    explicit Services(ServerMaps& m):maps(m),zones(m.size()) {}
    std::size_t index(const ConfiguredMap& map) const noexcept {for(std::size_t i=0;i<maps.size();++i) if(&maps.all()[i]==&map) return i;std::terminate();}
    ZoneMobs* primary_zone(const ConfiguredMap& map) const noexcept override {return zones[index(map)].get();}
    void packet(unsigned tag,const Packet& p) {trace.push_back(tag);trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());}
    void database(const Packet& p) override {packet(3,p);if(resetting&&scenario%11==0&&!dropped) {dropped=true;zones[0].reset();}}
    void world_broadcast(const Packet& p) override {packet(4,p);}
    void map_broadcast(const ConfiguredMap& map,ZoneMobs& zone,const Packet& p) override {const auto i=index(map);require(zones[i].get()==&zone,"Wrong actual primary-zone route");trace.push_back(2);trace.push_back(static_cast<std::uint32_t>(i));trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());}
    void user(unsigned map,const Packet& p) {trace.push_back(5);trace.push_back(map);trace.push_back(static_cast<std::uint32_t>(p.size()));trace.insert(trace.end(),p.begin(),p.end());}
};
struct User final:ObeliskRuntimeUserPublication {Services& s;unsigned map;User(Services& s_,unsigned i):s(s_),map(i){}void send_user(const Packet& p) override {s.user(map,p);}};
struct Clock final:ObeliskRuntimeOwnershipClock {std::uint32_t packed_now() override {ZoneServices::unexpected();}std::uint32_t seconds_between(std::uint32_t,std::uint32_t) override {ZoneServices::unexpected();}};
struct World final:ObeliskRuntimeWorld {
    ObeliskRuntimeMaps& owner;std::size_t map;World(ObeliskRuntimeMaps& o,std::size_t i):owner(o),map(i){}
    std::string_view mob_name(const MobRuntime&) override {ZoneServices::unexpected();}
    void remove_by_id(std::uint32_t) override {ZoneServices::unexpected();}
    void send_change(std::size_t i) override {owner.publish_change(map,i);}
    void add_kill_count(std::int32_t,std::int32_t) override {ZoneServices::unexpected();}
    bool kill_count_status14(std::int32_t) override {ZoneServices::unexpected();}
    void subtract_kill_count(std::int32_t,std::int32_t) override {ZoneServices::unexpected();}
    void broadcast(const Packet& p) override {owner.broadcast(p);}
    void game_log(const Packet&) override {ZoneServices::unexpected();}
    void database(const Packet& p) override {owner.database(p);}
    void update_portal_links() override {owner.update_portals();}
    MobRuntime* allocate(bool) override {ZoneServices::unexpected();}
    void prepare_companion(MobRuntime&) override {ZoneServices::unexpected();}
    void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&) override {ZoneServices::unexpected();}
    void enter(MobRuntime&) override {ZoneServices::unexpected();}
    std::int32_t random() override {ZoneServices::unexpected();}
};
void accessor_checks(ServerMaps& maps) {
    require(!maps.mutable_portal_country(65535,0)&&!maps.mutable_portal_country(0,SIZE_MAX),"Missing map/slot did not return null");
    for(std::size_t i=0;i<maps.size();++i) {
        const auto& map=maps.all()[i];require(!maps.mutable_portal_country(static_cast<std::uint16_t>(i),map.definition.portals.size()),"One-past portal slot admitted");
        for(std::size_t j=0;j<map.definition.portals.size();++j) {auto* c=maps.mutable_portal_country(static_cast<std::uint16_t>(i),j);const auto& p=map.definition.portals[j];require(c==&p.country,"Portal country is not actual retained storage");const auto old=*c;*c=3;require(maps.find(static_cast<std::uint16_t>(i))->definition.portals[j].country==3&&p.base_country==old,"Mutation not visible or base country changed");*c=old;}
    }
}
void run(int scenario,const std::vector<std::uint32_t>& expected,const std::filesystem::path& root,const MobRuntimeTemplates& runtime,MobTargetingBindingCatalog& full) {
    auto maps=ServerMaps::load(MapConfiguration::decode("[BASE]\nMapCount=2\n"),root);accessor_checks(maps);Services services(maps);services.scenario=scenario;
    for(std::size_t i=0;i<2;++i) if(scenario%static_cast<int>(3+i)) services.zones[i]=std::make_unique<ZoneMobs>(maps.all()[i].definition.geometry(),services.zone_services);
    std::ostringstream ini;for(unsigned mi=0;mi<2;++mi) {ini<<"[zone_"<<mi<<"]\n";for(unsigned row=0;row<2;++row) ini<<"[Obelisk_"<<row+1<<"]=("<<row+1.25<<",2.5,"<<row+3.75<<"),(1,2,3),"<<1+(scenario+static_cast<int>(mi+row))%3<<'\n';}
    ini<<"[Portal_10,0,1001]\n[light]=0,Obelisk_1,1,Obelisk_2\n[fury]=0,Obelisk_2\n[Portal_11,1,1002]\n[light]=1,Obelisk_1\n[fury]=0,Obelisk_2,1,Obelisk_2\n";
    for(unsigned i=1;i<=3;++i) full.mutable_mob_record(i)[0x7c]=std::byte{0};
    ObeliskRuntimeConfigurationState state;state.next_id=scenario%2?0xfffffffdU:0x123;state.maximum_kill_count={1000,-45};
    auto owner=bind_obelisk_runtime_maps(maps,runtime,full,ini.str(),state,services);Clock clock;
    const auto snapshot=[&] {
        auto& t=services.trace;t.insert(t.end(),{90,state.next_id,static_cast<std::uint32_t>(state.total),static_cast<std::uint32_t>(state.kill_count_add[0]),static_cast<std::uint32_t>(state.kill_count_add[1])});
        for(const auto& map:maps.all()) for(const auto& p:map.definition.portals) t.push_back(static_cast<std::uint32_t>(p.country));
        for(const auto& p:owner->portals()) {t.push_back(p.open[0]);t.push_back(p.open[1]);}
        for(std::size_t mi=0;mi<2;++mi) for(const auto& o:owner->records(mi)) {t.push_back(static_cast<std::uint32_t>(o.country));t.push_back(static_cast<std::uint32_t>(o.last_destroy_country));t.push_back(o.respawn.next);}
        for(unsigned i=1;i<=3;++i) t.push_back(std::to_integer<std::uint32_t>(full.mob_record(i)[0x7c]));
    };
    const auto scene=[&] {for(unsigned mi=0;mi<2;++mi) {User user(services,mi);owner->send_user(mi,user);}};
    snapshot();owner->restore(0x1000000aU+static_cast<unsigned>(scenario%2),static_cast<std::uint32_t>(scenario/2%5-1),0,clock);snapshot();scene();
    owner->restore(static_cast<std::uint32_t>(scenario/5%2)*65536U+static_cast<std::uint32_t>(scenario/7%2+1),static_cast<std::uint32_t>(scenario%3)|((static_cast<std::uint32_t>(scenario/3%3)+1U)<<4),0,clock);
    const auto map=static_cast<std::size_t>(scenario/9%2);World world(*owner,map);services.resetting=true;
    owner->reset(map,[&]{const auto tick=(scenario%2?0xffffff00U:100U)+static_cast<std::uint32_t>(scenario);services.trace.insert(services.trace.end(),{6,tick});return tick;},world);
    services.resetting=false;snapshot();scene();owner->persist();snapshot();
    if(services.trace!=expected) {std::size_t i=0;while(i<services.trace.size()&&i<expected.size()&&services.trace[i]==expected[i]) ++i;std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<services.trace.size()?std::to_string(services.trace[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';throw std::runtime_error("Actual ServerMaps ownership/reset/scene comparison failed");}
}
void asset(const std::filesystem::path& root,const MobRuntimeTemplates& runtime,MobTargetingBindingCatalog& full) {
    auto maps=ServerMaps::load(MapConfiguration::load(root/"Map.ini"),root/"Map");accessor_checks(maps);Services services(maps);
    std::ifstream in(root/"Obelisk.ini",std::ios::binary);require(in.good(),"Missing recovered obelisk asset");const std::string text((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    ObeliskRuntimeConfigurationState state;state.maximum_kill_count={1000,-45};auto owner=bind_obelisk_runtime_maps(maps,runtime,full,text,state,services);
    std::size_t records=0;for(std::size_t i=0;i<maps.size();++i) {records+=owner->records(i).size();auto& view=owner->runtime_map(i);require(view.blocked_tiles.data()==maps.all()[i].definition.tiles.data(),"Terrain was copied instead of borrowed");}
    require(records==9,"Recovered ordinary records not bound to real maps");std::cout<<maps.size()<<" recovered maps, "<<records<<" obelisks, "<<owner->portals().size()<<" retained portal links assembled\n";
}
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Expected map composition fixture, private asset root, and ignored work directory");write_maps(argv[3]);auto runtime=templates();auto full=catalog();
        std::ifstream input(argv[1]);require(input.good(),"Missing map composition oracle");std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);run(scenario,expected,argv[3],runtime,full);++count;}
        require(count==64,"Incomplete actual map composition fixture");asset(argv[2],runtime,full);std::cout<<count<<" original actual-map ownership/reset/scene compositions passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
