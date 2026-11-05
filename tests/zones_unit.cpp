#include "eternity/game/zones.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/world_thread_zones.hpp"
#include "eternity/game/zone_users.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>
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
    std::vector<std::string> events;unsigned int failure=0,flags=0;bool boot=true,fail_release=false;std::function<void(RuntimeZone&)> on_enqueue;
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
    void release_objects(RuntimeZone& zone) override {events.push_back("R"+std::to_string(zone.id));if(fail_release){fail_release=false;throw std::runtime_error("retry release");}}
    void enqueue_wait(RuntimeZone& zone) override {events.push_back("Q"+std::to_string(zone.id)+":"+std::to_string(zone.owner)+":"+std::to_string(zone.expiry_tick));if(on_enqueue)on_enqueue(zone);}
    ZoneLocalTime local_time() override {events.emplace_back("T");return {11,1,static_cast<std::uint16_t>(flags&32?2:1),21};}
};
struct Scene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::logic_error("unexpected damage reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("unexpected mob");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("unexpected taunt");}
    ZoneUsers* user_zone() override {return nullptr;}
    UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("unexpected damage");}
};
struct AdmissionServices final:WorldAdmissionServices {
    RuntimeZone* zone=nullptr;
    void packets(WorldAdmissionUser&) override {throw std::logic_error("unexpected packets");}
    RuntimeZone* select_zone(WorldAdmissionUser&) override {return zone;}
    void register_world(WorldAdmissionUser&) override {}
    bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override {throw std::logic_error("unexpected entry");}
    void close(WorldAdmissionUser&,std::uint32_t) override {throw std::logic_error("unexpected close");}
    void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override {throw std::logic_error("unexpected diagnostic");}
    std::optional<std::uint32_t> rank_window(RuntimeZone&) override {throw std::logic_error("unexpected rank");}
    bool lock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild");}
    void unlock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild");}
};
struct ActiveList final:ZoneActiveListRemoval {
    std::vector<RuntimeZone*> active;unsigned removals=0;
    void remove_active(RuntimeZone& zone) noexcept override {
        auto it=std::find(active.begin(),active.end(),&zone);if(it==active.end())std::terminate();active.erase(it);++removals;
    }
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
        for(int mode=1;mode<=6;++mode) {
            MapConfiguration instance_config;instance_config.maps.resize(1);auto& settings=instance_config.maps[0];settings.filename="0.svmap";settings.create_type=mode;settings.expire_ms=10;
            const auto instance_maps=ServerMaps::load(instance_config,folder.path);Services instance_services;ZoneRegistry instances(instance_maps,instance_services);require(instances.initialize(),"Instance bootstrap");instance_services.boot=false;
            ZoneUserAccess user;user.permission=0;user.instance_owner=42;user.party_id=42;user.guild=ZoneGuildAccess{42,0,1,1};
            auto* zone=instances.select(0,user);require(zone,"Instance selection");zone->users=std::make_unique<ZoneUsers>(1,1,0);
            ActiveList list;list.active.push_back(zone);AdmissionServices admission_services;admission_services.zone=zone;WorldAdmission admission(admission_services);
            require(instances.expire_instance(*zone,admission,100,list)==InstanceExpiryResult::waiting && zone->expiry_tick==110,"Instance deadline arm");
            Scene scene;ItemDefinitions items;EnchantValues enchants;MobAttackTemplates mob_templates;Actor actor(123,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{items,enchants,mob_templates});
            zone->users->attach(actor,0,0,{});require(instances.expire_instance(*zone,admission,112,list)==InstanceExpiryResult::occupied && zone->expiry_tick==110,"Actor protects instance");require(zone->users->detach(actor.id()),"Actor detach");
            WorldAdmissionUser waiting;waiting.user_id=10;admission.track_selection(waiting);require(admission.wait(waiting),"Queue wait");admission.update_packets();
            require(instances.expire_instance(*zone,admission,115,list)==InstanceExpiryResult::occupied && zone->expiry_tick==110 && list.removals==0,"Waiter protects instance");
            admission.untrack(waiting);
            if(mode==3) {
                instance_services.fail_release=true;bool failed=false;try{(void)instances.expire_instance(*zone,admission,115,list);}catch(const std::runtime_error&){failed=true;}
                require(failed && list.removals==1,"Retirement records failed release");bool blocked=false;try{(void)instances.select(0,user);}catch(const std::logic_error&){blocked=true;}require(blocked,"Retiring instance blocks selections");
            }
            require(instances.expire_instance(*zone,admission,115,list)==InstanceExpiryResult::retired && list.removals==1 && list.active.empty(),"Retirement and active-list removal");
            const auto st=instances.statistics();
            if(mode==4) require(st.party_current==UINT32_MAX && st.total_created==UINT32_MAX,"Original guild counter underflow");
            else require(st.party_current==0 && st.rank_current==0 && st.house_current==0 && st.total_created==0,"Original instance counters");
            auto* recycled=instances.select(0,user);require(recycled==zone && recycled->expiry_tick==0,"Original instance pool reuse");
        }
        for(const std::uint32_t delay:{0U,10U,32U}) {
            MapConfiguration instance_config;instance_config.maps.resize(1);instance_config.maps[0].filename="0.svmap";instance_config.maps[0].create_type=1;instance_config.maps[0].expire_ms=delay;
            const auto instance_maps=ServerMaps::load(instance_config,folder.path);Services instance_services;ZoneRegistry instances(instance_maps,instance_services);require(instances.initialize(),"Expiry bootstrap");instance_services.boot=false;
            auto* zone=instances.select(0,{});zone->users=std::make_unique<ZoneUsers>(1,1,0);ActiveList list;list.active.push_back(zone);AdmissionServices admission_services;WorldAdmission admission(admission_services);
            auto result=instances.expire_instance(*zone,admission,0xfffffff0U,list);
            if(delay==10) {require(result==InstanceExpiryResult::waiting && zone->expiry_tick==0xfffffffaU,"Deadline arm without wrap");result=instances.expire_instance(*zone,admission,0xfffffffaU,list);}
            if(delay==32) {require(result==InstanceExpiryResult::waiting && zone->expiry_tick==16,"DWORD deadline wrap");result=instances.expire_instance(*zone,admission,0xfffffff1U,list);}
            require(result==InstanceExpiryResult::retired && list.removals==1,"Immediate and unsigned wrapped expiry");
        }
        {
            MapConfiguration cfg;cfg.maps.resize(2);for(auto& entry:cfg.maps)entry.filename="0.svmap";cfg.maps[1].id=1;cfg.maps[1].create_type=1;cfg.maps[1].expire_ms=0;
            const auto queue_maps=ServerMaps::load(cfg,folder.path);Services queue_services;WorldThreadZones thread;queue_services.on_enqueue=[&](RuntimeZone& zone){thread.enqueue_wait(zone);};
            ZoneRegistry queue_registry(queue_maps,queue_services);require(queue_registry.initialize(),"Queue bootstrap");queue_services.boot=false;
            auto* first_primary=queue_registry.select(0,{});thread.add_active(*first_primary);
            auto* first_dynamic=queue_registry.select(1,{});first_dynamic->users=std::make_unique<ZoneUsers>(1,1,0);
            require(thread.active_count()==1 && thread.pending_count()==1 && thread.promote(0)==1 && thread.next_promotion()==100,"Initial active promotion");
            auto* second_dynamic=queue_registry.select(1,{});second_dynamic->users=std::make_unique<ZoneUsers>(1,1,0);
            bool duplicate=false;try{thread.enqueue_wait(*second_dynamic);}catch(const std::logic_error&){duplicate=true;}require(duplicate,"Duplicate queue entry rejected");
            require(thread.promote(99)==0 && thread.pending_count()==1 && thread.promote(100)==1 && thread.has_active_instances() && !thread.primary_shutdown_ready(),"Promotion deadline equality and active-instance shutdown fence");
            AdmissionServices queue_admission_services;WorldAdmission queue_admission(queue_admission_services);std::vector<std::uint32_t> visited;
            thread.visit_active([&](RuntimeZone& zone){visited.push_back(zone.id);if(&zone!=first_primary)require(queue_registry.expire_instance(zone,queue_admission,100,thread)==InstanceExpiryResult::retired,"Active instance retirement");});
            require(visited==std::vector<std::uint32_t>{first_primary->id,first_dynamic->id,second_dynamic->id} && thread.active_count()==1 && thread.primary_shutdown_ready(),"Live cursor survives current removal");
            require(thread.promote(0xfffffff0U)==0 && thread.next_promotion()==84,"Promotion DWORD wrap");auto* recycled=queue_registry.select(1,{});recycled->users=std::make_unique<ZoneUsers>(1,1,0);
            require(!thread.primary_shutdown_ready() && thread.promote(83)==0 && thread.promote(84)==1,"Pending instance blocks shutdown and promotes at wrap deadline");
            require(queue_registry.expire_instance(*recycled,queue_admission,84,thread)==InstanceExpiryResult::retired && thread.primary_shutdown_ready(),"Recycled instance leaves active owner");
            thread.clear_drained_primaries();require(thread.active_count()==0,"Drained primary list cleared");
        }
        std::cout<<checked<<" original zone lifecycles, "<<windows<<" rank windows and "<<houses<<" house predicates passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
