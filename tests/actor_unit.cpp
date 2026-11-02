#include "eternity/game/actor.hpp"
#include "eternity/game/actor_load.hpp"
#include "eternity/game/character_entry.hpp"
#include "eternity/game/zone_entry.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/world_users.hpp"
#include "eternity/game/world_lifecycle.hpp"
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/entry_delivery.hpp"
#include <bit>
#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene {
    std::vector<std::string> events;std::optional<bool> party;std::uint32_t next=100;
    void record(char route,const net::Packet& packet) {static constexpr char hex[]="0123456789abcdef";std::string result(1,route);for(auto byte:packet){result+=hex[byte>>4];result+=hex[byte&15];}events.push_back(std::move(result));}
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void reset_damage() override {events.emplace_back("D");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return next++;}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {return 13001;}
    void unexpected() {throw std::runtime_error("Unexpected scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected actor operation oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open actor oracle");std::string line;unsigned int count=0;
        const ItemDefinitions default_items;const EnchantValues default_enchant;const MobAttackTemplates default_mobs;
        const ActorEquipmentRules equipment_rules{default_items,default_enchant,default_mobs};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int op=0,s=0;row>>op>>s;
            require(static_cast<bool>(row) && op<7 && s<128,"Malformed actor vector");Scene scene;if(s&8) scene.party=(s&16)!=0;
            EffectStatus status;status.in_zone=(s&1)!=0;status.shape=std::array<std::uint8_t,6>{0,13,100,101,102,255}[s%6];status.form_mob=0x1234;
            status.movement_block=s&1;status.action_block_a=(s>>1)&1;status.action_block_b=(s>>2)&1;status.casting=(s>>3)&1;status.fixed_form=(s>>4)&1;status.movement_restricted=(s>>5)&1;
            status.skills.equipment.attack_speed=static_cast<std::int32_t>(s%13)-2;status.skills.equipment.move_speed=static_cast<std::int32_t>(s%8)-2;
            status.skills.equipment.base.maximum={1000,2000,3000};status.current_target_type=2;
            const auto current=std::array<PlayerResources,4>{{{100,200,300},{999,1999,2999},{1000,2000,3000},{70000,80000,90000}}}[s%4];
            Actor actor(9001,status,current,{},scene,equipment_rules);actor.initializing((s&4)!=0);
            actor.appearance()={(s&2)!=0,static_cast<std::int32_t>(s%4),25,77,0x78563412};
            actor.attack_state()={std::array<std::int32_t,4>{0,1,2,-1}[s%4],std::array<std::int32_t,4>{-2,0,1,7}[s%4],123,456};
            if(op==0) actor.send_shape_self();else if(op==1) actor.shape_changed(actor.status());else if(op==2) actor.send_vehicle_partner();
            else if(op==3) actor.speed_changed(actor.status());else if(op==4) actor.run_mode_changed(actor.status(),(s&1)!=0);else if(op==5) actor.stop_attack(actor.status());
            else {
                SkillDefinition skill;skill.id=321;skill.level=2;skill.effect=4;skill.detail=81;skill.time_heal={3,7,65535};
                const auto elapsed=std::array<std::uint32_t,4>{0,1001,3000,UINT32_MAX}[s%4];
                EffectSource source;source.previous_tick=13001-elapsed;remove_active_effect(actor.status(),skill,source,13001,{},ItemDefinitions{},actor);
            }
            std::int32_t mode=0,recovery=0,target=0;std::uint32_t previous=0,following=0;PlayerResources resources{};std::size_t events=0;
            require(static_cast<bool>(row>>mode>>recovery>>target>>previous>>following>>resources.hp>>resources.mp>>resources.sp>>events),"Missing actor results");
            const auto context=" at actor vector "+std::to_string(count);
            require(actor.attack_state().mode==mode && actor.attack_state().recovery==recovery && actor.status().current_target_type==target && actor.attack_state().previous_tick==previous && actor.attack_state().next_tick==following,"Actor attack state differs"+context);
            require(actor.resources().hp==resources.hp && actor.resources().mp==resources.mp && actor.resources().sp==resources.sp,"Actor resources differ"+context);
            require(scene.events.size()==events,"Actor notification count differs"+context);
            for(const auto& event:scene.events) {std::string original;row>>original;require(event==original,"Actor notification differs"+context+": "+event+" versus "+original);}
            std::string extra;require(!(row>>extra),"Unexpected actor vector data");++count;
        }
        require(count==896,"Actor oracle coverage changed");
        // An expiry traverses the concrete actor adapter, mutates its DB-agent replica and releases its runtime ID.
        Scene scene;EffectStatus status;status.skills.equipment.base.maximum={100,100,100};Actor actor(9001,status,{75,50,25},{{321,1,1},{321,1,5}},scene,equipment_rules);
        SkillDefinition effect;effect.id=321;effect.level=1;effect.effect=4;effect.detail=82;effect.abilities[0]={1,10};
        const auto id=actor.allocate_id();require(id.has_value(),"Actor allocation failed");actor.effects().append(*id,{{321,1,1},1000,2000,false},effect);
        actor.initializing(true);actor.effects().initialize(actor.status(),actor);actor.initializing(false);scene.events.clear();
        actor.effects().expire(2000,actor.status(),{},ItemDefinitions{},actor,actor);
        require(actor.status().skills.equipment.base.maximum.hp==100 && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].remaining==5 && !actor.effects().contains(*id),"Concrete actor expiry did not reverse and update replica");
        require(scene.events.size()==2 && scene.events[0]=="S0e0564000000" && scene.events[1]=="F100","Concrete actor expiry ordering changed");
        storage::ResultSet skill_table;skill_table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
        for(unsigned int id:{100,101,102}) {
            storage::Row row(skill_table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]="1";row[2]=id==100?"4":"82";row[3]=id==100?"0":"4";
            row[8]="1";row[9]=id==100?"40":"100";row[10]="2";row[11]=id==100?"60":"100";row[12]="4";row[13]=id==100?"7":"5";row[22]="100";
            if(id==102) row[2]="93";skill_table.rows.push_back(row);
        }
        const auto skills=SkillDefinitions::load(skill_table);storage::ResultSet status_table{{"Level","Job","HP","MP","SP"},{{"1","0","1000","2000","3000"}}};
        const auto statuses=StatusRules::load(status_table);const ItemDefinitions items;const EnchantValues enchant;
        const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});const ActorLoadRules rules{statuses,items,enchant,skills,quests,default_mobs};
        PlayerSnapshot player;auto& character=player.core.character;character.id=9001;character.level=1;character.map=1;character.strength=10;character.dexterity=20;
        character.recovery=30;character.intelligence=40;character.wisdom=50;character.luck=60;character.hp=2000;character.mp=3000;character.sp=4000;
        player.skills={{100,5,1,0,0}};player.effects={{101,1,30}};player.core.user_id=12345;player.core.money=0xfffffff0U;
        BlessingDefinition blessing;blessing.modifiers[1]=10;blessing.modifiers[5]=5;blessing.modifiers[8]=3;Scene loading;
        {
            auto loaded=load_actor(player,rules,{10000,0,2,&blessing},loading);
            require(loaded.actor && !loaded.learned.invalid && loaded.learned.active.size()==1 && loaded.learned.active[0].end_tick==15000,"Actor load lost learned cooldown");
            require(loaded.actor->account_id()==12345 && loaded.actor->money()==0xfffffff0U,"Actor load lost account identity or unsigned money");
            const auto& loaded_status=loaded.actor->status();const auto maximum=loaded_status.skills.equipment.base.maximum;
            require(maximum.hp==1429 && maximum.mp==2425 && maximum.sp==3110 && loaded_status.skills.equipment.base.attributes.strength==22,"Actor equipment/blessing/passive/effect order changed");
            require(loaded.actor->resources().hp==1429 && loaded.actor->resources().mp==2325 && loaded.actor->resources().sp==3110,"Actor load postponed intermediate effect clamping until final maxima");
            require(loading.events==std::vector<std::string>{"S0b0529230000027909"},"Actor load initialization suppression or active MP notification changed");
            require(loaded_status.skills.benefits.bank_double==1 && loaded_status.skills.benefits.no_durability_loss==1 && loaded_status.skills.benefits.experience_half_bonus==1,"Actor billing phase changed");
            require(loaded.actor->effects().size()==1 && loaded.actor->replica().snapshot().size()==1,"Actor load lost its runtime effect or replica");
        }
        require(loading.events==std::vector<std::string>{"S0b0529230000027909","F100"},"Actor destruction reversed effects or failed to release its instance");
        {
            GuildUser guild_user;guild_user.character_id=9001;std::vector<net::Packet> guild_packets;unsigned int guild_allocations=0,guild_releases=0;
            GuildMembership guild(55,{[&](const net::Packet&){++guild_allocations;return true;},[&]{++guild_releases;},[&](GuildUser& user,net::Packet p){require(&user==&guild_user,"Guild transport lost the loaded actor");guild_packets.push_back(std::move(p));}});
            auto guild_record=std::make_shared<GuildMember>();guild_record->id=9001;guild_record->name="NativeHero";guild_record->rank=-1;guild_record->ranking_entry=77;guild.offline.emplace(9001,guild_record);GuildDirectory guild_directory;guild_directory.add(9001,&guild);
            Scene entry_scene;ActorLoadPipeline pipeline(player,rules,{10000,0,2,&blessing},entry_scene);
            require(pipeline.result().actor && entry_scene.events.empty(),"DB restoration initialized actor effects before entry notification");
            bool rejected=false;try{pipeline.passive();}catch(const std::logic_error&){rejected=true;}require(rejected,"Actor initialization accepted out-of-order passive work");rejected=false;try{auto early=pipeline.release();}catch(const std::logic_error&){rejected=true;}require(rejected,"Uninitialized actor escaped its pipeline");
            CharacterEntryState entry;entry.user_id=player.core.user_id;entry.character_id=character.id;entry.level=character.level;
            CharacterEntryPorts ports;ports.send_client=[&](net::Packet p){entry_scene.record('S',p);};ports.send_manager=[&](net::Packet p){entry_scene.record('M',p);};ports.world_day=[](){return 1234U;};ports.system_time=[](){return 5678U;};ports.lock_guild=[](){return false;};ports.unlock_guild=[](){throw std::runtime_error("Unowned guild lock released");};std::vector<CharacterEntryStep> remaining_services;
            ports.step=[&](CharacterEntryStep step) {
                switch(step) {
                case CharacterEntryStep::equipment:pipeline.equipment();break;
                case CharacterEntryStep::passive:pipeline.passive();break;
                case CharacterEntryStep::effects:pipeline.effects();break;
                case CharacterEntryStep::billing: {
                    pipeline.billing();const auto& actor=*pipeline.result().actor;const auto maximum=actor.status().skills.equipment.base.maximum;const auto current=actor.resources();entry.maximum={std::bit_cast<std::int32_t>(maximum.hp),std::bit_cast<std::int32_t>(maximum.mp),std::bit_cast<std::int32_t>(maximum.sp)};entry.current={std::bit_cast<std::int32_t>(current.hp),std::bit_cast<std::int32_t>(current.mp),std::bit_cast<std::int32_t>(current.sp)};break;
                }
                case CharacterEntryStep::init_guild:guild_directory.initialize(guild_user);pipeline.result().actor->relations().guild_id=guild_user.guild_id;break;
                case CharacterEntryStep::stop_attack: {auto& actor=*pipeline.result().actor;actor.stop_attack(actor.status());break;}
                default:remaining_services.push_back(step);break; // Explicit test ports for catalog/social/zone delivery.
                }
            };
            complete_character_selection(entry,ports);auto initialized=pipeline.release();
            require(initialized.actor && entry.ready && entry.runtime_id==9001 && entry.current==std::array<std::int32_t,3>{1429,2325,3110},"Entry pipeline changed original initialization/clamp results");
            require(entry_scene.events==std::vector<std::string>{"M030839300000","S0b0529230000027909","S04010029230000","S0404d2040000","S14012e160000","S1b020001"},"Entry sent its ACK before native effect/resource initialization or reordered its envelopes");
            require(guild_user.guild==&guild && guild_user.guild_id==55 && guild_user.rank==-1 && guild.online.size()==1 && guild.offline.empty() && guild_packets.size()==1 && guild_packets[0][0]==0x12 && guild_packets[0][1]==0x0d && initialized.actor->relations().guild_id==55,"Selection did not bind actual native guild membership before catalog delivery");
            require(!remaining_services.empty() && remaining_services.front()==CharacterEntryStep::wait_user && remaining_services.back()==CharacterEntryStep::flush,"Entry lost its external scene/catalog service ordering");
            SocialCatalog social(player.core.user_id,9001),peer_social(999,41);social.add_friend({41,0,0,5,"Peer","memo"});social.add_block({52,"Block",""});peer_social.add_friend({9001,0,0,0,"Owner",""});EntryCatalogs catalogs(player.core.user_id);std::vector<net::Packet> delivered;std::vector<std::uint32_t> notified;EntryDeliveryPorts delivery_ports;delivery_ports.send_self=[&](net::Packet p){delivered.push_back(std::move(p));};delivery_ports.send_peer=[&](std::uint32_t id,net::Packet p){require(p==net::Packet{7,0x22,0x29,0x23,0,0,1},"Native friend login notice lost original identity/status");notified.push_back(id);};delivery_ports.social=[&](std::uint32_t id)->const SocialCatalog*{return id==41?&peer_social:nullptr;};delivery_ports.tick=[](){return 10000U;};delivery_ports.kill_count=[](std::uint8_t country){require(country==2,"Entry blessing status used another country");return EntryKillCount{0xabcdef,0};};EntryDelivery delivery(player,initialized,social,catalogs,{0,100},"",delivery_ports);initialized.actor->money()=77;
            for(auto step:{CharacterEntryStep::init_friends,CharacterEntryStep::data,CharacterEntryStep::items,CharacterEntryStep::bank,CharacterEntryStep::skills,CharacterEntryStep::effect_packets,CharacterEntryStep::quests,CharacterEntryStep::finished_quests,CharacterEntryStep::friends,CharacterEntryStep::speed,CharacterEntryStep::kc,CharacterEntryStep::save_points,CharacterEntryStep::honor,CharacterEntryStep::purchase_history,CharacterEntryStep::stored_points,CharacterEntryStep::quick}) require(delivery.send(step),"Native catalog owner rejected a required entry step");
            std::vector<unsigned int> opcodes;for(const auto& p:delivered) opcodes.push_back(unsigned(p[0])|(unsigned(p[1])<<8));require(opcodes==std::vector<unsigned int>{0x105,0x108,0x10a,0x2201,0x2208,0x51c,0x211,0x230,0x2601,0x10b} && notified==std::vector<std::uint32_t>{41},"Actual actor/catalog entry delivery omitted or reordered original packets");require(delivered[0][26]==77 && delivered[0][27]==0 && delivered[0][28]==0 && delivered[0][29]==0 && player.core.money==0xfffffff0U,"Entry data used stale snapshot money instead of the actor's balance");require(!delivery.send(CharacterEntryStep::wait_user),"Unbound entry lifecycle step reported fabricated success");bool wrong_owner=false;try{SocialCatalog foreign(999,9001);EntryDelivery invalid_delivery(player,initialized,foreign,catalogs,{0,100},"",delivery_ports);}catch(const std::invalid_argument&){wrong_owner=true;}require(wrong_owner,"Entry delivery accepted another account's social catalog");
            ConfiguredMap map;map.settings.id=20;map.settings.dungeon=true;map.definition.size=81;RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);
            auto& admitted=*initialized.actor;admitted.attack_state().motion=13;admitted.status().current_target_type=3;admitted.appearance().vehicle_mode=2;admitted.status().shape=13;entry_scene.events.clear();ZoneEntryState membership;membership.character_id=9001;membership.permission=10;membership.direction=0x1234;membership.position={40,-17.5F,79};membership.guild_user_level=guild_user.rank;
            ZoneActorEntryServices services;services.send_view=[&](CellCoordinates c,net::Packet p){require(c.x==1 && c.z==1,"Zone enter broadcast used the wrong cell");entry_scene.record('V',p);};services.cell_info=[&](CellCoordinates c){require(c.x==1 && c.z==1 && admitted.status().in_zone && zone.users->position(9001).has_value(),"Cell snapshot preceded actual actor membership");entry_scene.events.push_back("I");};services.obelisks=[&](){entry_scene.events.push_back("O");};services.nprotect=[&](){NProtectState disabled;require(send_nprotect_query(false,disabled,{}),"Disabled NProtect rejected actual actor entry");entry_scene.events.push_back("E");};services.diagnostic=[&](){entry_scene.events.push_back("DIAGNOSTIC");};services.close=[&](std::uint32_t reason){entry_scene.events.push_back("CLOSE"+std::to_string(reason));};
            require(enter_zone_actor(zone,admitted,membership,services) && membership.bound && membership.map==20 && zone.users->size()==1 && admitted.appearance().vehicle_mode==0,"Concrete actor did not enter its original map cell or dismount in a dungeon");
            require(entry_scene.events.size()==7 && entry_scene.events[0]=="V0102292300000234120000204200008cc100009e42ffffffff00000000" && entry_scene.events[1]=="C1d05292300000d" && entry_scene.events[2].starts_with("N1c0529230000") && entry_scene.events[3]=="I" && entry_scene.events[4]=="S5104000000" && entry_scene.events[5]=="O" && entry_scene.events[6]=="E","Concrete zone entry changed view/shape/speed/cell/weather/obelisk/lifecycle ordering");
            require(admitted.attack_state().motion==0 && admitted.movement_check().previous_time==13001 && admitted.movement_check().check_time==18001 && admitted.movement_check().previous.x==40 && admitted.status().current_target_type==0,"Actual actor zone lifecycle did not reset movement/combat state");
            require(zone.users->detach(9001),"Cannot detach native entry fixture");membership.position.x=81;admitted.appearance().vehicle_mode=2;entry_scene.events.clear();require(!enter_zone_actor(zone,admitted,membership,services) && !admitted.status().in_zone && !zone.users->size() && admitted.appearance().vehicle_mode==2 && entry_scene.events==std::vector<std::string>{"DIAGNOSTIC","CLOSE9"},"Out-of-map entry mutated membership/vehicle state or sent success data");
            struct WorldServices final:WorldAdmissionServices {
                RuntimeZone& zone;std::function<bool()> entry;std::vector<std::string>& events;std::function<void(WorldAdmissionUser&)> register_user;
                WorldServices(RuntimeZone& z,std::function<bool()> e,std::vector<std::string>& v):zone(z),entry(std::move(e)),events(v) {}
                void packets(WorldAdmissionUser&) override {throw std::runtime_error("Unexpected selection/connection packet work");}
                RuntimeZone* select_zone(WorldAdmissionUser&) override {events.push_back("SELECT");return &zone;}
                void register_world(WorldAdmissionUser& user) override {events.push_back("REGISTER");register_user(user);}
                bool enter_zone(WorldAdmissionUser&,RuntimeZone& selected) override {require(&selected==&zone,"Admission changed selected zone");return entry();}
                void close(WorldAdmissionUser&,std::uint32_t) override {throw std::runtime_error("Unexpected world-state rejection");}
                void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override {throw std::runtime_error("Unexpected world-state diagnostic");}
                std::optional<std::uint32_t> rank_window(RuntimeZone&) override {throw std::runtime_error("Unexpected ranked-map clock");}
                bool lock_guild(WorldAdmissionUser&) override {throw std::runtime_error("Unexpected guild lock");}
                void unlock_guild(WorldAdmissionUser&) override {throw std::runtime_error("Unexpected guild unlock");}
            };
            WorldAdmissionUser waiting;waiting.user_id=player.core.user_id;waiting.map=20;waiting.logout_type=7;waiting.logout_tick=8;waiting.access.last_rank_entry=guild_user.ranking_entry;
            WorldServices world_services(zone,[&](){return enter_zone_actor(zone,admitted,membership,services);},entry_scene.events);WorldUser live{admitted,waiting};WorldUsers* lookup=nullptr;
            WorldLifecycleState lifecycle;lifecycle.subject={player.core.user_id,admitted.id(),player.core.experience,"native",admitted.appearance().identity.name,character.level,20,membership.position};lifecycle.money=admitted.money();lifecycle.initial_money=admitted.money();lifecycle.bank_money=static_cast<std::uint32_t>(player.core.stored_money);
            OfflineMoney offline;std::vector<net::Packet> lifecycle_logs,lifecycle_client;std::vector<WorldLifecycleStep> lifecycle_steps;WorldLifecyclePorts lifecycle_ports;
            lifecycle_ports.admin_enter=[]()->bool{throw std::runtime_error("Unexpected administrator entry");};lifecycle_ports.close=[](std::uint32_t){throw std::runtime_error("Unexpected lifecycle close");};lifecycle_ports.local_calendar=[](){return std::chrono::sys_seconds{std::chrono::seconds{1700000000}};};lifecycle_ports.game_log=[&](net::Packet p){lifecycle_logs.push_back(std::move(p));};lifecycle_ports.manager=[](net::Packet){throw std::runtime_error("Unexpected lifecycle diagnostic");};lifecycle_ports.take_money=[&](std::uint32_t id){return offline.take(id);};lifecycle_ports.save_money=[&](std::uint32_t id,std::uint32_t money){offline.save(id,money);};
            SocialCatalog departing(player.core.user_id,admitted.id());departing.add_friend({admitted.id()});
            lifecycle_ports.step=[&](WorldLifecycleStep step){
                lifecycle_steps.push_back(step);
                if(step==WorldLifecycleStep::guild_list) for(auto& p:guild_listings({{55,{55,0,0,0,"NativeGuild","Leader",""}}},0)) lifecycle_client.push_back(std::move(p));
                else if(step==WorldLifecycleStep::release_guild) {guild_user.ranking_entry=waiting.access.last_rank_entry;guild_directory.release(guild_user);}
                else if(step==WorldLifecycleStep::release_friends) departing.notify_offline([&](std::uint32_t id)->const SocialCatalog* {return lookup->find(id)?&departing:nullptr;},[](std::uint32_t,net::Packet){throw std::runtime_error("Departing actor remained visible to ReleaseFriend");});
                // Party/guild/admin/dead-list internals are explicit test ports.
            };
            WorldUsers users({[&](WorldUser& user){require(&user==&live && lookup->find(admitted.id())==&live && !lookup->find_zone(admitted.id()),"World entry callback preceded indexing or exposed a waiting actor as in-zone");require(enter_world(lifecycle,lifecycle_ports),"Actual actor lifecycle rejected entry");},[&](WorldUser& user){require(&user==&live && !lookup->find(admitted.id()),"World leave callback preceded index removal");lifecycle.money=admitted.money();lifecycle.vehicle_mode=static_cast<std::uint32_t>(admitted.appearance().vehicle_mode);leave_world(lifecycle,lifecycle_ports);}});lookup=&users;
            world_services.register_user=[&](WorldAdmissionUser& user){require(&user==&waiting,"World index lost admission identity");users.enter(live);};WorldAdmission world(world_services);world.track_selection(waiting);entry_scene.events.clear();require(world.wait(waiting) && waiting.location==2 && !waiting.logout_type && !waiting.logout_tick,"Native world wait did not clear logout state");world.update_packets();require(waiting.location==3 && waiting.zone==&zone && !world.selection_count() && !world.waiting_count() && world.zone_waiting_count(zone)==1 && entry_scene.events==std::vector<std::string>{"REGISTER","SELECT"},"Native waiting user did not reach the selected zone queue");
            entry_scene.events.clear();require(!world.client_enter(waiting) && waiting.location==3 && world.zone_waiting_count(zone)==1 && entry_scene.events==std::vector<std::string>{"DIAGNOSTIC","CLOSE9"},"Failed native zone admission did not return its waiting user to the zone queue");membership.position.x=40;entry_scene.events.clear();require(world.client_enter(waiting) && waiting.location==4 && !world.zone_waiting_count(zone) && admitted.status().in_zone && zone.users->size()==1,"Native waiting/0201/actor path failed to complete zone admission");
            require(users.find_zone(admitted.id())==&live && users.find_zone_move(admitted.id())==&live,"Admitted actor was absent from live world lookup");waiting.logout_tick=1;require(users.find_zone(admitted.id())==&live && !users.find_zone_move(admitted.id()),"Pending logout did not exclude actor from move lookup");waiting.access.last_rank_entry=88;users.leave(live);require(users.id_count()==0 && users.name_count()==0,"World index retained departing actor");
            require(lifecycle_logs.size()==2 && lifecycle_logs[0][0]==6 && lifecycle_logs[0].size()==113 && lifecycle_logs[1][0]==7 && lifecycle_logs[1].size()==108 && offline.take(admitted.id())==admitted.money(),"Indexed actor lifecycle lost source logs or offline money");require(lifecycle_client.size()==3 && lifecycle_client.front()==net::Packet{0x0d,0x0d} && lifecycle_client[1][0]==0x2f && lifecycle_client.back()==net::Packet{0x0e,0x0d} && lifecycle_steps==std::vector<WorldLifecycleStep>{WorldLifecycleStep::guild_list,WorldLifecycleStep::release_guild,WorldLifecycleStep::release_friends,WorldLifecycleStep::leave_dead},"Indexed lifecycle reordered actual guild catalog/social cleanup or required services");require(guild.online.empty() && guild.offline.at(9001)==guild_record && !guild_record->user && guild_record->ranking_entry==88 && guild.leaders[0]==nullptr && guild_allocations==2 && guild_releases==2,"Actual guild departure did not preserve the ranking stamp, free the officer slot or release both broadcast buffers");
        }
        loading.events.clear();player.skills[0].id=999;const auto invalid=load_actor(player,rules,{10000,0,0,nullptr},loading);
        require(!invalid.actor && invalid.learned.invalid && loading.events.empty(),"Invalid learned definition proceeded into actor initialization");
        player.skills[0].id=100;player.effects={{102,1,30}};bool scene_failure=false;
        try {auto failed=load_actor(player,rules,{10000,0,0,nullptr},loading);}catch(const std::runtime_error&){scene_failure=true;}
        require(scene_failure && loading.events==std::vector<std::string>{"F101"},"Failed actor initialization leaked an allocated effect or emitted reversal packets");
        const auto equipped_definitions=ItemDefinitions::load(item_definitions());EnchantValues equipped_enchant;
        for(std::size_t i=0;i<21;++i) equipped_enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};
        const auto mob_templates=MobAttackTemplates::load({{"MobID","Level","HP","Attack1","Attackplus1","Defense","Magic"},{{"1","38","2765","167","83","38","31"}}});
        PlayerItem equipped_weapon;equipped_weapon.type=5;equipped_weapon.type_id=1;equipped_weapon.slot=5;equipped_weapon.craft="01020304050607080903";equipped_weapon.gems[0]=1;
        PlayerItem equipped_armor;equipped_armor.type=16;equipped_armor.type_id=1;equipped_armor.slot=1;
        EffectStatus transformed;transformed.level=30;transformed.in_zone=true;
        transformed.skills.equipment=initialize_equipment({{100,200,300,400,500,600},{10000,20000,30000}},{equipped_weapon,equipped_armor},equipped_definitions,equipped_enchant);
        transformed.skills.equipment.quality[5]=1;transformed.skills.equipment.weapon_damage.fill(7);
        const auto previous_maximum=transformed.skills.equipment.base.maximum;const auto previous_range=transformed.skills.equipment.range;
        Scene transformed_scene;Actor copied(9001,transformed,{50,60,70},{},transformed_scene,{equipped_definitions,equipped_enchant,mob_templates},{equipped_weapon,equipped_armor});
        SkillDefinition transformation;transformation.id=200;transformation.level=1;transformation.effect=4;transformation.detail=90;transformation.change_type=7;transformation.change_level=1;
        apply_active_effect(copied.status(),transformation,0,copied);
        require(copied.status().shape==7 && copied.status().skills.equipment.attack.item_attack==167 && copied.status().skills.equipment.variance==83,"Actor transformation did not use its mob attack template");
        remove_active_effect(copied.status(),transformation,{},10000,{},equipped_definitions,copied);
        const auto& rebuilt=copied.status().skills.equipment;
        require(copied.status().shape==0 && rebuilt.attack.item_attack==559 && rebuilt.variance==533 && rebuilt.attack.physical_defense==33 && rebuilt.attack.magic_defense==34,"Actor transformation reversal did not rebuild cached-quality equipment");
        require(rebuilt.base.maximum.hp==previous_maximum.hp && rebuilt.base.maximum.mp==previous_maximum.mp && rebuilt.base.maximum.sp==previous_maximum.sp && rebuilt.range==previous_range,"Actor attack rebuild changed resource maxima or attack range");
        require(transformed_scene.events==std::vector<std::string>{"C1d052923000007","C1d052923000000"},"Actor transformation view sequence changed");
        Scene elemental_scene;EffectStatus elemental_status;elemental_status.skills.equipment.base.maximum={1000,1000,1000};
        std::array<SkillDefinition,3> elemental_skills{};
        Actor elemental(9001,elemental_status,{500,500,500},{{200,1,30},{201,1,30},{202,1,30}},elemental_scene,equipment_rules);
        for(std::size_t i=0;i<3;++i) {auto& skill=elemental_skills[i];skill.id=static_cast<std::uint16_t>(200+i);skill.level=1;skill.effect=4;skill.detail=i==0?82:94;skill.attribute=static_cast<std::uint8_t>(i);skill.abilities[0]={1,1};elemental.effects().append(static_cast<std::uint32_t>(1000+i),{{skill.id,1,30},10000,12000,false},skill);}
        elemental.effects().initialize(elemental.status(),elemental);
        require(elemental.status().skills.equipment.base.maximum.hp==1002 && elemental.effects().size()==2 && elemental.replica().snapshot().size()==2 && elemental.replica().snapshot()[1].skill_id==202,"Concrete actor lost original shared-cursor element reinitialization behavior");
        require(elemental_scene.events==std::vector<std::string>{"S0e05e9030000","F1001"},"Concrete actor element replacement notifications changed");
        Scene wear_scene;EffectStatus wear_status;wear_status.level=15;
        auto worn=equipped_armor;worn.slot=1;worn.quality=501;worn.bag=0;
        wear_status.skills.equipment=initialize_equipment({}, {worn},equipped_definitions,equipped_enchant);
        recalculate_attack(wear_status);
        Actor wearing(9001,wear_status,{100,100,100},{},wear_scene,{equipped_definitions,equipped_enchant,mob_templates},{worn});
        wearing.wear_defense();
        const auto& wear_equipment=wearing.status().skills.equipment;
        require(wearing.inventory()[0].quality==500 && wear_equipment.quality[1]==1 && wearing.defense_wear_cursor()==3,"Actor wear did not select armor or cross durability threshold");
        require(wear_equipment.attack.physical_defense==wear_status.skills.equipment.attack.physical_defense-502 && wear_equipment.attack.magic_defense==wear_status.skills.equipment.attack.magic_defense-502,"Actor wear did not update quality-dependent defenses");
        const auto calculated=calculate_attack(wearing.status().level,wear_equipment.base.attributes,wear_equipment.attack);
        for(std::size_t category=0;category<3;++category) require(wearing.status().calculated_attack[category].defense==calculated[category].defense,"Actor wear left final defense stale");
        wearing.status().shape=13;wearing.wear_defense();require(wearing.defense_wear_cursor()==3,"Transformed actor advanced wear cursor");
        wearing.status().shape=0;wearing.status().skills.benefits.no_durability_loss=1;wearing.wear_defense();
        require(wearing.defense_wear_cursor()==3 && wearing.inventory()[0].quality==500 && wear_scene.events.empty(),"Durability protection mutated gear or emitted packets");
        SkillDefinition removable;removable.id=300;removable.level=1;removable.effect=4;removable.detail=82;removable.abilities={{{1,17},{4,31},{16,9}}};
        SkillDefinition persistent;persistent.id=301;persistent.level=1;persistent.effect=4;persistent.detail=95;persistent.attribute=8;persistent.preserve_on_death=true;persistent.offline_countdown=true;
        const auto before_death_hp=wearing.status().skills.equipment.base.maximum.hp;
        wearing.effects().append(2000,{{300,1,30},0,30000,false},removable);wearing.persist_applied({300,1,30});
        wearing.effects().append(2001,{{301,1,30},0,30000,true},persistent);wearing.persist_applied({301,1,30});
        wearing.effects().initialize(wearing.status(),wearing);wearing.appearance().copied_user.emplace();
        require(wearing.status().skills.equipment.defense_attribute==8,"Persistent defense effect did not initialize");
        wearing.clear_effects_on_death(5);
        require(wearing.effects().size()==1 && wearing.effects().contains(2001) && wearing.replica().snapshot().size()==1 && wearing.replica().snapshot()[0].skill_id==301,"Actor death cleanup removed a persistent effect or retained its removed replica");
        require(wearing.status().skills.equipment.base.maximum.hp==before_death_hp && wearing.status().skills.equipment.defense_attribute==1 && wearing.status().skills.equipment.defense_attribute_skill && wearing.appearance().copied_user.has_value(),"Actor death cleanup did not reverse resources, restore armor element or retain cached copied appearance");
        require(wear_scene.events==std::vector<std::string>{"F2000"},"Actor death cleanup emitted expiry notices or changed callback order");
        std::cout<<count<<" original actor operations, concrete effect expiry and durability integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
