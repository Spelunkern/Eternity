#include "mob_damage_fixture.hpp"
#include "eternity/game/experience_award.hpp"
#include "eternity/game/mob_experience.hpp"
#include "eternity/storage/sql_server.hpp"
#include "eternity/auth/accounts.hpp"
#include "eternity/game/definitions.hpp"
#include "eternity/game/mobs.hpp"
#include "eternity/game/mob_population.hpp"
#include "eternity/game/zones.hpp"
#include "eternity/game/linked_mob_config.hpp"
#include "eternity/game/characters.hpp"
#include "eternity/game/stats.hpp"
#include "eternity/game/equipment.hpp"
#include "eternity/game/skills.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <array>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void verify_mob_population(const eternity::game::MobRuntimeTemplates& definitions) {
    using namespace eternity::game;
    struct Services final:MobPoolServices,ZoneMobServices {
    MobDamageFixture damage_fixture;MobDamagePool& damage_pool() override {return damage_fixture.pool;}

        unsigned int views=0,releases=0,removals=0;
        std::unique_ptr<MobRuntime> construct() override {auto mob=std::make_unique<MobRuntime>();mob->drop_random=ServerRandom(unix_time());return mob;}
        std::uint32_t unix_time() override {return 1700000000;}
        std::uint32_t tick_ms() override {return 0xfffffff0;}
        void release_compiled_ai(MobRuntime& mob) override {require(!mob.use_pais,"Unexpected compiled AI resource in SQL spawn fixture");++releases;}
        void release_stack_ai(MobRuntime&) override {throw std::runtime_error("Unexpected stack AI in SQL spawn fixture");}
        void close_lua(void*) override {throw std::runtime_error("Unexpected Lua resource in SQL spawn fixture");}
        void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua entry in SQL spawn fixture");}
        void send_cell_view(CellCoordinates cell,const eternity::net::Packet& packet) override {if(packet.size()==6 && packet[0]==2 && packet[1]==6) {++removals;return;}require(cell.x==0 && cell.z==0 && packet.size()==17 && packet[0]==1 && packet[1]==6 && packet[6]==1,"SQL mob entry packet/cell changed");++views;}
        std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected SQL mob movement");}
    } services;
    MapDefinition map;map.size=81;map.tiles.resize((81*81+7)/8);MapMobArea area;area.box.minimum={-0.25F,7,0.75F};area.spawns={{1,2},{0,3}};map.mobs.push_back(area);
    MobPopulation population(map,definitions);std::uint32_t serial=100;MobPool pool(serial,services);ConfiguredMap runtime_map;RuntimeZone runtime;runtime.map=&runtime_map;runtime.mobs=std::make_unique<ZoneMobs>(map.geometry(),services);auto& zone=*runtime.mobs;MicrosoftRandom random;
    MobInitPorts ports{random,[&]{return services.tick_ms();},[](MobRuntime&,std::uint8_t)->bool {throw std::runtime_error("Unexpected compiled AI creation in SQL spawn fixture");},[](MobRuntime&) {throw std::runtime_error("Unexpected Lua creation in SQL spawn fixture");}};
    unsigned int linked=0;require(population.initialize(zone,pool,ports,[&]{++linked;require(zone.size()==2,"Linked groups initialized before regular SQL mobs");return false;}),"Linked-group failure changed original regular spawn result");
    require(linked==1 && services.views==2 && serial==102 && population.areas()[0].spawns[0].current==2 && population.areas()[0].spawns[1].current==0,"SQL population counts/bindings changed");
    const auto first_population=zone.all();auto* retained_damage=first_population[0]->damage.get();
    require(retained_damage && retained_damage->fair_combat && retained_damage->records().empty(),"SQL mob constructor did not bind an empty fair damage owner");
    retained_damage->fair_combat=false;retained_damage->add(1,777,100,500,7);

    runtime.instance_portals.fill({55,1});runtime.release_mob_population(pool);
    require(zone.size()==0 && pool.available()==2 && services.releases==2 && runtime.instance_portals[0].id==0 && runtime.instance_portals[9].country==0,"SQL bulk mob release/portal reset changed");
    for(auto* mob:first_population) require(mob->zone==&zone && mob->cell.has_value() && mob->respawn==&population.areas()[0].spawns[0],"SQL bulk release cleared retained source bindings");
    require(population.initialize(zone,pool,ports,[&]{++linked;return false;}) && linked==2 && services.views==4 && zone.find(103)==first_population[1] && zone.find(104)==first_population[0],"SQL population did not reuse released mobs in original LIFO order");
    MobDamageRecord credit;require(zone.find(104)->damage.get()==retained_damage && !retained_damage->fair_combat && retained_damage->maximum_damage(credit) && credit.id==777 && credit.damage==100 && credit.party==7 && zone.find(103)->damage->records().empty(),"SQL mob recycling/Init changed damage ownership or retained credit");
    retained_damage->reset();require(retained_damage->fair_combat && retained_damage->records().empty() && services.damage_fixture.pool.available().size()==1,"SQL explicit damage reset did not return the shared record");

    for(auto* mob:zone.all()) {
        require(mob->definition==definitions.find(1) && mob->respawn==&population.areas()[0].spawns[0] && mob->hp==2765 && mob->sp==874 && mob->mp==1578 && mob->dexterity==60 && mob->wisdom==40 && mob->status==0 && mob->next_new_target==4984 && mob->position.x==-0.25F && mob->position.y==7 && mob->respawn_position.z==0.75F && mob->drop_random.seed()==(1700000000U^mob->id),"Restored SQL mob did not survive allocation, initialization and zone entry");
        require(zone.leave(*mob),"Cannot detach SQL fixture mob");pool.release(*mob);
    }
    require(zone.size()==0 && pool.available()==2 && services.releases==4,"SQL mob pool/zone cleanup changed");
    std::array<ConfiguredMap,1> linked_maps{};for(int id:{1010,1011}) {MapPortal portal;portal.id=id;linked_maps[0].definition.portals.push_back(portal);}
    const auto linked_configuration=LinkedMobConfiguration::decode(
        "[bothmob_1,0,1,10,0,1010]=(1.25,9,0.75),1\n[Mob]=(3.5,9,4,0.5),(1,1),2\n"
        "[bothmob_2,0,1,10,0,1011]=(8,9,10),1\n[RandomRespawn]=1,0,(8,9,10)\n",linked_maps,definitions);
    require(linked_configuration.for_map(0).size()==2 && linked_configuration.for_map(0)[0].portal==&linked_maps[0].definition.portals[0],"SQL linked configuration/portal binding changed");
    runtime.linked_mobs=std::make_unique<LinkedMobPopulation>(linked_configuration.for_map(0));
    require(runtime.linked_mobs->initialize(map,zone,pool,ports,[&]{return services.unix_time();}),"Restored SQL linked population failed");
    auto groups=runtime.linked_mobs->groups();
    require(zone.size()==3 && services.views==7 && serial==107 && groups[0].leader.members[0]==zone.find(105) && groups[0].spawns[0].members[0]==zone.find(106) && groups[0].spawns[0].members[1]==zone.find(107) && groups[1].leader.respawn.current==0 && groups[1].leader.respawn.next==59984,"SQL linked leader/companions/deferred timer changed");
    require(groups[0].spawns[0].members[0]->position.x==3 && groups[0].spawns[0].members[0]->position.z==3.5F,"SQL linked companion placement changed");
    for(auto* mob:zone.all()) require(mob->hp==2765 && mob->sp==874 && mob->mp==1578 && mob->respawn->both_source!=nullptr,"SQL linked resources/respawn binding changed");
    runtime.release_mob_population(pool);require(zone.size()==0 && pool.available()==3 && services.releases==7 && groups[0].leader.members[0]->zone==&zone && groups[0].leader.respawn.current==1 && groups[0].spawns[0].respawn.current==2,"SQL linked population release changed retained records");
    runtime.mob_population=std::make_unique<MobPopulation>(map,definitions);auto& respawn=runtime.mob_population->areas()[0].spawns[0];
    std::vector<char> update_order;
    const auto obelisks=[&](std::uint32_t){update_order.push_back('O');};const auto linked_update=[&](std::uint32_t){update_order.push_back('B');};
    runtime.update_mob_population(pool,ports,1000,obelisks,linked_update);
    require(zone.size()==1 && respawn.current==1 && respawn.next==181000 && update_order==std::vector<char>{'O','B'},"SQL first periodic respawn/deadline/callback order changed");
    runtime.update_mob_population(pool,ports,180999,obelisks,linked_update);require(zone.size()==1 && respawn.current==1 && serial==108,"SQL periodic respawn ignored its deadline");
    runtime.update_mob_population(pool,ports,181000,obelisks,linked_update);
    require(zone.size()==2 && respawn.current==2 && respawn.next==361000 && serial==109 && services.views==9,"SQL periodic respawn did not replenish one record at its deadline");
    runtime.release_mob_population(pool);require(pool.available()==3 && services.releases==9,"SQL periodic mob release changed");
    struct World final:LinkedMobWorld,MobRemovalOwners {
        ZoneMobs& zone;MobPool& pool;RuntimeZone& runtime;std::span<ConfiguredMap> maps;
        World(ZoneMobs& z,MobPool& p,RuntimeZone& r,std::span<ConfiguredMap> m):zone(z),pool(p),runtime(r),maps(m) {}
        void remove_by_id(std::uint32_t id) override {static_cast<void>(zone.remove_by_id(id,pool,*this));}
        void destroy_obelisk(MobRuntime&,std::int32_t) override {throw std::runtime_error("Unexpected SQL obelisk principal");}
        void remove_obelisk_member(MobRuntime&,void*) override {throw std::runtime_error("Unexpected SQL obelisk member");}
        void destroy_linked_group(std::int32_t) override {throw std::runtime_error("Unexpected SQL linked principal removal");}
        void remove_linked_member(MobRuntime& mob,void* source) override {static_cast<LinkedMobSpawn*>(source)->remove(&mob);}
        void set_portal_country(const MapPortal& target,std::int32_t country) override {for(auto& map:maps) for(auto& portal:map.definition.portals) if(&portal==&target) {portal.country=country;runtime.update_instance_portal_country(portal.id,country);return;}throw std::runtime_error("Unknown linked SQL portal owner");}
    } world(zone,pool,runtime,linked_maps);
    runtime_map.settings.create_type=3;runtime.linked_mobs->respawn(0,map,zone,pool,ports,world);
    require(zone.size()==3 && serial==112 && linked_maps[0].definition.portals[0].country==3 && runtime.instance_portal_country(1010)==3,"SQL linked respawn did not set shared/instance portal state");
    auto* principal=groups[0].leader.members[0];require(principal && principal->respawn->both,"SQL linked principal lost timer binding");
    principal->respawn->next=UINT32_MAX;auto expected_random=random;
    const auto expected_deadline=1000U+groups[0].respawn_time+static_cast<std::uint32_t>(expected_random.next()%120-60)*60000U;
    runtime.linked_mobs->account_principal_corpse(*principal,1000,random);
    require(principal->respawn->next==expected_deadline && principal->respawn->current==1 && random.state()==expected_random.state(),"SQL linked principal corpse timer/count/random changed");
    auto* dead=groups[0].leader.members[0];require(dead && dead->id==110 && zone.leave(*dead),"SQL linked leader did not bind/release");pool.release(*dead);
    // The death owner supplies this accounting before invoking BothMobDestroy.
    groups[0].leader.respawn.current=0;groups[0].leader.respawn.next=5000;
    runtime.linked_mobs->destroyed(0,map,zone,pool,ports,world);
    require(zone.size()==2 && serial==114 && groups[0].current_index==0 && groups[0].leader.members[0]==nullptr && linked_maps[0].definition.portals[0].country==0 && runtime.instance_portal_country(1010)==0,"SQL linked terminal phase/removal/reward/portal state changed");
    const auto actual_linked=[&](std::uint32_t tick){runtime.linked_mobs->update(map,zone,pool,ports,tick,world);};
    const auto unexpected_obelisk=[](std::uint32_t){throw std::runtime_error("Instance zone updated static obelisks");};
    runtime.update_mob_population(pool,ports,4999,unexpected_obelisk,actual_linked);require(serial==114 && zone.size()==2,"SQL linked updater ignored the principal deadline");
    runtime.update_mob_population(pool,ports,5000,unexpected_obelisk,actual_linked);
    require(zone.size()==3 && serial==117 && groups[0].leader.members[0]==zone.find(115) && groups[0].spawns[0].members[0]==zone.find(116) && groups[0].spawns[0].members[1]==zone.find(117) && runtime.instance_portal_country(1010)==3,"SQL linked updater did not remove rewards and recreate the group");
    for(auto* mob:zone.all()) require(mob->definition->respawn_selector==3 && mob->hp==2765 && mob->respawn->both_source!=nullptr,"SQL linked lifecycle lost restored resources or bindings");
    runtime.release_mob_population(pool);require(pool.available()==3 && services.releases==17 && services.removals==4,"SQL linked lifecycle did not recycle its allocations");
    const auto* restored=definitions.find(1);require(restored && restored->experience==704 && mob_experience(restored->level,restored->experience,38,0,0,1.0)==844 && restored->respawn_selector==3,"SQL corpse selector was not preserved");
    const auto before_random=random.state();respawn.current=respawn.count;respawn.next=123;
    account_regular_mob_corpse(respawn,*restored,10000,random);
    require(respawn.current==respawn.count-1 && respawn.next==190000 && random.state()==before_random,"SQL corpse accounting changed restored fixed interval or consumed random");
    require(runtime.mob_population->initialize(zone,pool,ports,[]{return true;}),"SQL corpse fixture could not restore regular population");
    require(zone.size()==2 && serial==119 && respawn.current==2,"SQL corpse fixture initial population changed");
    for(auto* mob:zone.all()) {mob->status=2;mob->death_time=100;}
    zone.update_deaths(pool,random,100,*runtime.linked_mobs);require(zone.size()==2 && respawn.current==2,"SQL corpse cleanup used non-strict deadline");
    zone.update_deaths(pool,random,101,*runtime.linked_mobs);
    require(zone.size()==0 && respawn.current==0 && respawn.next==180101 && pool.available()==3 && services.removals==6,"SQL corpse cleanup did not unlink/account/recycle both regular actors");
    runtime.mob_population->update(zone,pool,ports,180100);require(zone.size()==0 && serial==119,"SQL corpse respawn ignored deadline");
    runtime.mob_population->update(zone,pool,ports,180101);
    require(zone.size()==1 && serial==120 && respawn.current==1 && respawn.next==360101 && zone.find(120)->hp==2765,"SQL corpse did not replenish from restored definitions at deadline");
    runtime.release_mob_population(pool);require(pool.available()==3 && services.releases==20,"SQL corpse cycle did not release reused actors");





}

int main() {
    using namespace eternity::storage;
    try {
        SqlServer database(SqlServerConfig::from_environment());
        const auto result = database.query(
            "SET NOCOUNT ON; SELECT CAST(NULL AS varchar(8)) AS NullValue, '' AS EmptyValue, "
            "CAST(9223372036854775807 AS bigint) AS MaxId, "
            "REPLICATE(CAST('abcdef' AS varchar(max)),2000) AS LongValue, N'á漢字' AS UnicodeValue; "
            "SELECT 7 AS SecondResult;");
        require(result.size() == 2, "Multiple result sets not preserved");
        require(result[0].columns.size() == 5 && result[0].rows.size() == 1, "Result shape changed");
        const auto& row = result[0].rows[0];
        require(!row[0], "NULL must remain distinct from empty strings");
        require(row[1] && row[1]->empty(), "Empty string lost");
        require(row[2] == Cell{"9223372036854775807"}, "64-bit identifier lost precision");
        require(row[3] && row[3]->size() == 12000 && row[3]->substr(11994) == "abcdef", "Long cell truncated");
        require(row[4] == Cell{"á漢字"}, "Unicode text was not preserved as UTF-8");
        require(result[1].rows[0][0] == Cell{"7"}, "Second result lost");
        const std::array<Parameter, 4> parameters = {
            std::string{"O'Brien; SELECT 99; --"}, std::int64_t{9223372036854775807LL},
            std::monostate{}, 12.5};
        const auto bound = database.query(
            "SELECT CAST(? AS varchar(128)), CAST(? AS bigint), CAST(? AS varchar(8)), CAST(? AS float);", parameters);
        require(bound.size() == 1 && bound[0].rows.size() == 1, "Parameterized result shape changed");
        require(bound[0].rows[0][0] == Cell{"O'Brien; SELECT 99; --"}, "Bound string was interpreted as SQL");
        require(bound[0].rows[0][1] == Cell{"9223372036854775807"}, "Bound bigint lost precision");
        require(!bound[0].rows[0][2], "Bound NULL lost");
        require(bound[0].rows[0][3] == Cell{"12.5"}, "Bound floating-point value changed");
        const std::array<Parameter,1> unicode_parameter={std::string{"á漢字기본아이템😀"}};
        const auto unicode_bound=database.query("SELECT CAST(? AS nvarchar(128));",unicode_parameter);
        require(unicode_bound.size()==1 && unicode_bound[0].rows[0][0]==Cell{"á漢字기본아이템😀"},
                "Bound Unicode text did not preserve UTF-8/surrogate pairs");
        const std::array<Parameter,1> empty_parameter={std::string{}};
        const auto empty_bound=database.query("SELECT CAST(? AS nvarchar(128));",empty_parameter);
        require(empty_bound.size()==1 && empty_bound[0].rows[0][0]==Cell{""},"Empty Unicode parameter became SQL NULL");
        bool invalid_utf8=false;
        try {
            const std::array<Parameter,1> truncated_utf8={std::string{"\xe2\x82"}};
            (void)database.query("SELECT CAST(? AS nvarchar(128));",truncated_utf8);
        } catch(const std::exception&) {invalid_utf8=true;}
        require(invalid_utf8,"Truncated UTF-8 parameter was silently accepted");
        database.query("CREATE TABLE #EternityTest(Value int NOT NULL);");
        database.begin();
        bool nested_rejected = false;
        try { database.begin(); } catch (const std::logic_error&) { nested_rejected = true; }
        require(nested_rejected, "Nested transaction was silently admitted");
        database.query("INSERT INTO #EternityTest VALUES(1);");
        database.commit();
        database.begin();
        database.query("INSERT INTO #EternityTest VALUES(2);");
        database.rollback();
        database.begin();
        bool conversion_failed=false;
        try {database.query("DECLARE @Value int; SELECT @Value=CAST(N'invalid' AS int);");}
        catch(const std::runtime_error&) {conversion_failed=true;}
        require(conversion_failed,"Pre-transaction SQL conversion was accepted");
        database.rollback();
        require(!database.transaction_active(),"Rollback after early SQL error did not restore connection mode");
        database.begin();
        database.query("INSERT INTO #EternityTest VALUES(3);");
        database.rollback();
        database.begin();
        database.query("INSERT INTO #EternityTest VALUES(4); ROLLBACK TRANSACTION;");
        database.rollback();
        require(!database.transaction_active(),"Rollback after server-ended transaction did not restore connection mode");
        const auto transaction = database.query("SELECT COUNT(*) FROM #EternityTest; DROP TABLE #EternityTest;");
        require(transaction.size() == 1 && transaction[0].rows[0][0] == Cell{"1"}, "Commit/rollback did not preserve state");
        bool failed = false;
        try { (void)database.query("SELECT * FROM dbo.EternityNonexistentTable;"); }
        catch (const SqlError& error) { failed = error.native_error==208 && error.sqlstate.size()==5; }
        require(failed, "SQL errors must propagate to the caller");
        const std::string query_state(database.last_query_sqlstate());
        require(database.last_query_native_error()==208,"Connection did not retain the first native SQL query error");
        require(query_state.size()==5,"Connection did not retain original query SQLSTATE");
        const auto recovered = database.query("SELECT 1;");
        require(database.last_query_sqlstate()==query_state && database.last_query_native_error()==208,"Successful SQL query cleared original diagnostic state");
        database.begin();database.query("SELECT 1;");database.rollback();
        require(database.last_query_sqlstate()==query_state && database.last_query_native_error()==208,"Transaction cleanup cleared original query diagnostic state");
        require(recovered.size() == 1 && recovered[0].rows[0][0] == Cell{"1"}, "Connection unusable after query error");
        const auto definitions = eternity::game::Definitions::load(database);
        require(definitions.catalogs.size() == 8 && !definitions.catalogs.at("Mobs").rows.empty(),
                "Original definition procedures did not load");
        require(definitions.catalogs.at("Mobs").columns.size() == 69, "Original mob definition contract changed");
        const auto statuses=eternity::game::StatusRules::load(definitions.catalogs.at("StatusDefs"));
        eternity::game::CharacterSummary fighter;fighter.level=1;fighter.family=0;fighter.grow=1;
        fighter.strength=14;fighter.dexterity=12;fighter.recovery=9;fighter.intelligence=8;fighter.wisdom=7;fighter.luck=15;
        const auto fighter_status=statuses.base(fighter);
        require(fighter_status.maximum.hp==250 && fighter_status.maximum.mp==85 && fighter_status.maximum.sp==195,
                "Restored status definitions disagree with original fighter resources");
        eternity::game::PlayerCore advancing;advancing.character=fighter;advancing.stat_points=1;advancing.skill_points=2;auto advanced_status=fighter_status;
        statuses.level_up(advancing,advanced_status);const auto reloaded_level_status=statuses.base(advancing.character);
        require(advancing.character.level==2 && advancing.character.strength==15 && advancing.stat_points==6 && advancing.skill_points==5 && advanced_status.attributes.strength==15 && advanced_status.maximum.hp==reloaded_level_status.maximum.hp && advanced_status.maximum.mp==reloaded_level_status.maximum.mp && advanced_status.maximum.sp==reloaded_level_status.maximum.sp,"SQL-backed fighter level-up did not preserve original status growth");
        for(std::uint8_t job=0;job<6;++job) {
            eternity::game::PlayerCore growth;growth.character=fighter;growth.character.job=job;auto before_growth=statuses.base(growth.character);auto after_growth=before_growth;
            statuses.level_up(growth,after_growth);const auto reloaded=statuses.base(growth.character);
            const auto& c=growth.character;const std::array<std::uint16_t,6> actual={c.strength,c.dexterity,c.recovery,c.intelligence,c.wisdom,c.luck};std::array<std::uint16_t,6> expected={14,12,9,8,7,15};const std::array<unsigned,6> class_attribute={0,2,1,5,3,4};++expected[class_attribute[job]];
            require(actual==expected && after_growth.attributes.strength==reloaded.attributes.strength && after_growth.attributes.dexterity==reloaded.attributes.dexterity && after_growth.attributes.recovery==reloaded.attributes.recovery && after_growth.attributes.intelligence==reloaded.attributes.intelligence && after_growth.attributes.wisdom==reloaded.attributes.wisdom && after_growth.attributes.luck==reloaded.attributes.luck && after_growth.maximum.hp==reloaded.maximum.hp && after_growth.maximum.mp==reloaded.maximum.mp && after_growth.maximum.sp==reloaded.maximum.sp,"SQL level-up class attribute/resource mapping changed");
        }
        const auto experience=eternity::game::Experience::load(definitions.catalogs.at("ExpDefs"));
        require(experience.bounds(1,1).previous==0 && experience.bounds(1,1).next>0,"Restored experience curve did not load");
        struct AwardPorts final:eternity::game::ExperienceAwardPorts {
            std::vector<char> order;eternity::net::Packet database,client;std::uint32_t amount=0,source=0;
            void send_database(const eternity::net::Packet& p) override {order.push_back('B');database=p;}
            void send_self(const eternity::net::Packet& p) override {order.push_back('C');client=p;}
            void level_up(eternity::game::PlayerCore& p,eternity::game::BaseStatus&,std::uint32_t value,std::uint32_t origin) override {require(p.character.level==2,"SQL experience award did not update level before notification");order.push_back('L');amount=value;source=origin;}
        } award_ports;
        eternity::game::PlayerCore awarded;awarded.character=fighter;awarded.character.id=42;awarded.user_id=7;auto award_status=fighter_status;const auto threshold=experience.bounds(1,1).next;
        require(threshold<INT32_MAX && experience.bounds(1,2).next>threshold+1,"SQL award fixture needs increasing early levels");
        eternity::game::award_experience(awarded,award_status,statuses,experience,777,static_cast<std::int32_t>(threshold),{true,false,false,false,0},award_ports);
        require(awarded.character.level==2 && awarded.character.strength==15 && awarded.stat_points==5 && awarded.skill_points==3 && awarded.experience==threshold && award_ports.order==std::vector<char>{'B','L'} && award_ports.amount==threshold && award_ports.source==777 && award_ports.database.size()==10 && award_ports.database[0]==2 && award_ports.database[1]==6 && award_ports.database[2]==7,"SQL-backed experience award/level-up/save ordering changed");
        award_ports.order.clear();eternity::game::award_experience(awarded,award_status,statuses,experience,777,1,{true,false,false,false,0},award_ports);
        require(awarded.character.level==2 && awarded.experience==threshold+1 && award_ports.order==std::vector<char>{'B','C'} && award_ports.client==eternity::net::Packet({7,2,1,0,0,0,0,0,0,0}),"SQL-backed experience award without level-up changed");
        const auto items=eternity::game::ItemDefinitions::load(definitions.catalogs.at("Items"));
        require(items.find(5,1) && items.find(30,1),"Restored weapon/gem definitions did not load");
        const auto mobs=eternity::game::MobAttackTemplates::load(definitions.catalogs.at("Mobs"));
        require(mobs.find(1) && mobs.find(1)->level==38 && mobs.find(1)->hp==2765 && mobs.find(1)->attack==167 &&
                mobs.find(1)->variance==83 && mobs.find(1)->physical_defense==38 && mobs.find(1)->magic_defense==31,"Restored mob transformation attack projection changed");
        const auto runtime_mobs=eternity::game::MobRuntimeTemplates::load(definitions.catalogs.at("Mobs"));const auto* first_mob=runtime_mobs.find(1);
        require(first_mob && first_mob->level==38 && first_mob->ai==1 && first_mob->hp==2765 && first_mob->sp==874 && first_mob->mp==1578 && first_mob->dexterity==60 && first_mob->wisdom==40 && first_mob->luck==53 && first_mob->attribute==4 && first_mob->physical_defense==38 && first_mob->magic_defense==31 && first_mob->shield_defense==0 && first_mob->respawn_time==180000,"Original runtime mob projection differs from restored SQL");
        verify_mob_population(runtime_mobs);
        const auto map_config=eternity::game::MapConfiguration::load(".local/server-data/Map.ini");
        const auto server_maps=eternity::game::ServerMaps::load(map_config,".local/server-data/Map");
        const auto linked_config=eternity::game::LinkedMobConfiguration::load(".local/server-data/Obelisk.ini",server_maps.all(),runtime_mobs);
        const std::array<std::pair<std::uint16_t,std::size_t>,9> linked_counts={{{14,2},{17,2},{19,1},{20,1},{48,7},{49,7},{50,3},{64,1},{67,1}}};
        std::size_t linked_total=0,linked_bound=0;
        for(std::size_t id=0;id<server_maps.size();++id) {const auto groups=linked_config.for_map(static_cast<std::uint16_t>(id));linked_total+=groups.size();for(const auto& group:groups) if(group.leader.definitions[0]) ++linked_bound;}
        for(auto [id,count]:linked_counts) require(linked_config.for_map(id).size()==count,"Restored linked configuration map/group distribution changed");
        require(linked_total==25 && linked_bound!=0,"Restored linked configuration lost all SQL mob bindings");

        const auto skills=eternity::game::SkillDefinitions::load(definitions.catalogs.at("Skills"));
        require(skills.find(1,1) && !skills.find(1,4),"Restored original three-level skill definitions did not load");
        eternity::game::Characters characters(database,255,std::chrono::sys_days{std::chrono::year{2024}/1/1});
        const auto empty_roster=characters.load(2147483000);
        require(empty_roster.new_account && empty_roster.country==2 && empty_roster.max_grow==2,
                "Runtime login cannot read the original character/country contracts");
        // Synthetic account and all login log writes remain inside a rolled-back transaction.
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count() % 1000000000;
        const std::string username = "et_it_" + std::to_string(stamp);
        database.begin();
        try {
            const std::array<Parameter, 1> account = {username};
            const auto created = database.query(
                "DECLARE @Name varchar(18)=?; "
                "IF EXISTS (SELECT 1 FROM PS_UserData.dbo.Users_Master WHERE UserID=@Name) "
                "THROW 50001, 'Synthetic test account already exists', 1; "
                "DECLARE @Id int=(SELECT ISNULL(MAX(UserUID),0)+1 FROM PS_UserData.dbo.Users_Master); "
                "INSERT INTO PS_UserData.dbo.Users_Master "
                "(UserUID,UserID,Pw,Admin,AdminLevel,UseQueue,Status,[Leave],UserType,Enpassword) "
                "VALUES(@Id,@Name,'testpass',0,0,0,0,0,'N',master.dbo.fn_md5('testpass')); "
                "SELECT @Id;", account);
            require(created.size() == 1, "Synthetic test account was not created");
            eternity::auth::Accounts accounts(database);
            const auto accepted = accounts.login(username, "testpass", 9000000000001LL, "127.0.0.1");
            require(accepted.status == 0 && std::to_string(accepted.user_id) == *created[0].rows[0][0],
                    "Valid account rejected by original login procedure");
            const auto rejected = accounts.login(username, "wrong", 9000000000002LL, "127.0.0.1");
            require(rejected.status == -1, "Invalid password did not preserve original status");
            const auto unknown = accounts.login("et_missing_000001", "wrong", 9000000000003LL, "127.0.0.1");
            require(unknown.status == -3, "Missing account did not preserve original status");
            database.rollback();
            const auto remaining = database.query("SELECT COUNT(*) FROM PS_UserData.dbo.Users_Master WHERE UserID=?;", account);
            require(remaining[0].rows[0][0] == Cell{"0"}, "Synthetic account was not rolled back");
        } catch (...) {
            try { database.rollback(); } catch (...) {}
            throw;
        }
        std::cout << "SQL Server integration passed: NULL, UTF-8, bigint, long cells, result sets, rollback and error recovery.\n";
        std::cout << "Original definition loaders and transactional account authentication passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
