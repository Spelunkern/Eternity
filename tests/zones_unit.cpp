#include "eternity/game/zones.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Folder {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("eternity-zones-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Folder() {
        std::filesystem::create_directory(path);std::array<char,33> bytes{};bytes[0]=1;
        std::ofstream file(path/"0.svmap",std::ios::binary);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));require(file.good(),"Cannot write temporary map fixture");
    }
    ~Folder() {std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
struct TestZone final:RuntimeZone {
    std::vector<std::string>& events;
    explicit TestZone(std::vector<std::string>& events_):events(events_) {}
    ~TestZone() override {events.push_back("D"+std::to_string(id));}
};
struct Services final:ZoneServices {
    std::vector<std::string> events;unsigned int failure=0,flags=0;bool boot=true;
    std::unique_ptr<RuntimeZone> allocate() override {
        if((boot && failure==1) || (!boot && failure==6)) return {};return std::make_unique<TestZone>(events);
    }
    bool initialize_map(RuntimeZone& zone,const ConfiguredMap& map) override {
        require(zone.map==&map,"Zone map binding was not installed before preparation");events.push_back("M"+std::to_string(zone.id));
        return !((boot && failure==2) || (!boot && failure==4));
    }
    bool initialize_objects(RuntimeZone& zone) override {
        events.push_back("O"+std::to_string(zone.id));zone.expiry_tick=12345;
        return !((boot && failure==3) || (!boot && failure==5));
    }
    void release_objects(RuntimeZone& zone) override {events.push_back("R"+std::to_string(zone.id));}
    void enqueue_wait(RuntimeZone& zone) override {events.push_back("Q"+std::to_string(zone.id)+":"+std::to_string(zone.owner)+":"+std::to_string(zone.expiry_tick));}
    ZoneLocalTime local_time() override {events.emplace_back("T");return {11,1,static_cast<std::uint16_t>(flags&32?2:1),21};}
};
std::array<std::uint32_t,7> stats(const ZoneStatistics& value) {return {value.house_current,value.rank_current,value.house_peak,value.rank_peak,value.party_current,value.party_peak,value.total_created};}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected zones oracle path");std::ifstream input(argv[1]);require(input.good(),"Cannot open zones oracle");
        Folder folder;std::string line;unsigned int checked=0,windows=0,houses=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
            if(line[0]=='W') {
                char kind=0;row>>kind;MapSettings map;for(auto& time:map.open_times) row>>time.day>>time.hour>>time.keep_hours;
                ZoneLocalTime time;bool opened=false;std::uint32_t stamp=0;row>>time.month>>time.day>>time.weekday>>time.hour>>opened>>stamp;
                const auto actual=guild_rank_window(map,time);require(actual.has_value()==opened && actual.value_or(0)==stamp,"Original guild-rank opening window differs at case "+std::to_string(windows));++windows;
            } else if(line[0]=='H') {
                char kind=0;unsigned int rank=0,acquired=0,enabled=0;bool expected=false;row>>kind>>rank>>acquired>>enabled>>expected;
                require(guild_house_enabled({0,static_cast<std::uint8_t>(rank),static_cast<std::uint8_t>(acquired),static_cast<std::uint8_t>(enabled)})==expected,"Original guild-house admission differs");++houses;
            } else {
                unsigned int mode=0,flags=0,failure=0;bool boot_expected=false;std::uint32_t serial_expected=0;row>>mode>>flags>>failure>>boot_expected>>serial_expected;
                std::array<std::uint32_t,7> expected_stats{};for(auto& value:expected_stats) row>>value;
                std::array<std::array<std::uint32_t,4>,3> expected_results{};for(auto& result:expected_results) for(auto& value:result) row>>value;
                std::size_t event_count=0;row>>event_count;std::vector<std::string> expected_events(event_count);for(auto& event:expected_events) row>>event;
                MapConfiguration config;config.maps.resize(1);config.maps[0].filename="0.svmap";config.maps[0].create_type=static_cast<std::int32_t>(mode);config.maps[0].open_times[0]={1,21,1};
                const auto maps=ServerMaps::load(config,folder.path);Services services;services.flags=flags;services.failure=failure;
                const std::uint32_t seed=flags&64?UINT32_MAX:0;ZoneRegistry registry(maps,services,seed,{seed,seed,seed,seed,seed,seed,seed});
                const bool initialized=registry.initialize();const auto context=" at zone vector "+std::to_string(checked)+" (mode "+std::to_string(mode)+", flags "+std::to_string(flags)+", failure "+std::to_string(failure)+")";
                require(initialized==boot_expected,"Original zone initialization result differs"+context);std::array<std::array<std::uint32_t,4>,3> results{};
                if(initialized) {
                    services.boot=false;ZoneUserAccess user;user.permission=std::array<std::uint8_t,4>{0,10,11,255}[flags&3];user.instance_owner=flags&4?77:0;
                    if(flags&8) user.party_id=flags&256?0:42;
                    if(flags&16) user.guild=ZoneGuildAccess{flags&256?0U:91U,static_cast<std::uint8_t>(flags&32?30:31),1,1};
                    user.last_rank_entry=flags&(32|128)?UINT32_C(0x1501010b):0;
                    for(std::size_t query=0;query<3;++query) {
                        if(query==2) {if(user.instance_owner) ++user.instance_owner;if(user.party_id) ++*user.party_id;if(user.guild) ++user.guild->id;}
                        if(const auto* zone=registry.select(0,user)) results[query]={1,zone->id,zone->owner,zone->expiry_tick};
                    }
                }
                require(registry.serial()==serial_expected && stats(registry.statistics())==expected_stats,"Original zone serial/statistics differ"+context);
                require(results==expected_results,"Original zone selection identity/owner/expiry differs"+context);
                if(services.events!=expected_events) {
                    std::ostringstream detail;for(const auto& event:services.events) detail<<event<<' ';detail<<"versus ";for(const auto& event:expected_events) detail<<event<<' ';
                    throw std::runtime_error("Original zone operation order differs"+context+": "+detail.str());
                }
                ++checked;
            }
            std::string extra;require(!(row>>extra),"Unexpected zone oracle data");
        }
        require(checked==1053 && windows==2240 && houses==36,"Zone oracle coverage changed");
        MapConfiguration config;config.maps.resize(2);for(auto& map:config.maps) map.filename="0.svmap";config.maps[1].id=1;config.maps[1].create_type=1;
        const auto maps=ServerMaps::load(config,folder.path);Services services;ZoneRegistry registry(maps,services);require(registry.initialize(),"Combined registry initialization failed");services.boot=false;
        const auto* permanent=registry.select(0,{});const auto* first=registry.select(1,{});const auto* second=registry.select(1,{});
        require(permanent && first && second && permanent->id==1 && first->id==2 && second->id==3 && first!=second && !registry.select(2,{}),"Per-map pooling or permanent selection changed");
        require(services.events==std::vector<std::string>{"M1","O1","M2","O2","R2","O2","Q2:0:0","M3","O3","Q3:0:0"},"Combined registry preparation/pool reuse order changed");
        std::cout<<checked<<" original zone lifecycles, "<<windows<<" rank windows and "<<houses<<" house predicates passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
