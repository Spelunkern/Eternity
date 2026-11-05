#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "mob_damage_fixture.hpp"
#include "eternity/game/world_shutdown_poller.hpp"
#include "eternity/game/world_db_update.hpp"
#include "eternity/game/world_db_driver.hpp"
#include "eternity/game/world_native_send_services.hpp"
#include "eternity/game/world_retained_send_ports.hpp"
#include "eternity/game/world_retained_global_send.hpp"
#include "eternity/game/world_retained_session_lost.hpp"
#include "eternity/game/world_send_composition.hpp"
#include "eternity/game/world_kill_count_state.hpp"
#include "eternity/game/world_kill_count_bootstrap.hpp"
#include "eternity/game/world_kill_count_db_owner.hpp"
#include "eternity/game/world_db_agent_save.hpp"
#include "eternity/game/world_db_obelisks.hpp"
#include "eternity/game/world_db_agent_world.hpp"
#include "eternity/game/world_db_agent_save_driver.hpp"
#include "eternity/game/world_db_agent_disconnect.hpp"
#include "eternity/game/world_kill_count_entry.hpp"
#include "eternity/game/world_kill_count_composition.hpp"
#include "eternity/game/world_retained_kill_count_zone.hpp"
#include "eternity/game/world_kill_count_obelisk_binding.hpp"
#include "eternity/game/world_users.hpp"
#include "eternity/game/user_view_world_channel.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>
using namespace eternity::game;
namespace {
struct KillBootstrapDatabase {
    struct Cursor {
        eternity::storage::SqlCursorRead result;eternity::storage::TypedSqlRow row;
        eternity::storage::SqlCursorRead next(eternity::storage::TypedSqlRow& out){out=row;return result;}
        const eternity::storage::SqlError* error()const{return nullptr;}
    };
    Cursor value;bool called=false;
    Cursor cursor(std::string_view sql,std::span<const eternity::storage::Parameter> parameters,std::span<const eternity::storage::SqlColumnType> columns) {
        if(sql!="EXEC usp_Read_GodBless_R" || !parameters.empty() || columns.size()!=2 ||
           columns[0]!=eternity::storage::SqlColumnType::long_integer || columns[1]!=eternity::storage::SqlColumnType::long_integer)
            throw std::runtime_error("Invalid kill-count SQL request");
        called=true;return value;
    }
};
struct KillSaveDatabase {
    std::vector<std::int32_t> results;unsigned calls=0;std::array<std::int64_t,2> received{};bool fail_sql=false;
    std::vector<eternity::storage::ResultSet> query(std::string_view sql,std::span<const eternity::storage::Parameter> parameters) {
        if(sql!="DECLARE @Saved int; EXEC @Saved=usp_Save_GodBless_E ?,?; SELECT @Saved;" || parameters.size()!=2)throw std::runtime_error("Invalid kill-count save request");
        received={std::get<std::int64_t>(parameters[0]),std::get<std::int64_t>(parameters[1])};++calls;
        if(fail_sql)throw eternity::storage::SqlError("fixture SQL failure","HY000",1);
        return {{{"Saved"},{{std::to_string(results.at(calls-1))}}}};
    }
};
struct KillOwnerDatabase:KillBootstrapDatabase,KillSaveDatabase {};
struct KeepDatabase {
    struct Cursor {std::vector<eternity::storage::TypedSqlRow> rows;std::size_t index=0;
        eternity::storage::SqlCursorRead next(eternity::storage::TypedSqlRow& row){if(index==rows.size())return eternity::storage::SqlCursorRead::end;row=rows[index++];return eternity::storage::SqlCursorRead::row;}
        const eternity::storage::SqlError* error()const{return nullptr;}};
    std::vector<std::string> operations;bool fail=false;
    Cursor cursor(std::string_view sql,std::span<const eternity::storage::Parameter> parameters,std::span<const eternity::storage::SqlColumnType> columns) {
        if(sql!="EXEC usp_Read_Keep_R" || !parameters.empty() || columns.size()!=3 || columns[1]!=eternity::storage::SqlColumnType::tiny_integer)throw std::runtime_error("Invalid keep read");
        return {{{7,1,100}}};
    }
    std::vector<eternity::storage::ResultSet> query(std::string_view sql,std::span<const eternity::storage::Parameter> parameters) {
        if(parameters.size()!=3)throw std::runtime_error("Invalid keep write");operations.emplace_back(sql);
        if(fail)throw eternity::storage::SqlError("keep failure","HY000",1);return {};
    }
};
struct AgentWorldDatabase {
    std::vector<std::string> writes;
    KeepDatabase::Cursor cursor(std::string_view sql,std::span<const eternity::storage::Parameter> parameters,std::span<const eternity::storage::SqlColumnType> columns) {
        if(!parameters.empty())throw std::runtime_error("Unexpected DB parameters");
        if(sql=="EXEC usp_Read_GodBless_R" && columns.size()==2)return {{{12,70}}};
        if(sql=="EXEC usp_Read_Keep_R" && columns.size()==3)return {{{7,1,100}}};
        throw std::runtime_error("Unexpected DB query");
    }
    std::vector<eternity::storage::ResultSet> query(std::string_view sql,std::span<const eternity::storage::Parameter>) {
        writes.emplace_back(sql);
        if(sql.starts_with("DECLARE @Saved"))return {{{"Saved"},{{"0"}}}};
        if(sql=="EXEC usp_Save_Keep_E ?,?,?")return {};
        throw std::runtime_error("Unexpected DB save");
    }
};
struct AgentCalendarFixture final:ObeliskRuntimeCalendarPlatform {
    std::vector<unsigned> calls;
    ObeliskRuntimeCalendar local_calendar()override{calls.push_back(1);return {2024,1,0,2,0,0,0,0};}
    bool variant_day(const ObeliskRuntimeCalendar& value,double& output)override{calls.push_back(value.day==1?2:3);output=value.day==1?40000.0:40001.0;return true;}
};
struct AgentSaveDriverFixture {
    std::vector<unsigned>& events;bool fail=false;
    WorldDbAgentSaveResult periodic(std::uint32_t now){events.push_back(2);if(fail)throw std::runtime_error("DB save driver fixture");check_tick(now);WorldDbAgentSaveResult result;result.due=true;result.saved={true,false,true};return result;}
    static void check_tick(std::uint32_t now){if(now!=1000)throw std::runtime_error("Wrong DB save tick");}
};
struct AgentDisconnectFixture {
    std::vector<unsigned>& events;
    WorldDbAgentSaveResult save_all(){events.push_back(2);WorldDbAgentSaveResult result;result.due=true;result.saved={true,false,true};return result;}
};
struct SavePhaseFixture {
    std::vector<unsigned>& events;unsigned id;bool ok=true,throws=false;
    bool save(){events.push_back(id);if(throws)throw std::runtime_error("save failure");return ok;}
};
void check(bool b,std::source_location at=std::source_location::current()){if(!b){std::cerr<<"Primary authority mismatch at "<<at.line()<<'\n';throw std::runtime_error("Primary authority mismatch at "+std::to_string(at.line()));}}
void put(std::vector<std::uint8_t>& out,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i) out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void real(std::vector<std::uint8_t>& out,float f) {put(out,std::bit_cast<std::uint32_t>(f));}
void write_maps(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::vector<std::uint8_t> bytes;put(bytes,81);bytes.resize(bytes.size()+(81*81+7)/8);
    put(bytes,0);put(bytes,0);put(bytes,1);for(float n:{10.F,0.F,10.F,11.F,0.F,11.F})real(bytes,n);put(bytes,1);put(bytes,1);put(bytes,1);put(bytes,0);put(bytes,3);
    for(unsigned j=0;j<3;++j) {real(bytes,20.25F+static_cast<float>(j)*10);real(bytes,99);real(bytes,30.5F+static_cast<float>(j)*10);put(bytes,j==2?1002:1001);put(bytes,0,2);put(bytes,60,2);put(bytes,0);for(unsigned i=0;i<3;++i) real(bytes,0);}
    put(bytes,0);put(bytes,0);
    for(unsigned i=0;i<2;++i) {std::ofstream out(root/(std::to_string(i)+".svmap"),std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
}
MobRuntimeTemplates templates() {
    eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};
    for(unsigned i=1;i<4;++i) rows.rows.push_back({std::to_string(i),i%7==0?"0":"1","0","1","0","0","0","0","0","0","0","0","0","0","0"});return MobRuntimeTemplates::load(rows);
}
MobTargetingBindingCatalog catalog() {
    eternity::storage::ResultSet rows,skills;rows.columns.resize(69);skills.columns.resize(82);
    for(unsigned i=1;i<4;++i) {eternity::storage::Row row(69,std::string("0"));row[0]=std::to_string(i);row[1]="Fixture";row[2]=i%7==0?"0":"1";row[8]="1";rows.rows.push_back(std::move(row));}
    eternity::storage::Row skill(82,std::string("0"));skill[0]="1";skill[2]="S";skill[16]="3";skill[17]="99";skills.rows.push_back(skill);
    return MobTargetingBindingCatalog::load(3,1,rows,skills);
}

struct SkillFactory:MobAppliedSkillPoolServices{std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct Stats{unsigned zones=0,destroyed=0,objects=0,released=0,ai=0,binds=0,db=0,world=0,map=0,entries=0;};
struct Zone final:RuntimeZone{std::shared_ptr<Stats> stats;std::function<void(RuntimeZone&)> cleanup;Zone(std::shared_ptr<Stats> s,std::function<void(RuntimeZone&)> fn):stats(std::move(s)),cleanup(std::move(fn)){++stats->zones;}~Zone(){cleanup(*this);++stats->destroyed;}};
struct Frame;
struct Backend:ZoneServices,MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,ZoneMobServices,MobRemovalOwners{
    Frame& frame;explicit Backend(Frame& f):frame(f){}
    bool omit_mobs=false,fail_objects=false,fail_release=false;std::function<void()> on_entry,on_initialize;
    std::unique_ptr<RuntimeZone> allocate()override;
    bool initialize_map(RuntimeZone&,const ConfiguredMap&)override;
    bool initialize_objects(RuntimeZone&)override;
    void release_objects(RuntimeZone&)override;
    void enqueue_wait(RuntimeZone&)override{throw std::logic_error("Unexpected dynamic selection");}
    ZoneLocalTime local_time()override{throw std::logic_error("Unexpected local calendar");}
    std::unique_ptr<MobRuntime> construct()override;
    std::uint32_t unix_time()override{return 123456;}
    MobDamagePool& damage_pool()override;
    void release_compiled_ai(MobRuntime&)override;
    void release_stack_ai(MobRuntime&)override{throw std::logic_error("Unexpected VM stack");}
    void close_lua(void*)override{throw std::logic_error("Unexpected VM close");}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override;
    std::uint32_t tick()noexcept override{return 77;}
    void log(const MobAppliedSkill&,std::string_view,std::uint8_t)noexcept override{std::terminate();}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
    std::uint32_t tick_ms()override{return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&)override;
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::logic_error("Unexpected buffer");}
    void lua_init(MobRuntime&)override{throw std::logic_error("Unexpected Lua entry");}
    void destroy_obelisk(MobRuntime&,std::int32_t)override{throw std::logic_error("Unexpected linked delegation");}
    void remove_obelisk_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked delegation");}
    void destroy_linked_group(std::int32_t)override{throw std::logic_error("Unexpected linked group");}
    void remove_linked_member(MobRuntime&,void*)override{throw std::logic_error("Unexpected linked member");}
};
struct Frame{
    std::shared_ptr<ServerMaps> maps;MobRuntimeTemplates runtime=templates();MobTargetingBindingCatalog full=catalog();
    std::shared_ptr<Stats> stats=std::make_shared<Stats>();MobDamageFixture damage;SkillFactory skills;MobAppliedSkillPool heap{skills};
    std::atomic<std::uint32_t> skill_ids{0};unsigned mob_ids=100;Backend backend{*this};MobAppliedSkillsRuntime binding{full,heap,skill_ids,backend};
    MicrosoftRandom random{17};MobInitPorts init{random,[]{return 77U;},[](MobRuntime&,std::uint8_t){throw std::logic_error("Unexpected compiled backend");return false;},[](MobRuntime&){throw std::logic_error("Unexpected Lua backend");}};
    ObeliskRuntimeConfigurationState state;MobAppliedSkillsObeliskMapsServices world{[](int,int){throw std::logic_error("Unexpected KC");},[](int,int){throw std::logic_error("Unexpected KC");},[](int)->bool{throw std::logic_error("Unexpected KC");},[](const eternity::net::Packet& p){check(p.size()>=48);},backend};
    ObeliskRuntimeMaps* records=nullptr;MobAppliedSkillsObeliskMaps* router=nullptr;MobPool* pool_view=nullptr;
    std::vector<std::unique_ptr<MobPopulation>> retired_population;
    std::vector<std::unique_ptr<LinkedMobPopulation>> retired_linked;
    std::unique_ptr<MobPool> pool; // Destroy actors BEFORE retained source/services.
    explicit Frame(std::shared_ptr<ServerMaps> m):maps(std::move(m)){pool=std::make_unique<MobPool>(mob_ids,backend,binding);pool_view=pool.get();}
};
std::unique_ptr<RuntimeZone> Backend::allocate(){return std::make_unique<Zone>(frame.stats,[this](RuntimeZone& z){if(z.mobs)release_objects(z);});}
bool Backend::initialize_map(RuntimeZone& z,const ConfiguredMap& map){check(z.map==&map);z.environment=ZoneEnvironment::prepare(map);return true;}
bool Backend::initialize_objects(RuntimeZone& z){++frame.stats->objects;if(on_initialize)on_initialize();check(z.map);if(!z.environment)z.environment=ZoneEnvironment::prepare(*z.map);if(!omit_mobs)z.mobs=std::make_unique<ZoneMobs>(z.map->definition.geometry(),*this);z.mob_population=std::make_unique<MobPopulation>(z.map->definition,frame.runtime);z.linked_mobs=std::make_unique<LinkedMobPopulation>(std::span<const LinkedMobGroup>{});if(z.mobs)z.mob_population->initialize(*z.mobs,*frame.pool_view,frame.init,[&]{return z.linked_mobs->initialize(z.map->definition,*z.mobs,*frame.pool_view,frame.init,[&]{return unix_time();});});return !fail_objects;}
void Backend::release_objects(RuntimeZone& z){++frame.stats->released;if(fail_release){fail_release=false;throw std::runtime_error("Observed backend release failure");}if(z.mobs && z.mobs->size())z.release_mob_population(*frame.pool_view);
    if(z.mob_population)frame.retired_population.push_back(std::move(z.mob_population));if(z.linked_mobs)frame.retired_linked.push_back(std::move(z.linked_mobs));z.mobs.reset();z.environment.reset();}
std::unique_ptr<MobRuntime> Backend::construct(){auto m=std::make_unique<MobRuntime>();m->drop_random=ServerRandom(unix_time());return m;}
MobDamagePool& Backend::damage_pool(){return frame.damage.pool;}
void Backend::release_compiled_ai(MobRuntime& m){check(m.damage && m.has_applied_skills());if(frame.records)check(frame.records->records(0).size()==1);if(m.respawn)check(m.respawn->count>0);++frame.stats->ai;}
MobAppliedSkillsCatalogPorts& Backend::ports(MobRuntime&){++frame.stats->binds;return *this;}
void Backend::send_cell_view(CellCoordinates,const eternity::net::Packet& p){check(p.size()>=6 && p[1]==6);if(p[0]==1){++frame.stats->entries;if(on_entry)on_entry();}else check(p[0]==2);}
std::shared_ptr<MobPrimaryZones> root_for(const std::shared_ptr<Frame>& f){
    const auto weak=std::weak_ptr<Frame>(f);MobPrimaryZoneTransport transport;
    transport.database=[weak](const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2);++f->stats->db;};
    transport.world=[weak](const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2);++f->stats->world;};
    transport.map=[weak](const ConfiguredMap& m,ZoneMobs&,const eternity::net::Packet& p){auto f=weak.lock();check(f && p.size()>=2 && m.settings.create_type==0);++f->stats->map;};
    auto* address=f->pool.get();auto result=MobPrimaryZones::create(f->maps,std::shared_ptr<ZoneServices>(f,&f->backend),std::move(f->pool),std::move(transport));check(&result->mob_pool()==address);return result;
}
void attach(const std::shared_ptr<Frame>& f,const std::shared_ptr<MobPrimaryZones>& root){
    std::string ini="[zone_0]\n[Obelisk_1]=(20,0,20),(1,2,3),1\n[mob]=(30,0,30),1,(1,2,3),1\n[zone_1]\n[Obelisk_1]=(20,0,20),(1,2,3),2\n[mob]=(30,0,30),1,(1,2,3),1\n";
    auto records=bind_obelisk_runtime_maps(*f->maps,f->runtime,f->full,ini,f->state,*root);
    auto router=std::make_unique<MobAppliedSkillsObeliskMaps>(*f->maps,f->runtime,f->full,*records,*root,root->mob_pool(),f->init,f->state.kill_count_add,f->world);
    f->records=records.get();f->router=router.get();root->attach_obelisks(std::move(records),std::move(router));
}
}
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/world_frame_cadence.hpp"
#include "eternity/game/world_frame_driver.hpp"
#include "eternity/game/world_shutdown_coordinator.hpp"
#include "eternity/game/world_root.hpp"
#include "eternity/game/world_primary_mob_phase.hpp"
#include "eternity/game/item_pool.hpp"
#include <future>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include "eternity/game/vertical_mobs.hpp"
struct LinkedFixture:LinkedMobWorld {void remove_by_id(std::uint32_t) override {throw std::logic_error("unexpected removal");} void set_portal_country(const MapPortal&,std::int32_t) override {throw std::logic_error("unexpected portal");}};
struct RootBoundMobs final:ZoneMobServices {
    std::shared_ptr<Frame> frame;RuntimeZone& zone;
    RootBoundMobs(std::shared_ptr<Frame> f,RuntimeZone& z):frame(std::move(f)),zone(z) {}
    std::uint32_t tick_ms() override {return frame->backend.tick_ms();}
    void send_cell_view(CellCoordinates cell,const eternity::net::Packet& p) override {check(zone.map && zone.mobs);frame->backend.send_cell_view(cell,p);}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet& p,std::optional<std::uint8_t> priority) override {return frame->backend.allocate_broadcast(p,priority);}
    void lua_init(MobRuntime& mob) override {frame->backend.lua_init(mob);}
};
struct RootNpcBroadcast final:NpcBroadcast {
    unsigned& deliveries;explicit RootNpcBroadcast(unsigned& value):deliveries(value) {}
    void send_cell(CellCoordinates) override {++deliveries;}
};
struct RootOtherServices final:ZoneNpcServices,ZoneItemServices {
    NpcPool npc_pool;GroundItemPool item_pool;WorldCorpses corpses;unsigned npc_ids=0;std::vector<eternity::net::Packet> item_packets,npc_packets,npc_moves;unsigned npc_deliveries=0;std::function<void()> on_item_release;
    Npc* allocate() override {return npc_pool.allocate();}
    std::uint32_t next_id() override {return ++npc_ids;}
    std::uint32_t tick_ms() const noexcept override {return 77;}
    std::uint32_t tick_ms() override {return 77;}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("unexpected guard");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p) override {
        check(p.size()>=2);if(p[1]==4 && (p[0]==1 || p[0]==2))item_packets.push_back(p);
        else if(p[1]==14 && (p[0]==1 || p[0]==2))npc_packets.push_back(p);else throw std::logic_error("unexpected NPC/item send");
    }
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const eternity::net::Packet& p,std::optional<std::uint8_t> priority) override {
        check(p.size()==19 && p[0]==3 && p[1]==14 && priority==6);npc_moves.push_back(p);return std::make_unique<RootNpcBroadcast>(npc_deliveries);
    }
    void release(GroundItem& item) override {if(on_item_release)on_item_release();item_pool.release(item);}
};
struct RootCorpseServices final:ZoneCorpseServices {
    CorpsePool pool;
    Corpse* allocate() override {return pool.allocate();}
    void release(Corpse& corpse) override {pool.release(corpse);}
    void diagnostic(CorpseIssue,std::uint32_t) override {throw std::logic_error("unexpected corpse diagnostic");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {throw std::logic_error("unexpected corpse send");}
};
struct RootAdmissionServices final:WorldAdmissionServices {
    RuntimeZone* zone=nullptr;unsigned packets_seen=0,registered=0;std::function<void()> on_packet;
    void packets(WorldAdmissionUser&) override {++packets_seen;if(on_packet)on_packet();}
    RuntimeZone* select_zone(WorldAdmissionUser&) override {return zone;}
    void register_world(WorldAdmissionUser&) override {++registered;}
    bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override {throw std::logic_error("unexpected entry");}
    void close(WorldAdmissionUser&,std::uint32_t) override {throw std::logic_error("unexpected close");}
    void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override {throw std::logic_error("unexpected diagnostic");}
    std::optional<std::uint32_t> rank_window(RuntimeZone&) override {throw std::logic_error("unexpected rank window");}
    bool lock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild lock");}
    void unlock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild unlock");}
};
struct IdleFixture:VerticalMobsWorld {
    unsigned calls=0;std::uint32_t expected=5077;std::function<void()> on_idle;std::vector<ZoneMobs*> visited_zones;
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override {throw std::logic_error("unexpected configured AI");}
    MobAiConfiguredServices& configured_services(MobRuntime&) override {throw std::logic_error("unexpected configured services");}
    MobAiStateServices& stack_services(MobRuntime&) override {throw std::logic_error("unexpected stack services");}
    void release_failed_stack(MobRuntime&) override {throw std::logic_error("unexpected stack release");}
    void return_home(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected return home");}
    void ordinary_idle(MobRuntime& mob,std::uint32_t now) override {check(now==expected);++calls;visited_zones.push_back(mob.zone);if(on_idle)on_idle();}
};
struct PhaseFixture final:PrimaryMobPhaseServices {
    Frame& frame;IdleFixture& idle;LinkedMobWorld& linked;std::shared_ptr<void> owner;bool omit_idle_pin=false,mismatch_random=false;MicrosoftRandom alternate_random{19};std::vector<std::string> events;std::function<void(RuntimeZone&)> on_traps;
    PhaseFixture(Frame& f,IdleFixture& i,LinkedMobWorld& l,std::shared_ptr<void> retained):frame(f),idle(i),linked(l),owner(std::move(retained)) {}
    PrimaryMobBindings bindings(RuntimeZone& zone) override {events.push_back("mob"+std::to_string(zone.map->settings.id));return {mismatch_random?static_cast<RandomSource&>(alternate_random):static_cast<RandomSource&>(frame.random),idle,frame.init,linked};}
    std::shared_ptr<void> retain_idle_mob(MobRuntime&) override {return omit_idle_pin?std::shared_ptr<void>{}:owner;}
    void update_traps(RuntimeZone& zone,std::uint32_t) override {events.push_back("trap"+std::to_string(zone.map->settings.id));if(on_traps)on_traps(zone);}
};
struct RootAttackScene final:ActorScene,ActorEffectDatabase {
    std::vector<eternity::net::Packet> effect_database_packets;unsigned released_effects=0;
    void send_effect_database(const eternity::net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    std::vector<eternity::net::Packet> self_packets,nearby_packets;
    void send_self(const eternity::net::Packet& packet) noexcept override {self_packets.push_back(packet);}
    void send_nearby(const eternity::net::Packet& packet) override {nearby_packets.push_back(packet);}
    void send_cell_view(const eternity::net::Packet&) override {throw std::logic_error("unexpected cell attack send");}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::logic_error("unexpected damage reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {++released_effects;}
    std::uint32_t tick_ms() const noexcept override {return 77;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("unexpected target stop");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("unexpected taunt");}
    ZoneUsers* user_zone() override {return nullptr;}
    UserShapeSocial user_shape_social() override {throw std::logic_error("unexpected social state");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("unexpected periodic damage");}
};
struct RootAttackWorld final:MobCombatSchedulerActorWorld {
    MobRuntime mob;unsigned hits=0;std::function<void()> on_hit;
    MobRuntime* enable_mob(Actor&,std::uint32_t) override {return &mob;}
    bool in_range(Actor&,MobRuntime&) override {return true;}
    bool is_both_mob(MobRuntime&) override {return false;}
    void attack_mob(Actor&,MobRuntime&) override {++hits;if(on_hit)on_hit();}
    [[noreturn]] static void unexpected(){throw std::logic_error("unexpected attack service");}
    void send_move_cancel(Actor&,const eternity::net::Packet&) override {unexpected();}
    Actor* find_user(Actor&,std::uint32_t) override {unexpected();}
    Actor* find_attack_user(Actor&,std::uint32_t) override {unexpected();}
    std::int32_t map_team_mode(const Actor&) const override {unexpected();}
    bool in_user_range(Actor&,Actor&) override {unexpected();}
    void attack_user(Actor&,Actor&) override {unexpected();}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&) const override {unexpected();}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t) override {unexpected();}
    void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override {unexpected();}
    void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override {unexpected();}
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override {unexpected();}
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override {unexpected();}
};
struct RootAttackServices final:PrimaryUserAttackServices,MobCombatSchedulerZoneServices {
    std::shared_ptr<Actor> actor;RootAttackWorld& attack;MobCombatSchedulerBindings binding{true};std::vector<int> zones;std::function<void()> on_zone;
    RootAttackServices(std::shared_ptr<Actor> a,RootAttackWorld& w):actor(std::move(a)),attack(w) {}
    MobCombatSchedulerZoneServices& bindings(RuntimeZone& zone) override {zones.push_back(zone.map->settings.id);if(on_zone)on_zone();return *this;}
    std::shared_ptr<void> retain(Actor& current) override {check(actor.get()==&current);return actor;}
    MobCombatSchedulerBindings& bindings(Actor&) override {return binding;}
    MobCombatSchedulerActorWorld& world(Actor&) override {return attack;}
};
struct RootQuestPoolFactory final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override {return std::make_unique<RestoredQuest>();}};
struct RootQuestClock final:ItemIdClock {
    std::uint32_t ticks_ms() override {return 77;}
    std::uint32_t packed_local_calendar() override {return 0;}
};
struct RootQuestPorts final:VerticalQuestPorts {
    unsigned locks=0;std::vector<eternity::net::Packet> database,logs;
    void send_database(Actor&,const eternity::net::Packet& packet) override {database.push_back(packet);}
    void send_client(Actor& actor,const eternity::net::Packet& packet) override {actor.send_self(packet);}
    std::uint32_t local_make_time(Actor&) override {return 1;}
    std::uint32_t tick_ms(Actor&) override {return 77;}
    std::uint32_t quest_tick_ms() override {return 77;}
    QuestStartLogContext start_log_context(Actor&) override {throw std::logic_error("unexpected quest start log");}
    void send_start_log(Actor&,const std::array<std::uint8_t,144>&) override {throw std::logic_error("unexpected quest start log");}
    void lock_quests(Actor&) override {++locks;}
    void unlock_quests(Actor&) noexcept override {--locks;}
    void fill_game_log_main(Actor&,std::span<std::uint8_t> packet) override {std::fill(packet.begin(),packet.end(),0);}
    void send_game_log(Actor&,const eternity::net::Packet& packet) override {logs.push_back(packet);}
    QuestRewardWorld& reward_world(Actor&) override {throw std::logic_error("unexpected quest reward");}
};
struct RootQuestPin {std::shared_ptr<Actor> actor;std::shared_ptr<QuestRuntime> runtime;std::shared_ptr<VerticalQuests> world;std::shared_ptr<const QuestDefinitions> definitions;std::uint8_t last_item=0;std::uint32_t deadline=0;};
struct RootQuestServices final:PrimaryQuestPhaseServices {
    std::shared_ptr<RootQuestPin> pin;std::function<void()> on_retain;unsigned visits=0;bool allow=true;
    explicit RootQuestServices(std::shared_ptr<RootQuestPin> p):pin(std::move(p)) {}
    std::shared_ptr<void> retain(Actor& actor) override {check(pin->actor.get()==&actor);if(on_retain)on_retain();return pin;}
    bool eligible(Actor& actor) override {check(pin->actor.get()==&actor);return allow;}
    VerticalQuests& quests(Actor& actor) override {check(pin->actor.get()==&actor && bool(pin->world));++visits;return *pin->world;}
    void failure(Actor&,std::exception_ptr error) override {std::rethrow_exception(error);}
};
eternity::net::Packet root_quest_data() {
    std::array<std::uint8_t,464> q{};q[0]=1;q[0x128]=1;
    eternity::net::Packet packet;put(packet,1);auto copy=[&](std::size_t at,std::size_t n){packet.insert(packet.end(),q.begin()+static_cast<std::ptrdiff_t>(at),q.begin()+static_cast<std::ptrdiff_t>(at+n));};
    copy(0,2);put(packet,0);put(packet,0);copy(0x102,2);copy(0x104,2);copy(0x106,10);copy(0x110,4);copy(0x114,3);copy(0x118,2);copy(0x11a,7);copy(0x124,20);copy(0x138,17);copy(0x14a,12);copy(0x156,2);copy(0x15a,1);copy(0x158,2);copy(0x15b,3);
    for(std::size_t at=0x160;at<0x1c0;at+=32){copy(at,5);copy(at+8,9);copy(at+20,12);}for(int i=0;i<7;++i)put(packet,0);return packet;
}
struct RootPrefixServices final:PrimaryFramePrefixServices {
    std::vector<std::string> events;
    void record(const char* name,RuntimeZone& zone) {events.push_back(std::string(name)+std::to_string(zone.map->settings.id));}
    std::shared_ptr<Actor> actor;std::shared_ptr<Frame> frame;std::shared_ptr<ItemDefinitions> items;unsigned mobs_recovered=0;bool omit_attack_pin=false;std::function<void()> on_actor_packets,on_actor_recover;std::function<void(RuntimeZone&)> on_mob_attack;
    std::shared_ptr<void> retain_skill_actor(Actor& value) override {check(actor.get()==&value);return actor;}
    const ItemDefinitions& actor_items(Actor& value) override {check(actor.get()==&value);return *items;}
    void update_actor_recover(RuntimeZone& z,Actor&,std::uint32_t) override {record("recover",z);if(on_actor_recover)on_actor_recover();}
    void update_actor_pvp_timeout(RuntimeZone& z,Actor&,std::uint32_t) override {record("pvp",z);}
    void update_actor_rebirth(RuntimeZone& z,Actor&,std::uint32_t) override {record("rebirth",z);}
    std::shared_ptr<void> retain_skill_mob(MobRuntime&) override {return frame;}
    void update_mob_recover(RuntimeZone&,MobRuntime&,std::uint32_t) override {++mobs_recovered;}
    std::shared_ptr<void> retain_packet_actor(Actor& value) override {check(actor.get()==&value);return actor;}
    void update_actor_packets(RuntimeZone& z,Actor& value) override {check(actor.get()==&value);record("packet",z);if(on_actor_packets)on_actor_packets();}
    std::uint32_t packet_tick(RuntimeZone& z) override {record("clock",z);return 567;}
    void update_actor_reset_position(RuntimeZone& z,Actor& value,std::uint32_t tick) override {check(actor.get()==&value && tick==567);record("reset",z);}
    std::shared_ptr<void> retain_effect_actor(Actor& value) override {check(actor.get()==&value);return actor;}
    void keep_actor_skills(RuntimeZone& z,Actor&,std::uint32_t) override {record("tick",z);}
    std::shared_ptr<void> retain_effect_mob(MobRuntime&) override {return frame;}
    std::uint32_t chat_deadline=0,move_deadline=0;
    std::shared_ptr<void> retain_effect_user(Actor& value) override {check(actor.get()==&value);return actor;}
    void update_timeovers(RuntimeZone& z,Actor&,std::uint32_t) override {record("timeout",z);}
    PrimaryEffectAdminDeadlines effect_admin_deadlines(Actor& value) override {check(actor.get()==&value);return {chat_deadline,move_deadline};}
    void update_admin_notice(RuntimeZone& z,Actor&,std::uint32_t) override {record("notice",z);}
    std::shared_ptr<void> retain_attack_mob(MobRuntime&) override {return omit_attack_pin?std::shared_ptr<void>{}:frame;}
    void update_attack_mob(RuntimeZone& z,MobRuntime&,std::uint32_t) override {
        const auto tag=std::string("mob-attack")+std::to_string(z.map->settings.id);
        if(events.empty() || events.back()!=tag)events.push_back(tag);
        if(on_mob_attack)on_mob_attack(z);
    }
};
struct RootNpcIdleServices final:PrimaryNpcIdleServices {
    NpcPool& pool;RandomSource& random;std::vector<int> zones;std::function<void(RuntimeZone&,Npc&,std::uint32_t)> guard;
    RootNpcIdleServices(NpcPool& p,RandomSource& r):pool(p),random(r) {}
    PrimaryNpcIdleBindings bindings(RuntimeZone& zone) override {zones.push_back(zone.map->settings.id);return {pool,random};}
    void update_guard(RuntimeZone& zone,Npc& npc,std::uint32_t tick) override {if(!guard)throw std::logic_error("unexpected guard AI");guard(zone,npc,tick);}
};
struct RootWeatherSink final:ZoneEnvironmentSink {
    std::vector<eternity::net::Packet> packets;std::function<void()> on_send;
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {throw std::logic_error("unexpected weather cell send");}
    void send_all(const eternity::net::Packet& packet) override {packets.push_back(packet);if(on_send)on_send();}
};
struct RootWeatherServices final:PrimaryWeatherServices {
    RandomSource& random;std::array<RootWeatherSink,2> sinks;std::vector<int> zones;
    explicit RootWeatherServices(RandomSource& r):random(r) {}
    PrimaryWeatherBindings bindings(RuntimeZone& zone) override {
        const auto id=zone.map->settings.id;check(id>=0 && id<2);zones.push_back(id);return {random,sinks[static_cast<std::size_t>(id)]};
    }
};
struct RootLogoutServices final:PrimaryLogoutServices {
    std::shared_ptr<void> pin;Actor* actor=nullptr;WorldAdmissionUser* actor_user=nullptr;
    std::vector<std::string> events;std::function<void(WorldAdmissionUser&)> on_wait;
    explicit RootLogoutServices(std::shared_ptr<void> owner):pin(std::move(owner)) {}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser&) override {return pin;}
    void logout_waiting(WorldAdmissionUser& user) override {events.push_back("wait"+std::to_string(user.user_id));if(on_wait)on_wait(user);}
    std::shared_ptr<void> retain_actor(Actor& value) override {check(&value==actor);return pin;}
    WorldAdmissionUser& admission(Actor& value) override {check(&value==actor && actor_user);return *actor_user;}
    void logout_actor(Actor& value,WorldAdmissionUser& user) override {check(&value==actor && &user==actor_user);events.push_back("actor"+std::to_string(value.id()));}
};
struct RootDbUpdateServices final:WorldDbUpdateServices {
    bool started=false;std::vector<std::string> events;std::function<void()> on_database;
    void update_manager_packets() override {events.push_back("manager");}
    void update_database_packets() override {events.push_back("database");if(on_database)on_database();}
    bool world_threads_started() const override {return started;}
    void check_experience_event() override {events.push_back("experience");}
};
struct RootKillCountServices final:WorldKillCountServices {
    BlessingDefinitions data;std::shared_ptr<Actor> actor;std::vector<std::string> events;std::vector<eternity::net::Packet> packets;
    explicit RootKillCountServices(std::shared_ptr<Actor> a):data(make_data()),actor(std::move(a)) {}
    static BlessingDefinitions make_data() {
        eternity::net::Packet bytes;put(bytes,2);for(std::uint8_t country=0;country<2;++country){put(bytes,country,1);put(bytes,country?70:50);put(bytes,0,2);for(int i=0;i<6;++i){put(bytes,0,1);put(bytes,0,2);}}return BlessingDefinitions::decode(bytes);
    }
    const BlessingDefinitions& definitions() const override {return data;}
    std::uint32_t count(std::uint8_t) const override {return 100;}
    std::shared_ptr<void> retain_actor(Actor& a) override {check(&a==actor.get());return actor;}
    void apply_actor(Actor&) override {events.push_back("actor");}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser&) override {return actor;}
    std::uint8_t waiting_country(WorldAdmissionUser&) override {return 0;}
    void apply_waiting(WorldAdmissionUser&) override {events.push_back("waiting");}
    void send_country(RuntimeZone& zone,std::uint8_t country,const eternity::net::Packet& packet) override {events.push_back("send"+std::to_string(country)+":"+std::to_string(zone.map->settings.id));packets.push_back(packet);}
};
template<class Root> struct RootKillCountZonePorts final:WorldKillCountZonePorts<Root> {
    std::shared_ptr<RootKillCountServices> delegate;explicit RootKillCountZonePorts(std::shared_ptr<RootKillCountServices> value):delegate(std::move(value)){}
    std::shared_ptr<void> retain_actor(Actor& actor) override{return delegate->retain_actor(actor);}
    void apply_actor(Actor& actor,const WorldKillCountState<Root>& state) override{check(state.country(0).count==100);delegate->apply_actor(actor);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override{return delegate->retain_waiting(user);}
    std::uint8_t waiting_country(WorldAdmissionUser& user) override{return delegate->waiting_country(user);}
    void apply_waiting(WorldAdmissionUser& user,const WorldKillCountState<Root>& state) override{check(state.country(0).count==100);delegate->apply_waiting(user);}
    void send_country(RuntimeZone& zone,std::uint8_t country,const eternity::net::Packet& packet) override{delegate->send_country(zone,country,packet);}
};
struct RootMarketServices final:WorldMarketServices {
    bool open=true;std::vector<std::uint32_t> ticks;std::function<void()> on_check;
    bool market_open() const override{return open;}
    void check_item_end_dates(std::uint32_t tick) override {ticks.push_back(tick);if(on_check)on_check();}
};
struct RootRankServices final:WorldRankServices {
    std::shared_ptr<Actor> actor;ZoneLocalTime time{1,1,1,9};std::optional<WorldRankNotice> current=WorldRankNotice{42,3};
    std::vector<int> moves, broadcasts;std::vector<eternity::net::Packet> packets;std::function<void(RuntimeZone&)> on_move;
    explicit RootRankServices(std::shared_ptr<Actor> a):actor(std::move(a)) {}
    ZoneLocalTime local_time() override{return time;}
    std::uint32_t tick_ms() override{return 222;}
    std::shared_ptr<void> retain_actor(Actor& a) override {check(&a==actor.get());return actor;}
    void move_to_bind(RuntimeZone& zone,Actor&,std::uint32_t tick) override {check(tick==222);moves.push_back(zone.map->settings.id);if(on_move)on_move(zone);}
    std::optional<WorldRankNotice> notice() const override{return current;}
    void broadcast(RuntimeZone& zone,const eternity::net::Packet& packet) override {broadcasts.push_back(zone.map->settings.id);packets.push_back(packet);}
};
struct RootSendServices final:WorldSendServices {
    std::shared_ptr<Actor> actor;std::vector<std::string> events;std::function<void(RuntimeZone&)> on_delivery;
    explicit RootSendServices(std::shared_ptr<Actor> a):actor(std::move(a)) {}
    void deliver(RuntimeZone& zone,const WorldZoneSendMessage& message) override {check(message.payload && !message.payload->empty());events.push_back("send"+std::to_string(zone.map->settings.id)+":"+std::to_string(static_cast<int>(message.kind)));if(on_delivery)on_delivery(zone);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser&) override{return actor;}
    void flush_waiting(WorldAdmissionUser&) override{events.push_back("wait");}
    std::shared_ptr<void> retain_actor(Actor& a) override{check(&a==actor.get());return actor;}
    void flush_actor(Actor&) override{events.push_back("actor");}
};
struct RootSendUpdateServices final:WorldSendUpdateServices {
    std::vector<std::string> events;bool rank_active=false;std::function<void()> on_packets;
    void update_packets() override{events.push_back("packets");if(on_packets)on_packets();}
    void update_net_send() override{events.push_back("net");}
    void update_session_lost(std::uint32_t) override{events.push_back("lost");}
    void update_kill_count(std::uint32_t) override{events.push_back("kill");}
    void check_guild_rank_notices() override{events.push_back("notice");}
    bool check_guild_rank_status() override{events.push_back("status");return rank_active;}
    void update_guild_point_rank() override{events.push_back("points");}
};
struct RootGlobalSendServices final:WorldGlobalSendServices {
    std::shared_ptr<void> pin;std::vector<std::uint32_t> users;unsigned retain_calls=0;std::function<void()> on_retain;
    std::shared_ptr<void> retain(WorldAdmissionUser&) override{++retain_calls;if(on_retain)on_retain();return pin;}
    void flush(WorldAdmissionUser& user) override{users.push_back(user.user_id);}
};
struct RootSessionLostServices final:WorldSessionLostServices {
    std::shared_ptr<void> pin;std::uint32_t tick=0;std::vector<std::pair<std::uint32_t,std::uint32_t>> closed;
    std::shared_ptr<void> retain(WorldAdmissionUser&) override{return pin;}
    std::uint32_t connected_tick(WorldAdmissionUser&) override{return tick;}
    void close(WorldAdmissionUser& user,std::uint32_t reason) override{closed.emplace_back(user.user_id,reason);}
};
struct RootSendOtherServices final:WorldSendOtherServices {
    void update_session_lost(std::uint32_t) override{throw std::logic_error("unexpected session-lost update");}
    void update_kill_count(std::uint32_t) override{throw std::logic_error("unexpected kill-count update");}
    void check_guild_rank_notices() override{throw std::logic_error("unexpected rank notice");}
    bool check_guild_rank_status() override{throw std::logic_error("unexpected rank status");}
    void update_guild_point_rank() override{throw std::logic_error("unexpected rank points");}
};
struct RootSendRemainingServices final:WorldSendRemainingServices {
    void update_kill_count(std::uint32_t) override{throw std::logic_error("unexpected kill-count update");}
    void check_guild_rank_notices() override{throw std::logic_error("unexpected rank notice");}
    bool check_guild_rank_status() override{throw std::logic_error("unexpected rank status");}
    void update_guild_point_rank() override{throw std::logic_error("unexpected rank points");}
};
struct KillTimerServicesFixture final:WorldKillCountTimerServices {
    std::array<WorldKillCountDeadlines,2> state{};std::array<std::uint32_t,2> counts{7,9};std::vector<std::string> events;std::vector<eternity::net::Packet> database;
    WorldKillCountDeadlines& deadlines(std::uint8_t country) override{return state.at(country);}
    void set_zero(std::uint8_t country) override{events.push_back("zero"+std::to_string(country));counts.at(country)=0;}
    std::uint32_t count(std::uint8_t country) override{return counts.at(country);}
    void send_database(const eternity::net::Packet& packet) override{events.push_back("database");database.push_back(packet);}
};
struct KillTimerRootFixture {
    bool running=true;WorldKillCountTimer* timer=nullptr;std::vector<std::uint8_t> requests,applies;
    std::unique_ptr<WorldKillCountTimer> owned_timer;unsigned obelisk_resets=0;
    bool world_send_running()const{return running;}
    bool world_accepts_initialization()const{return running;}
    void request_kill_count_send(std::uint8_t country){requests.push_back(country);}
    void request_kill_count_apply(std::uint8_t country){applies.push_back(country);}
    void request_obelisk_reset(){++obelisk_resets;}
    void bind_world_kill_count_timer(std::shared_ptr<WorldKillCountTimerServices> services){owned_timer=std::make_unique<WorldKillCountTimer>(std::move(services));timer=owned_timer.get();}
    bool update_world_kill_count_timer(std::uint32_t now){return timer->update(*this,now);}
};
struct KillTimerGuildFixture final:WorldSendGuildServices {
    void check_guild_rank_notices() override{throw std::logic_error("unexpected rank notice");}
    bool check_guild_rank_status() override{throw std::logic_error("unexpected rank status");}
    void update_guild_point_rank() override{throw std::logic_error("unexpected rank points");}
};
struct SendDriverFixture {
    unsigned calls=0;bool fail=false;
    bool update_world_send(std::uint32_t now) {++calls;if(fail)throw std::runtime_error("send failure");check(now==(calls==1?100U:500U));return calls<2;}
};
struct GlobalSendSessionFixture {WorldAdmissionUser user;eternity::net::WorldChannel socket;WorldAdmissionUser& admission(){return user;}eternity::net::WorldChannel& channel(){return socket;}};
struct SendCompositionRootFixture {
    VerticalSessionRegistry<GlobalSendSessionFixture> registry;RootAdmissionServices owner;WorldAdmission admission_{owner};
    std::unique_ptr<WorldSessionLostPhase> lost;std::shared_ptr<WorldSendUpdateServices> update;std::uint32_t period=0;bool running=true;
    auto& sessions(){return registry;}WorldAdmission& admission(){return admission_;}bool world_send_running()const{return running;}
    void update_admission(){admission_.update_packets();}
    void bind_session_lost_phase(std::shared_ptr<WorldSessionLostServices> services){lost=std::make_unique<WorldSessionLostPhase>(std::move(services));}
    void bind_world_send_update(std::shared_ptr<WorldSendUpdateServices> services,std::function<std::uint32_t()> tick){check(bool(tick));update=std::move(services);}
    void bind_world_send_driver(std::uint32_t value,WorldFrameDriver::OverrunCounter counter,WorldFrameDriver::Failure failure){check(bool(counter) && bool(failure));period=value;}
    bool update_session_lost(std::uint32_t now){return lost->update(*this,now);}
};
struct RootPriorityPool final:PartyMembershipSendWorld {
    PartyMembershipSendBuffer buffer;bool active=false;
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {if(active)throw std::logic_error("priority buffer still retained");active=true;return &buffer;}
    void free_send_buffer(PartyMembershipSendBuffer& value) override {check(&value==&buffer && value.references==0);active=false;}
    void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer&) override {throw std::logic_error("unexpected direct pool send");}
    std::uint32_t tick() override{throw std::logic_error("unexpected tick");}
    void lock(PartyNormalMembership&) override{throw std::logic_error("unexpected lock");}
    void unlock(PartyNormalMembership&) noexcept override{std::terminate();}
    void lock_effects(PartyMembershipUser&) override{throw std::logic_error("unexpected effect lock");}
    void unlock_effects(PartyMembershipUser&) noexcept override{std::terminate();}
    void send_user(PartyMembershipUser&,const eternity::net::Packet&) override{throw std::logic_error("unexpected user send");}
    void send_scene(PartyMembershipUser&,const eternity::net::Packet&) override{throw std::logic_error("unexpected scene send");}
    void unregister_party(PartyNormalMembership&) override{throw std::logic_error("unexpected unregister");}
    void cleanup_guild_creation(PartyMembershipUser&) override{throw std::logic_error("unexpected guild cleanup");}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override{throw std::logic_error("unexpected log");}
    void send_log(const eternity::net::Packet&) override{throw std::logic_error("unexpected log send");}
};
struct RootNativeSendPorts final:WorldNativeSendPorts {
    std::shared_ptr<Actor> actor;RootPriorityPool pool;UserViewSharedBuffers shared{pool};UserPriorityQueue queue{pool},waiting_queue{pool};PartyMembershipUser user,waiting_user;
    eternity::net::WorldChannel socket;std::vector<eternity::net::Packet> packets;bool allow_priority=true;
    explicit RootNativeSendPorts(std::shared_ptr<Actor> a):actor(std::move(a)){user.id=actor->id();user.send_socket=1;waiting_user.id=230;waiting_user.send_socket=2;socket.send=[this](eternity::net::Packet packet){packets.push_back(std::move(packet));};}
    std::shared_ptr<void> retain_actor(Actor& value) override{check(&value==actor.get());return actor;}
    eternity::net::WorldChannel& channel(Actor& value) override{check(&value==actor.get());return socket;}
    UserViewSharedBuffers& buffers() override{return shared;}
    PartyMembershipUser& connection(Actor& value) override{check(&value==actor.get());return user;}
    UserPriorityQueue& priority_queue(Actor& value) override{check(&value==actor.get());return queue;}
    PartyMembershipSendWorld& priority_pool() override{return pool;}
    bool can_submit_priority(Actor& value) override{check(&value==actor.get());return allow_priority;}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser&) override{return actor;}
    PartyMembershipUser& waiting_connection(WorldAdmissionUser& user_value) override{check(user_value.user_id==waiting_user.id);return waiting_user;}
    UserPriorityQueue& waiting_priority_queue(WorldAdmissionUser&) override{return waiting_queue;}
    eternity::net::WorldChannel& waiting_channel(WorldAdmissionUser&) override{return socket;}
    bool can_submit_waiting_priority(WorldAdmissionUser&) override{return allow_priority;}
};
struct RetainedSendSession {
    WorldAdmissionUser user;WorldUser* linked=nullptr;eternity::net::WorldChannel socket;
    WorldAdmissionUser& admission(){return user;}
    WorldUser* world_user(){return linked;}
    eternity::net::WorldChannel& channel(){return socket;}
};
struct RetainedSendResources {
    std::shared_ptr<RootNativeSendPorts> owner;
    PartyMembershipSendWorld& pool(){return owner->pool;}
    PartyMembershipUser& connection(){return owner->user;}
    UserPriorityQueue& queue(){return owner->queue;}
};
struct DbDriverFixture {
    std::vector<std::uint32_t> ticks;
    WorldDbUpdateResult update(std::uint32_t tick) {ticks.push_back(tick);return ticks.size()==1?WorldDbUpdateResult::completed:WorldDbUpdateResult::interrupted;}
};
int main(int argc,char** argv) {try {
    check(argc==2);
    check(world_frame_cadence(100,100,100).wait_ms==250);
    {
        ActiveWorldPrefixResult prefix;check(!active_world_net_send_due(false,prefix,false));
        check(active_world_net_send_due(true,prefix,false) && active_world_net_send_due(false,prefix,true));
        for(auto member:{&ActiveWorldPrefixResult::user_attack,&ActiveWorldPrefixResult::effects,&ActiveWorldPrefixResult::mob_npc_attack,&ActiveWorldPrefixResult::mob_idle,&ActiveWorldPrefixResult::npc_idle,&ActiveWorldPrefixResult::items,&ActiveWorldPrefixResult::quests,&ActiveWorldPrefixResult::weather}) {
            prefix.*member=true;check(active_world_net_send_due(false,prefix,false));prefix.*member=false;
        }
    }
    {
        asio::io_context io;auto strand=asio::make_strand(io);auto update=std::make_shared<DbDriverFixture>();
        auto heartbeat=std::make_shared<std::atomic<std::uint32_t>>(0),overruns=std::make_shared<std::atomic<std::uint32_t>>(0);
        std::vector<std::uint32_t> clocks{90,100,351,351,352};std::size_t clock_index=0;std::exception_ptr failure;
        auto driver=WorldDbDriver<DbDriverFixture>::create(strand,update,heartbeat,overruns,[&]{return clocks.at(clock_index++);},[&](std::exception_ptr error){failure=error;});
        asio::post(strand,[&]{driver->start();});io.run();
        check(!failure && (update->ticks==std::vector<std::uint32_t>{100,352}) && heartbeat->load()==100 && overruns->load()==1 && clock_index==clocks.size());
    }
    check(world_frame_cadence(100,349,350).wait_ms==1 && !world_frame_cadence(100,349,350).overrun);
    check(world_frame_cadence(100,350,351).wait_ms==0 && world_frame_cadence(100,350,351).overrun);
    check(world_frame_cadence(100,351,351).wait_ms==0);
    check(world_frame_cadence(0xfffffff0U,0x10U,0x10U).wait_ms==218);
    check(world_frame_cadence(100,100+0x800000faU,100).wait_ms==0);
    check(world_frame_cadence(100,100+0x800000fbU,100).wait_ms==0x7fffffffU);
    write_maps(argv[1]);
    auto config=MapConfiguration::decode("[BASE]\nMapCount=2\n");
    auto maps=std::make_shared<ServerMaps>(ServerMaps::load(config,argv[1]));
    auto f=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root;unsigned frames=0;
    MobPrimaryZoneTransport transport{[&](const eternity::net::Packet&){++f->stats->db;},[&](const eternity::net::Packet&){++f->stats->world;},[&](const ConfiguredMap&,ZoneMobs&,const eternity::net::Packet&){++f->stats->map;}};
    LinkedFixture linked;IdleFixture idle;std::unique_ptr<VerticalWorld> world;
    VerticalWorldPorts ports{[&](MobPrimaryZones&){attach(f,root);},[&](RuntimeZone& z,std::uint32_t now){
        check(z.mobs && now==5077);std::size_t id=z.map==&maps->all()[0]?0:1;
        if(id==0) check(f->records->records(0)[0].spawns[0].members[0]==nullptr);
        check(world->update_primary_mob_idle(id,now,f->random,idle,f->init,linked));
        ++frames;
    },[]{return 100U;}};
    world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f,&f->backend),std::move(f->pool),transport,ports);root=world->zones();check(world->start());root->serialized([&]{auto* mob=f->records->records(0)[0].spawns[0].members[0];check(mob && f->router->remove(0,mob->id));});world->frame(5077);check(frames==2 && idle.calls>0 && f->records->records(0)[0].spawns[0].members[0]);
    std::vector<std::size_t> passes;
    for(unsigned phase=0;phase<2;++phase) world->visit_primary_zones([&](RuntimeZone& z){
        passes.push_back(phase*2+(z.map==&maps->all()[0]?0:1));
    });
    check((passes==std::vector<std::size_t>{0,1,2,3}));
    std::atomic<bool> contender_ready=false,contender_entered=false;
    std::thread contender;bool blocked_at_second=false;
    world->visit_primary_zones([&](RuntimeZone& z){
        if(z.map==&maps->all()[0]) {
            contender=std::thread([&]{contender_ready=true;root->serialized([&]{contender_entered=true;});});
            while(!contender_ready.load()) std::this_thread::yield();
        } else blocked_at_second=!contender_entered.load();
    });
    contender.join();check(blocked_at_second && contender_entered.load());
    std::atomic<bool> phase_ready=false,phase_entered=false;
    std::thread phase_contender;bool between_phases_blocked=false;unsigned visited_phases=0;
    world->serialized_update([&](VerticalWorld& current){
        check(&current==world.get());
        current.visit_primary_zones([&](RuntimeZone&){++visited_phases;});
        phase_contender=std::thread([&]{phase_ready=true;root->serialized([&]{phase_entered=true;});});
        while(!phase_ready.load()) std::this_thread::yield();
        current.visit_primary_zones([&](RuntimeZone&){between_phases_blocked=!phase_entered.load();++visited_phases;});
    });
    phase_contender.join();check(between_phases_blocked && phase_entered.load() && visited_phases==4);
    bool invalid_update=false;try{world->serialized_update({});}catch(const std::invalid_argument&){invalid_update=true;}check(invalid_update);
    bool invalid_visit=false;try{world->visit_primary_zones({});}catch(const std::invalid_argument&){invalid_visit=true;}check(invalid_visit);
    auto held=root->lease(0);check(!world->shutdown());held.reset();check(world->shutdown());check(world->shutdown());
    bool rejected=false;try{world->frame(77);}catch(const std::logic_error&){rejected=true;}check(rejected);
    auto f2=std::make_shared<Frame>(maps);
    VerticalWorldPorts fail{[](MobPrimaryZones&){throw std::runtime_error("attach failed");},[](RuntimeZone&,std::uint32_t){throw std::logic_error("unexpected frame");},[]{return 100U;}};
    auto failed=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f2,&f2->backend),std::move(f2->pool),transport,fail);auto root2=failed->zones();rejected=false;try{failed->start();}catch(const std::runtime_error&){rejected=true;}check(rejected && failed->shutdown() && root2->mob_pool().available()==root2->mob_pool().size());
    auto f3=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root3;std::unique_ptr<VerticalWorld> stopping;unsigned stopped_frames=0;
    VerticalWorldPorts stop_during_frame{[&](MobPrimaryZones&){attach(f3,root3);},{},[]{return 100U;}};
    stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f3,&f3->backend),std::move(f3->pool),transport,stop_during_frame);root3=stopping->zones();check(stopping->start());
    rejected=false;try{stopping->frame(77);}catch(const std::logic_error&){rejected=true;}check(rejected);
    stopping->serialized_update([&](VerticalWorld& current){current.visit_primary_zones([&](RuntimeZone&){++stopped_frames;check(!stopping->shutdown());});});check(stopped_frames==1 && stopping->shutdown() && root3->mob_pool().available()==root3->mob_pool().size());
    auto f4=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root4;std::unique_ptr<VerticalWorld> interrupted;
    VerticalWorldPorts stop_attach{[&](MobPrimaryZones&){attach(f4,root4);check(interrupted->shutdown());},[](RuntimeZone&,std::uint32_t){throw std::logic_error("unexpected frame");},[]{return 100U;}};
    interrupted=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f4,&f4->backend),std::move(f4->pool),transport,stop_attach);root4=interrupted->zones();
    check(!interrupted->start() && interrupted->shutdown() && root4->mob_pool().available()==root4->mob_pool().size());
    auto f5=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root5;std::unique_ptr<VerticalWorld> stop_startup;
    bool entry_stop_requested=false;
    VerticalWorldPorts stop_entry{[&](MobPrimaryZones&){attach(f5,root5);f5->backend.on_entry=[&]{if(!entry_stop_requested){entry_stop_requested=true;check(!stop_startup->shutdown());}};},[](RuntimeZone&,std::uint32_t){throw std::logic_error("unexpected frame");},[]{return 100U;}};
    stop_startup=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f5,&f5->backend),std::move(f5->pool),transport,stop_entry);root5=stop_startup->zones();
    check(!stop_startup->start() && entry_stop_requested && stop_startup->shutdown() && root5->mob_pool().available()==root5->mob_pool().size());
    auto f6=std::make_shared<Frame>(maps);std::unique_ptr<VerticalWorld> stop_initialize;
    VerticalWorldPorts stop_init{[](MobPrimaryZones&){throw std::logic_error("unexpected attachment");},[](RuntimeZone&,std::uint32_t){throw std::logic_error("unexpected frame");},[]{return 100U;}};
    stop_initialize=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f6,&f6->backend),std::move(f6->pool),transport,stop_init);auto root6=stop_initialize->zones();
    bool init_stop_requested=false;f6->backend.on_initialize=[&]{if(!init_stop_requested){init_stop_requested=true;check(!stop_initialize->shutdown());check(!stop_initialize->shutdown());}};
    check(!stop_initialize->start() && stop_initialize->shutdown() && root6->mob_pool().available()==root6->mob_pool().size());
    auto f7=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root7;std::unique_ptr<VerticalWorld> mob_stopping;
    VerticalWorldPorts stop_mob{[&](MobPrimaryZones&){attach(f7,root7);},{},[]{return 100U;}};
    mob_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f7,&f7->backend),std::move(f7->pool),transport,stop_mob);root7=mob_stopping->zones();check(mob_stopping->start());
    IdleFixture stopping_idle;bool requested=false;stopping_idle.on_idle=[&]{if(!requested){requested=true;check(!mob_stopping->shutdown());}};
    check(!mob_stopping->update_primary_mob_idle(0,5077,f7->random,stopping_idle,f7->init,linked));
    check(requested && mob_stopping->shutdown() && root7->mob_pool().available()==root7->mob_pool().size());
    auto f8=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root8;std::unique_ptr<VerticalWorld> respawn_stopping;
    VerticalWorldPorts stop_respawn{[&](MobPrimaryZones&){attach(f8,root8);},{},[]{return 100U;}};
    respawn_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f8,&f8->backend),std::move(f8->pool),transport,stop_respawn);root8=respawn_stopping->zones();check(respawn_stopping->start());
    root8->serialized([&]{auto* mob=f8->records->records(0)[0].spawns[0].members[0];check(mob && f8->router->remove(0,mob->id));});
    bool respawn_requested=false;f8->backend.on_entry=[&]{if(!respawn_requested){respawn_requested=true;check(!respawn_stopping->shutdown());}};
    IdleFixture respawn_idle;
    check(!respawn_stopping->update_primary_mob_idle(0,5077,f8->random,respawn_idle,f8->init,linked));
    check(respawn_requested && respawn_stopping->shutdown() && root8->mob_pool().available()==root8->mob_pool().size());
    auto f9=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root9;
    VerticalWorldPorts loop_ports{[&](MobPrimaryZones&){attach(f9,root9);},{},[]{return 100U;}};
    std::shared_ptr<VerticalWorld> loop_world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f9,&f9->backend),std::move(f9->pool),transport,loop_ports);root9=loop_world->zones();check(loop_world->start());
    asio::io_context loop_io;WorldFrameDriver::Strand loop_strand=asio::make_strand(loop_io);
    {
        struct Connection {unsigned id;};Connection connection{9};std::vector<unsigned> events;AgentDisconnectFixture world{events};
        WorldDbAgentDisconnect<Connection,AgentDisconnectFixture> disconnect(world,[&](Connection& actual){check(&actual==&connection && actual.id==9);events.push_back(1);});
        const auto saved=disconnect.removed_game_connection(connection);check((events==std::vector<unsigned>{1,2}) && saved.due && !saved.success() && !saved.saved[1]);
        bool missing=false;try{WorldDbAgentDisconnect<Connection,AgentDisconnectFixture> invalid(world,{});}catch(const std::invalid_argument&){missing=true;}check(missing);
    }
    {
        asio::io_context io;auto strand=asio::make_strand(io);std::vector<unsigned> events;
        auto world=std::make_shared<AgentSaveDriverFixture>(AgentSaveDriverFixture{events});
        using Driver=WorldDbAgentSaveDriver<AgentSaveDriverFixture>;std::shared_ptr<Driver> driver;std::exception_ptr error;
        driver=Driver::create(strand,world,[]{return 1000U;},[&](std::uint32_t now){check(now==1000);events.push_back(1);},
            [&](const WorldDbAgentSaveResult& result){events.push_back(3);check(result.due && !result.success() && !result.saved[1]);driver->stop();},[&](std::exception_ptr failure){error=failure;});
        asio::post(strand,[&]{driver->start();});io.run();check(!error && (events==std::vector<unsigned>{1,2,3}));
        io.restart();events.clear();auto canceled=Driver::create(strand,world,[]{return 1000U;},[&](auto){events.push_back(1);},[&](const auto&){events.push_back(3);},[&](auto failure){error=failure;});
        asio::post(strand,[&]{canceled->start();canceled->stop();});io.run();check(events.empty() && !error);
        io.restart();world->fail=true;auto failed=Driver::create(strand,world,[]{return 1000U;},[&](auto){},[&](const auto&){},[&](auto failure){error=failure;});
        asio::post(strand,[&]{failed->start();});io.run();check(bool(error));
    }
    {
        auto services=std::make_shared<KillTimerServicesFixture>();services->state[0]={100,1};services->state[1]={0,100};
        WorldKillCountTimer timer(services);auto root=std::make_shared<KillTimerRootFixture>();root->timer=&timer;
        WorldSendRetainedRemaining<KillTimerRootFixture> remaining(root,std::make_shared<KillTimerGuildFixture>());
        remaining.update_kill_count(99);check(root->requests.empty() && services->database.empty());
        remaining.update_kill_count(100);check((root->requests==std::vector<std::uint8_t>{0,1}) && (services->events==std::vector<std::string>{"zero0","database"}));
        check(services->state[0].reset==0 && services->state[0].send==600100 && services->state[1].send==600100);
        check((services->database==std::vector<eternity::net::Packet>{{0x05,0x02,0,0,0,0,9,0,0,0}}));
        root->requests.clear();remaining.update_kill_count(600100);check((root->requests==std::vector<std::uint8_t>{0,1}) && services->database.size()==2);
    }
    {
        auto root=std::make_shared<KillTimerRootFixture>();eternity::net::Packet data;put(data,2);
        for(std::uint8_t country=0;country<2;++country){put(data,country,1);put(data,country?70:50);put(data,0,2);for(int i=0;i<6;++i){put(data,i==0?24:0,1);put(data,i==0?7:0,2);}}
        auto definitions=std::make_shared<BlessingDefinitions>(BlessingDefinitions::decode(data));
        std::array<WorldKillCountCountry,2> initial{};initial[0].count=50;initial[0].index=1;initial[0].deadlines={100,10};initial[1].count=9;initial[1].deadlines.send=1000;
        std::vector<eternity::net::Packet> database;unsigned ticks=0;
        std::vector<eternity::net::Packet> game_log;
        auto state=WorldKillCountState<KillTimerRootFixture>::create_bound(root,definitions,initial,[&]{++ticks;return 123U;},[&](const eternity::net::Packet& packet){database.push_back(packet);},[]{return false;},[&](const eternity::net::Packet& packet){game_log.push_back(packet);});
        WorldKillCountEntry<KillTimerRootFixture> entry(state);ActorLoadOptions options;options.country=0;entry.bind_actor_load(options);
        check(options.blessing==definitions->find(0,1));EntryDeliveryPorts delivery;entry.bind_delivery(delivery);
        auto entry_kills=delivery.kill_count(0);check(entry_kills.points==50 && entry_kills.expires==100);
        check((blessing_packet(0,entry_kills.points,entry_kills.expires,50)==eternity::net::Packet{0x11,0x02,0,50,0,0,0,50,0,0,0}));
        bool duplicate_delivery=false;try{entry.bind_delivery(delivery);}catch(const std::logic_error&){duplicate_delivery=true;}check(duplicate_delivery);
        check(root->update_world_kill_count_timer(100));
        check(state->country(0).count==0 && state->country(0).index==0 && state->country(0).next_threshold==50 && state->country(0).deadlines.reset==0 && state->country(0).deadlines.send==600100);
        check((root->applies==std::vector<std::uint8_t>{0}) && (root->requests==std::vector<std::uint8_t>{0}) && (database==std::vector<eternity::net::Packet>{{0x05,0x02,0,0,0,0,9,0,0,0}}));
        ActorLoadOptions after_reset;after_reset.country=0;entry.bind_actor_load(after_reset);entry_kills=delivery.kill_count(0);
        check(after_reset.blessing==definitions->find(0,0) && entry_kills.points==0 && entry_kills.expires==0);
        check(state->set_count(0,50) && state->country(0).index==1 && state->country(0).applied_modifiers[0]==7 && state->country(0).deadlines.reset==600123 && state->country(0).deadlines.send==600123);
        check(!state->set_count(0,50) && ticks==2 && (root->applies==std::vector<std::uint8_t>{0,0}));
        KillBootstrapDatabase db{{eternity::storage::SqlCursorRead::row,{0,70}}};
        const auto bootstrap=load_world_kill_count_bootstrap(db);check(db.called && state->initialize_from_database(bootstrap));
        check(state->country(0).count==0 && state->country(0).index==0 && state->country(0).next_threshold==50 && state->country(0).deadlines.reset==600123 && state->country(0).deadlines.send==600123);
        check(state->country(1).count==70 && state->country(1).index==1 && state->country(1).deadlines.reset==600123 && state->country(1).deadlines.send==1000 && state->country(1).applied_modifiers[0]==7 && ticks==3);
        check((root->applies==std::vector<std::uint8_t>{0,0}) && root->requests.size()==1 && database.size()==1);
        bool malformed=false;try{(void)state->initialize_from_database(eternity::net::Packet{0x05,0x02,0});}catch(const std::invalid_argument&){malformed=true;}
        check(malformed && state->country(0).count==0 && state->country(1).count==70 && ticks==3);
        db.value={eternity::storage::SqlCursorRead::end,{}};check((load_world_kill_count_bootstrap(db)==eternity::net::Packet{5,2,0,0,0,0,0,0,0,0}));
        db.value={eternity::storage::SqlCursorRead::row,{-1,std::nullopt}};check((load_world_kill_count_bootstrap(db)==eternity::net::Packet{5,2,255,255,255,255,0,0,0,0}));
        db.value={eternity::storage::SqlCursorRead::error,{}};bool sql_failed=false;try{(void)load_world_kill_count_bootstrap(db);}catch(const std::runtime_error&){sql_failed=true;}check(sql_failed);
        KillSaveDatabase save{{-1,-2,0}};check(save_world_kill_count(save,{0xffffffffU,70}));
        check(save.calls==3 && save.received[0]==-1 && save.received[1]==70);
        KillSaveDatabase rejected{{-1,-1,-1}};check(!save_world_kill_count(rejected,{0,0}) && rejected.calls==3);
        KillSaveDatabase failed;failed.fail_sql=true;bool save_failed=false;try{(void)save_world_kill_count(failed,{0,0});}catch(const eternity::storage::SqlError&){save_failed=true;}check(save_failed && failed.calls==3);
        KillOwnerDatabase owner_db;owner_db.value={eternity::storage::SqlCursorRead::row,{12,70}};owner_db.results={0,-1,-1,-1,0};
        WorldKillCountDbOwner owner(owner_db,100U);check(owner_db.called && owner.periodic_due(100U));
        check((owner.bootstrap_packet()==eternity::net::Packet{5,2,12,0,0,0,70,0,0,0}));
        owner.receive(eternity::net::Packet{5,2,0xff,0xff,0xff,0xff,40,0,0,0});
        check(owner_db.calls==0 && owner.save_periodic(100U) && owner_db.received[0]==-1 && owner_db.received[1]==40);
        check(!owner.periodic_due(900099U) && owner.periodic_due(900100U));
        check(!owner.save_periodic(900100U) && !owner.periodic_due(900100U));
        check(owner.save_all() && owner_db.calls==5 && !owner.periodic_due(900100U));
        bool bad_owner_packet=false;try{owner.receive(eternity::net::Packet{5,2,1});}catch(const std::invalid_argument&){bad_owner_packet=true;}
        check(bad_owner_packet && owner.counts()[0]==0xffffffffU && owner.counts()[1]==40);
        std::vector<unsigned> phases;SavePhaseFixture obelisks{phases,1,false,true},guilds{phases,2};
        WorldDbAgentSave world_save(owner,obelisks,guilds);
        auto not_due=world_save.periodic(900100U);check(!not_due.due && phases.empty());
        owner_db.results.insert(owner_db.results.end(),{-1,-1,-1});
        auto phase_result=world_save.periodic(1800100U);
        check(phase_result.due && !phase_result.success() && !phase_result.saved[0] && !phase_result.saved[1] && phase_result.saved[2] && phase_result.errors[1] && (phases==std::vector<unsigned>{1,2}));
        phases.clear();owner_db.results.push_back(0);obelisks.throws=false;obelisks.ok=true;
        auto all_result=world_save.save_all();check(all_result.success() && (phases==std::vector<unsigned>{1,2}));
        KeepDatabase keep_db;WorldDbObelisks keep(keep_db);
        check((keep.packets()==std::vector<eternity::net::Packet>{{6,2,1,7,0,0,0,1,100,0,0,0}}));
        check(keep.receive(eternity::net::Packet{6,2,2,7,0,0,0,2,1,0,0,0,8,0,0,0,0,2,0,0,0}));
        check(keep.records().at(7).country==1 && keep.records().at(8).dirty==1);
        check(keep.receive(eternity::net::Packet{7,2,8,0,0,0,1,9,0,0,0}) && keep.records().at(8).dirty==1);
        check(keep.receive(eternity::net::Packet{7,2,7,0,0,0,2,8,0,0,0}) && keep.records().at(7).dirty==2);
        check(!keep.receive(eternity::net::Packet{7,2,9,0,0,0,2,8,0,0,0}));
        keep_db.fail=true;bool keep_failed=false;try{(void)keep.save();}catch(const eternity::storage::SqlError&){keep_failed=true;}
        check(keep_failed && keep.records().at(7).dirty==2 && keep.records().at(8).dirty==1);
        keep_db.fail=false;check(keep.save() && keep.records().at(7).dirty==0 && keep.records().at(8).dirty==0);
        check((keep_db.operations==std::vector<std::string>{"EXEC usp_Save_Keep_E ?,?,?","EXEC usp_Save_Keep_E ?,?,?","EXEC usp_Create_Keep_E ?,?,?"}));
        AgentWorldDatabase composed_db;std::vector<unsigned> composed_events;SavePhaseFixture composed_guild{composed_events,3};
        WorldDbAgentWorld composed(composed_db,composed_guild,100U,123U);
        const auto initial_packets=composed.handshake_world_packets(2100U);
        check((initial_packets==std::vector<eternity::net::Packet>{{4,2,125,0,0,0},{5,2,12,0,0,0,70,0,0,0},{6,2,1,7,0,0,0,1,100,0,0,0}}));
        check(composed.world_day(1099U)==123U && composed.world_day(1100U)==124U);
        AgentWorldDatabase wrap_db;WorldDbAgentWorld wrap(wrap_db,composed_guild,0xffffff00U,0xffffffffU);
        check(wrap.world_day(0x2e8U)==0U);
        check(composed.receive(eternity::net::Packet{5,2,10,0,0,0,20,0,0,0}));
        check(composed.receive(eternity::net::Packet{7,2,7,0,0,0,2,8,0,0,0}));
        check(composed.periodic(100U).success() && composed_db.writes.size()==2 && (composed_events==std::vector<unsigned>{3}));
        check(composed.kills().counts()[0]==10 && composed.obelisks().records().at(7).dirty==0 && !composed.periodic(100U).due);
        AgentWorldDatabase startup_db;AgentCalendarFixture calendar;const auto base=std::chrono::sys_days{std::chrono::year{2024}/1/1};
        auto startup=make_world_db_agent_world(startup_db,composed_guild,base,calendar,[&]{calendar.calls.push_back(4);return 4242U;});
        check((calendar.calls==std::vector<unsigned>{1,2,3,4}) && startup->world_day(4242U)==86400U && startup->world_day(5242U)==86401U);
    }
    {
        auto root=std::make_shared<KillTimerRootFixture>();eternity::net::Packet data;put(data,3);
        for(const auto [country,threshold]:std::array<std::pair<std::uint8_t,std::uint32_t>,3>{{{0,50},{0,100},{1,70}}}) {
            put(data,country,1);put(data,threshold);put(data,0,2);for(int i=0;i<6;++i){put(data,0,1);put(data,0,2);}
        }
        auto definitions=std::make_shared<BlessingDefinitions>(BlessingDefinitions::decode(data));std::array<WorldKillCountCountry,2> initial{};
        initial[0].count=10;initial[0].next_threshold=50;initial[1].count=12;initial[1].next_threshold=70;
        std::vector<eternity::net::Packet> database,game_log;bool disabled=false;
        auto state=WorldKillCountState<KillTimerRootFixture>::create_bound(root,definitions,initial,[]{return 123U;},
            [&](const eternity::net::Packet& packet){database.push_back(packet);},[&]{return disabled;},[&](const eternity::net::Packet& packet){game_log.push_back(packet);});
        struct Routes {std::function<void(std::int32_t,std::int32_t)> add_kill_count,subtract_kill_count;std::function<bool(std::int32_t)> kill_count_status14;} routes;
        bind_world_kill_count_obelisks(state,routes);
        routes.add_kill_count(0,40);check(state->country(0).count==50 && state->country(0).index==1 && state->country(0).next_threshold==100 && root->obelisk_resets==0);
        routes.add_kill_count(0,50);check(state->country(0).count==100 && routes.kill_count_status14(0) && root->obelisk_resets==1 && state->country(1).count==0);
        check((root->applies==std::vector<std::uint8_t>{0,0,1}) && (root->requests==std::vector<std::uint8_t>{1}));
        check((database==std::vector<eternity::net::Packet>{{0x05,0x02,100,0,0,0,0,0,0,0}}) && (game_log==std::vector<eternity::net::Packet>{{0x01,0x0a,0}}));
        routes.add_kill_count(1,20);routes.subtract_kill_count(0,50);check(state->country(1).count==0 && state->country(0).count==100);
        state->deadlines(0).reset=0;disabled=true;routes.subtract_kill_count(0,60);check(state->country(0).count==100);
        disabled=false;routes.subtract_kill_count(0,60);check(state->country(0).count==40 && state->country(0).index==0 && state->country(0).next_threshold==50);
        routes.subtract_kill_count(0,100);check(state->country(0).count==0 && state->country(0).index==0);
        auto grade_root=std::make_shared<KillTimerRootFixture>();std::array<WorldKillCountCountry,2> grade_initial{};
        grade_initial[0].count=60;grade_initial[0].index=1;grade_initial[0].next_threshold=100;grade_initial[1].count=12;grade_initial[1].next_threshold=70;
        std::vector<eternity::net::Packet> grade_database,grade_log;bool grade_disabled=false;
        auto grade=WorldKillCountState<KillTimerRootFixture>::create_bound(grade_root,definitions,grade_initial,[]{return 123U;},
            [&](const eternity::net::Packet& packet){grade_database.push_back(packet);},[&]{return grade_disabled;},[&](const eternity::net::Packet& packet){grade_log.push_back(packet);});
        check(grade->adjust_grade(0,-1) && grade->country(0).count==0 && grade->country(0).index==1 && grade_root->applies.empty());
        check(!grade->adjust_grade(0,-2) && !grade->adjust_grade(0,0x7fffffff) && !grade->adjust_grade(2,1));
        grade_disabled=true;check(!grade->adjust_grade(0,1));grade_disabled=false;
        check(grade->adjust_grade(0,100) && grade->country(0).count==100 && grade->country(0).index==2 && grade->status14(0));
        check(grade_root->obelisk_resets==1 && grade->country(1).count==0 && !grade->adjust_grade(1,1));
        check((grade_database==std::vector<eternity::net::Packet>{{0x05,0x02,100,0,0,0,0,0,0,0}}) && (grade_log==std::vector<eternity::net::Packet>{{0x01,0x0a,0}}));
    }
    {
        VerticalSessionRegistry<GlobalSendSessionFixture> registry;auto selection=std::make_shared<GlobalSendSessionFixture>();selection->user.user_id=99;
        auto connected=std::make_shared<GlobalSendSessionFixture>();connected->user.location=0;registry.reserve(99,selection);
        unsigned unreserved_lookups=0;std::vector<std::string> sends;bool keep_global_send=true,stop_on_now=false;
        WorldRetainedGlobalSend<GlobalSendSessionFixture> global(registry,[&](WorldAdmissionUser& user){++unreserved_lookups;return &user==&connected->user?connected:std::shared_ptr<GlobalSendSessionFixture>{};},
            [&](GlobalSendSessionFixture& session){sends.push_back(&session==selection.get()?"selection-now":"connected-now");if(stop_on_now)keep_global_send=false;},
            [&](GlobalSendSessionFixture& session){sends.push_back(&session==selection.get()?"selection-priority":"connected-priority");},[&]{return keep_global_send;});
        auto selection_pin=global.retain(selection->user);auto connected_pin=global.retain(connected->user);
        check(registry.release(99,*selection));global.flush(selection->user);global.flush(connected->user);
        check(unreserved_lookups==1 && (sends==std::vector<std::string>{"selection-now","selection-priority","connected-now","connected-priority"}));
        selection_pin.reset();bool missing_owner=false;try{global.flush(selection->user);}catch(const std::logic_error&){missing_owner=true;}check(missing_owner);
        connected_pin.reset();global.flush(connected->user);check(unreserved_lookups==3);
        sends.clear();stop_on_now=true;global.flush(connected->user);check((sends==std::vector<std::string>{"connected-now"}));
        const auto stopped_lookups=unreserved_lookups;global.flush(connected->user);check(unreserved_lookups==stopped_lookups && sends.size()==1);
    }
    {
        VerticalSessionRegistry<GlobalSendSessionFixture> registry;auto connection=std::make_shared<GlobalSendSessionFixture>();connection->user.user_id=321;registry.reserve(321,connection);
        unsigned reasons=0;connection->socket.disconnect_with_reason=[&](std::uint32_t reason){check(reason==8);++reasons;};
        auto sessions=std::make_shared<WorldRetainedGlobalSend<GlobalSendSessionFixture>>(registry,
            [](WorldAdmissionUser&){return std::shared_ptr<GlobalSendSessionFixture>{};},
            [](GlobalSendSessionFixture&){throw std::logic_error("unexpected immediate send");},
            [](GlobalSendSessionFixture&){throw std::logic_error("unexpected priority send");},[]{return true;});
        WorldRetainedSessionLost<GlobalSendSessionFixture> lost(sessions,[&](GlobalSendSessionFixture& session){check(&session==connection.get());return 123U;});
        auto pin=lost.retain(connection->user);check(bool(pin) && registry.release(321,*connection));
        check(lost.connected_tick(connection->user)==123U);lost.close(connection->user,8);check(reasons==1);
        bool wrong_reason=false;try{lost.close(connection->user,9);}catch(const std::invalid_argument&){wrong_reason=true;}check(wrong_reason && reasons==1);
        connection->socket.disconnect_with_reason={};bool missing_close=false;try{lost.close(connection->user,8);}catch(const std::logic_error&){missing_close=true;}check(missing_close);
        pin.reset();bool missing_session=false;try{(void)lost.connected_tick(connection->user);}catch(const std::logic_error&){missing_session=true;}check(missing_session);
    }
    {
        auto root=std::make_shared<SendCompositionRootFixture>();auto connection=std::make_shared<GlobalSendSessionFixture>();
        connection->user.user_id=456;connection->user.location=0;root->registry.reserve(456,connection);root->admission_.track_connected(connection->user);
        std::vector<std::string> sends;unsigned closed=0;connection->socket.disconnect_with_reason=[&](std::uint32_t reason){check(reason==8);++closed;};
        using Composition=WorldSendComposition<SendCompositionRootFixture,GlobalSendSessionFixture>;
        Composition::Inputs inputs{[](WorldAdmissionUser&){return std::shared_ptr<GlobalSendSessionFixture>{};},
            [&](GlobalSendSessionFixture&){sends.push_back("now");},[&](GlobalSendSessionFixture&){sends.push_back("priority");},
            [](GlobalSendSessionFixture&){return 100U;},std::make_shared<RootSendRemainingServices>(),[]{return 100U;},250,
            std::make_shared<std::atomic<std::uint32_t>>(0),[](std::exception_ptr){throw std::logic_error("unexpected send failure");}};
        Composition::bind(root,std::move(inputs));check(root->period==250 && root->update && root->lost);
        root->update->update_packets();root->update->update_net_send();root->update->update_session_lost(30101);
        check(root->owner.packets_seen==1 && (sends==std::vector<std::string>{"now","priority"}) && closed==1);
        root->admission_.untrack(connection->user);check(root->registry.release(456,*connection));
    }
    const std::array<std::uint32_t,6> loop_ticks{100,120,351,5077,5077,5077};std::size_t tick_index=0;unsigned updates=0;IdleFixture loop_idle;std::exception_ptr loop_failure;
    auto shared_overruns=std::make_shared<std::atomic<std::uint32_t>>(0);
    auto send_fixture=std::make_shared<SendDriverFixture>();auto send_overruns=std::make_shared<std::atomic<std::uint32_t>>(0);
    const std::array<std::uint32_t,6> send_ticks{100,120,351,500,500,500};std::size_t send_tick_index=0;std::exception_ptr send_driver_failure;
    auto send_driver=WorldSendDriver<SendDriverFixture>::create(loop_strand,send_fixture,20,[&]{return send_ticks.at(send_tick_index++);},send_overruns,[&](std::exception_ptr error){send_driver_failure=error;});
    asio::post(loop_strand,[&]{send_driver->start();});loop_io.run();
    check(!send_driver_failure && send_fixture->calls==2 && send_tick_index==6 && send_overruns->load()==1);
    loop_io.restart();auto canceled_send_fixture=std::make_shared<SendDriverFixture>();
    auto canceled_send_driver=WorldSendDriver<SendDriverFixture>::create(loop_strand,canceled_send_fixture,20,[]{return 100U;},send_overruns,[&](std::exception_ptr error){send_driver_failure=error;});
    asio::post(loop_strand,[&]{canceled_send_driver->start();canceled_send_driver->stop();});loop_io.run();check(canceled_send_fixture->calls==0 && !send_driver_failure);
    loop_io.restart();auto failed_send_fixture=std::make_shared<SendDriverFixture>();failed_send_fixture->fail=true;
    auto failed_send_driver=WorldSendDriver<SendDriverFixture>::create(loop_strand,failed_send_fixture,20,[]{return 100U;},send_overruns,[&](std::exception_ptr error){send_driver_failure=error;});
    asio::post(loop_strand,[&]{failed_send_driver->start();});loop_io.run();check(failed_send_fixture->calls==1 && bool(send_driver_failure));
    loop_io.restart();
    auto driver=WorldFrameDriver::create(loop_strand,loop_world,shared_overruns,[&]{return loop_ticks.at(tick_index++);},[&](VerticalWorld& owner,std::uint32_t tick){
        ++updates;if(updates==1) {check(tick==100);return true;}
        check(tick==5077);check(owner.update_primary_mob_idle(0,tick,f9->random,loop_idle,f9->init,linked));return false;
    },[&](std::exception_ptr error){loop_failure=error;});
    asio::post(loop_strand,[&]{driver->start();});loop_io.run();
    loop_io.restart();std::uint32_t overruns=0;asio::post(loop_strand,[&]{overruns=driver->overruns();});loop_io.run();
    check(!loop_failure && updates==2 && tick_index==6 && overruns==1 && loop_idle.calls>0);
    loop_io.restart();std::exception_ptr driver_failure;unsigned failed_updates=0;
    auto failing_driver=WorldFrameDriver::create(loop_strand,loop_world,shared_overruns,[]{return 1U;},[&](VerticalWorld&,std::uint32_t)->bool{++failed_updates;throw std::runtime_error("expected frame failure");},[&](std::exception_ptr error){driver_failure=error;});
    asio::post(loop_strand,[&]{failing_driver->start();});loop_io.run();check(failed_updates==1 && driver_failure);
    loop_io.restart();unsigned canceled_updates=0;
    auto canceled_driver=WorldFrameDriver::create(loop_strand,loop_world,shared_overruns,[]{return 1U;},[&](VerticalWorld&,std::uint32_t){++canceled_updates;return true;},[&](std::exception_ptr error){loop_failure=error;});
    asio::post(loop_strand,[&]{canceled_driver->start();canceled_driver->stop();});loop_io.run();check(!loop_failure && canceled_updates==0);
    loop_io.restart();unsigned native_visits=0;std::uint32_t native_tick=0;const auto before_native=world_frame_tick_ms();
    auto native_driver=WorldFrameDriver::create(loop_strand,loop_world,shared_overruns,[&](VerticalWorld& owner,std::uint32_t tick){
        native_tick=tick;owner.visit_primary_zones([&](RuntimeZone& zone){check(zone.map && zone.mobs);++native_visits;});return false;
    },[&](std::exception_ptr error){loop_failure=error;});
    asio::post(loop_strand,[&]{native_driver->start();});loop_io.run();const auto after_native=world_frame_tick_ms();
    check(!loop_failure && native_visits==2 && native_tick-before_native<=after_native-before_native);
    loop_io.restart();asio::post(loop_strand,[&]{check(native_driver->overruns()==shared_overruns->load());});loop_io.run();
    auto f10=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root10;
    VerticalWorldPorts startup_ports{[&](MobPrimaryZones&){attach(f10,root10);},{},[]{return 100U;}};
    std::shared_ptr<VerticalWorld> startup_world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f10,&f10->backend),std::move(f10->pool),transport,startup_ports);root10=startup_world->zones();
    unsigned startup_visits=0;auto startup_driver=WorldFrameDriver::create(loop_strand,startup_world,shared_overruns,[&](VerticalWorld& owner,std::uint32_t){
        owner.visit_primary_zones([&](RuntimeZone& zone){check(bool(zone.mobs));++startup_visits;});return false;
    },[&](std::exception_ptr error){loop_failure=error;});
    loop_io.restart();bool before_start_rejected=false;asio::post(loop_strand,[&]{try{startup_driver->start();}catch(const std::logic_error&){before_start_rejected=true;}});loop_io.run();
    check(before_start_rejected && startup_visits==0 && !loop_failure && !startup_world->running());
    check(startup_world->start() && startup_world->running());loop_io.restart();asio::post(loop_strand,[&]{startup_driver->start();});loop_io.run();
    check(!loop_failure && startup_visits==2 && startup_world->shutdown() && !startup_world->running() && root10->mob_pool().available()==root10->mob_pool().size());
    auto f11=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root11;
    VerticalWorldPorts shutdown_ports{[&](MobPrimaryZones&){attach(f11,root11);},{},[]{return 100U;}};
    std::shared_ptr<VerticalWorld> shutdown_world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f11,&f11->backend),std::move(f11->pool),transport,shutdown_ports);root11=shutdown_world->zones();check(shutdown_world->start());
    std::shared_ptr<WorldFrameDriver> shutdown_driver;unsigned shutdown_visits=0;bool deferred=false;
    shutdown_driver=WorldFrameDriver::create(loop_strand,shutdown_world,shared_overruns,[]{return 77U;},[&](VerticalWorld& owner,std::uint32_t){
        owner.visit_primary_zones([&](RuntimeZone& zone){check(bool(zone.mobs));++shutdown_visits;deferred=!shutdown_driver->shutdown();});return true;
    },[&](std::exception_ptr error){loop_failure=error;});
    loop_io.restart();asio::post(loop_strand,[&]{shutdown_driver->start();});loop_io.run();
    bool drained=false;loop_io.restart();asio::post(loop_strand,[&]{drained=shutdown_driver->shutdown();});loop_io.run();
    check(!loop_failure && deferred && drained && shutdown_visits==1 && !shutdown_world->running() && root11->mob_pool().available()==root11->mob_pool().size());
    struct TestSession {};
    auto f12=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root12;
    VerticalWorldPorts drain_ports{[&](MobPrimaryZones&){attach(f12,root12);},{},[]{return 100U;}};
    std::shared_ptr<VerticalWorld> drain_world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f12,&f12->backend),std::move(f12->pool),transport,drain_ports);root12=drain_world->zones();check(drain_world->start());auto drain_held=root12->lease(0);check(bool(drain_held));
    asio::io_context drain_io;auto drain_guard=asio::make_work_guard(drain_io);WorldFrameDriver::Strand drain_strand=asio::make_strand(drain_io);
    DbJobDispatcher dispatcher(false,1);VerticalSessionRegistry<TestSession> registry;auto session=std::make_shared<TestSession>();
    std::promise<void> setup,worker_started,worker_continue,finished;auto setup_done=setup.get_future(),worker_ready=worker_started.get_future(),finish_done=finished.get_future();auto worker_allowed=worker_continue.get_future().share();
    std::shared_ptr<VerticalTrackedDbJobs<TestSession>> tracker;std::shared_ptr<WorldShutdownCoordinator<TestSession>> shutdown_group;unsigned delivered=0,unexpected_frames=0;
    auto drain_driver=WorldFrameDriver::create(drain_strand,drain_world,shared_overruns,[&](VerticalWorld& owner,std::uint32_t){owner.visit_primary_zones([&](RuntimeZone&){++unexpected_frames;});return true;},[&](std::exception_ptr error){finished.set_exception(error);});
    std::jthread drain_thread([&](std::stop_token stop){std::stop_callback on_stop(stop,[&]{drain_io.stop();});drain_io.run();});
    asio::post(drain_strand,[&]{try {
        registry.reserve(42,session);
        tracker=VerticalTrackedDbJobs<TestSession>::create(drain_strand,registry,session,42,dispatcher,
            [&](TestSession&,std::uint32_t,const VerticalTrackedDbJobs<TestSession>::Retry&){worker_started.set_value();worker_allowed.wait();return std::vector<eternity::net::Packet>{{1,2}};},
            [&](TestSession&,std::vector<eternity::net::Packet> packets){check(packets.size()==1);++delivered;},[&](std::exception_ptr error){finished.set_exception(error);});
        check(tracker->submit(1));drain_driver->start();
        shutdown_group=WorldShutdownCoordinator<TestSession>::start(drain_strand,drain_driver,registry,{tracker},[&]{try {
            check(delivered==1 && registry.pending_database_work(42)==0 && !shutdown_group->try_finish());
            check(registry.release(42,*session) && !shutdown_group->try_finish());drain_held.reset();check(shutdown_group->try_finish());finished.set_value();
        }catch(...){finished.set_exception(std::current_exception());}});
        check(!tracker->submit(2) && !shutdown_group->try_finish() && drain_world->running());setup.set_value();
    }catch(...){setup.set_exception(std::current_exception());}});
    setup_done.get();worker_ready.get();worker_continue.set_value();finish_done.get();
    std::promise<void> empty_done;auto empty_finished=empty_done.get_future();std::shared_ptr<WorldShutdownCoordinator<TestSession>> empty_group;bool start_returned=false;
    asio::post(drain_strand,[&]{try {
        empty_group=WorldShutdownCoordinator<TestSession>::start(drain_strand,drain_driver,registry,{},[&]{try{check(start_returned && empty_group->try_finish());empty_done.set_value();}catch(...){empty_done.set_exception(std::current_exception());}});
        start_returned=true;
    }catch(...){empty_done.set_exception(std::current_exception());}});
    empty_finished.get();dispatcher.wait_idle();dispatcher.shutdown();drain_guard.reset();drain_thread.join();
    check(!dispatcher.failure() && unexpected_frames==0 && !drain_world->running() && root12->mob_pool().available()==root12->mob_pool().size());
    check(loop_world->shutdown() && root9->mob_pool().available()==root9->mob_pool().size());
    struct RootSession {};
    auto f13=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root13;auto admission_services=std::make_shared<RootAdmissionServices>();
    auto other=std::make_shared<RootOtherServices>();auto corpse_services=std::make_shared<RootCorpseServices>();NpcDefinitions npc_definitions;
    MobZoneServicesInputs root_inputs{f13,*f13->pool,f13->runtime,npc_definitions,f13->init,other->corpses,
        [f13,other,corpse_services](RuntimeZone& zone){return MobZoneServicesPorts{std::make_shared<RootBoundMobs>(f13,zone),other,corpse_services,other};},
        [](const ConfiguredMap&)->std::span<const LinkedMobGroup>{return {};},[]{return 123456U;},
        [other](Npc& npc){other->npc_pool.release(npc);},[](RuntimeZone&){throw std::logic_error("unexpected instance queue");},[]{return ZoneLocalTime{1,2,3,4};}};
    VerticalWorldPorts root_ports{[&](MobPrimaryZones&){attach(f13,root13);},{},[]{return 100U;}};
    using ConcreteRoot=WorldRoot<RootSession>;unsigned root_frames=0,root_visits=0;std::exception_ptr root_failure;
    IdleFixture root_idle;auto root_phase_services=std::make_shared<PhaseFixture>(*f13,root_idle,linked,f13);auto root_weather=std::make_shared<RootWeatherServices>(f13->random);
    auto root_npc_idle=std::make_shared<RootNpcIdleServices>(other->npc_pool,f13->random);NpcDefinition patrol_definition;patrol_definition.record[8]=5;patrol_definition.record[12]=100;
    MapNpc patrol_source;patrol_source.type=1;patrol_source.positions={{{10,0,10},0},{{11,0,10},0}};Npc* patrol=nullptr;
    auto root_items=std::make_shared<ItemDefinitions>();EnchantValues root_enchant;MobAttackTemplates root_mobs;RootAttackScene attack_scene;RootAttackWorld attack_world;
    auto attack_actor=std::make_shared<Actor>(901,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},attack_scene,ActorEquipmentRules{*root_items,root_enchant,root_mobs});
    attack_actor->account_id()=99;attack_actor->attack_state().mode=2;attack_actor->attack_state().next_tick=1;attack_actor->status().current_target_type=2;attack_actor->status().current_target=77;attack_actor->status().skills.equipment.attack_speed=4;
    auto attack_services=std::make_shared<RootAttackServices>(attack_actor,attack_world);auto prefix_services=std::make_shared<RootPrefixServices>();prefix_services->actor=attack_actor;prefix_services->frame=f13;prefix_services->items=root_items;RuntimeZone* attack_zone=nullptr;MobRuntime* expired_mob=nullptr;
    SkillDefinition expiring_definition;expiring_definition.id=234;expiring_definition.level=1;EffectTimer expiring;expiring.effect.skill_id=234;expiring.effect.level=1;expiring.end=0;attack_actor->effects().append(444,expiring,expiring_definition);
    WorldAdmissionUser waiting,actor_logout;waiting.user_id=99;auto root_session=std::make_shared<RootSession>();
    auto logout_pin=std::make_shared<std::pair<std::shared_ptr<Actor>,std::shared_ptr<RootSession>>>(attack_actor,root_session);
    auto logout_services=std::make_shared<RootLogoutServices>(logout_pin);logout_services->actor=attack_actor.get();logout_services->actor_user=&actor_logout;
    auto quest_definitions=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(root_quest_data()));check(quest_definitions->find(1) && quest_definitions->find(1)->maximum_minutes()==1);
    RootQuestPoolFactory quest_factory;auto quest_pool=std::make_shared<QuestRuntimePool>(quest_factory);auto quest_runtime=std::make_shared<QuestRuntime>(99,*quest_pool);RootQuestPorts quest_ports;
    RootQuestClock quest_clock;auto quest_ids=std::make_shared<ItemIds>(1,quest_clock);
    auto quest_item_pool=std::shared_ptr<GroundItemPool>(other,&other->item_pool);
    auto quest_pin=std::make_shared<RootQuestPin>();quest_pin->actor=attack_actor;quest_pin->runtime=quest_runtime;quest_pin->definitions=quest_definitions;
    auto quest_services=std::make_shared<RootQuestServices>(quest_pin);
    RestoredQuest timed;timed.definition=quest_definitions->find(1);timed.packet.id=1;timed.packet.end_tick=0;check(quest_runtime->load_active(timed)!=nullptr);
    // A live root and the socket-facing owner must share both the registry
    // and the same admission queues, and pin that owner for their lifetime.
    struct SharedRootAdmission {VerticalSessionRegistry<RootSession> sessions;std::shared_ptr<RootAdmissionServices> services;WorldAdmission admission;
        explicit SharedRootAdmission(std::shared_ptr<RootAdmissionServices> value):services(std::move(value)),admission(*services) {}};
    auto live_frame=std::make_shared<Frame>(maps);auto live_owner=std::make_shared<SharedRootAdmission>(std::make_shared<RootAdmissionServices>());
    std::weak_ptr<SharedRootAdmission> weak_live_owner=live_owner;
    auto live_other=std::make_shared<RootOtherServices>();auto live_corpses=std::make_shared<RootCorpseServices>();NpcDefinitions live_npcs;
    MobZoneServicesInputs live_inputs{live_frame,*live_frame->pool,live_frame->runtime,live_npcs,live_frame->init,live_other->corpses,
        [live_frame,live_other,live_corpses](RuntimeZone& zone){return MobZoneServicesPorts{std::make_shared<RootBoundMobs>(live_frame,zone),live_other,live_corpses,live_other};},
        [](const ConfiguredMap&)->std::span<const LinkedMobGroup>{return {};},[]{return 123456U;},
        [live_other](Npc& npc){live_other->npc_pool.release(npc);},[](RuntimeZone&){throw std::logic_error("unexpected instance queue");},[]{return ZoneLocalTime{1,2,3,4};}};
    VerticalWorldPorts live_ports{[](MobPrimaryZones&){},{},[]{return 100U;}};
    auto live_root=ConcreteRoot::create_with_session_owner(loop_strand,maps,std::move(live_inputs),std::move(live_frame->pool),transport,std::move(live_ports),
        std::shared_ptr<VerticalSessionRegistry<RootSession>>(live_owner,&live_owner->sessions),std::shared_ptr<WorldAdmission>(live_owner,&live_owner->admission),[live_owner]{live_owner->admission.update_packets();},[live_owner](RuntimeZone& zone){live_owner->admission.update_zone_wait_packets(zone);},shared_overruns,
        [](ConcreteRoot&,std::uint32_t){return false;},[](std::exception_ptr){throw std::logic_error("unexpected live root failure");});
    auto* expected_registry=&live_owner->sessions;auto* expected_admission=&live_owner->admission;live_owner.reset();
    WorldAdmissionUser live_waiting;live_waiting.user_id=71;live_waiting.location=0;
    loop_io.restart();asio::post(loop_strand,[&]{auto owner=weak_live_owner.lock();check(owner && &live_root->sessions()==expected_registry && &live_root->admission()==expected_admission);
        owner->admission.track_connected(live_waiting);live_root->update_admission();check(owner->services->packets_seen==1 && live_root->admission().connected_count()==1);
        live_root->admission().untrack(live_waiting);RuntimeZone waiting_zone;owner->services->zone=&waiting_zone;
        live_waiting.location=1;owner->admission.track_selection(live_waiting);check(owner->admission.wait(live_waiting));live_root->update_admission();
        check(live_waiting.location==3 && live_root->admission().zone_waiting_count(waiting_zone)==1);
        live_root->update_zone_wait_packets(waiting_zone);check(owner->services->packets_seen==2);
        live_root->admission().untrack(live_waiting);check(owner->admission.empty());});loop_io.run();
    live_root.reset();check(weak_live_owner.expired());
    VerticalSessionRegistry<RootSession>* admission_registry=nullptr;
    auto concrete=ConcreteRoot::create(loop_strand,maps,std::move(root_inputs),std::move(f13->pool),transport,root_ports,
        ConcreteRoot::AdmissionFactory([&](auto& registry){admission_registry=&registry;return admission_services;}),shared_overruns,
        [&](ConcreteRoot& root,std::uint32_t now){++root_frames;check(root.thread_zones().active_count()==2 && root.thread_zones().next_promotion()==now+100U);root.world().visit_primary_zones([&](RuntimeZone& zone){check(bool(zone.mobs));++root_visits;});
            root_idle.expected=now;check(root.update_primary_frame_prefix(now));check(root.update_primary_frame_prefix(now));
            check(root.update_primary_weather_phase(now)==PrimaryWeatherResult::completed);return false;},
        [&](std::exception_ptr error){root_failure=error;});
    bool root_started=false;
    loop_io.restart();asio::post(loop_strand,[&]{
        bool missing_phase=false;try{(void)concrete->update_primary_mob_phase(0);}catch(const std::logic_error&){missing_phase=true;}check(missing_phase);
        bool missing_attack=false;try{(void)concrete->update_primary_user_attack_phase(0);}catch(const std::logic_error&){missing_attack=true;}check(missing_attack);
        bool missing_quests=false;try{(void)concrete->update_primary_quest_phase(0);}catch(const std::logic_error&){missing_quests=true;}check(missing_quests);
        bool missing_weather=false;try{(void)concrete->update_primary_weather_phase(0);}catch(const std::logic_error&){missing_weather=true;}check(missing_weather);
        bool missing_logout=false;try{(void)concrete->update_primary_logout_phase(0);}catch(const std::logic_error&){missing_logout=true;}check(missing_logout);
        bool missing_npc=false;try{(void)concrete->update_primary_npc_idle_phase(0);}catch(const std::logic_error&){missing_npc=true;}check(missing_npc);
        bool missing_prefix=false;try{(void)concrete->update_primary_frame_prefix(0);}catch(const std::logic_error&){missing_prefix=true;}check(missing_prefix);
        concrete->bind_primary_frame_prefix(prefix_services);concrete->bind_primary_npc_idle_phase(root_npc_idle);concrete->bind_primary_logout_phase(logout_services);concrete->bind_primary_weather_phase(root_weather);concrete->bind_primary_quest_phase(quest_services);concrete->bind_primary_user_attack_phase(attack_services);concrete->bind_primary_mob_phase(root_phase_services);root13=concrete->world().zones();root_started=concrete->start();
        concrete->world().visit_primary_zones([&](RuntimeZone& zone){check(bool(zone.environment));auto& weather=zone.environment->weather();weather.definition.rate=100;weather.definition.state=1;weather.definition.power=2;weather.next_update=0;weather.active=0;
            if(!admission_services->zone){admission_services->zone=&zone;attack_zone=&zone;check(bool(zone.users) && bool(zone.npcs));zone.users->attach(*attack_actor,0,0,{});
            for(auto* mob:zone.mobs->all()) if(!mob->lua_ai && (!mob->respawn || !mob->respawn->both)){expired_mob=mob;break;}check(expired_mob);auto* active=expired_mob->applied_skills().owner().append_from_pool();check(active);active->definition=f13->full.skill_definition(1);check(active->definition);active->end_tick=0;
            patrol=other->npc_pool.allocate();check(patrol);patrol->id=501;patrol->type=1;patrol->position={10,0,10};patrol->source=&patrol_source;patrol->definition=&patrol_definition;patrol->next_position=1;check(zone.npcs->enter(*patrol));
            quest_pin->world=std::make_shared<VerticalQuests>(QuestStartItemsActorWorldOwners{root_items,quest_item_pool,quest_ids,quest_definitions,quest_pool,attack_actor,quest_runtime},QuestStartItemsActorUseState{quest_pin->last_item,quest_pin->deadline},*zone.npcs,other->npc_pool,quest_ports);
        }});
        check(admission_registry==&concrete->sessions());check(concrete->admission().empty());concrete->sessions().reserve(99,root_session);concrete->admission().track_selection(waiting);
        check(!concrete->admission().empty() && concrete->admission().wait(waiting) && !concrete->admission().empty());
        concrete->update_admission();check(waiting.zone==admission_services->zone && waiting.location==3);
        concrete->admission().update_zone_wait_packets(*waiting.zone);check(admission_services->packets_seen==1 && admission_services->registered==1);admission_services->on_packet=[&]{prefix_services->events.push_back("wait");};
        waiting.logout_type=2;waiting.logout_tick=100;actor_logout.logout_type=7;actor_logout.logout_tick=100;
    });loop_io.run();check(root_started && !root_failure && root_frames==1 && root_visits==2 && root_idle.calls>0);
    check((root_phase_services->events==std::vector<std::string>{"mob0","trap0","mob1","trap1"}));
    check((prefix_services->events==std::vector<std::string>{"wait","packet0","clock0","reset0","clock1","recover0","pvp0","rebirth0","tick0","timeout0","notice0","mob-attack0","mob-attack1","wait","packet0","clock0","reset0","clock1"}) && attack_world.hits==1 && attack_services->zones==std::vector<int>{0,1});
    loop_io.restart();asio::post(loop_strand,[&]{
        auto instance_map=maps->all().front();instance_map.settings.create_type=1;RuntimeZone instance_probe;instance_probe.map=&instance_map;
        concrete->thread_zones().add_active(instance_probe);bool rejected_instance=false;try{(void)concrete->update_primary_frame_prefix(100);}catch(const std::logic_error&){rejected_instance=true;}
        check(rejected_instance);concrete->thread_zones().remove_active(instance_probe);
        auto* keep=expired_mob->applied_skills().owner().append_from_pool();check(keep);keep->definition=f13->full.skill_definition(1);keep->end_tick=0xffffffffU;keep->name="unvisited";
        check(update_primary_effect_ticks(concrete->world(),*attack_zone,*prefix_services,100) && keep->name=="S");
        keep->name="unvisited";const auto old_status=expired_mob->status;auto old_stack=expired_mob->state_stack;expired_mob->state_stack.reset();expired_mob->status=2;
        attack_actor->attack_state().mode=1;auto previous_events=prefix_services->events.size();
        check(update_primary_effect_ticks(concrete->world(),*attack_zone,*prefix_services,200) && keep->name=="unvisited" && prefix_services->events.size()==previous_events);
        expired_mob->status=old_status;expired_mob->state_stack=std::move(old_stack);attack_actor->attack_state().mode=2;expired_mob->applied_skills().clear();
        RuntimeZone missing_users;bool missing_packet_users=false;try {(void)update_primary_zone_packets(concrete->world(),concrete->admission(),missing_users,*prefix_services);}catch(const std::logic_error&){missing_packet_users=true;}check(missing_packet_users);
        check(concrete->update_primary_logout_phase(99)==PrimaryLogoutResult::completed && logout_services->events.empty());
        check(concrete->update_primary_logout_phase(1098)==PrimaryLogoutResult::skipped);
        check(concrete->update_primary_logout_phase(1099)==PrimaryLogoutResult::completed);
        check((logout_services->events==std::vector<std::string>{"wait99","actor901"}) && waiting.logout_tick==0 && actor_logout.logout_tick==0);
        WorldPrimaryLogoutPhase wrap(logout_services);
        check(wrap.update(concrete->world(),concrete->admission(),0xfffffff0U)==PrimaryLogoutResult::completed && wrap.next_deadline()==984);
        check(wrap.update(concrete->world(),concrete->admission(),983)==PrimaryLogoutResult::skipped && wrap.update(concrete->world(),concrete->admission(),984)==PrimaryLogoutResult::completed);
    });loop_io.run();
    check(admission_services->packets_seen==3 && prefix_services->mobs_recovered>0 && expired_mob && expired_mob->applied_skills().owner().records().empty());
    check(attack_actor->effects().size()==0 && attack_scene.effect_database_packets.size()==1 && attack_scene.released_effects==1 && attack_scene.self_packets.size()==2 && attack_scene.self_packets[0]==effect_removed_packet(444));attack_scene.self_packets.erase(attack_scene.self_packets.begin());
    check(patrol && patrol->position.x==11 && other->npc_moves.size()==1 && other->npc_deliveries>0 && root_npc_idle->zones==std::vector<int>{0,1,0,1});
    MapNpc guard_source;guard_source.type=8;guard_source.positions={{{10,0,10},0}};NpcDefinition guard_definition;Npc* guard=nullptr;Npc* disabled=nullptr;Npc* appended=nullptr;Npc* no_source=nullptr;unsigned guard_calls=0;
    loop_io.restart();asio::post(loop_strand,[&]{
        auto make_guard=[&](std::uint32_t id,std::uint32_t disabled_until){auto* npc=other->npc_pool.allocate();check(npc);npc->id=id;npc->type=8;npc->position={10,0,10};npc->source=&guard_source;npc->definition=&guard_definition;npc->disable_time=disabled_until;check(attack_zone->npcs->enter(*npc));return npc;};
        guard=make_guard(601,0);disabled=make_guard(602,0xffffffffU);no_source=make_guard(604,0);no_source->source=nullptr;
        root_npc_idle->guard=[&](RuntimeZone& zone,Npc& npc,std::uint32_t tick){
            check(&zone==attack_zone && &npc==guard && tick==100);++guard_calls;check(zone.npcs->leave(npc));other->npc_pool.release(npc);
            appended=make_guard(603,0xffffffffU);check(appended!=guard);
        };
        check(concrete->update_primary_npc_attack(*attack_zone,100) && guard_calls==1);
        WorldPrimaryNpcIdlePhase probe(root_npc_idle);
        check(probe.update(concrete->world(),100)==PrimaryNpcIdleResult::completed && probe.next_deadline()==3100 && guard_calls==1);
        auto* recycled=other->npc_pool.allocate();check(recycled==guard);other->npc_pool.release(*recycled);
        check(probe.update(concrete->world(),3099)==PrimaryNpcIdleResult::skipped);
        check(probe.update(concrete->world(),3100)==PrimaryNpcIdleResult::completed && probe.next_deadline()==6100);
        check(probe.update(concrete->world(),0xfffffff0U)==PrimaryNpcIdleResult::completed && probe.next_deadline()==2984);
        check(probe.update(concrete->world(),2983)==PrimaryNpcIdleResult::skipped && probe.update(concrete->world(),2984)==PrimaryNpcIdleResult::completed && guard_calls==1);root_npc_idle->guard={};
    });loop_io.run();
    check((root_weather->zones==std::vector<int>{0,1}));
    for(const auto& sink:root_weather->sinks)check(sink.packets==std::vector<eternity::net::Packet>{eternity::net::Packet{0x51,4,1,1,2}});
    loop_io.restart();asio::post(loop_strand,[&]{
        concrete->world().visit_primary_zones([&](RuntimeZone& zone){auto& weather=zone.environment->weather();weather.next_update=0xffffffffU;});
        WorldPrimaryWeatherPhase probe(root_weather);
        check(probe.update(concrete->world(),100)==PrimaryWeatherResult::completed && probe.next_deadline()==60100);
        check(probe.update(concrete->world(),60099)==PrimaryWeatherResult::skipped);
        check(probe.update(concrete->world(),60100)==PrimaryWeatherResult::completed && probe.next_deadline()==120100);
        check(probe.update(concrete->world(),0xfffffff0U)==PrimaryWeatherResult::completed && probe.next_deadline()==59984);
        check(probe.update(concrete->world(),59983)==PrimaryWeatherResult::skipped && probe.update(concrete->world(),59984)==PrimaryWeatherResult::completed);
    });loop_io.run();
    check(quest_runtime->active().empty() && quest_runtime->finished().size()==1 && quest_ports.locks==0 && quest_ports.database.size()==1 && quest_ports.logs.size()==1);
    check(quest_ports.database[0].size()==9 && quest_ports.database[0][0]==2 && quest_ports.database[0][1]==10 && quest_ports.database[0][2]==99 && quest_ports.logs[0][0]==2 && quest_ports.logs[0][1]==5);
    check(attack_scene.self_packets.size()==1 && attack_scene.self_packets[0][0]==3 && attack_scene.self_packets[0][1]==9);
    loop_io.restart();asio::post(loop_strand,[&]{
        auto& service=*prefix_services;service.chat_deadline=100;service.move_deadline=100;
        const auto packets=attack_scene.self_packets.size(),nearby=attack_scene.nearby_packets.size(),events=service.events.size();
        check(update_primary_effects(concrete->world(),*attack_zone,service,100));
        check(service.chat_deadline==100 && service.move_deadline==100 && attack_scene.self_packets.size()==packets && attack_scene.nearby_packets.size()==nearby);
        attack_actor->attack_state().mode=1;check(update_primary_effects(concrete->world(),*attack_zone,service,101));
        check(service.chat_deadline==100 && service.move_deadline==100 && service.events.size()==events+2);
        attack_actor->attack_state().mode=2;check(update_primary_effects(concrete->world(),*attack_zone,service,101));
        check(service.chat_deadline==0 && service.move_deadline==0 && service.events.size()==events+4);
        check(attack_scene.self_packets.size()==packets+2 && attack_scene.self_packets[packets]==eternity::net::Packet({0x0e,0xfa}) && attack_scene.self_packets[packets+1]==eternity::net::Packet({0x10,0xfa}));
        check(attack_scene.nearby_packets.size()==nearby+1);
    });loop_io.run();
    loop_io.restart();asio::post(loop_strand,[&]{
        WorldPrimaryQuestPhase probe(quest_services);
        check(probe.update(concrete->world(),100)==PrimaryQuestPhaseResult::completed && probe.next_deadline()==60100);
        check(probe.update(concrete->world(),60099)==PrimaryQuestPhaseResult::skipped);
        check(probe.update(concrete->world(),60100)==PrimaryQuestPhaseResult::completed && probe.next_deadline()==120100);
        check(probe.update(concrete->world(),0xfffffff0U)==PrimaryQuestPhaseResult::completed && probe.next_deadline()==59984);
        check(probe.update(concrete->world(),59983)==PrimaryQuestPhaseResult::skipped && probe.update(concrete->world(),59984)==PrimaryQuestPhaseResult::completed);
    });loop_io.run();
    loop_io.restart();asio::post(loop_strand,[&]{
        WorldPrimaryUserAttackPhase probe(attack_services);
        check(probe.update(concrete->world(),100)==PrimaryUserAttackResult::completed && probe.next_deadline()==200);
        check(probe.update(concrete->world(),199)==PrimaryUserAttackResult::skipped);
        check(probe.update(concrete->world(),200)==PrimaryUserAttackResult::completed && probe.next_deadline()==300);
        check(probe.update(concrete->world(),0xfffffff0U)==PrimaryUserAttackResult::completed && probe.next_deadline()==84);
        check(probe.update(concrete->world(),83)==PrimaryUserAttackResult::skipped && probe.update(concrete->world(),84)==PrimaryUserAttackResult::completed);
    });loop_io.run();
    loop_io.restart();asio::post(loop_strand,[&]{
        auto* item=other->item_pool.allocate();check(item);item->position={10,0,10};check(attack_zone->items->enter(*item,1,23));
        WorldPrimaryItemPhase items;check(items.update(concrete->world(),100)==PrimaryItemPhaseResult::completed && items.next_deadline()==10100 && attack_zone->items->size()==1);
        check(items.update(concrete->world(),10099)==PrimaryItemPhaseResult::skipped && attack_zone->items->size()==1);
        check(items.update(concrete->world(),10100)==PrimaryItemPhaseResult::completed && attack_zone->items->size()==0 && other->item_pool.statistics().available==1);
        check(other->item_packets.size()==2 && other->item_packets[0][0]==1 && other->item_packets[1][0]==2);
        WorldPrimaryItemPhase wrapping;check(wrapping.update(concrete->world(),0xfffffff0U)==PrimaryItemPhaseResult::completed && wrapping.next_deadline()==9984U);
        check(wrapping.update(concrete->world(),9983)==PrimaryItemPhaseResult::skipped && wrapping.update(concrete->world(),9984)==PrimaryItemPhaseResult::completed);
    });loop_io.run();
    bool root_drained=false;loop_io.restart();asio::post(loop_strand,[&]{
        concrete->begin_shutdown_after_transport_closed({},[&]{
            check(!concrete->try_finish_shutdown());check(concrete->sessions().release(99,*root_session));
            check(!concrete->try_finish_shutdown());concrete->admission().untrack(waiting);check(concrete->admission().empty());
            check(attack_zone->users->detach(attack_actor->id()));quest_pin->world.reset();
            check(attack_zone->npcs->leave(*patrol));other->npc_pool.release(*patrol);check(attack_zone->npcs->leave(*disabled));other->npc_pool.release(*disabled);
            check(attack_zone->npcs->leave(*appended));other->npc_pool.release(*appended);check(attack_zone->npcs->leave(*no_source));other->npc_pool.release(*no_source);root_drained=concrete->try_finish_shutdown();
        });
    });loop_io.run();check(root_drained && root13->mob_pool().available()==root13->mob_pool().size());
    loop_io.restart();asio::post(loop_strand,[&]{check(concrete->thread_zones().active_count()==0 && concrete->thread_zones().pending_count()==0);});loop_io.run();
    auto cfg23=config;cfg23.maps[1].create_type=2;cfg23.maps[1].expire_ms=0;cfg23.maps[1].market=true;
    auto rank_map23=cfg23.maps[1];rank_map23.id=2;rank_map23.create_type=5;rank_map23.market=false;rank_map23.open_times[0]={1,10,2};cfg23.maps.push_back(rank_map23);
    auto maps23=std::make_shared<ServerMaps>(ServerMaps::load(cfg23,argv[1]));auto f23=std::make_shared<Frame>(maps23);std::shared_ptr<MobPrimaryZones> authority23;
    auto weather23=std::make_shared<RootWeatherServices>(f23->random);
    auto market23=std::make_shared<RootMarketServices>();
    auto rank23=std::make_shared<RootRankServices>(attack_actor);
    auto send23=std::make_shared<RootSendServices>(attack_actor);
    auto kill23=std::make_shared<RootKillCountServices>(attack_actor);
    auto db_services23=std::make_shared<RootDbUpdateServices>();auto db_heartbeat23=std::make_shared<std::atomic<std::uint32_t>>(0);auto db_overruns23=std::make_shared<std::atomic<std::uint32_t>>(0);
    WorldAdmissionUser logout_actor23;auto logout23=std::make_shared<RootLogoutServices>(attack_actor);logout23->actor=attack_actor.get();logout23->actor_user=&logout_actor23;
    auto other23=std::make_shared<RootOtherServices>();auto corpses23=std::make_shared<RootCorpseServices>();auto admission23=std::make_shared<RootAdmissionServices>();auto npc23=std::make_shared<RootNpcIdleServices>(other23->npc_pool,f23->random);auto packet23=std::make_shared<RootPrefixServices>();packet23->actor=attack_actor;packet23->frame=f23;packet23->items=root_items;RootAttackWorld attack_world23;auto attack23=std::make_shared<RootAttackServices>(attack_actor,attack_world23);std::weak_ptr<ConcreteRoot> weak23;
    MobZoneServicesInputs input23{f23,*f23->pool,f23->runtime,npc_definitions,f23->init,other23->corpses,
        [f23,other23,corpses23](RuntimeZone& zone){return MobZoneServicesPorts{std::make_shared<RootBoundMobs>(f23,zone),other23,corpses23,other23};},
        [](const ConfiguredMap&)->std::span<const LinkedMobGroup>{return {};},[]{return 123456U;},[other23](Npc& npc){other23->npc_pool.release(npc);},
        [&weak23](RuntimeZone& zone){auto owner=weak23.lock();check(bool(owner));owner->thread_zones().enqueue_wait(zone);},[]{return ZoneLocalTime{1,2,3,4};}};
    IdleFixture idle23;auto mob23=std::make_shared<PhaseFixture>(*f23,idle23,linked,f23);
    auto quest_runtime23=std::make_shared<QuestRuntime>(99,*quest_pool);RootQuestPorts quest_ports23;
    auto quest_pin23=std::make_shared<RootQuestPin>();quest_pin23->actor=attack_actor;quest_pin23->runtime=quest_runtime23;quest_pin23->definitions=quest_definitions;
    auto quest_services23=std::make_shared<RootQuestServices>(quest_pin23);RestoredQuest timed23;timed23.definition=quest_definitions->find(1);timed23.packet.id=1;timed23.packet.end_tick=0;check(quest_runtime23->load_active(timed23)!=nullptr);
    VerticalWorldPorts ports23{[&](MobPrimaryZones&){attach(f23,authority23);},{},[]{return 100U;}};std::exception_ptr failure23;bool drained23=false;std::shared_ptr<WorldShutdownPoller<ConcreteRoot>> poller23;
    DbJobDispatcher db_dispatch23(false,1);auto db_session23=std::make_shared<RootSession>();std::shared_ptr<VerticalTrackedDbJobs<RootSession>> db_tracker23;
    std::promise<void> db_continue23;auto db_allowed23=db_continue23.get_future().share();bool db_signaled23=false;unsigned db_delivered23=0;
    auto world23=ConcreteRoot::create(loop_strand,maps23,std::move(input23),std::move(f23->pool),transport,ports23,admission23,shared_overruns,
        [](ConcreteRoot&,std::uint32_t){throw std::logic_error("unexpected instance fixture frame");return false;},[&](std::exception_ptr error){failure23=error;});weak23=world23;
    auto send_update23=std::make_shared<RootSendUpdateServices>();
    loop_io.restart();asio::post(loop_strand,[&]{
        bool missing_packets=false;try{(void)world23->update_active_packet_phase();}catch(const std::logic_error&){missing_packets=true;}check(missing_packets);
        auto lost23=std::make_shared<RootSessionLostServices>();lost23->pin=attack_actor;world23->bind_session_lost_phase(lost23);
        world23->bind_world_send_update(send_update23,[]{return 100U;});world23->bind_world_send_driver(250,shared_overruns,[&](std::exception_ptr error){failure23=error;});
        bool missing_active_attack=false;try{(void)world23->update_active_user_attack_phase(0);}catch(const std::logic_error&){missing_active_attack=true;}check(missing_active_attack);
        bool missing_active_effects=false;try{(void)world23->update_active_effect_phases(0);}catch(const std::logic_error&){missing_active_effects=true;}check(missing_active_effects);
        bool missing_active_mob_attack=false;try{(void)world23->update_active_mob_npc_attack(0);}catch(const std::logic_error&){missing_active_mob_attack=true;}check(missing_active_mob_attack);
        bool missing_active_idle=false;try{(void)world23->update_active_mob_phase(0);}catch(const std::logic_error&){missing_active_idle=true;}check(missing_active_idle);
        bool missing_active_npc=false;try{(void)world23->update_active_npc_idle_phase(0);}catch(const std::logic_error&){missing_active_npc=true;}check(missing_active_npc);
        bool missing_active_quests=false;try{(void)world23->update_active_quest_phase(0);}catch(const std::logic_error&){missing_active_quests=true;}check(missing_active_quests);
        bool missing_active_weather=false;try{(void)world23->update_active_weather_phase(0);}catch(const std::logic_error&){missing_active_weather=true;}check(missing_active_weather);
        bool missing_market=false;try{(void)world23->update_market_phase(0);}catch(const std::logic_error&){missing_market=true;}check(missing_market);
        bool missing_rank=false;try{(void)world23->update_rank_phase(0);}catch(const std::logic_error&){missing_rank=true;}check(missing_rank);
        bool missing_send=false;try{(void)world23->update_send_phase(false);}catch(const std::logic_error&){missing_send=true;}check(missing_send);
        bool missing_composite=false;try{(void)world23->update_active_through_kill_count(0,[]{return 0U;});}catch(const std::logic_error&){missing_composite=true;}check(missing_composite);
        bool missing_full_frame=false;try{(void)world23->update_active_frame(0,[]{return 0U;});}catch(const std::logic_error&){missing_full_frame=true;}check(missing_full_frame);
        bool missing_registry_logout=false;try{(void)world23->update_registry_logout_phase(0);}catch(const std::logic_error&){missing_registry_logout=true;}check(missing_registry_logout);
        bool missing_active_prefix=false;try{(void)world23->update_active_world_prefix(0);}catch(const std::logic_error&){missing_active_prefix=true;}check(missing_active_prefix);
        std::array<WorldKillCountCountry,2> initial_kills23{};for(auto& value:initial_kills23){value.count=100;value.index=1;}
        using KillComposition23=WorldKillCountComposition<ConcreteRoot>;
        auto kill_state23=KillComposition23::bind(world23,{std::make_shared<BlessingDefinitions>(kill23->data),initial_kills23,[]{return 100U;},
            [](const eternity::net::Packet&){throw std::logic_error("unexpected kill-count DB send");},[]{return false;},
            [](const eternity::net::Packet&){throw std::logic_error("unexpected kill-count log send");},std::make_shared<RootKillCountZonePorts<ConcreteRoot>>(kill23)});
        check(kill_state23->initialize_from_database(eternity::net::Packet{0x05,0x02,100,0,0,0,100,0,0,0}));
        check(kill_state23->country(0).count==100 && kill_state23->country(0).deadlines.reset==600100);
        world23->bind_send_phase(send23);world23->bind_rank_phase(rank23);world23->bind_market_phase(market23);world23->bind_primary_logout_phase(logout23);world23->bind_world_db_driver(db_services23,db_heartbeat23,db_overruns23,[]{return 100U;},[&](std::exception_ptr error){failure23=error;});world23->bind_primary_weather_phase(weather23);world23->bind_primary_quest_phase(quest_services23);world23->bind_primary_mob_phase(mob23);world23->bind_primary_npc_idle_phase(npc23);world23->bind_primary_user_attack_phase(attack23);world23->bind_active_packet_phase(packet23);world23->bind_primary_frame_prefix(packet23);authority23=world23->world().zones();check(world23->start() && db_heartbeat23->load()==100);
        WorldAdmissionUser connected_send23,selection_send23;connected_send23.user_id=2311;connected_send23.location=0;selection_send23.user_id=2312;
        world23->admission().track_connected(connected_send23);world23->admission().track_selection(selection_send23);
        lost23->tick=100;check(world23->update_session_lost(30100) && lost23->closed.empty());
        WorldSendRetainedOther<ConcreteRoot> other_send23(world23,std::make_shared<RootSendRemainingServices>());other_send23.update_session_lost(30101);
        check((lost23->closed==std::vector<std::pair<std::uint32_t,std::uint32_t>>{{2311,8}}));
        lost23->closed.clear();lost23->tick=0xfffffff0U;check(world23->update_session_lost(30000) && lost23->closed.size()==1);
        auto global_send23=std::make_shared<RootGlobalSendServices>();global_send23->pin=attack_actor;
        WorldSendRetainedUpdate<ConcreteRoot> retained_send23(world23,global_send23,std::make_shared<RootSendOtherServices>());
        const auto packet_count23=admission23->packets_seen;retained_send23.update_packets();check(admission23->packets_seen==packet_count23+2);
        retained_send23.update_net_send();check((global_send23->users==std::vector<std::uint32_t>{2311,2312}));
        global_send23->pin.reset();bool no_global_pin=false;try{retained_send23.update_net_send();}catch(const std::logic_error&){no_global_pin=true;}check(no_global_pin && global_send23->users.size()==2);
        world23->admission().untrack(connected_send23);world23->admission().untrack(selection_send23);
        auto broadcast23=std::make_shared<const eternity::net::Packet>(eternity::net::Packet{0x34,0x12});
        std::weak_ptr<const eternity::net::Packet> broadcast_weak23=broadcast23;
        world23->enqueue_world_broadcast({WorldZoneSendKind::priority_country,broadcast23,1,5,6,7});broadcast23.reset();
        auto lease=world23->select_instance(1,{});check(bool(lease));const auto first_generation=lease.generation();RuntimeZone* selected=nullptr;
        lease.visit([&](RuntimeZone& zone){selected=&zone;check(zone.users && zone.items && zone.mobs);});
        check(!broadcast_weak23.expired() && selected->world_sends.size()==0);
        world23->world().visit_primary_zones([&](RuntimeZone& zone){zone.world_sends.drain([&](const WorldZoneSendMessage& message){check(message.kind==WorldZoneSendKind::priority_country && message.country==1 && message.source==5 && message.destination==6 && message.priority==7);});});
        check(!broadcast_weak23.expired());check(world23->update_world_send(400) && selected->world_sends.size()==0);
        check(world23->update_world_send(401) && selected->world_sends.size()==1);
        check((send_update23->events==std::vector<std::string>{"packets","net","kill","packets","net","kill"}));send_update23->events.clear();
        send_update23->rank_active=true;check(world23->update_world_send(60101));
        check((send_update23->events==std::vector<std::string>{"packets","net","lost","kill","notice","status"}));send_update23->events.clear();
        send_update23->rank_active=false;check(world23->update_world_send(120102));send_update23->events.clear();
        check(world23->update_world_send(1920102));check(std::find(send_update23->events.begin(),send_update23->events.end(),"points")==send_update23->events.end());send_update23->events.clear();
        check(world23->update_world_send(1920103));check(send_update23->events.back()=="points");
        selected->world_sends.drain([&](const WorldZoneSendMessage& message){check(message.kind==WorldZoneSendKind::priority_country && message.payload==broadcast_weak23.lock());});check(broadcast_weak23.expired());
        bool invalid_broadcast23=false;try{world23->enqueue_world_broadcast({WorldZoneSendKind::all,{}});}catch(const std::invalid_argument&){invalid_broadcast23=true;}check(invalid_broadcast23 && selected->world_sends.size()==0);
        WorldAdmissionUser pending_wait;pending_wait.user_id=231;admission23->zone=selected;world23->admission().track_selection(pending_wait);check(world23->admission().wait(pending_wait));world23->update_admission();
        selected->users->attach(*attack_actor,0,0,{});pending_wait.logout_type=2;pending_wait.logout_tick=10;logout_actor23.logout_type=7;logout_actor23.logout_tick=10;
        logout23->pin.reset();bool missing_wait_pin=false;try{(void)world23->update_registry_logout_phase(10);}catch(const std::logic_error&){missing_wait_pin=true;}
        check(missing_wait_pin && pending_wait.logout_tick==10 && logout_actor23.logout_tick==10 && logout23->events.empty());logout23->pin=attack_actor;
        check(world23->thread_zones().pending_count()==1 && world23->update_registry_logout_phase(10)==PrimaryLogoutResult::completed);
        check((logout23->events==std::vector<std::string>{"wait231","actor901"}) && pending_wait.logout_tick==0 && logout_actor23.logout_tick==0);
        check(world23->update_primary_logout_phase(1009)==PrimaryLogoutResult::skipped && world23->update_registry_logout_phase(1010)==PrimaryLogoutResult::completed);
        WorldDbUpdate<ConcreteRoot> db_update23(world23,db_services23);
        check(db_update23.update(2010)==WorldDbUpdateResult::completed && (db_services23->events==std::vector<std::string>{"manager","database"}));
        db_services23->started=true;db_services23->events.clear();pending_wait.logout_type=2;pending_wait.logout_tick=2010;
        logout23->on_wait=[&](WorldAdmissionUser&){db_services23->events.push_back("logout");};
        check(db_update23.update(2010)==WorldDbUpdateResult::completed && (db_services23->events==std::vector<std::string>{"manager","database","logout","experience"}) && db_update23.next_experience_deadline()==62010);
        logout23->on_wait={};db_services23->events.clear();check(db_update23.update(62009)==WorldDbUpdateResult::completed && (db_services23->events==std::vector<std::string>{"manager","database"}));
        db_services23->events.clear();check(db_update23.update(62010)==WorldDbUpdateResult::completed && (db_services23->events==std::vector<std::string>{"manager","database","experience"}));
        check(selected->users->detach(attack_actor->id()));world23->admission().untrack(pending_wait);
        check(world23->thread_zones().pending_count()==1 && world23->thread_zones().promote(0)==1);
        lease.visit([&](RuntimeZone& zone){check(&zone==selected && world23->update_instance_cleanup(100)==InstanceCleanupResult::deferred && world23->next_instance_cleanup()==0);lease.reset();check(world23->update_instance_cleanup(100)==InstanceCleanupResult::deferred);});
        std::vector<RuntimeZone*> active_visits;
        check(world23->visit_active_zones([&](RuntimeZone& zone){active_visits.push_back(&zone);if(&zone==selected)check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);}));
        check(active_visits.size()==2 && active_visits[0]->map==maps23->find(0) && active_visits[1]==selected);
        auto foreign_map=maps23->all().front();foreign_map.settings.create_type=1;RuntimeZone foreign_zone;foreign_zone.map=&foreign_map;world23->thread_zones().add_active(foreign_zone);
        bool rejected_foreign=false;try{(void)world23->visit_active_zones([](RuntimeZone&){});}catch(const std::logic_error&){rejected_foreign=true;}
        check(rejected_foreign);world23->thread_zones().remove_active(foreign_zone);
        auto other_lease=world23->select_instance(1,{});check(other_lease && other_lease.generation()==first_generation && world23->thread_zones().pending_count()==0);
        WorldAdmissionUser instance_wait;instance_wait.user_id=230;other_lease.visit([&](RuntimeZone& zone){check(&zone==selected);admission23->zone=&zone;world23->admission().track_selection(instance_wait);check(world23->admission().wait(instance_wait));world23->update_admission();zone.users->attach(*attack_actor,0,0,{});
            auto zone_items=std::shared_ptr<GroundItemPool>(other23,&other23->item_pool);
            quest_pin23->world=std::make_shared<VerticalQuests>(QuestStartItemsActorWorldOwners{root_items,zone_items,quest_ids,quest_definitions,quest_pool,attack_actor,quest_runtime23},QuestStartItemsActorUseState{quest_pin23->last_item,quest_pin23->deadline},*zone.npcs,other23->npc_pool,quest_ports23);
        });other_lease.reset();
        admission23->on_packet=[&]{packet23->events.push_back("wait");};packet23->on_actor_packets=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_active_packet_phase());check((packet23->events==std::vector<std::string>{"clock0","wait","packet1","clock1","reset1"}));
        attack_actor->attack_state().mode=2;attack_actor->attack_state().next_tick=1;attack_actor->attack_state().resume_tick=0;attack_actor->status().current_target_type=2;attack_actor->status().current_target=77;
        attack_world23.on_hit=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_active_user_attack_phase(100)==PrimaryUserAttackResult::completed && attack_world23.hits==1);
        check(world23->update_primary_user_attack_phase(199)==PrimaryUserAttackResult::skipped);
        attack_actor->attack_state().resume_tick=0;check(world23->update_active_user_attack_phase(200)==PrimaryUserAttackResult::completed && attack_world23.hits==2);
        check((attack23->zones==std::vector<int>{0,1,0,1}));attack_world23.on_hit={};
        WorldPrimaryUserAttackPhase active_attack_wrap(attack23);attack_actor->attack_state().mode=0;
        check(active_attack_wrap.update_active(*world23,0xfffffff0U)==PrimaryUserAttackResult::completed && active_attack_wrap.next_deadline()==84);
        check(active_attack_wrap.update_active(*world23,83)==PrimaryUserAttackResult::skipped && active_attack_wrap.update_active(*world23,84)==PrimaryUserAttackResult::completed);attack_actor->attack_state().mode=2;
        SkillDefinition active_effect;active_effect.id=235;active_effect.level=1;EffectTimer actor_timer;actor_timer.effect.skill_id=235;actor_timer.effect.level=1;actor_timer.end=0;attack_actor->effects().append(445,actor_timer,active_effect);
        MobRuntime* effect_mob=nullptr;auto effect_setup=world23->select_instance(1,{});effect_setup.visit([&](RuntimeZone& zone){
            for(auto* mob:zone.mobs->all())if(!mob->lua_ai && (!mob->respawn || !mob->respawn->both)){effect_mob=mob;break;}check(effect_mob);
            auto* effect_node=effect_mob->applied_skills().owner().append_from_pool();check(effect_node);effect_node->definition=f23->full.skill_definition(1);check(effect_node->definition);effect_node->end_tick=0;
        });effect_setup.reset();
        const auto prior_db_effects=attack_scene.effect_database_packets.size();packet23->on_actor_recover=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        const auto effects_first=world23->update_active_effect_phases(100);check(effects_first.completed && effects_first.ran && attack_actor->effects().size()==0 && attack_scene.effect_database_packets.size()==prior_db_effects+1);
        auto effect_inspect=world23->select_instance(1,{});effect_inspect.visit([&](RuntimeZone&){check(effect_mob->applied_skills().owner().records().empty());});effect_inspect.reset();
        check((packet23->events==std::vector<std::string>{"clock0","wait","packet1","clock1","reset1","recover1","pvp1","rebirth1","tick1","timeout1","notice1"}));
        const auto effects_skipped=world23->update_active_effect_phases(1099);check(effects_skipped.completed && !effects_skipped.ran);
        const auto effects_equal=world23->update_active_effect_phases(1100);check(effects_equal.completed && effects_equal.ran);
        WorldPrimaryFramePrefix effects_wrap(packet23);const auto wrapped=effects_wrap.update_active_effects(*world23,0xfffffff0U);check(wrapped.completed && wrapped.ran);
        const auto before_wrap=effects_wrap.update_active_effects(*world23,983);check(before_wrap.completed && !before_wrap.ran);
        const auto at_wrap=effects_wrap.update_active_effects(*world23,984);check(at_wrap.completed && at_wrap.ran);packet23->on_actor_recover={};
        Npc* guard23=nullptr;Npc* disabled23=nullptr;Npc* unsourced23=nullptr;Npc* patrol23=nullptr;
        auto guard_setup=world23->select_instance(1,{});guard_setup.visit([&](RuntimeZone& zone){
            auto make_guard=[&](std::uint32_t id,std::uint32_t disabled){auto* npc=other23->npc_pool.allocate();check(npc);npc->id=id;npc->type=8;npc->position={10,0,10};npc->source=&guard_source;npc->definition=&guard_definition;npc->disable_time=disabled;check(zone.npcs->enter(*npc));return npc;};
            guard23=make_guard(801,100);disabled23=make_guard(802,0xffffffffU);unsourced23=make_guard(803,0);unsourced23->source=nullptr;
            patrol23=other23->npc_pool.allocate();check(patrol23);patrol23->id=804;patrol23->type=1;patrol23->position={10,0,10};patrol23->source=&patrol_source;patrol23->definition=&patrol_definition;patrol23->next_position=1;patrol23->next_move=0;patrol23->users_using=0;patrol23->forward=true;check(zone.npcs->enter(*patrol23));
        });
        packet23->omit_attack_pin=true;bool missing_mob_pin=false;guard_setup.visit([&](RuntimeZone& zone){try{(void)update_zone_mob_attacks(world23->world(),zone,*packet23,100);}catch(const std::logic_error&){missing_mob_pin=true;}});check(missing_mob_pin);packet23->omit_attack_pin=false;
        guard_setup.reset();packet23->events.clear();
        packet23->on_mob_attack=[&](RuntimeZone& zone){check(bool(zone.mobs));if(&zone==selected)check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);};
        npc23->guard=[&](RuntimeZone& zone,Npc& npc,std::uint32_t){check(&zone==selected && &npc==guard23);packet23->events.push_back("guard1");check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);};
        const auto attack_first=world23->update_active_mob_npc_attack(100);check(attack_first.completed && attack_first.ran);
        const auto attack_skipped=world23->update_active_mob_npc_attack(599);check(attack_skipped.completed && !attack_skipped.ran);
        const auto attack_equal=world23->update_active_mob_npc_attack(600);check(attack_equal.completed && attack_equal.ran);
        check((packet23->events==std::vector<std::string>{"mob-attack0","mob-attack1","guard1","mob-attack0","mob-attack1","guard1"}) && (npc23->zones==std::vector<int>{0,1,0,1}));
        WorldPrimaryFramePrefix attack_wrap(packet23);const auto wrapped_attack=attack_wrap.update_active_attacks(*world23,0xfffffff0U);check(wrapped_attack.completed && wrapped_attack.ran);
        const auto before_attack_wrap=attack_wrap.update_active_attacks(*world23,483);check(before_attack_wrap.completed && !before_attack_wrap.ran);
        const auto at_attack_wrap=attack_wrap.update_active_attacks(*world23,484);check(at_attack_wrap.completed && at_attack_wrap.ran);packet23->on_mob_attack={};npc23->guard={};
        WorldPrimaryMobPhase mismatched_idle_rng(mob23);mob23->mismatch_random=true;bool rejected_idle_rng=false;
        try{(void)mismatched_idle_rng.update_active(*world23,100);}catch(const std::invalid_argument&){rejected_idle_rng=true;}check(rejected_idle_rng && mismatched_idle_rng.next_deadline()==0);mob23->mismatch_random=false;
        WorldPrimaryMobPhase missing_idle_pin(mob23);mob23->omit_idle_pin=true;bool rejected_idle_pin=false;
        try{(void)missing_idle_pin.update_active(*world23,100);}catch(const std::logic_error&){rejected_idle_pin=true;}check(rejected_idle_pin && missing_idle_pin.next_deadline()==0);mob23->omit_idle_pin=false;mob23->events.clear();
        std::vector<ZoneMobs*> active_mob_owners;check(world23->visit_active_zones([&](RuntimeZone& zone){active_mob_owners.push_back(zone.mobs.get());}));check(active_mob_owners.size()==2);
        mob23->on_traps=[&](RuntimeZone& zone){if(&zone==selected)check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);};idle23.expected=100;
        check(world23->update_active_mob_phase(100)==PrimaryMobPhaseResult::completed && idle23.calls>0);
        check((mob23->events==std::vector<std::string>{"mob0","trap0","mob1","trap1"}));
        check(std::find(idle23.visited_zones.begin(),idle23.visited_zones.end(),active_mob_owners[0])!=idle23.visited_zones.end() && std::find(idle23.visited_zones.begin(),idle23.visited_zones.end(),active_mob_owners[1])!=idle23.visited_zones.end());
        check(world23->update_active_mob_phase(1099)==PrimaryMobPhaseResult::skipped && world23->update_primary_mob_phase(1099)==PrimaryMobPhaseResult::skipped);idle23.expected=1100;check(world23->update_active_mob_phase(1100)==PrimaryMobPhaseResult::completed);
        WorldPrimaryMobPhase active_idle_wrap(mob23);idle23.expected=0xfffffff0U;check(active_idle_wrap.update_active(*world23,0xfffffff0U)==PrimaryMobPhaseResult::completed && active_idle_wrap.next_deadline()==984);
        check(active_idle_wrap.update_active(*world23,983)==PrimaryMobPhaseResult::skipped);idle23.expected=984;check(active_idle_wrap.update_active(*world23,984)==PrimaryMobPhaseResult::completed);mob23->on_traps={};
        npc23->zones.clear();unsigned idle_guard_calls=0;npc23->guard=[&](RuntimeZone& zone,Npc& npc,std::uint32_t){check(&zone==selected && &npc==guard23);++idle_guard_calls;check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);};
        const auto npc_moves_before=other23->npc_moves.size();
        check(world23->update_active_npc_idle_phase(100)==PrimaryNpcIdleResult::completed && idle_guard_calls==1 && patrol23->position.x==11 && other23->npc_moves.size()==npc_moves_before+1);
        check((npc23->zones==std::vector<int>{0,1}) && world23->update_primary_npc_idle_phase(3099)==PrimaryNpcIdleResult::skipped);
        check(world23->update_active_npc_idle_phase(3100)==PrimaryNpcIdleResult::completed && idle_guard_calls==2);
        WorldPrimaryNpcIdlePhase active_npc_wrap(npc23);patrol23->users_using=1;
        check(active_npc_wrap.update_active(*world23,0xfffffff0U)==PrimaryNpcIdleResult::completed && active_npc_wrap.next_deadline()==2984);
        check(active_npc_wrap.update_active(*world23,2983)==PrimaryNpcIdleResult::skipped && active_npc_wrap.update_active(*world23,2984)==PrimaryNpcIdleResult::completed && idle_guard_calls==4);npc23->guard={};
        GroundItem* timed_item23=nullptr;auto item_setup23=world23->select_instance(1,{});item_setup23.visit([&](RuntimeZone& zone){timed_item23=other23->item_pool.allocate();check(timed_item23);timed_item23->position={10,0,10};check(zone.items->enter(*timed_item23,1,23));});item_setup23.reset();
        unsigned item_release23=0;other23->on_item_release=[&]{++item_release23;check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_active_item_phase(100)==PrimaryItemPhaseResult::completed && item_release23==0);
        check(world23->update_primary_item_phase(10099)==PrimaryItemPhaseResult::skipped);
        check(world23->update_active_item_phase(10100)==PrimaryItemPhaseResult::completed && item_release23==1 && other23->item_pool.statistics().available==1);other23->on_item_release={};
        WorldPrimaryItemPhase active_item_wrap;check(active_item_wrap.update_active(*world23,0xfffffff0U)==PrimaryItemPhaseResult::completed && active_item_wrap.next_deadline()==9984);
        check(active_item_wrap.update_active(*world23,9983)==PrimaryItemPhaseResult::skipped && active_item_wrap.update_active(*world23,9984)==PrimaryItemPhaseResult::completed);
        quest_services23->on_retain=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_active_quest_phase(100)==PrimaryQuestPhaseResult::completed && quest_runtime23->active().empty() && quest_runtime23->finished().size()==1 && quest_ports23.database.size()==1 && quest_ports23.logs.size()==1);
        check(world23->update_primary_quest_phase(60099)==PrimaryQuestPhaseResult::skipped && world23->update_active_quest_phase(60100)==PrimaryQuestPhaseResult::completed);
        const auto visits_before_ineligible=quest_services23->visits;quest_services23->allow=false;WorldPrimaryQuestPhase ineligible_quest_phase(quest_services23);
        check(ineligible_quest_phase.update_active(*world23,100)==PrimaryQuestPhaseResult::completed && quest_services23->visits==visits_before_ineligible);
        quest_services23->allow=true;
        WorldPrimaryQuestPhase active_quest_wrap(quest_services23);check(active_quest_wrap.update_active(*world23,0xfffffff0U)==PrimaryQuestPhaseResult::completed && active_quest_wrap.next_deadline()==59984);
        check(active_quest_wrap.update_active(*world23,59983)==PrimaryQuestPhaseResult::skipped && active_quest_wrap.update_active(*world23,59984)==PrimaryQuestPhaseResult::completed);quest_services23->on_retain={};
        check(world23->visit_active_zones([&](RuntimeZone& zone){auto& weather=zone.environment->weather();weather.definition.rate=100;weather.definition.state=1;weather.definition.power=2;weather.next_update=0;weather.active=0;}));
        weather23->sinks[1].on_send=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_active_weather_phase(100)==PrimaryWeatherResult::completed);
        check((weather23->zones==std::vector<int>{0,1}));
        check((weather23->sinks[0].packets==std::vector<eternity::net::Packet>{{0x51,4,1,1,2}} && weather23->sinks[1].packets==std::vector<eternity::net::Packet>{{0x51,4,1,1,2}}));
        weather23->sinks[1].on_send={};check(world23->visit_active_zones([&](RuntimeZone& zone){zone.environment->weather().next_update=0xffffffffU;}));
        check(world23->update_primary_weather_phase(60099)==PrimaryWeatherResult::skipped && world23->update_active_weather_phase(60100)==PrimaryWeatherResult::completed);
        WorldPrimaryWeatherPhase active_weather_wrap(weather23);check(active_weather_wrap.update_active(*world23,0xfffffff0U)==PrimaryWeatherResult::completed && active_weather_wrap.next_deadline()==59984);
        check(active_weather_wrap.update_active(*world23,59983)==PrimaryWeatherResult::skipped && active_weather_wrap.update_active(*world23,59984)==PrimaryWeatherResult::completed);
        const auto before_prefix=packet23->events.size();const auto prefix23=world23->update_active_world_prefix(101);
        check(prefix23.completed && !prefix23.user_attack && !prefix23.effects && !prefix23.mob_npc_attack && !prefix23.mob_idle && !prefix23.npc_idle && !prefix23.items && !prefix23.quests && !prefix23.weather);
        check(packet23->events.size()>before_prefix);
        GroundItem* prefix_item=nullptr;auto prefix_lease=world23->select_instance(1,{});prefix_lease.visit([&](RuntimeZone& zone){prefix_item=other23->item_pool.allocate();check(prefix_item);prefix_item->position={10,0,10};check(zone.items->enter(*prefix_item,1,23));});prefix_lease.reset();
        check(world23->visit_active_zones([&](RuntimeZone& zone){auto& weather=zone.environment->weather();weather.active=0;weather.next_update=0;}));
        packet23->events.clear();attack_actor->attack_state().mode=0;idle23.expected=120100;
        attack23->on_zone=[&]{packet23->events.push_back("user-attack");};
        npc23->guard=[&](RuntimeZone&,Npc&,std::uint32_t){packet23->events.push_back("guard");};
        mob23->on_traps=[&](RuntimeZone&){packet23->events.push_back("trap");};
        other23->on_item_release=[&]{packet23->events.push_back("item");};
        quest_services23->on_retain=[&]{packet23->events.push_back("quest");};
        weather23->sinks[0].on_send=[&]{packet23->events.push_back("weather");};
        const auto due_prefix=world23->update_active_world_prefix(120100);
        check(due_prefix.completed && due_prefix.user_attack && due_prefix.effects && due_prefix.mob_npc_attack && due_prefix.mob_idle && due_prefix.npc_idle && due_prefix.items && due_prefix.quests && due_prefix.weather);
        const auto index=[&](const char* event){auto it=std::find(packet23->events.begin(),packet23->events.end(),event);check(it!=packet23->events.end());return std::distance(packet23->events.begin(),it);};
        check(index("packet1")<index("user-attack") && index("user-attack")<index("recover1") && index("recover1")<index("tick1") && index("tick1")<index("timeout1") && index("timeout1")<index("mob-attack0") && index("mob-attack0")<index("trap") && index("trap")<index("item") && index("item")<index("quest") && index("quest")<index("weather"));
        attack23->on_zone={};npc23->guard={};mob23->on_traps={};other23->on_item_release={};quest_services23->on_retain={};weather23->sinks[0].on_send={};
        market23->on_check=[&]{check(world23->expire_instance(*selected,100)==MobInstanceExpiry::deferred);};
        check(world23->update_market_phase(100)==WorldMarketResult::completed && (market23->ticks==std::vector<std::uint32_t>{100}));
        check(world23->update_market_phase(60099)==WorldMarketResult::skipped);market23->open=false;
        check(world23->update_market_phase(60100)==WorldMarketResult::completed && market23->ticks.size()==1);market23->open=true;
        check(world23->update_market_phase(120100)==WorldMarketResult::completed && market23->ticks.size()==2);
        WorldMarketPhase market_wrap(market23);check(market_wrap.update(*world23,0xfffffff0U)==WorldMarketResult::completed && market_wrap.next_deadline()==59984);
        check(market_wrap.update(*world23,59983)==WorldMarketResult::skipped && market_wrap.update(*world23,59984)==WorldMarketResult::completed);market23->on_check={};
        WorldMarketPhase market_failure(market23);market23->on_check=[]{throw std::runtime_error("market failure");};bool failed_market=false;
        try{(void)market_failure.update(*world23,42);}catch(const std::runtime_error&){failed_market=true;}check(failed_market && market_failure.next_deadline()==0);
        market23->on_check={};check(market_failure.update(*world23,42)==WorldMarketResult::completed && market_failure.next_deadline()==60042);
        attack_actor->appearance().identity.country=0;world23->request_kill_count_apply(0);world23->request_kill_count_send(1);check(world23->update_kill_count()==WorldKillCountResult::completed);
        check((kill23->events==std::vector<std::string>{"actor","waiting","send0:0","send0:1","send1:0","send1:1"}));
        check(kill23->packets.size()==4 && (kill23->packets[0]==eternity::net::Packet{0x0f,0x02,0,50,0,0,0}) && (kill23->packets[2]==eternity::net::Packet{0x0f,0x02,1,70,0,0,0}));
        check(world23->update_kill_count()==WorldKillCountResult::completed && kill23->packets.size()==4);
        ZoneUserAccess rank_access;rank_access.permission=0;rank_access.instance_owner=11;auto rank_lease=world23->select_instance(2,rank_access);check(bool(rank_lease));RuntimeZone* rank_zone=nullptr;
        check(selected->users->detach(attack_actor->id()));rank_lease.visit([&](RuntimeZone& zone){rank_zone=&zone;zone.users->attach(*attack_actor,0,0,{});});rank_lease.reset();
        check(world23->thread_zones().promote(100)==1);rank23->on_move=[&](RuntimeZone& zone){check(&zone==rank_zone && world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);};
        check(world23->update_rank_phase(100)==WorldRankResult::completed && (rank23->moves==std::vector<int>{2}) && (rank23->broadcasts==std::vector<int>{0,1,2}));
        check(rank23->packets.size()==3 && (rank23->packets[0]==eternity::net::Packet{0x26,0x0d,42,0,3}));
        check(world23->update_rank_phase(60099)==WorldRankResult::skipped);rank23->time.hour=10;rank23->current.reset();
        check(world23->update_rank_phase(60100)==WorldRankResult::completed && rank23->moves.size()==1 && rank23->broadcasts.size()==3);
        rank23->time.hour=9;check(world23->update_rank_phase(120100)==WorldRankResult::completed && rank23->moves.size()==2);
        WorldRankPhase rank_wrap(rank23);check(rank_wrap.update(*world23,0xfffffff0U)==WorldRankResult::completed && rank_wrap.next_deadline()==59984);
        check(rank_wrap.update(*world23,59983)==WorldRankResult::skipped && rank_wrap.update(*world23,59984)==WorldRankResult::completed);rank23->on_move={};
        check(rank_zone->users->detach(attack_actor->id()) && world23->expire_instance(*rank_zone,100)==MobInstanceExpiry::retired);selected->users->attach(*attack_actor,0,0,{});
        check(world23->update_obelisk_reset([]{return 120100U;})==WorldObeliskResetResult::skipped);
        world23->request_obelisk_reset();bool missing_reset_clock=false;try{(void)world23->update_obelisk_reset({});}catch(const std::invalid_argument&){missing_reset_clock=true;}check(missing_reset_clock);
        const auto instance_mobs_before=selected->mobs->size();
        check(world23->update_obelisk_reset([]{return 120100U;})==WorldObeliskResetResult::completed && selected->mobs->size()==instance_mobs_before);
        check(world23->update_obelisk_reset([]{return 120100U;})==WorldObeliskResetResult::skipped);
        idle23.expected=180100;npc23->guard=[&](RuntimeZone& zone,Npc& npc,std::uint32_t){check(&zone==selected && &npc==guard23);};
        world23->request_kill_count_send(0);world23->request_obelisk_reset();
        const auto through_kill=world23->update_active_through_kill_count(180100,[]{return 180100U;});
        check(through_kill.completed && through_kill.net_send_due && through_kill.prefix.completed && through_kill.prefix.user_attack && through_kill.prefix.effects && through_kill.prefix.mob_npc_attack && through_kill.prefix.mob_idle && through_kill.prefix.npc_idle && through_kill.prefix.items && through_kill.prefix.quests && through_kill.prefix.weather && through_kill.rank && through_kill.market && through_kill.obelisk_reset && kill23->packets.size()==6);
        npc23->guard={};quest_pin23->world.reset();
        auto shared_world_packet=std::make_shared<const eternity::net::Packet>(eternity::net::Packet{0x23,0x01});
        check(world23->visit_active_zones([&](RuntimeZone& zone){if(&zone==selected)zone.world_sends.push({WorldZoneSendKind::country,shared_world_packet,1});else zone.world_sends.push({WorldZoneSendKind::all,shared_world_packet});}));
        bool enqueued_during_send=false;send23->on_delivery=[&](RuntimeZone& zone){if(&zone==selected){check(world23->expire_instance(zone,100)==MobInstanceExpiry::deferred);if(!enqueued_during_send){enqueued_during_send=true;zone.world_sends.push({WorldZoneSendKind::priority,shared_world_packet,0,1,2,3});}}};
        check(world23->update_send_phase(false) && (send23->events==std::vector<std::string>{"send0:1","send1:2","send1:3"}));
        send23->on_delivery={};send23->events.clear();check(world23->update_send_phase(true) && (send23->events==std::vector<std::string>{"wait","actor"}));
        auto native_ports=std::make_shared<RootNativeSendPorts>(attack_actor);WorldNativeSendServices native_send(native_ports);
        native_send.deliver(*selected,{WorldZoneSendKind::all,shared_world_packet});native_send.deliver(*selected,{WorldZoneSendKind::country,shared_world_packet,0});native_send.deliver(*selected,{WorldZoneSendKind::country,shared_world_packet,1});
        native_send.deliver(*selected,{WorldZoneSendKind::priority,shared_world_packet,0,5,6,7});check(native_ports->queue.size()==1 && native_ports->pool.active);
        native_send.flush_actor(*attack_actor);check(!native_ports->pool.active && native_ports->queue.size()==0);
        native_send.deliver(*selected,{WorldZoneSendKind::priority_country,shared_world_packet,1,5,6,7});
        native_send.deliver(*selected,{WorldZoneSendKind::priority_country,shared_world_packet,0,attack_actor->id(),6,7});
        check(native_ports->packets.size()==4 && native_ports->packets[0]==*shared_world_packet && native_ports->queue.size()==0 && !native_ports->pool.active);
        native_send.deliver(*selected,{WorldZoneSendKind::priority,shared_world_packet,0,5,6,7});native_ports->allow_priority=false;native_send.flush_actor(*attack_actor);
        check(native_ports->packets.size()==4 && native_ports->queue.size()==0 && !native_ports->pool.active);
        native_ports->allow_priority=true;native_send.deliver(*selected,{WorldZoneSendKind::priority,shared_world_packet,0,5,6,7});native_ports->user.send_socket=-1;native_send.flush_actor(*attack_actor);
        check(native_ports->packets.size()==4 && native_ports->queue.size()==0 && !native_ports->pool.active);native_ports->user.send_socket=1;
        auto waiting_buffer=native_ports->shared.allocate(*shared_world_packet);check(bool(waiting_buffer));
        check(native_ports->shared.enqueue_priority(native_ports->waiting_user,*waiting_buffer,native_ports->waiting_queue,7,65000.0F));waiting_buffer.reset();
        native_send.flush_waiting(instance_wait);check(native_ports->packets.size()==5 && native_ports->packets.back()==*shared_world_packet && native_ports->waiting_queue.size()==0 && !native_ports->pool.active);
        VerticalSessionRegistry<RetainedSendSession> retained_registry;auto retained_session=std::make_shared<RetainedSendSession>();retained_session->user.user_id=attack_actor->account_id();
        WorldUser retained_user{*attack_actor,retained_session->user};retained_session->linked=&retained_user;retained_session->socket=native_ports->socket;retained_registry.retain(retained_session,*attack_actor);
        auto retained_resources=std::make_shared<RetainedSendResources>();retained_resources->owner=native_ports;
        unsigned retained_lookups=0;std::function<void()> on_retained_lookup;
        auto retained_ports=std::make_shared<WorldRetainedSendPorts<RetainedSendSession,RetainedSendResources>>(retained_registry,native_ports->shared,native_ports->pool,
            [&](RetainedSendSession& session){check(&session==retained_session.get());++retained_lookups;if(on_retained_lookup)on_retained_lookup();return retained_resources;},[](RetainedSendSession&){return true;});
        auto retained_pin=retained_ports->retain_actor(*attack_actor);check(bool(retained_pin) && retained_lookups==1);
        check(&retained_ports->connection(*attack_actor)==&native_ports->user && &retained_ports->priority_queue(*attack_actor)==&native_ports->queue && bool(retained_ports->retain_waiting(retained_session->user)) && retained_lookups==1);retained_pin.reset();
        on_retained_lookup=[&]{(void)retained_ports->retain_actor(*attack_actor);};bool rejected_reentry=false;
        try{(void)retained_ports->retain_actor(*attack_actor);}catch(const std::logic_error&){rejected_reentry=true;}check(rejected_reentry);on_retained_lookup={};
        WorldAdmissionUser foreign_wait;foreign_wait.user_id=retained_session->user.user_id;bool rejected_foreign_wait=false;
        try{(void)retained_ports->retain_waiting(foreign_wait);}catch(const std::logic_error&){rejected_foreign_wait=true;}check(rejected_foreign_wait);
        WorldNativeSendServices retained_send(retained_ports);retained_send.deliver(*selected,{WorldZoneSendKind::priority,shared_world_packet,0,5,6,7});
        check(native_ports->queue.size()==1);retained_send.flush_waiting(retained_session->user);
        check(native_ports->queue.size()==0 && !native_ports->pool.active && native_ports->packets.size()==6);
        world23->admission().untrack(instance_wait);world23->admission().track_selection(retained_session->user);
        check(world23->admission().wait(retained_session->user));world23->update_admission();
        unsigned applied_retained=0;WorldRetainedKillCountZone<ConcreteRoot,RetainedSendSession> kill_ports(world23,retained_registry,
            [&](RetainedSendSession& session,const WorldKillCountState<ConcreteRoot>& state){check(&session==retained_session.get() && &state==kill_state23.get());++applied_retained;});
        auto kill_actor_pin=kill_ports.retain_actor(*attack_actor);kill_ports.apply_actor(*attack_actor,*kill_state23);
        auto kill_wait_pin=kill_ports.retain_waiting(retained_session->user);check(kill_ports.waiting_country(retained_session->user)==0);kill_ports.apply_waiting(retained_session->user,*kill_state23);
        kill_ports.send_country(*selected,0,*shared_world_packet);kill_ports.send_country(*selected,1,*shared_world_packet);
        check(applied_retained==2 && native_ports->packets.size()==8 && native_ports->packets[6]==*shared_world_packet && native_ports->packets[7]==*shared_world_packet);
        kill_actor_pin.reset();kill_wait_pin.reset();world23->admission().untrack(retained_session->user);
        instance_wait.location=1;world23->admission().track_selection(instance_wait);check(world23->admission().wait(instance_wait));world23->update_admission();
        check(retained_registry.release(retained_session->user.user_id,*retained_session));
        send23->events.clear();selected->world_sends.push({WorldZoneSendKind::all,shared_world_packet});
        send23->on_delivery=[&](RuntimeZone& zone){check(&zone==selected && world23->expire_instance(zone,180200)==MobInstanceExpiry::deferred);};
        const auto full_frame=world23->update_active_frame(180200,[]{return 180200U;});
        check(full_frame.completed && full_frame.through_kill_count.completed && full_frame.through_kill_count.net_send_due && full_frame.cleanup==InstanceCleanupResult::completed && world23->next_instance_cleanup()==181200);
        check((send23->events==std::vector<std::string>{"send1:1","wait","actor"}) && authority23->instance_count()==1);send23->on_delivery={};
        auto guard_cleanup=world23->select_instance(1,{});guard_cleanup.visit([&](RuntimeZone& zone){for(auto* npc:{guard23,disabled23,unsourced23,patrol23}){check(zone.npcs->leave(*npc));other23->npc_pool.release(*npc);}});guard_cleanup.reset();
        auto detach_lease=world23->select_instance(1,{});detach_lease.visit([&](RuntimeZone& zone){check(zone.users->detach(attack_actor->id()));});detach_lease.reset();
        check(world23->update_instance_cleanup(181200)==InstanceCleanupResult::completed && world23->next_instance_cleanup()==182200 && authority23->instance_count()==1);world23->admission().untrack(instance_wait);
        auto item_lease=world23->select_instance(1,{});GroundItem* item=nullptr;item_lease.visit([&](RuntimeZone& zone){item=other23->item_pool.allocate();check(item);item->position={10,0,10};check(zone.items->enter(*item,1,23));});item_lease.reset();
        check(world23->update_instance_cleanup(182199)==InstanceCleanupResult::skipped);
        selected->world_sends.push({WorldZoneSendKind::priority_country,shared_world_packet,1,5,6,7});
        bool failed=false;try{(void)world23->update_instance_cleanup(182200);}catch(const std::logic_error&){failed=true;}check(failed && authority23->has_retiring_instance() && authority23->instance_count()==1 && !world23->thread_zones().contains_active(*selected));
        bool blocked23=false;try{(void)world23->select_instance(1,{});}catch(const std::logic_error&){blocked23=true;}check(blocked23);
        world23->visit_retiring_instance([&](RuntimeZone& zone){check(world23->update_instance_cleanup(182200)==InstanceCleanupResult::deferred);check(zone.items->leave(*item));other23->item_pool.release(*item);});
        check(world23->update_instance_cleanup(182200)==InstanceCleanupResult::completed && world23->next_instance_cleanup()==183200 && !authority23->has_retiring_instance() && authority23->instance_count()==0);
        GroundItem* shutdown_item=nullptr;auto recycled=world23->select_instance(1,{});check(recycled && recycled.generation()!=first_generation);recycled.visit([&](RuntimeZone& zone){check(&zone==selected && zone.world_sends.size()==1);zone.world_sends.drain([&](const WorldZoneSendMessage& message){check(message.kind==WorldZoneSendKind::priority_country && message.country==1 && message.source==5 && message.destination==6 && message.priority==7 && message.payload==shared_world_packet);});shutdown_item=other23->item_pool.allocate();check(shutdown_item);shutdown_item->position={10,0,10};check(zone.items->enter(*shutdown_item,1,23));});recycled.reset();
        check(world23->thread_zones().pending_count()==1 && !world23->thread_zones().contains_active(*selected));
        bool early_promotion=false;try{(void)world23->promote_pending_for_shutdown();}catch(const std::logic_error&){early_promotion=true;}check(early_promotion);
        bool early_poll=false;try{(void)world23->poll_shutdown(2100);}catch(const std::logic_error&){early_poll=true;}check(early_poll);
        world23->sessions().reserve(777,db_session23);
        db_tracker23=VerticalTrackedDbJobs<RootSession>::create(loop_strand,world23->sessions(),db_session23,777,db_dispatch23,
            [&](RootSession&,std::uint32_t,const VerticalTrackedDbJobs<RootSession>::Retry&){db_allowed23.wait();return std::vector<eternity::net::Packet>{{1,2}};},
            [&](RootSession&,std::vector<eternity::net::Packet> packets){check(packets.size()==1);++db_delivered23;},[&](std::exception_ptr error){failure23=error;});
        check(db_tracker23->submit(1));db_services23->started=false;
        db_services23->on_database=[&]{if(!db_signaled23){db_signaled23=true;db_continue23.set_value();}};
        bool rejected_empty_tracker=false;try{world23->begin_shutdown_after_transport_closed({{}},[]{});}catch(const std::invalid_argument&){rejected_empty_tracker=true;}check(rejected_empty_tracker && world23->world_send_running());
        world23->admission().track_connected(connected_send23);world23->admission().track_selection(selection_send23);
        global_send23->pin=attack_actor;global_send23->users.clear();global_send23->retain_calls=0;
        send_update23->events.clear();send_update23->on_packets=[&]{retained_send23.update_net_send();};
        global_send23->on_retain=[&]{world23->begin_shutdown_after_transport_closed({db_tracker23},[&]{
            check(db_signaled23 && db_delivered23==1 && world23->sessions().pending_database_work(777)==0);
            check(world23->sessions().release(777,*db_session23));
            check(world23->thread_zones().pending_count()==0 && world23->thread_zones().contains_active(*selected));
            check(world23->promote_pending_for_shutdown()==0);
            check(!world23->try_finish_shutdown());
            auto held_during_drain=authority23->lease_instance(*selected);check(bool(held_during_drain));
            check(!authority23->drain_all([]{return 100U;}));check(!authority23->lease_instance(*selected));
            check(world23->update_instance_cleanup(183199)==InstanceCleanupResult::skipped);
            check(!world23->poll_shutdown(183200) && world23->world().running() && authority23->instance_count()==1);held_during_drain.reset();
            bool shutdown_release_failed=false;try{(void)world23->poll_shutdown(183200);}catch(const std::logic_error&){shutdown_release_failed=true;}
            check(shutdown_release_failed && authority23->has_retiring_instance() && !world23->thread_zones().contains_active(*selected));
            check(!world23->try_finish_shutdown() && world23->world().running());
            world23->visit_retiring_instance([&](RuntimeZone& zone){check(zone.items->leave(*shutdown_item));other23->item_pool.release(*shutdown_item);});
            check(world23->update_instance_cleanup(183200)==InstanceCleanupResult::completed && authority23->instance_count()==0);
            WorldInstanceCleanupPhase wrap_cleanup;
            check(wrap_cleanup.update(*world23,0xfffffff0U)==InstanceCleanupResult::completed && wrap_cleanup.next_deadline()==984);
            check(wrap_cleanup.update(*world23,983)==InstanceCleanupResult::skipped && wrap_cleanup.update(*world23,984)==InstanceCleanupResult::completed);
            RuntimeZone* primary23=nullptr;check(world23->visit_active_zones([&](RuntimeZone& zone){primary23=&zone;zone.users->attach(*attack_actor,0,0,{});}));check(primary23);
            bool shutdown_visited=false;packet23->on_actor_packets=[&]{shutdown_visited=true;check(!world23->world().shutdown());};
            const auto previous_packet_events=packet23->events.size();check(!world23->update_active_world_prefix(101).completed && shutdown_visited && packet23->events.size()==previous_packet_events+1 && packet23->events.back()=="packet0");
            check(primary23->users->detach(attack_actor->id()));
            poller23=WorldShutdownPoller<ConcreteRoot>::start(loop_strand,world23,std::chrono::milliseconds(1),[]{return 2100U;},[&](std::exception_ptr error){check(!error);drained23=true;});
        });};
        check(!world23->update_world_send(2000000));send_update23->on_packets={};global_send23->on_retain={};
        check(global_send23->retain_calls==1 && global_send23->users.empty());
        world23->admission().untrack(connected_send23);world23->admission().untrack(selection_send23);
        check(!world23->world_send_running() && world23->world().running() && (send_update23->events==std::vector<std::string>{"packets"}));
    });loop_io.run();db_dispatch23.wait_idle();db_dispatch23.shutdown();check(drained23 && !failure23 && (send_update23->events==std::vector<std::string>{"packets"}) && authority23->mob_pool().available()==authority23->mob_pool().size());
    auto f18=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root18;std::unique_ptr<VerticalWorld> quest_stopping;RuntimeZone* quest_zone18=nullptr;
    VerticalWorldPorts interrupt_quest_ports{[&](MobPrimaryZones&){attach(f18,root18);},{},[]{return 100U;}};
    quest_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f18,&f18->backend),std::move(f18->pool),transport,interrupt_quest_ports);root18=quest_stopping->zones();check(quest_stopping->start());
    quest_stopping->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){zone.users=std::make_unique<ZoneUsers>(1,1,0);if(id==0){quest_zone18=&zone;zone.users->attach(*attack_actor,0,0,{});}});
    quest_services->on_retain=[&]{check(!quest_stopping->shutdown());};WorldPrimaryQuestPhase interrupted_quests(quest_services);
    check(interrupted_quests.update(*quest_stopping,100)==PrimaryQuestPhaseResult::interrupted && interrupted_quests.next_deadline()==0 && quest_ports.logs.size()==1);
    root18->serialized([&]{check(quest_zone18->users->detach(attack_actor->id()));});quest_services->on_retain={};check(quest_stopping->shutdown() && root18->mob_pool().available()==root18->mob_pool().size());
    auto f20=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root20;std::unique_ptr<VerticalWorld> logout_stopping;RuntimeZone* logout_zone20=nullptr;
    VerticalWorldPorts interrupt_logout_ports{[&](MobPrimaryZones&){attach(f20,root20);},{},[]{return 100U;}};
    logout_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f20,&f20->backend),std::move(f20->pool),transport,interrupt_logout_ports);root20=logout_stopping->zones();check(logout_stopping->start());
    logout_stopping->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){zone.users=std::make_unique<ZoneUsers>(1,1,0);if(id==0)logout_zone20=&zone;});
    RootAdmissionServices admission20_services;admission20_services.zone=logout_zone20;WorldAdmission admission20(admission20_services);WorldAdmissionUser wait20a,wait20b,wait20c;wait20a.user_id=1;wait20b.user_id=2;wait20c.user_id=3;
    admission20.track_selection(wait20a);admission20.track_selection(wait20b);admission20.track_selection(wait20c);check(admission20.wait(wait20a) && admission20.wait(wait20b) && admission20.wait(wait20c));admission20.update_packets();
    wait20a.logout_type=2;wait20b.logout_type=2;wait20c.logout_type=1;wait20a.logout_tick=100;wait20b.logout_tick=100;wait20c.logout_tick=100;
    auto logout20=std::make_shared<RootLogoutServices>(std::make_shared<int>(1));bool logout_stop_requested=false;
    logout20->on_wait=[&](WorldAdmissionUser&){if(!logout_stop_requested){logout_stop_requested=true;check(!logout_stopping->shutdown());}};WorldPrimaryLogoutPhase stopping_logout(logout20);
    check(stopping_logout.update(*logout_stopping,admission20,100)==PrimaryLogoutResult::interrupted && stopping_logout.next_deadline()==0);
    check(logout_stop_requested && (logout20->events==std::vector<std::string>{"wait1","wait2"}) && wait20a.logout_tick==0 && wait20b.logout_tick==0 && wait20c.logout_tick==100);
    admission20.untrack(wait20a);admission20.untrack(wait20b);admission20.untrack(wait20c);check(logout_stopping->shutdown() && root20->mob_pool().available()==root20->mob_pool().size());
    auto f21=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root21;std::unique_ptr<VerticalWorld> npc_stopping;RuntimeZone* npc_zone21=nullptr;Npc* guard21=nullptr;
    VerticalWorldPorts interrupt_npc_ports{[&](MobPrimaryZones&){attach(f21,root21);},{},[]{return 100U;}};
    npc_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f21,&f21->backend),std::move(f21->pool),transport,interrupt_npc_ports);root21=npc_stopping->zones();check(npc_stopping->start());
    npc_stopping->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){zone.npcs=std::make_unique<ZoneNpcs>(zone.map->definition.geometry(),*other);if(id==0){npc_zone21=&zone;guard21=other->npc_pool.allocate();check(guard21);guard21->id=701;guard21->type=8;guard21->position={10,0,10};guard21->source=&guard_source;guard21->definition=&guard_definition;guard21->disable_time=0;check(zone.npcs->enter(*guard21));}});
    auto npc21=std::make_shared<RootNpcIdleServices>(other->npc_pool,f21->random);npc21->guard=[&](RuntimeZone&,Npc&,std::uint32_t){check(!npc_stopping->shutdown());};WorldPrimaryNpcIdlePhase interrupted_npcs(npc21);
    check(interrupted_npcs.update(*npc_stopping,100)==PrimaryNpcIdleResult::interrupted && interrupted_npcs.next_deadline()==0);
    root21->serialized([&]{check(npc_zone21->npcs->leave(*guard21));other->npc_pool.release(*guard21);});check(npc_stopping->shutdown() && root21->mob_pool().available()==root21->mob_pool().size());
    auto f22=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root22;std::unique_ptr<VerticalWorld> npc_attack_stopping;RuntimeZone* zone22=nullptr;Npc* guard22=nullptr;
    VerticalWorldPorts attack_npc_ports{[&](MobPrimaryZones&){attach(f22,root22);},{},[]{return 100U;}};
    npc_attack_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f22,&f22->backend),std::move(f22->pool),transport,attack_npc_ports);root22=npc_attack_stopping->zones();check(npc_attack_stopping->start());
    npc_attack_stopping->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){zone.npcs=std::make_unique<ZoneNpcs>(zone.map->definition.geometry(),*other);if(id==0){zone22=&zone;guard22=other->npc_pool.allocate();check(guard22);guard22->id=702;guard22->type=8;guard22->position={10,0,10};guard22->source=&guard_source;guard22->definition=&guard_definition;guard22->disable_time=0;check(zone.npcs->enter(*guard22));}});
    auto npc22=std::make_shared<RootNpcIdleServices>(other->npc_pool,f22->random);npc22->guard=[&](RuntimeZone&,Npc&,std::uint32_t){check(!npc_attack_stopping->shutdown());};WorldPrimaryNpcIdlePhase attack_probe(npc22);
    check(!attack_probe.update_attack(*npc_attack_stopping,*zone22,100) && attack_probe.next_deadline()==0);
    root22->serialized([&]{check(zone22->npcs->leave(*guard22));other->npc_pool.release(*guard22);});check(npc_attack_stopping->shutdown() && root22->mob_pool().available()==root22->mob_pool().size());
    auto f16=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root16;std::unique_ptr<VerticalWorld> attack_stopping;
    VerticalWorldPorts interrupt_attack_ports{[&](MobPrimaryZones&){attach(f16,root16);},{},[]{return 100U;}};
    attack_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f16,&f16->backend),std::move(f16->pool),transport,interrupt_attack_ports);root16=attack_stopping->zones();check(attack_stopping->start());
    attack_stopping->visit_primary_zones([&](RuntimeZone& zone){zone.users=std::make_unique<ZoneUsers>(1,1,0);});
    attack_services->zones.clear();attack_services->on_zone=[&]{check(!attack_stopping->shutdown());};WorldPrimaryUserAttackPhase interrupted_attack(attack_services);
    check(interrupted_attack.update(*attack_stopping,100)==PrimaryUserAttackResult::interrupted && interrupted_attack.next_deadline()==0);
    check((attack_services->zones==std::vector<int>{0}) && attack_stopping->shutdown() && root16->mob_pool().available()==root16->mob_pool().size());attack_services->on_zone={};
    auto f17=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root17;auto items17=std::make_shared<RootOtherServices>();auto corpses17=std::make_shared<RootCorpseServices>();NpcDefinitions npcs17;
    MobZoneServicesInputs inputs17{f17,*f17->pool,f17->runtime,npcs17,f17->init,items17->corpses,
        [f17,items17,corpses17](RuntimeZone& zone){return MobZoneServicesPorts{std::make_shared<RootBoundMobs>(f17,zone),items17,corpses17,items17};},
        [](const ConfiguredMap&)->std::span<const LinkedMobGroup>{return {};},[]{return 123456U;},
        [items17](Npc& npc){items17->npc_pool.release(npc);},[](RuntimeZone&){throw std::logic_error("unexpected instance queue");},[]{return ZoneLocalTime{1,2,3,4};}};
    VerticalWorldPorts item_ports{[&](MobPrimaryZones&){attach(f17,root17);},{},[]{return 100U;}};
    auto item_world=VerticalWorld::create(maps,std::move(inputs17),std::move(f17->pool),transport,item_ports);root17=item_world->zones();check(item_world->start());
    item_world->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){if(id==0){auto* item=items17->item_pool.allocate();check(item);item->position={10,0,10};check(zone.items->enter(*item,1,23));}});
    bool item_shutdown_requested=false;items17->on_item_release=[&]{item_shutdown_requested=true;check(!item_world->shutdown());};WorldPrimaryItemPhase stopping_items;
    check(stopping_items.update(*item_world,101)==PrimaryItemPhaseResult::interrupted && stopping_items.next_deadline()==0);
    check(item_shutdown_requested && items17->item_pool.statistics().available==1 && item_world->shutdown() && root17->mob_pool().available()==root17->mob_pool().size());
    auto f14=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root14;
    VerticalWorldPorts phase_ports{[&](MobPrimaryZones&){attach(f14,root14);},{},[]{return 100U;}};
    auto phase_world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f14,&f14->backend),std::move(f14->pool),transport,phase_ports);root14=phase_world->zones();check(phase_world->start());
    RootAdmissionServices empty_admission_services;WorldAdmission empty_admission(empty_admission_services);WorldPrimaryLogoutPhase missing_logout_users(logout_services);bool missing_logout_users_rejected=false;
    try{(void)missing_logout_users.update(*phase_world,empty_admission,5077);}catch(const std::logic_error&){missing_logout_users_rejected=true;}check(missing_logout_users_rejected && missing_logout_users.next_deadline()==0);
    WorldPrimaryNpcIdlePhase missing_npcs(root_npc_idle);bool missing_npcs_rejected=false;
    try{(void)missing_npcs.update(*phase_world,5077);}catch(const std::logic_error&){missing_npcs_rejected=true;}check(missing_npcs_rejected && missing_npcs.next_deadline()==0);
    WorldPrimaryQuestPhase missing_quest_users(quest_services);bool missing_quest_users_rejected=false;
    try{(void)missing_quest_users.update(*phase_world,5077);}catch(const std::logic_error&){missing_quest_users_rejected=true;}check(missing_quest_users_rejected && missing_quest_users.next_deadline()==0);
    WorldPrimaryItemPhase missing_items_phase;bool missing_items_rejected=false;
    try{(void)missing_items_phase.update(*phase_world,5077);}catch(const std::logic_error&){missing_items_rejected=true;}check(missing_items_rejected && missing_items_phase.next_deadline()==0);
    WorldPrimaryUserAttackPhase missing_users_phase(attack_services);bool missing_users_rejected=false;
    try{(void)missing_users_phase.update(*phase_world,5077);}catch(const std::logic_error&){missing_users_rejected=true;}
    check(missing_users_rejected && missing_users_phase.next_deadline()==0);
    IdleFixture phase_idle;auto phase_services=std::make_shared<PhaseFixture>(*f14,phase_idle,linked,f14);WorldPrimaryMobPhase mob_phase(phase_services);
    check(mob_phase.update(*phase_world,5077)==PrimaryMobPhaseResult::completed && mob_phase.next_deadline()==6077 && phase_idle.calls>0);
    check((phase_services->events==std::vector<std::string>{"mob0","trap0","mob1","trap1"}));
    check(mob_phase.update(*phase_world,6076)==PrimaryMobPhaseResult::skipped && phase_services->events.size()==4);
    phase_idle.expected=6077;check(mob_phase.update(*phase_world,6077)==PrimaryMobPhaseResult::completed && mob_phase.next_deadline()==7077 && phase_services->events.size()==8);
    phase_idle.expected=0xfffffff0U;check(mob_phase.update(*phase_world,0xfffffff0U)==PrimaryMobPhaseResult::completed && mob_phase.next_deadline()==984U);
    check(mob_phase.update(*phase_world,983)==PrimaryMobPhaseResult::skipped);phase_idle.expected=984;
    check(mob_phase.update(*phase_world,984)==PrimaryMobPhaseResult::completed);
    phase_world->visit_primary_zones_indexed([&](std::size_t id,RuntimeZone& zone){if(id==0)zone.environment.reset();});
    WorldPrimaryWeatherPhase missing_environment(root_weather);bool missing_environment_rejected=false;
    try{(void)missing_environment.update(*phase_world,100);}catch(const std::logic_error&){missing_environment_rejected=true;}
    check(missing_environment_rejected && missing_environment.next_deadline()==0 && phase_world->shutdown());
    auto f15=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root15;std::unique_ptr<VerticalWorld> phase_stopping;
    VerticalWorldPorts interrupt_phase_ports{[&](MobPrimaryZones&){attach(f15,root15);},{},[]{return 100U;}};
    phase_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f15,&f15->backend),std::move(f15->pool),transport,interrupt_phase_ports);root15=phase_stopping->zones();check(phase_stopping->start());
    IdleFixture interrupted_idle;auto interrupted_services=std::make_shared<PhaseFixture>(*f15,interrupted_idle,linked,f15);WorldPrimaryMobPhase interrupted_phase(interrupted_services);
    interrupted_services->on_traps=[&](RuntimeZone&){check(!phase_stopping->shutdown());};
    check(interrupted_phase.update(*phase_stopping,5077)==PrimaryMobPhaseResult::interrupted && interrupted_phase.next_deadline()==0);
    check((interrupted_services->events==std::vector<std::string>{"mob0","trap0"}) && phase_stopping->shutdown() && root15->mob_pool().available()==root15->mob_pool().size());
    auto f19=std::make_shared<Frame>(maps);std::shared_ptr<MobPrimaryZones> root19;std::unique_ptr<VerticalWorld> weather_stopping;
    VerticalWorldPorts weather_ports{[&](MobPrimaryZones&){attach(f19,root19);},{},[]{return 100U;}};
    weather_stopping=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f19,&f19->backend),std::move(f19->pool),transport,weather_ports);root19=weather_stopping->zones();check(weather_stopping->start());
    weather_stopping->visit_primary_zones([&](RuntimeZone& zone){auto& weather=zone.environment->weather();weather.definition.rate=100;weather.next_update=0;});
    auto weather19=std::make_shared<RootWeatherServices>(f19->random);weather19->sinks[0].on_send=[&]{check(!weather_stopping->shutdown());};WorldPrimaryWeatherPhase interrupted_weather(weather19);
    check(interrupted_weather.update(*weather_stopping,100)==PrimaryWeatherResult::interrupted && interrupted_weather.next_deadline()==0);
    check((weather19->zones==std::vector<int>{0}) && weather19->sinks[0].packets.size()==1 && weather19->sinks[1].packets.empty() && weather_stopping->shutdown() && root19->mob_pool().available()==root19->mob_pool().size());
    std::cout<<"World startup, frame, deferred shutdown and startup cleanup passed\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
