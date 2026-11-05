#include "eternity/game/vertical_protocol.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/vertical_session_character.hpp"
#include "eternity/game/vertical_entry_admission.hpp"
#include "eternity/game/vertical_social_selection.hpp"
#include "eternity/game/vertical_world_session_social.hpp"
#include "equipment_fixture.hpp"
#include <bit>
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene ,public ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
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
int main() {try {
        const MobAttackTemplates default_mobs;
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
        Scene scene;ActorLoadPipeline pipeline(player,rules,{10000,0,0,nullptr},scene);
        SocialCatalog social(player.core.user_id,character.id);EntryCatalogs catalogs(player.core.user_id);
        CharacterEntryState state;state.user_id=player.core.user_id;state.character_id=character.id;state.level=character.level;
        VerticalProtocolServices services;services.delivery.send_self=[&](net::Packet p){scene.record('S',p);};
        services.delivery.send_peer=[](std::uint32_t,net::Packet){};services.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
        services.delivery.tick=[](){return 10000U;};services.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
        services.send_manager=[&](net::Packet p){scene.record('M',p);};services.world_day=[](){return 1234U;};services.system_time=[](){return 5678U;};
        services.lock_guild=[](){return false;};services.unlock_guild=[](){};
        std::vector<CharacterEntryStep> lifecycle;services.lifecycle=[&](CharacterEntryStep step,Actor& actor){require(&actor==pipeline.result().actor.get(),"wrong actor");lifecycle.push_back(step);if(step==CharacterEntryStep::pvp_reset)actor.status().shape=13;};
        deliver_vertical_entry(state,pipeline,player,social,catalogs,{0,100},"",services);
        require(state.ready && state.runtime_id==character.id,"entry did not finish");
        require(lifecycle==std::vector<CharacterEntryStep>{CharacterEntryStep::wait_user,CharacterEntryStep::init_guild,CharacterEntryStep::pvp_reset,CharacterEntryStep::run_mode,CharacterEntryStep::flush},"lifecycle order differs");
        require(scene.events.size()>5 && scene.events[0]=="M030839300000" && scene.events[1].starts_with("S0b05") && scene.events[2]=="S04010029230000" && scene.events[3]=="S0404d2040000", "original 0407 initialization/ACK order differs");
        auto loaded=pipeline.release();require(loaded.actor && loaded.actor->id()==character.id,"pipeline ownership lost");
        Scene other_scene;ActorLoadPipeline other(player,rules,{10000,0,0,nullptr},other_scene);state.user_id++;
        bool rejected=false;try{deliver_vertical_entry(state,other,player,social,catalogs,{0,100},"",services);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && other_scene.events.empty(),"foreign state caused side effects");
        state.user_id=player.core.user_id;auto copied_player=player;copied_player.core.money++;const auto event_count=scene.events.size();
        rejected=false;try{deliver_vertical_entry(state,other,copied_player,social,catalogs,{0,100},"",services);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && other_scene.events.empty() && scene.events.size()==event_count,"different snapshot with matching identity caused side effects");
        other.result().actor->account_id()++;rejected=false;
        try{deliver_vertical_entry(state,other,player,social,catalogs,{0,100},"",services);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && other_scene.events.empty() && scene.events.size()==event_count,"foreign actor account caused side effects");
        other.result().actor->account_id()=player.core.user_id;state.level++;rejected=false;
        try{deliver_vertical_entry(state,other,player,social,catalogs,{0,100},"",services);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && other_scene.events.empty() && scene.events.size()==event_count,"mismatched entry level caused side effects");
        {
            static_assert(!std::is_move_constructible_v<VerticalSessionCharacter>);
            InventoryReplicaPool replica_pool;Scene retained_scene;
            auto copy=player;auto replica=std::make_unique<CharacterReplica>(copy.core,replica_pool);
            VerticalSessionCharacter retained({std::move(copy),std::move(replica)},rules,{10000,0,0,nullptr},retained_scene);
            require(&retained.pipeline().snapshot()==&retained.snapshot() && retained.replica().core().user_id==player.core.user_id,"session lost its stable selected snapshot");
            CharacterEntryState retained_state;retained_state.user_id=player.core.user_id;retained_state.character_id=character.id;retained_state.level=character.level;
            auto retained_services=services;
            retained_services.delivery.send_self=[&](net::Packet p){retained_scene.record('S',p);};
            retained_services.send_manager=[&](net::Packet p){retained_scene.record('M',p);};
            retained_services.lifecycle=[&](CharacterEntryStep,Actor& actor){require(&actor==retained.pipeline().result().actor.get(),"session delivery used a different actor");};
            std::vector<CharacterEntryStep> owned_quests;retained_services.quest_entry=[&](CharacterEntryStep step,Actor& actor){require(&actor==retained.pipeline().result().actor.get(),"quest entry used another actor");owned_quests.push_back(step);};
            deliver_vertical_entry(retained_state,retained.pipeline(),retained.snapshot(),social,catalogs,{0,100},"",retained_services);
            require(owned_quests==std::vector<CharacterEntryStep>({CharacterEntryStep::quests,CharacterEntryStep::finished_quests}),"retained quest entry steps were not routed in order");
            auto retained_actor=retained.pipeline().release();
            require(retained_state.ready && retained_actor.actor && retained_actor.actor->id()==character.id && retained.replica().core().character.id==character.id,"session entry did not retain its actor and replica");
            auto wrong=player;wrong.core.user_id++;rejected=false;
            try{VerticalSessionCharacter bad({std::move(wrong),std::make_unique<CharacterReplica>(player.core,replica_pool)},rules,{10000,0,0,nullptr},retained_scene);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"session accepted a replica belonging to another account");
        }
        {
            // The DB agent and game actor allocate from different pools. A DB
            // failure changes resolved slots; a later game failure must affect
            // the sent game inventory without rewriting the DB replica.
            const auto definitions=ItemDefinitions::load(item_definitions());
            const ActorLoadRules load_rules{statuses,definitions,enchant,skills,quests,default_mobs};
            auto selected=player;selected.skills.clear();selected.effects.clear();selected.catalogs["PointLog"]={};selected.catalogs["Items"]={};selected.catalogs["Bank"]={};
            const auto row=[](unsigned uid,bool bank) {storage::Row data={"5001","5","1",std::to_string(uid),"1","0","100","0","0","0","0","0","0","","1","N","2024","1","1","0","0","0"};if(bank)data.erase(data.begin()+4);return data;};
            selected.catalogs["Items"].rows={row(1,false),row(2,false),row(3,false)};selected.catalogs["Bank"].rows={row(4,true),row(5,true)};
            unsigned db_attempt=0;InventoryReplicaPool db_pool([&]()->std::unique_ptr<CachedInventoryItem>{if(++db_attempt==1)return {};return std::make_unique<CachedInventoryItem>();});
            auto replica=vertical_persistence_restore(selected,0,db_pool);const auto db_items=replica->inventory().inventory();
            require(db_items.size()==2 && db_items[0].item.uid==2 && db_items[1].item.uid==3 && db_items[1].item.slot==1,"DB allocation did not resolve retained items");
            unsigned game_attempt=0;GroundItemPool game_pool(1,[&]()->std::unique_ptr<GroundItem>{if(++game_attempt==1)return {};return std::make_unique<GroundItem>();});
            Scene actual_scene;ActorLoadOptions options;options.inventory_pool=&game_pool;
            VerticalSessionCharacter retained({std::move(selected),std::move(replica)},load_rules,options,actual_scene);
            const auto& actor=*retained.pipeline().result().actor;require(actor.inventory().size()==1 && actor.inventory()[0].uid==3 && actor.inventory()[0].slot==1,"Game item allocation did not preserve resolved snapshot");
            std::vector<net::Packet> emitted;auto delivery_ports=services.delivery;delivery_ports.send_self=[&](net::Packet packet){emitted.push_back(std::move(packet));};
            EntryDelivery delivery(retained.snapshot(),retained.pipeline().result(),social,catalogs,{0,100},"",delivery_ports);
            require(delivery.send(CharacterEntryStep::items) && delivery.send(CharacterEntryStep::bank),"Inventory entry delivery did not run");
            auto expected=character_items(actor.inventory(),actor.status().skills.max_bags);auto bank=character_bank(retained.snapshot().bank,retained.snapshot().core.stored_money);expected.insert(expected.end(),bank.begin(),bank.end());
            require(!emitted.empty() && emitted==expected && retained.snapshot().bank.size()==2 && retained.replica().inventory().inventory().size()==2,"Entry packets or retained DB state lost their owning inventory");
        }
        {
            struct Admission final:WorldAdmissionServices {
                std::vector<std::string>& events;explicit Admission(std::vector<std::string>& e):events(e){}
                void packets(WorldAdmissionUser&) override{} RuntimeZone* select_zone(WorldAdmissionUser&) override{return nullptr;}
                void register_world(WorldAdmissionUser&) override{events.push_back("wait");} bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override{return false;}
                void close(WorldAdmissionUser&,std::uint32_t) override{events.push_back("close");} void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override{}
                std::optional<std::uint32_t> rank_window(RuntimeZone&) override{return {};} bool lock_guild(WorldAdmissionUser&) override{return false;} void unlock_guild(WorldAdmissionUser&) override{}
            };
            std::vector<std::string> sequence;Admission owner(sequence);WorldAdmission world(owner);WorldAdmissionUser admission;admission.user_id=player.core.user_id;admission.map=character.map;world.track_selection(admission);
            auto snapshot=player;InventoryReplicaPool pool;auto replica=std::make_unique<CharacterReplica>(snapshot.core,pool);Scene session_scene;
            VerticalSessionCharacter session({std::move(snapshot),std::move(replica)},rules,{10000,0,0,nullptr},session_scene);
            CharacterEntryState entry;entry.user_id=player.core.user_id;entry.character_id=character.id;entry.level=character.level;
            auto bound=services;bound.delivery.send_self=[&](net::Packet p){if(p.size()>=2 && p[0]==4 && p[1]==1)sequence.push_back("ack");};bound.send_manager=[](net::Packet){};
            bound.lifecycle=[&](CharacterEntryStep step,Actor& actor){require(&actor==session.pipeline().result().actor.get(),"admission changed actor");if(step==CharacterEntryStep::init_guild)sequence.push_back("guild");};
            require(deliver_vertical_entry_with_admission(entry,session,world,admission,social,catalogs,{0,100},"",bound),"entry did not enter world wait");
            require(entry.ready && admission.location==2 && world.waiting_count()==1 && world.selection_count()==0 && sequence==std::vector<std::string>{"ack","wait","guild"},"entry admission order differs");
            world.untrack(admission);
        }
        {
            auto selected=player;selected.catalogs["Friends"].columns.resize(7);selected.catalogs["Friends"].rows={{"71","Ally","1","2","3","hello","0"}};
            selected.catalogs["Blocked"].columns.resize(3);selected.catalogs["Blocked"].rows={{"72","Blocked","memo"}};
            VerticalSocialReplica db_social(selected.core.user_id);auto social_selection=vertical_load_social_selection(selected,db_social);
            require(social_selection.friends_loaded && social_selection.blocks_loaded && social_selection.catalog.friends().size()==1 && social_selection.catalog.blocked().size()==1,"retained social selection failed");
            Scene social_scene;ActorLoadPipeline social_pipeline(selected,rules,{10000,0,0,nullptr},social_scene);
            SocialCatalog peer(700,71);peer.add_friend({selected.core.character.id,0,0,0,"",""});std::vector<net::Packet> self_packets,peer_packets;
            auto social_ports=services.delivery;social_ports.send_self=[&](net::Packet p){self_packets.push_back(std::move(p));};social_ports.send_peer=[&](std::uint32_t id,net::Packet p){require(id==71,"wrong social peer");peer_packets.push_back(std::move(p));};
            social_ports.social=[&](std::uint32_t id)->const SocialCatalog*{return id==71?&peer:nullptr;};
            EntryDelivery delivery(selected,social_pipeline.result(),social_selection.catalog,catalogs,{0,100},"",social_ports);
            require(delivery.send(CharacterEntryStep::init_friends) && delivery.send(CharacterEntryStep::friends),"social delivery did not run");
            require(peer_packets.size()==1 && peer_packets[0][0]==7 && peer_packets[0][1]==0x22 && self_packets.size()==2 && self_packets[0][0]==1 && self_packets[0][1]==0x22 && self_packets[0][8]==1 && self_packets[1][0]==8 && self_packets[1][1]==0x22,"social selection did not reach online/friend/block packets");
            selected.catalogs["Friends"]={};selected.catalogs["Blocked"]={};auto after_failure=vertical_load_social_selection(selected,db_social);
            require(!after_failure.friends_loaded && !after_failure.blocks_loaded && after_failure.catalog.friends().size()==1 && after_failure.catalog.blocked().size()==1,"failed loaders erased retained social slots");
            auto account_social=std::make_shared<VerticalWorldAccountSocial>(selected.core.user_id);
            auto shared_mutex=account_social->shared_mutex();std::weak_ptr<VerticalWorldAccountSocial> weak_social=account_social;
            require(shared_mutex.get()==&account_social->mutex(),"social save mutex differs from account mutex");
            account_social->replica().friends()[0].id=71;
            VerticalWorldSessionSocial retained(account_social,selected);
            require(!retained.friends_loaded() && retained.catalog().friends().size()==1 && retained.account()==account_social,"session did not retain account social slots");
            auto wrong=selected;wrong.core.user_id++;
            bool rejected=false;try{VerticalWorldSessionSocial mismatch(account_social,wrong);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"social account mismatch accepted");
            struct SocialOwner {SocialCatalog value;explicit SocialOwner(SocialCatalog catalog):value(std::move(catalog)){} const SocialCatalog& catalog() const{return value;}};
            auto transferred=retained.take_catalog();bool unavailable=false;try{(void)retained.catalog();}catch(const std::logic_error&){unavailable=true;}
            require(unavailable,"entry observed a moved social catalog");
            auto wrong_owner=std::make_shared<SocialOwner>(SocialCatalog(selected.core.user_id,selected.core.character.id+1));
            rejected=false;try{retained.bind_catalog(wrong_owner);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"foreign social catalog accepted");
            auto owner=std::make_shared<SocialOwner>(std::move(transferred));std::weak_ptr<SocialOwner> weak_owner=owner;
            retained.bind_catalog(owner);owner->value.add_friend({73,0,0,0,"Later",""});owner.reset();
            require(!weak_owner.expired() && retained.catalog().friends().size()==2,"entry did not use retained live social catalog");
            rejected=false;try{(void)retained.take_catalog();}catch(const std::logic_error&){rejected=true;}require(rejected,"social catalog transferred twice");
            account_social.reset();require(!weak_social.expired(),"session or mutex failed to retain social account");
            auto separate=std::make_shared<VerticalWorldAccountSocial>(selected.core.user_id);std::weak_ptr<VerticalWorldAccountSocial> weak_separate=separate;
            auto lease=separate->shared_mutex();separate.reset();require(!weak_separate.expired(),"social mutex did not retain its account");
            lease.reset();require(weak_separate.expired(),"social mutex retained its account after release");
        }
        {
            const auto definitions=ItemDefinitions::load(item_definitions());
            const ActorLoadRules pool_rules{statuses,definitions,enchant,skills,quests,default_mobs};
            PlayerSnapshot pooled=player;PlayerItem first;first.bag=1;first.slot=0;first.type=5;first.type_id=1;first.count=1;
            auto second=first;second.slot=1;pooled.items={first,second};
            unsigned int attempts=0;GroundItemPool item_pool(1,[&]()->std::unique_ptr<GroundItem>{if(++attempts==1)return {};return std::make_unique<GroundItem>();});
            Scene pool_scene;
            {ActorLoadOptions options;options.inventory_pool=&item_pool;ActorLoadPipeline partial(pooled,pool_rules,options,pool_scene);
             require(attempts==2 && partial.result().actor && partial.result().actor->inventory().size()==1 && partial.result().actor->inventory()[0].slot==1,"allocation failure did not skip just its inventory row");}
            require(item_pool.statistics().available==1,"loaded item was not returned on actor teardown");
        }
        std::cout<<"vertical entry composition passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
