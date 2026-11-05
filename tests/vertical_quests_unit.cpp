#include "quest_start_items_actor_fixture.hpp"
#include "eternity/game/vertical_quests.hpp"
#include "eternity/game/vertical_quests_world_session.hpp"
#include "eternity/game/vertical_world_session_preparation.hpp"
#include "eternity/game/vertical_quests_sql_preparation_provider.hpp"
#include "eternity/game/vertical_world_session_packet_router.hpp"
#include "eternity/game/vertical_inventory.hpp"
#include "mob_damage_fixture.hpp"
#include "eternity/game/npc_patrol.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"
#include "eternity/game/quest_runtime_save.hpp"
#include "eternity/game/quest_save.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include "eternity/game/vertical_persistence.hpp"
#include "eternity/game/vertical_session_character.hpp"
#include "eternity/game/mob_drops_delivery.hpp"
#include <iostream>
#include <limits>
#include <future>
#include <thread>
#include <asio/thread_pool.hpp>
using namespace quest_actor_test;
namespace {
struct PoolFactory final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override {return std::make_unique<RestoredQuest>();}};
struct QuestAdmissionFixture final:WorldAdmissionServices {
    unsigned registered=0;
    [[noreturn]] static void unexpected() {throw std::logic_error("unexpected admission callback");}
    void packets(WorldAdmissionUser&) override {unexpected();}RuntimeZone* select_zone(WorldAdmissionUser&) override {unexpected();}
    void register_world(WorldAdmissionUser&) override {++registered;}bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override {unexpected();}
    void close(WorldAdmissionUser&,std::uint32_t) override {unexpected();}void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override {unexpected();}
    std::optional<std::uint32_t> rank_window(RuntimeZone&) override {unexpected();}bool lock_guild(WorldAdmissionUser&) override {unexpected();}void unlock_guild(WorldAdmissionUser&) override {unexpected();}
};
struct SuccessfulQuestSessionFixture final:VerticalWorldSessionServices {
    ActorLoadRules& rules;VerticalWorldSession::Strand strand;std::promise<void>& ready;std::shared_ptr<Scene> scene=std::make_shared<Scene>();
    std::function<void(VerticalWorldSession&)> on_prepare,on_disconnect;std::function<void(VerticalWorldSession&,std::function<void(std::exception_ptr)>)> async_prepare;std::unique_ptr<SocialCatalog> catalog;bool unbound=false;
    SuccessfulQuestSessionFixture(ActorLoadRules& r,VerticalWorldSession::Strand s,std::promise<void>& p):rules(r),strand(std::move(s)),ready(p) {}
    const ActorLoadRules& actor_rules() override {return rules;}ActorLoadOptions actor_options(const PlayerSnapshot&) override {ActorLoadOptions value;value.country=0;return value;}
    VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot&) override {return {scene,[](Actor&){},[this]{unbound=true;}};}
    void configure_admission(VerticalWorldSession&,WorldAdmissionUser&,const PlayerSnapshot&) override {}
    void prepare(VerticalWorldSession& s,std::function<void(std::exception_ptr)> done) override {if(async_prepare){async_prepare(s,std::move(done));return;}try {on_prepare(s);asio::post(strand,[done=std::move(done)]{done({});});} catch(...) {done(std::current_exception());}}
    void cancel_preparation(VerticalWorldSession&,std::function<void()> done) override {done();}
    CharacterEntryState entry_state(VerticalWorldSession& s) override {CharacterEntryState value;const auto& core=s.character()->snapshot().core;value.user_id=core.user_id;value.character_id=core.character.id;value.level=core.character.level;return value;}
    const SocialCatalog& social(VerticalWorldSession& s) override {if(!catalog)catalog=std::make_unique<SocialCatalog>(s.account_id(),s.world_user()->actor.id());return *catalog;}
    ExperienceBounds experience(VerticalWorldSession&) override {return {0,100};}std::string guild_name(VerticalWorldSession&) override {return {};}
    VerticalProtocolServices protocol(VerticalWorldSession& s) override {VerticalProtocolServices p;auto* actor=&s.world_user()->actor;p.delivery.send_self=[actor](net::Packet packet){actor->send_self(packet);};p.delivery.send_peer=[](std::uint32_t,net::Packet){};p.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};p.delivery.tick=[] {return 100U;};p.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};p.lifecycle=[](CharacterEntryStep,Actor&){};p.send_manager=[](net::Packet){};p.world_day=[] {return 1U;};p.system_time=[] {return 2U;};p.lock_guild=[] {return false;};p.unlock_guild=[] {};return p;}
    void waiting(VerticalWorldSession&) override {ready.set_value();}void receive(VerticalWorldSession&,const net::Packet&) override {throw std::logic_error("unexpected receive");}
    void receive_nprotect(VerticalWorldSession&,const net::Packet&) override {throw std::logic_error("unexpected nprotect");}void unavailable(VerticalWorldSession&) override {throw std::logic_error("unexpected unavailable");}
    void failure(VerticalWorldSession&,std::exception_ptr error) override {ready.set_exception(error);}void disconnect(VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done) override {if(on_disconnect)on_disconnect(owner);done();}
};
struct QuestWorldSessionFixture final:VerticalWorldSessionServices {
    ActorLoadRules& rules;std::shared_ptr<Scene> scene=std::make_shared<Scene>();std::function<void(VerticalWorldSession&)> on_prepare,on_disconnect;std::function<void(std::exception_ptr)> on_failure;std::function<void(std::function<void()>)> schedule;std::promise<void>& ready;bool unbound=false;unsigned cancelled_preparations=0;
    QuestWorldSessionFixture(ActorLoadRules& r,std::promise<void>& p):rules(r),ready(p) {}
    [[noreturn]] static void unexpected() {throw std::logic_error("unexpected session callback");}
    const ActorLoadRules& actor_rules() override {return rules;}ActorLoadOptions actor_options(const PlayerSnapshot&) override {ActorLoadOptions options;options.country=0;return options;}
    VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot&) override {return {scene,[](Actor&){},[this]{unbound=true;}};}
    void configure_admission(VerticalWorldSession&,WorldAdmissionUser&,const PlayerSnapshot&) override {}
    void prepare(VerticalWorldSession& s,std::function<void(std::exception_ptr)> done) override {try {on_prepare(s);schedule([this,done=std::move(done)]{try {done({});ready.set_value();} catch(...) {ready.set_exception(std::current_exception());}});} catch(...) {ready.set_exception(std::current_exception());}}
    void cancel_preparation(VerticalWorldSession&,std::function<void()> done) override {++cancelled_preparations;done();}
    CharacterEntryState entry_state(VerticalWorldSession&) override {unexpected();}const SocialCatalog& social(VerticalWorldSession&) override {unexpected();}
    ExperienceBounds experience(VerticalWorldSession&) override {unexpected();}std::string guild_name(VerticalWorldSession&) override {unexpected();}VerticalProtocolServices protocol(VerticalWorldSession&) override {unexpected();}
    void waiting(VerticalWorldSession&) override {unexpected();}void receive(VerticalWorldSession&,const net::Packet&) override {unexpected();}
    void receive_nprotect(VerticalWorldSession&,const net::Packet&) override {unexpected();}void unavailable(VerticalWorldSession&) override {unexpected();}
    void failure(VerticalWorldSession&,std::exception_ptr error) override {if(on_failure)on_failure(error);else ready.set_exception(error);}
    void disconnect(VerticalWorldSession& s,std::optional<std::uint32_t>,std::function<void()> done) override {if(on_disconnect) on_disconnect(s);done();}
};
struct PickupWorld final:ItemPickupWorld,ZoneItemServices {
    unsigned queries=0;
    [[noreturn]] static void unexpected() {throw std::logic_error("unexpected pickup service");}
    std::uint32_t pick_tick_ms() override {unexpected();}std::uint32_t pick_party_id(Actor&) override {return 0;}
    UserPosition pick_position(const Actor&) override {return {10,0,10};}
    bool pick_quest_active(Actor&,std::uint16_t) override {++queries;unexpected();}
    std::uint32_t lock_inventory_party(Actor&) override {unexpected();}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {unexpected();}void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}
    void send_inventory_database(const net::Packet&) override {unexpected();}std::string inventory_item_name(const ItemDefinition&) override {unexpected();}
    std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();}bool return_inventory_item(Actor&,GroundItem&) override {unexpected();}
    void send_money_database(const net::Packet&) override {unexpected();}GameLogSubject money_log_subject(const Actor&) override {unexpected();}
    std::chrono::sys_seconds money_local_calendar() override {unexpected();}void send_money_log(const net::Packet&) override {unexpected();}
    std::uint32_t tick_ms() override {return 100;}void send_cell_view(CellCoordinates,const net::Packet&) override {}void release(GroundItem&) override {unexpected();}
};
struct PatternPoolFactory final:QuestRuntimePoolServices {
    std::vector<bool> outcomes;std::size_t next=0;
    explicit PatternPoolFactory(std::vector<bool> value):outcomes(std::move(value)) {}
    std::unique_ptr<RestoredQuest> construct() override {
        if(next>=outcomes.size()) throw std::logic_error("unexpected quest allocation");
        return outcomes[next++]?std::make_unique<RestoredQuest>():nullptr;
    }
};
struct EntryTrace final:VerticalQuestEntryPorts {
    std::vector<char> events;std::vector<net::Packet> packets;bool active=false,finished=false,fail_full=false;
    std::uint32_t tick_ms() override {events.push_back('T');return 100;}
    void lock_active() override {check(!active && !finished,"overlapping entry locks");active=true;events.push_back('A');}
    void unlock_active() noexcept override {active=false;events.push_back('a');}
    void lock_finished() override {check(!active && !finished,"overlapping entry locks");finished=true;events.push_back('F');}
    void unlock_finished() noexcept override {finished=false;events.push_back('f');}
    void send_self(const net::Packet& packet) override {
        events.push_back(active || finished?'S':'s');
        if(fail_full && (active || finished)) throw std::runtime_error("entry transport failed");
        packets.push_back(packet);
    }
};
struct NpcServices final:ZoneNpcServices {
    struct Broadcast final:NpcBroadcast {
        unsigned& delivered;explicit Broadcast(unsigned& value):delivered(value) {}
        void send_cell(CellCoordinates) override {++delivered;}
    };
    bool fail_broadcast=false;unsigned deliveries=0;std::vector<net::Packet> broadcasts;
    Npc* allocate() override {throw std::logic_error("unexpected allocation");}
    std::uint32_t next_id() override {throw std::logic_error("unexpected id");}
    std::uint32_t tick_ms() const noexcept override {return 100;}
    void invalid_guard_position(const MapNpc&) override {throw std::logic_error("unexpected guard");}
    void send_cell_view(CellCoordinates,const net::Packet&) override {if(fail_broadcast) throw std::runtime_error("NPC broadcast failed");}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const net::Packet& packet,std::optional<std::uint8_t> priority) override {
        check(priority && *priority==6,"unexpected NPC broadcast priority");broadcasts.push_back(packet);return std::make_unique<Broadcast>(deliveries);
    }
};
struct PacketActions final:VerticalQuestPacketActions {
    Actor& actor;std::vector<unsigned> calls;std::function<void()> after_shop;
    explicit PacketActions(Actor& value):actor(value) {}
    void cancel_action_exchange(Actor& value) override {check(&value==&actor,"wrong packet actor");calls.push_back(1);}
    void end_shop(Actor& value) override {check(&value==&actor,"wrong packet actor");calls.push_back(2);if(after_shop) after_shop();}
    void close_connection(Actor& value,std::uint32_t reason) override {check(&value==&actor && reason==9,"wrong close");calls.push_back(3);}
};
struct NpcPacketActions final:VerticalNpcPacketActions {
    Actor& actor;unsigned closes=0;
    explicit NpcPacketActions(Actor& value):actor(value) {}
    void close_connection(Actor& value,std::uint32_t reason) override {check(&value==&actor && reason==9,"wrong NPC packet close");++closes;}
};
struct NpcMoveServices final:VerticalMoveInteractionServices {
    Actor& actor;unsigned pvp=0;bool fail_pvp=false;std::function<void()> on_pvp;
    explicit NpcMoveServices(Actor& value):actor(value) {}
    UserPosition position(const Actor& value) override {check(&value==&actor,"wrong moving actor");return {100,0,100};}
    std::shared_ptr<void> retain(Actor&) override {throw std::logic_error("unexpected exchange peer");}
    void refresh_guild_mark(Actor&) override {throw std::logic_error("unexpected guild mark");}
    void check_pvp(Actor& value) override {check(&value==&actor,"wrong PvP actor");++pvp;if(on_pvp) on_pvp();if(fail_pvp) throw std::runtime_error("PvP service failed");}
};
struct NpcLeaveServices final:ZoneLeaveInteractionServices,ZoneShopShutdownServices {
    Actor& actor;VerticalNpcInteraction& interaction;Npc& npc;std::int32_t expected_users;unsigned shutdowns=0;std::function<void()> on_shutdown;
    NpcLeaveServices(Actor& a,VerticalNpcInteraction& i,Npc& n,std::int32_t expected):actor(a),interaction(i),npc(n),expected_users(expected) {}
    Actor* find_in_zone(Actor&,std::uint32_t) override {throw std::logic_error("unexpected shop visitor lookup");}
    void shutdown_shop(Actor& value,ZoneLeaveShopState& state) override {
        check(&value==&actor && !interaction.npc && npc.users_using==expected_users,"NPC interaction was not cleared before shop shutdown");
        ++shutdowns;if(on_shutdown) on_shutdown();zone_shutdown_shop(value,state,*this);
    }
    std::shared_ptr<void> retain(Actor&) override {throw std::logic_error("unexpected peer retention");}
    std::uint32_t& exchange_timer(Actor&) override {throw std::logic_error("unexpected exchange timer");}
    Actor* find_customer(Actor&,std::uint32_t) override {throw std::logic_error("unexpected customer lookup");}
    ZoneLeaveShopState& shop_state(Actor&) override {throw std::logic_error("unexpected customer shop");}
    std::optional<CellCoordinates> cell(Actor&) override {throw std::logic_error("unexpected shop cell");}
    void send_view(CellCoordinates,const net::Packet&) override {throw std::logic_error("unexpected shop broadcast");}
};
struct Services final:VerticalQuestPorts,QuestRewardWorld,QuestRuntimeWorld,VerticalQuestRewardServices {
    Actor& actor;QuestRuntime& runtime;Scene& scene;QuestReplicaPool replica_pool;QuestReplica replica;InventoryReplicaPool inventory_pool;InventoryReplica inventory;MoneyReplica money;CharacterReplicaDelivery* character_delivery=nullptr;
    std::vector<unsigned> db;std::vector<unsigned> order;bool check_success_order=true,forbid_reward_remove=false,forbid_reward_world=false;QuestRewardWorld* reward_override=nullptr;VerticalQuestRewardItems* item_reward=nullptr;VerticalQuestRewardExperience* experience_reward=nullptr;std::vector<net::Packet> game_logs;std::function<void()> on_add,on_remove,on_progress,on_lock;std::function<void(const net::Packet&)> on_database;std::array<std::uint8_t,144> start_log{};unsigned locks=0,lock_calls=0;std::int32_t experience=0;
    Services(Actor& a,QuestRuntime& r,Scene& s):actor(a),runtime(r),scene(s),replica(a.account_id(),replica_pool),inventory(a.account_id(),0,inventory_pool),money(a.account_id(),a.money()) {}
    void send_database(Actor& a,const net::Packet& p) override {check(&a==&actor,"wrong actor");send_database(p);}
    void send_database(const net::Packet& p) override {auto op=word(p,0,2);db.push_back(op);order.push_back(op);if(character_delivery) check(character_delivery->apply(p)==InventoryReplicaResult::applied,"character replica rejected quest packet");if(check_success_order) check(scene.packets.size()==(op==0xa01?0U:op==0xa05?1U:3U),"database/client ordering differs");if(op==0x701 || op==0x702) check(inventory.apply(p)==InventoryReplicaResult::applied,"inventory replica rejected packet");
        else if(op==0x603) check(money.apply(p),"money replica rejected packet");else check(replica.apply(p)==InventoryReplicaResult::applied,"quest replica rejected packet");if(on_database) on_database(p);if(op==0xa01 && on_add) on_add();if(op==0xa02 && on_remove) on_remove();if(op==0xa05 && on_progress) on_progress();}
    void send_client(Actor& a,const net::Packet& p) override {check(&a==&actor,"wrong client actor");order.push_back(word(p,0,2));a.send_self(p);}
    std::uint32_t local_make_time(Actor&) override {return 1;}
    std::uint32_t tick_ms(Actor&) override {return 100;}
    std::uint32_t quest_tick_ms() override {return 100;}
    void lock_quests(Actor& a) override {check(&a==&actor,"wrong quest lock actor");++locks;++lock_calls;if(on_lock) on_lock();}
    void unlock_quests(Actor&) noexcept override {--locks;}
    void fill_game_log_main(Actor&,std::span<std::uint8_t> p) override {std::fill(p.begin(),p.end(),0);}
    void send_game_log(Actor&,const net::Packet& p) override {order.push_back(word(p,0,2));game_logs.push_back(p);}
    QuestRewardWorld& reward_world(Actor& a) override {check(&a==&actor && !forbid_reward_world,"wrong or unexpected upstream reward owner");return reward_override?*reward_override:*this;}
    QuestStartLogContext start_log_context(Actor&) override {return {};}
    void send_start_log(Actor&,const std::array<std::uint8_t,144>& p) override {start_log=p;order.push_back(0x501);}
    void add_experience(std::uint32_t source,std::int32_t n,bool bypass) override {if(experience_reward) experience_reward->award(source,n,bypass);else experience+=n;order.push_back(0xffff);}
    void fill_game_log_main(std::span<std::uint8_t> p) override {fill_game_log_main(actor,p);}
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t t,std::uint8_t id,std::uint8_t count) override {if(item_reward) return item_reward->receive(t,id,count);check(t==0,"unexpected reward item");return {};}
    void send_self(const net::Packet& p) override {order.push_back(word(p,0,2));actor.send_self(p);}
    void remove_quest(RestoredQuest& q,bool success) override {check(!forbid_reward_remove,"underlying reward removal was called");runtime.remove(q,success,*this);}
    void send_game_log(const net::Packet& p) override {send_game_log(actor,p);}
};
struct DeathServices final:MobDeathWorld,VerticalQuestMobUsers {
    VerticalQuests& quests;MobDeathUser user;std::vector<char> events;VerticalQuestSessionMobCredit* credit=nullptr;
    DeathServices(VerticalQuests& q,std::uint32_t id):quests(q),user{id,20,0} {}
    VerticalQuests& quests_for(MobDeathUser&) override {return quests;}
    void send_death(MobRuntime&,const net::Packet&,std::uint8_t) override {throw std::logic_error("unexpected death broadcast");}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t id) override {check(id==user.id,"wrong death credit");return &user;}
    MobDeathParty* party(MobDeathUser&) override {return nullptr;}
    void lock_party(MobDeathParty&) override {throw std::logic_error("unexpected party lock");}
    void unlock_party(MobDeathParty&) noexcept override {std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t) override {throw std::logic_error("unexpected party range");}
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t) override {events.push_back('E');}
    void drop_items(MobRuntime&,MobDeathUser* credited,MobDeathParty* party) override {check(credited==&user && !party,"wrong death drop recipients");events.push_back('D');}
    void quest_kill(MobDeathUser& value,MobRuntime& mob) override {if(!credit) throw std::logic_error("underlying quest callback was used");credit->apply(value,mob);}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected obelisk");}
    void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t) override {throw std::logic_error("unexpected obelisk");}
    void destroy_linked_group(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected group");}
    void remove_obelisk_member(MobRuntime&,void*) override {throw std::logic_error("unexpected obelisk member");}
    void remove_linked_member(MobRuntime&,void*) override {throw std::logic_error("unexpected linked member");}
    void log_death(MobRuntime&,MobDeathUser*,bool) override {throw std::logic_error("unexpected death log");}
    void clear_applied_skills(MobRuntime&) override {events.push_back('C');}
};
struct PartyDeathServices final:MobDeathWorld,VerticalQuestMobUsers {
    std::array<MobDeathUser,3> users{{{9011,20,0},{9012,20,0},{9013,20,0}}};
    std::array<VerticalQuests*,3> quests{};MobDeathParty group;bool locked=false,wrong_owner=false;VerticalQuestSessionMobCredit* credit=nullptr;
    std::vector<char> events;
    PartyDeathServices() {group.type=0;group.maximum_level=20;for(auto& user:users) group.members.push_back(&user);}
    VerticalQuests& quests_for(MobDeathUser& user) override {
        check(locked,"quest lookup outside party lock");
        for(std::size_t i=0;i<users.size();++i) if(&user==&users[i]) return *quests[wrong_owner && i==1?0:i];
        throw std::logic_error("unknown party quest owner");
    }
    void send_death(MobRuntime&,const net::Packet&,std::uint8_t) override {throw std::logic_error("unexpected death broadcast");}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t id) override {check(id==users[0].id,"wrong party credit");return &users[0];}
    MobDeathParty* party(MobDeathUser&) override {return &group;}
    void lock_party(MobDeathParty& value) override {check(&value==&group && !locked,"wrong party lock");locked=true;events.push_back('[');}
    void unlock_party(MobDeathParty&) noexcept override {locked=false;events.push_back(']');}
    bool in_reward_range(MobDeathUser& credit,MobDeathUser& member,std::int32_t radius) override {
        check(locked && &credit==&users[0] && radius==100,"wrong party range context");events.push_back('R');return &member!=&users[2];
    }
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t) override {check(locked,"experience outside party lock");events.push_back('E');}
    void drop_items(MobRuntime&,MobDeathUser* credited,MobDeathParty* value) override {check(locked && credited==&users[0] && value==&group,"wrong party drop context");events.push_back('D');}
    void quest_kill(MobDeathUser& user,MobRuntime& mob) override {if(!credit) throw std::logic_error("underlying quest callback was used");credit->apply(user,mob);}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected obelisk");}
    void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t) override {throw std::logic_error("unexpected obelisk");}
    void destroy_linked_group(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected group");}
    void remove_obelisk_member(MobRuntime&,void*) override {throw std::logic_error("unexpected obelisk member");}
    void remove_linked_member(MobRuntime&,void*) override {throw std::logic_error("unexpected linked member");}
    void log_death(MobRuntime&,MobDeathUser*,bool) override {throw std::logic_error("unexpected death log");}
    void clear_applied_skills(MobRuntime&) override {events.push_back('C');}
};
struct ZoneQuestUsers final:VerticalQuestZoneUsers {
    std::array<std::shared_ptr<Actor>,3>& actors;std::array<std::shared_ptr<VerticalQuests>,3>& quests;
    std::vector<std::uint32_t> visits;bool wrong=false;
    ZoneQuestUsers(std::array<std::shared_ptr<Actor>,3>& a,std::array<std::shared_ptr<VerticalQuests>,3>& q):actors(a),quests(q) {}
    std::shared_ptr<VerticalQuests> retain_quests(Actor& actor) override {
        visits.push_back(actor.id());
        for(std::size_t i=0;i<actors.size();++i) if(actors[i].get()==&actor) return quests[wrong && i==0?1:i];
        throw std::logic_error("unknown zone quest owner");
    }
};
// Encode a single record in CQuestData::Load order, with one NPC-gated mob quest.
net::Packet quest_data(bool timed=false,bool selection=false,std::uint8_t kind=2,bool reward_item=false,std::uint8_t reward_money=0,bool repeat=false) {
    std::array<std::uint8_t,464> q{};q[0]=1;q[0x104]=100;q[0x106]=2;q[0x10a]=1;
    if(timed) q[0x128]=1;
    if(repeat) q[0x134]=1;
    if(selection) q[0x15d]=2;
    if(reward_item) {q[0x17c]=5;q[0x17d]=1;}
    q[0x138]=1;q[0x139]=2;q[0x13a]=9;q[0x147]=kind;q[0x148]=2;q[0x14a]=9;
    q[0x156]=3;q[0x15a]=1;q[0x174]=10;q[0x178]=reward_money;
    if(kind==1) {q[0x14c]=5;q[0x14d]=1;q[0x14e]=2;}
    if(kind==3) q[0x155]=2;
    net::Packet p;append(p,1);auto copy=[&](std::size_t at,std::size_t n){p.insert(p.end(),q.begin()+static_cast<std::ptrdiff_t>(at),q.begin()+static_cast<std::ptrdiff_t>(at+n));};
    copy(0,2);append(p,0);append(p,0);copy(0x102,2);copy(0x104,2);copy(0x106,10);copy(0x110,4);copy(0x114,3);copy(0x118,2);copy(0x11a,7);copy(0x124,20);copy(0x138,17);copy(0x14a,12);copy(0x156,2);copy(0x15a,1);copy(0x158,2);copy(0x15b,3);
    for(std::size_t at=0x160;at<0x1c0;at+=32) {copy(at,5);copy(at+8,9);copy(at+20,12);}for(int i=0;i<7;++i) append(p,0);return p;
}
ItemQuestLinks linked_item() {
    std::vector<std::uint8_t> bytes(65536*8,0);const auto offset=(5*256+1)*8;
    bytes[offset+4]=1;bytes.insert(bytes.begin()+static_cast<std::ptrdiff_t>(offset+8),{1,0});
    return ItemQuestLinks::decode(bytes);
}
}
int main() {try {
    auto defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data()));auto items=catalog(5,{1});auto item_pool=std::make_shared<GroundItemPool>();Clock clock;auto ids=std::make_shared<ItemIds>(3,clock);Scene scene;EnchantValues enchant;MobAttackTemplates mobs;
    auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});
    VerticalWorldSession::Registry empty_world_sessions;VerticalWorldQuestSessions empty_quest_sessions(empty_world_sessions);VerticalWorldPrimaryQuestServices empty_primary_quests(empty_world_sessions);
    bool missing_death_identity=false;try {VerticalWorldQuestMobActors invalid(empty_world_sessions,{});} catch(const std::invalid_argument&) {missing_death_identity=true;}
    check(missing_death_identity,"mob credit accepted missing death identity service");
    bool missing_loot_identity=false;try {VerticalWorldQuestLootActors invalid(empty_world_sessions,{});} catch(const std::invalid_argument&) {missing_loot_identity=true;}
    check(missing_loot_identity,"quest loot accepted missing identity service");
    check(!empty_quest_sessions.retain(*actor) && !empty_primary_quests.retain(*actor),"unknown actor resolved a world quest session");
    bool unknown_primary=false;try {(void)empty_primary_quests.quests(*actor);} catch(const std::logic_error&) {unknown_primary=true;}
    check(unknown_primary,"primary quest phase accepted an unregistered actor");
    actor->account_id()=77;actor->status().level=20;actor->appearance().identity.job=0;
    PoolFactory factory;auto pool=std::make_shared<QuestRuntimePool>(factory);auto runtime=std::make_shared<QuestRuntime>(77,*pool);Services services(*actor,*runtime,scene);
    {
        InventoryReplicaPool failed_items([]{return std::unique_ptr<CachedInventoryItem>{};});QuestReplicaPool failed_quests([]{return std::unique_ptr<CachedQuest>{};});
        PlayerCore core;core.user_id=actor->account_id();core.character.id=actor->id();CharacterReplica replica(core,failed_items,nullptr,&failed_quests);
        auto mutex=std::make_shared<std::recursive_mutex>();VerticalWorldQuestReplicaPorts checked_ports(*actor,replica,mutex,services);
        actor->money()=42;bool wrong_money=false;
        try {checked_ports.send_database(*actor,money_database_packet(77,41));} catch(const std::runtime_error&) {wrong_money=true;}
        check(wrong_money && replica.core().money==0,"quest replica accepted a different actor balance");
        checked_ports.send_database(*actor,money_database_packet(77,42));check(replica.core().money==42,"quest money did not reach replica");actor->money()=0;
        const net::Packet add_quest{1,10,77,0,0,0,1,0,0,0};bool quest_failed=false;
        try {checked_ports.send_database(*actor,add_quest);} catch(const std::runtime_error&) {quest_failed=true;}
        check(quest_failed && replica.quests().active().empty(),"quest replica allocation failure was reported as success");
        bool missing_progress=false,missing_remove=false;
        try {checked_ports.send_database(*actor,net::Packet{5,10,77,0,0,0,1,0,0,1});} catch(const std::runtime_error&) {missing_progress=true;}
        try {checked_ports.send_database(*actor,net::Packet{2,10,77,0,0,0,1,0,2});} catch(const std::runtime_error&) {missing_remove=true;}
        check(missing_progress && missing_remove && replica.quests().active().empty(),"missing quest progress or removal was reported as success");
        GroundItem item;item.definition=items->find(5,1);item.item.uid=111;item.item.id=item.definition->id;item.item.type=5;item.item.type_id=1;item.item.count=1;
        const auto add_item=inventory_add_packet(actor->account_id(),1,0,item);bool item_failed=false;
        bool missing_actor_item=false;try {checked_ports.send_database(*actor,add_item);} catch(const std::runtime_error&) {missing_actor_item=true;}
        check(missing_actor_item && replica.inventory().inventory().empty(),"quest item insertion accepted a missing Actor item");
        auto failed_item_actor=std::make_shared<Actor>(actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});failed_item_actor->account_id()=77;
        auto failed_item=item.item;failed_item.bag=1;failed_item.slot=0;check(failed_item_actor->load_inventory_item(failed_item,*item_pool),"failed item actor fixture failed");
        VerticalWorldQuestReplicaPorts failed_item_ports(*failed_item_actor,replica,mutex,services);
        auto retained_failed_item=failed_item_actor->retained_inventory_item(1,0);check(retained_failed_item.has_value(),"failed item actor lease missing");retained_failed_item->item->item.uid=112;
        bool wrong_insert=false;try {failed_item_ports.send_database(*failed_item_actor,add_item);} catch(const std::runtime_error&) {wrong_insert=true;}
        retained_failed_item->item->item.uid=111;check(wrong_insert && failed_items.statistics().attempts==0,"quest item insertion accepted a different Actor item");
        try {failed_item_ports.send_database(*failed_item_actor,add_item);} catch(const std::runtime_error&) {item_failed=true;}
        check(item_failed && replica.inventory().inventory().empty(),"quest reward replica allocation failure was reported as success");
        InventoryReplicaPool occupied_pool;CharacterReplica occupied(core,occupied_pool);PlayerItem existing;existing.uid=222;existing.type=5;existing.type_id=1;existing.bag=1;existing.slot=0;existing.count=1;
        auto inventory_actor=std::make_shared<Actor>(actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*items,enchant,mobs});inventory_actor->account_id()=77;
        check(inventory_actor->load_inventory_item(existing,*item_pool),"occupied actor fixture failed");
        check(occupied.inventory().load(existing),"occupied replica fixture failed");VerticalWorldQuestReplicaPorts occupied_ports(*inventory_actor,occupied,mutex,services);bool collision=false;
        try {occupied_ports.send_database(*inventory_actor,add_item);} catch(const std::runtime_error&) {collision=true;}
        const auto rows=occupied.inventory().inventory();check(collision && rows.size()==1 && rows.front().item.uid==222 && rows.front().item.count==1,"quest reward overwrote an occupied replica slot");
        bool missing_count=false,missing_item=false;
        try {occupied_ports.send_database(*inventory_actor,net::Packet{7,7,77,0,0,0,1,1,3});} catch(const std::runtime_error&) {missing_count=true;}
        try {occupied_ports.send_database(*inventory_actor,net::Packet{2,7,77,0,0,0,1,1,1});} catch(const std::runtime_error&) {missing_item=true;}
        check(missing_count && missing_item && occupied.inventory().inventory().front().item.count==1,"missing quest inventory update was accepted");
        auto actor_item=inventory_actor->retained_inventory_item(1,0);check(actor_item.has_value(),"actor inventory identity fixture failed");actor_item->item->item.uid=333;
        bool wrong_uid=false;try {occupied_ports.send_database(*inventory_actor,net::Packet{2,7,77,0,0,0,1,0,1});} catch(const std::runtime_error&) {wrong_uid=true;}
        actor_item->item->item.uid=222;check(wrong_uid && occupied.inventory().inventory().front().item.uid==222,"quest replica accepted a different Actor item");
        bool wrong_count=false;try {occupied_ports.send_database(*inventory_actor,net::Packet{7,7,77,0,0,0,1,0,3});} catch(const std::runtime_error&) {wrong_count=true;}
        check(wrong_count && occupied.inventory().inventory().front().item.count==1,"quest replica accepted actor count divergence");
        check(inventory_actor->set_inventory_item_count(1,0,3),"actor count fixture failed");occupied_ports.send_database(*inventory_actor,net::Packet{7,7,77,0,0,0,1,0,3});
        check(inventory_actor->set_inventory_item_count(1,0,1),"actor partial count fixture failed");occupied_ports.send_database(*inventory_actor,net::Packet{2,7,77,0,0,0,1,0,2});
        check(occupied.inventory().inventory().front().item.count==1,"quest inventory partial removal differs");
        occupied_ports.send_database(*inventory_actor,net::Packet{2,7,77,0,0,0,1,0,1});
        check(occupied.inventory().inventory().empty(),"quest inventory full removal differs");
        Scene consumption_scene;auto consumption_actor=std::make_shared<Actor>(actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},consumption_scene,ActorEquipmentRules{*items,enchant,mobs});
        consumption_actor->account_id()=77;consumption_actor->status().skills.max_bags=1;
        PlayerItem consumption_item=existing;consumption_item.uid=444;consumption_item.count=3;
        check(consumption_actor->load_inventory_item(consumption_item,*item_pool),"quest consumption Actor fixture failed");
        InventoryReplicaPool consumption_pool;CharacterReplica consumption_replica(core,consumption_pool);
        check(consumption_replica.inventory().load(consumption_item),"quest consumption replica fixture failed");
        VerticalWorldQuestReplicaPorts consumption_ports(*consumption_actor,consumption_replica,mutex,services);
        QuestRequestsActorInventory consumption(*consumption_actor,consumption_ports);
        check(consumption.find_item(5,1,true),"quest single-item consumption failed");
        check(consumption_actor->retained_inventory_item(1,0)->item->item.count==2 && consumption_replica.inventory().inventory().front().item.count==2,"quest partial consumption diverged");
        auto consumption_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,1)));RestoredQuest consumption_quest;consumption_quest.definition=consumption_defs->find(1);
        check(consumption.find_items(consumption_quest,true),"quest full-stack consumption failed");
        check(!consumption_actor->retained_inventory_item(1,0) && consumption_replica.inventory().inventory().empty(),"quest full-stack consumption diverged");
        InventoryReplicaPool bulk_items;QuestReplicaPool bulk_quests;CharacterReplica bulk_replica(core,bulk_items,nullptr,&bulk_quests);
        check(bulk_replica.quests().load({1,0,{0,0,0}}) && bulk_replica.quests().load({2,0,{0,0,0}}) && bulk_replica.quests().load({1,0,{0,0,0}}),"bulk quest fixture failed");
        VerticalWorldQuestReplicaPorts bulk_ports(*actor,bulk_replica,mutex,services);
        const net::Packet bulk_save{4,10,77,0,0,0,2,1,0,7,0,3,4,5,1,0,8,0,6,7,8};
        bulk_ports.send_database(*actor,bulk_save);const auto saved=bulk_replica.quests().active();
        check(saved[0].delay==7 && saved[0].counters[0]==3 && saved[1].delay==0 && saved[2].delay==8 && saved[2].counters[2]==8,"ordered duplicate bulk quest update differs");
        const net::Packet missing_bulk{4,10,77,0,0,0,2,2,0,9,0,1,1,1,2,0,10,0,2,2,2};bool bulk_failed=false;
        try {bulk_ports.send_database(*actor,missing_bulk);} catch(const std::runtime_error&) {bulk_failed=true;}
        const auto unchanged=bulk_replica.quests().active();
        check(bulk_failed && unchanged[1].delay==0 && unchanged[1].counters[0]==0,"incomplete bulk quest update partially mutated replica");
    }
    NpcServices npc_services;NpcDefinition npc_definition,replacement_definition;NpcPool npc_pool;ZoneNpcs zone(MapGeometry(81),npc_services);
    npc_definition.record[0]=2;npc_definition.record[20]='A';replacement_definition.record[20]='B';
    auto* npc=npc_pool.allocate();npc->id=99;npc->type=2;npc->type_id=9;npc->definition=&npc_definition;check(zone.enter(*npc),"NPC entry failed");
    Npc* replacement=nullptr;
    std::uint8_t last=0;std::uint32_t deadline=0;
    VerticalQuests quests({items,item_pool,ids,defs,pool,actor,runtime},{last,deadline},zone,npc_pool,services);
    auto wrong_runtime=std::make_shared<QuestRuntime>(78,*pool);bool bad_account=false;
    try {VerticalQuests bad({items,item_pool,ids,defs,pool,actor,wrong_runtime},{last,deadline},zone,npc_pool,services);} catch(const std::invalid_argument&) {bad_account=true;}
    auto other_pool=std::make_shared<QuestRuntimePool>(factory);bool bad_pool=false;
    try {VerticalQuests bad({items,item_pool,ids,defs,other_pool,actor,runtime},{last,deadline},zone,npc_pool,services);} catch(const std::invalid_argument&) {bad_pool=true;}
    check(bad_account && bad_pool && runtime->active().empty() && services.db.empty(),"mismatched quest owners were accepted or changed state");
    actor->account_id()=78;PacketActions mismatch_actions(*actor);bool changed_account=false,changed_dispatch=false;
    try {(void)quests.start(1,99);} catch(const std::logic_error&) {changed_account=true;}
    try {(void)quests.receive(net::Packet{2,9,99,0,0,0,1,0},mismatch_actions);} catch(const std::logic_error&) {changed_dispatch=true;}
    actor->account_id()=77;
    check(changed_account && changed_dispatch && mismatch_actions.calls.empty() && runtime->active().empty() && services.db.empty(),"changed account was not rejected before quest side effects");
    services.on_add=[&] {
        check(zone.leave(*npc),"nested NPC leave failed");npc_pool.release(*npc);
        replacement=npc_pool.allocate();check(replacement!=npc,"leased NPC was recycled during callback");
        replacement->id=99;replacement->type=2;replacement->type_id=9;replacement->definition=&replacement_definition;
        check(zone.enter(*replacement),"replacement NPC entry failed");
        check(!quests.start(1,99),"nested duplicate start accepted");
    };
    check(!quests.start(1,98),"missing NPC accepted");check(quests.start(1,99),"quest not accepted");check(runtime->active().size()==1 && runtime->mob_kill_quests()==1,"accept did not use actual runtime");
    check(services.start_log[112]=='A',"start log used replacement NPC");
    auto* recycled=npc_pool.allocate();check(recycled==npc,"retired NPC not released after lease");npc_pool.release(*recycled);
    PacketActions actions(*actor);
    const net::Packet end_packet{3,9,99,0,0,0,1,0};
    check(quests.receive(end_packet,actions)==VerticalQuestPacketResult::handled,"end packet not handled");
    check(runtime->active().size()==1,"unfinished quest completed");
    MobQuestLinks links({{}, {}, {}, {1}});actor->account_id()=78;bool progress_rejected=false;
    try {quests.kill_mob(3,links);} catch(const std::logic_error&) {progress_rejected=true;}
    actor->account_id()=77;
    check(progress_rejected && runtime->active().front()->packet.counters[0]==0 && services.replica.active().front().counters[0]==0,"account change advanced a live quest");
    MobDamageFixture damage;MobRuntime mob;MobRuntimeTemplate mob_definition;
    mob_definition.id=88;mob_definition.level=20;mob.type=3;mob.definition=&mob_definition;
    mob.damage=std::make_unique<MobDamageLedger>(damage.pool);mob.damage->add(1,actor->id(),10,0,0);
    DeathServices death(quests,actor->id());VerticalQuestMobDeath bridged(death,death,links);
    services.on_progress=[&]{death.events.push_back('P');};
    mob_death(mob,1,actor->id(),MobDeathExperience{},bridged);services.on_progress={};
    check(death.events==std::vector<char>({'E','D','P','C'}),"mob death reward/drop/quest order differs");
    check(runtime->active().front()->packet.counters[0]==1,"actual mob death did not update quest counter");check(runtime->active().size()==1,"NPC-gated quest completed early");
    MobDeathUser wrong{actor->id()+1,20,0};bool wrong_rejected=false;
    try {bridged.quest_kill(wrong,mob);} catch(const std::logic_error&) {wrong_rejected=true;}
    check(wrong_rejected,"mismatched quest owner accepted");
    quests.end(1,98);check(runtime->active().size()==1,"wrong NPC completed quest");
    check(quests.receive(end_packet,actions)==VerticalQuestPacketResult::handled,"completion packet not handled");
    check(runtime->active().empty() && runtime->finished().size()==1 && runtime->mob_kill_quests()==0,"completion did not remove actual quest");check(services.experience==20,"experience callback mismatch");check(services.replica.active().empty(),"DB replica retains completed quest");
    check(scene.packets.size()==3 && word(scene.packets[0],0,2)==0x902 && word(scene.packets[1],0,2)==0x905 && word(scene.packets[2],0,2)==0x903,"client packet order differs");
    check(services.db==std::vector<unsigned>({0xa01,0xa05,0xa02}),"database ordering differs");check(services.locks==0,"quest lock leaked");
    // Three independent live users share the original mob-death party pass.
    std::array<Scene,3> party_scenes;
    std::array<std::shared_ptr<Actor>,3> party_actors;
    std::array<std::shared_ptr<QuestRuntime>,3> party_runtimes;
    std::array<std::unique_ptr<Services>,3> party_ports;
    std::array<std::unique_ptr<VerticalQuests>,3> party_quests;
    PartyDeathServices party_death;
    for(std::size_t i=0;i<party_actors.size();++i) {
        party_actors[i]=std::make_shared<Actor>(party_death.users[i].id,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},party_scenes[i],ActorEquipmentRules{*items,enchant,mobs});
        party_actors[i]->account_id()=static_cast<std::uint32_t>(80+i);party_actors[i]->status().level=20;party_actors[i]->appearance().identity.job=0;
        party_runtimes[i]=std::make_shared<QuestRuntime>(party_actors[i]->account_id(),*pool);
        party_ports[i]=std::make_unique<Services>(*party_actors[i],*party_runtimes[i],party_scenes[i]);party_ports[i]->check_success_order=false;
        party_quests[i]=std::make_unique<VerticalQuests>(QuestStartItemsActorWorldOwners{items,item_pool,ids,defs,pool,party_actors[i],party_runtimes[i]},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,*party_ports[i]);
        party_death.quests[i]=party_quests[i].get();check(party_quests[i]->start(1,99),"party quest start failed");
        party_ports[i]->on_progress=[&party_death]{party_death.events.push_back('P');};
    }
    MobRuntime party_mob;party_mob.type=3;party_mob.definition=&mob_definition;party_mob.damage=std::make_unique<MobDamageLedger>(damage.pool);
    party_mob.damage->add(1,party_death.users[0].id,10,0,0);
    VerticalQuestMobDeath party_bridge(party_death,party_death,links);
    mob_death(party_mob,1,party_death.users[0].id,MobDeathExperience{},party_bridge);
    check(party_death.events==std::vector<char>({'[','R','E','P','R','E','P','R','D',']','C'}),"party mob quest event order differs");
    check(!party_death.locked,"party lock leaked");
    for(std::size_t i=0;i<party_runtimes.size();++i) {
        const auto expected=static_cast<std::uint8_t>(i<2?1:0);
        check(party_runtimes[i]->active().front()->packet.counters[0]==expected,"party quest credited wrong recipient");
        check(party_ports[i]->replica.active().front().counters[0]==expected,"party quest replica differs");
        check(party_scenes[i].packets.size()==(i<2?2U:1U),"party quest client packet count differs");
    }
    party_death.events.clear();party_death.wrong_owner=true;party_mob.damage->add(1,party_death.users[0].id,10,0,0);
    bool party_rejected=false;
    try {mob_death(party_mob,1,party_death.users[0].id,MobDeathExperience{},party_bridge);} catch(const std::logic_error&) {party_rejected=true;}
    check(party_rejected && !party_death.locked && party_death.events==std::vector<char>({'[','R','E','P','R','E',']'}),"failed party quest dispatch did not unlock");
    check(party_runtimes[0]->active().front()->packet.counters[0]==2 && party_runtimes[1]->active().front()->packet.counters[0]==1 && party_runtimes[2]->active().front()->packet.counters[0]==0,"failed party dispatch changed wrong quest");
    // Exercise UpdateQuest through the same real owners and failure completion path.
    auto timed_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(true)));
    auto timed_runtime=std::make_shared<QuestRuntime>(77,*pool);Services timed_services(*actor,*timed_runtime,scene);
    timed_services.check_success_order=false;
    VerticalQuests timed_quests({items,item_pool,ids,timed_defs,pool,actor,timed_runtime},{last,deadline},zone,npc_pool,timed_services);
    const auto packets_before=scene.packets.size();
    actions.calls.clear();
    const net::Packet start_packet{4,9,99,0,0,0,1,0};
    check(timed_quests.receive(start_packet,actions)==VerticalQuestPacketResult::handled,"timed quest packet not handled");
    check(actions.calls==std::vector<unsigned>({1,2}),"quest packet prefix order differs");
    check(timed_runtime->active().size()==1,"timed quest missing from runtime");
    const auto end_tick=timed_runtime->active().front()->packet.end_tick;
    check(end_tick==60100,"timed quest deadline differs");
    timed_services.on_remove=[&] {
        check(scene.packets.size()==packets_before+2 && word(scene.packets.back(),0,2)==0x903,"failure client packet did not precede database removal");
    };
    timed_quests.update_timers(end_tick-1);
    check(timed_runtime->active().size()==1,"timed quest expired before deadline");
    timed_quests.update_timers(end_tick);
    check(timed_runtime->active().empty() && timed_runtime->finished().size()==1,"timed failure retained quest");
    check(timed_runtime->mob_kill_quests()==0 && timed_services.replica.active().empty(),"timed failure did not update owners");
    check(timed_services.db==std::vector<unsigned>({0xa01,0xa02}),"timed database order differs");
    check(scene.packets.size()==packets_before+2 && word(scene.packets.back(),0,2)==0x903,"timed failure client packet missing");
    check(timed_services.order==std::vector<unsigned>({0xa01,0x902,0x501,0xa02,0x502}),"timed failure event order differs");
    check(timed_services.locks==0,"timed quest lock leaked");
    auto select_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,true)));
    auto select_runtime=std::make_shared<QuestRuntime>(77,*pool);Services select_services(*actor,*select_runtime,scene);
    select_services.check_success_order=false;
    VerticalQuests select_quests({items,item_pool,ids,select_defs,pool,actor,select_runtime},{last,deadline},zone,npc_pool,select_services);
    check(select_quests.receive(net::Packet{2,9,99,0,0,0,1,0},actions)==VerticalQuestPacketResult::handled && select_runtime->active().size()==1,"selection quest start failed");
    select_quests.kill_mob(3,links);
    check(select_quests.receive(end_packet,actions)==VerticalQuestPacketResult::handled && select_runtime->active().size()==1 && select_runtime->active().front()->success==1,"reward selection was not offered");
    check(word(scene.packets.back(),0,2)==0x907,"reward selection offer missing");
    check(select_quests.receive(net::Packet{7,9,1,0,2},actions)==VerticalQuestPacketResult::handled && select_runtime->active().size()==1,"invalid selection completed quest");
    check(select_quests.receive(net::Packet{7,9,1,0,0},actions)==VerticalQuestPacketResult::handled,"reward selection packet not handled");
    check(select_runtime->active().empty() && select_runtime->finished().size()==1 && select_services.experience==20,"selected reward did not complete actual quest");
    check(select_services.db==std::vector<unsigned>({0xa01,0xa05,0xa02}) && select_services.replica.active().empty(),"selection database result differs");
    auto abandon_runtime=std::make_shared<QuestRuntime>(77,*pool);Services abandon_services(*actor,*abandon_runtime,scene);
    abandon_services.check_success_order=false;
    VerticalQuests abandon_quests({items,item_pool,ids,defs,pool,actor,abandon_runtime},{last,deadline},zone,npc_pool,abandon_services);
    const net::Packet initial{2,9,99,0,0,0,1,0};
    check(abandon_quests.receive(initial,actions)==VerticalQuestPacketResult::handled && abandon_runtime->active().size()==1,"packet start failed");
    const net::Packet cancel{8,9,1,0};
    check(abandon_quests.receive(cancel,actions)==VerticalQuestPacketResult::handled,"abandon packet not handled");
    check(abandon_runtime->active().empty() && abandon_runtime->finished().size()==1 && abandon_services.replica.active().empty(),"abandon did not remove real quest");
    check(abandon_services.db==std::vector<unsigned>({0xa01,0xa02}),"abandon database order differs");
    const auto calls_before=actions.calls.size();
    check(abandon_quests.receive(net::Packet{8,9,1},actions)==VerticalQuestPacketResult::malformed,"short abandon accepted");
    check(actions.calls.size()==calls_before+2,"malformed quest packet skipped prefix");
    check(abandon_quests.receive(net::Packet{6,9},actions)==VerticalQuestPacketResult::closed && actions.calls.back()==3,"unknown quest packet not closed");
    check(abandon_quests.receive(net::Packet{7,9,1,0,0},actions)==VerticalQuestPacketResult::handled,"reward selection packet not handled");
    net::Packet changed{6,9,1,0};actions.after_shop=[&]{changed[0]=8;};
    check(abandon_quests.receive(changed,actions)==VerticalQuestPacketResult::handled,"quest opcode was read before callbacks");
    actions.after_shop={};
    const auto after_close=actions.calls.size();
    check(abandon_quests.receive(net::Packet{1,2},actions)==VerticalQuestPacketResult::unhandled && actions.calls.size()==after_close,"other family was consumed");
    // The gameplay session routes both original families through one actor,
    // zone and interaction, with the same live quest and NPC owners.
    auto packet_runtime=std::make_shared<QuestRuntime>(77,*pool);Services packet_services(*actor,*packet_runtime,scene);packet_services.check_success_order=false;
    VerticalQuests packet_quests({items,item_pool,ids,defs,pool,actor,packet_runtime},{last,deadline},zone,npc_pool,packet_services);
    VerticalNpcInteraction packet_npc;PacketActions packet_actions(*actor);NpcPacketActions packet_npc_actions(*actor);
    VerticalQuestNpcPackets session_packets(*actor,packet_quests,zone,packet_npc,packet_actions,packet_npc_actions);
    Scene impostor_scene;Actor impostor(actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},impostor_scene,ActorEquipmentRules{*items,enchant,mobs});
    ZoneNpcs wrong_zone(MapGeometry(81),npc_services);bool wrong_session_actor=false,wrong_session_zone=false;
    try {VerticalQuestNpcPackets bad(impostor,packet_quests,zone,packet_npc,packet_actions,packet_npc_actions);} catch(const std::invalid_argument&) {wrong_session_actor=true;}
    try {VerticalQuestNpcPackets bad(*actor,packet_quests,wrong_zone,packet_npc,packet_actions,packet_npc_actions);} catch(const std::invalid_argument&) {wrong_session_zone=true;}
    check(wrong_session_actor && wrong_session_zone,"quest/NPC packet session accepted wrong actor or zone");
    // A zone accidentally containing an NPC from another pool must not change
    // the selected pointer or use counts if the required new pin fails.
    NpcPool foreign_npc_pool;auto* old_npc=npc_pool.allocate();auto* foreign_npc=foreign_npc_pool.allocate();
    old_npc->id=109;old_npc->type=2;old_npc->type_id=9;old_npc->definition=&npc_definition;old_npc->position={0,0,0};old_npc->users_using=0;
    foreign_npc->id=110;foreign_npc->type=2;foreign_npc->type_id=9;foreign_npc->definition=&npc_definition;foreign_npc->position={0,0,0};foreign_npc->users_using=0;
    check(wrong_zone.enter(*old_npc) && wrong_zone.enter(*foreign_npc),"mixed-pool NPC fixture entry failed");
    auto mixed_runtime=std::make_shared<QuestRuntime>(77,*pool);Services mixed_services(*actor,*mixed_runtime,scene);mixed_services.check_success_order=false;
    VerticalQuests mixed_quests({items,item_pool,ids,defs,pool,actor,mixed_runtime},{last,deadline},wrong_zone,npc_pool,mixed_services);
    VerticalNpcInteraction mixed_interaction;PacketActions mixed_actions(*actor);NpcPacketActions mixed_npc_actions(*actor);
    VerticalQuestNpcPackets mixed_packets(*actor,mixed_quests,wrong_zone,mixed_interaction,mixed_actions,mixed_npc_actions);
    check(mixed_packets.receive(net::Packet{4,14,109,0,0,0})==VerticalQuestNpcPacketResult::handled && mixed_interaction.npc==old_npc && old_npc->users_using==1,"mixed-pool initial NPC selection failed");
    bool pin_failed=false;try {(void)mixed_packets.receive(net::Packet{4,14,110,0,0,0});} catch(const std::invalid_argument&) {pin_failed=true;}
    check(pin_failed && mixed_interaction.npc==old_npc && old_npc->users_using==1 && foreign_npc->users_using==0,"failed NPC pin changed the interaction or use counts");
    NpcMoveServices mixed_move(*actor);mixed_packets.move_interactions(false,mixed_move);
    check(!mixed_interaction.npc && old_npc->users_using==0 && wrong_zone.leave(*old_npc) && wrong_zone.leave(*foreign_npc),"mixed-pool NPC cleanup failed");npc_pool.release(*old_npc);foreign_npc_pool.release(*foreign_npc);
    check(session_packets.receive(net::Packet{4})==VerticalQuestNpcPacketResult::malformed && session_packets.receive(net::Packet{1,5})==VerticalQuestNpcPacketResult::unhandled,"session consumed malformed or other-family packet");
    check(session_packets.receive(net::Packet{4,14,99,0,0,0})==VerticalQuestNpcPacketResult::handled && packet_npc.npc==replacement && replacement->users_using==1,"session NPC selection failed");
    check(session_packets.receive(net::Packet{2,9,99,0,0,0,1,0})==VerticalQuestNpcPacketResult::handled && packet_runtime->active().size()==1,"session quest acceptance failed");
    packet_quests.kill_mob(3,links);
    check(session_packets.receive(end_packet)==VerticalQuestNpcPacketResult::handled && packet_runtime->active().empty() && packet_runtime->finished().size()==1 && packet_services.replica.active().empty(),"session quest completion failed");
    check(packet_actions.calls==std::vector<unsigned>({1,2,1,2}) && packet_services.db==std::vector<unsigned>({0xa01,0xa05,0xa02}),"session quest packet actions or persistence order differs");
    NpcMoveServices packet_move(*actor);session_packets.move_interactions(false,packet_move);
    check(!packet_npc.npc && replacement->users_using==0,"session NPC interaction did not reach movement cleanup");
    auto* retiring_npc=npc_pool.allocate();retiring_npc->id=103;retiring_npc->type=2;retiring_npc->type_id=9;retiring_npc->definition=&npc_definition;retiring_npc->position={0,0,0};
    check(zone.enter(*retiring_npc),"session NPC retirement entry failed");
    check(session_packets.receive(net::Packet{4,14,103,0,0,0})==VerticalQuestNpcPacketResult::handled && packet_npc.npc==retiring_npc,"session did not select retiring NPC");
    check(zone.leave(*retiring_npc),"session NPC retirement leave failed");npc_pool.release(*retiring_npc);
    auto* while_used=npc_pool.allocate();check(while_used!=retiring_npc,"selected NPC recycled while interaction still referenced it");npc_pool.release(*while_used);
    packet_move.fail_pvp=true;bool move_failed=false;
    try {session_packets.move_interactions(false,packet_move);} catch(const std::runtime_error&) {move_failed=true;}
    check(move_failed && !packet_npc.npc && retiring_npc->users_using==0,"failed movement did not clean retired NPC interaction");packet_move.fail_pvp=false;
    auto* after_cleanup=npc_pool.allocate();check(after_cleanup==retiring_npc,"NPC remained pinned after failed movement cleanup");npc_pool.release(*after_cleanup);
    auto* leaving_npc=npc_pool.allocate();leaving_npc->id=104;leaving_npc->type=2;leaving_npc->type_id=9;leaving_npc->definition=&npc_definition;
    check(zone.enter(*leaving_npc),"leave NPC entry failed");
    check(session_packets.receive(net::Packet{4,14,104,0,0,0})==VerticalQuestNpcPacketResult::handled && packet_npc.npc==leaving_npc,"leave NPC selection failed");
    check(zone.leave(*leaving_npc),"leave NPC retirement failed");npc_pool.release(*leaving_npc);
    ZoneLeaveShopState packet_shop;NpcLeaveServices packet_leave(*actor,packet_npc,*leaving_npc,0);
    session_packets.leave_interactions(packet_shop,packet_leave);
    check(!packet_npc.npc && packet_leave.shutdowns==1,"session leave did not run real interaction services");
    auto* after_leave=npc_pool.allocate();check(after_leave==leaving_npc,"NPC remained pinned after zone leave");npc_pool.release(*after_leave);
    check(session_packets.receive(net::Packet{5,14})==VerticalQuestNpcPacketResult::closed && packet_npc_actions.closes==1,"session NPC close was not forwarded");
    // Item-pick progress and NPC turn-in share the retained inventory owner.
    auto item_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,1)));
    Scene item_scene;auto item_actor=std::make_shared<Actor>(9021,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},item_scene,ActorEquipmentRules{*items,enchant,mobs});
    item_actor->account_id()=91;item_actor->status().level=20;item_actor->status().skills.max_bags=1;item_actor->appearance().identity.job=0;
    auto item_runtime=std::make_shared<QuestRuntime>(91,*pool);Services item_services(*item_actor,*item_runtime,item_scene);item_services.check_success_order=false;
    VerticalQuests item_quests({items,item_pool,ids,item_defs,pool,item_actor,item_runtime},{last,deadline},zone,npc_pool,item_services);
    auto* required=item_pool->allocate();check(required!=nullptr,"quest item allocation failed");required->definition=items->find(5,1);
    required->item.uid=0x1234;required->item.type=5;required->item.type_id=1;required->item.count=2;
    check(required->definition && item_actor->claim_inventory_item(1,0,*required,*item_pool),"quest item claim failed");
    check(item_services.inventory.load(required->item),"quest item replica load failed");
    check(item_quests.start(1,99),"item quest start failed");const auto item_links=linked_item();
    item_quests.pick_item(5,1,item_links);item_quests.pick_item(5,1,item_links);
    check(item_runtime->active().front()->packet.counters[0]==2 && item_services.replica.active().front().counters[0]==2,"item progress did not reach real quest owners");
    check(item_actor->retained_inventory_item(1,0).has_value(),"NPC-gated item consumed before turn-in");
    item_quests.end(1,98);check(item_runtime->active().size()==1,"wrong NPC accepted item turn-in");
    item_quests.end(1,99);
    check(item_runtime->active().empty() && item_runtime->finished().size()==1 && item_services.experience==20,"item quest did not complete");
    check(!item_actor->retained_inventory_item(1,0).has_value() && item_services.inventory.inventory().empty() && item_services.inventory.deleted_inventory()==std::vector<std::uint64_t>({0x1234}),"quest item was not removed from actor and replica");
    check(item_services.db==std::vector<unsigned>({0xa01,0xa05,0xa05,0x702,0xa02}) && item_services.locks==0,"item quest persistence order differs");
    // Player-kill progress remains pending until the configured NPC turn-in.
    auto pc_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,3)));
    auto pc_runtime=std::make_shared<QuestRuntime>(77,*pool);Services pc_services(*actor,*pc_runtime,scene);pc_services.check_success_order=false;
    VerticalQuests pc_quests({items,item_pool,ids,pc_defs,pool,actor,pc_runtime},{last,deadline},zone,npc_pool,pc_services);
    struct KillContinuation final:VerticalQuestKillPcContinuation {
        Actor& expected_killer;Actor& expected_victim;QuestRuntime& runtime;Services& services;unsigned calls=0;bool fail=false;
        KillContinuation(Actor& a,Actor& b,QuestRuntime& r,Services& s):expected_killer(a),expected_victim(b),runtime(r),services(s) {}
        void after_quest(Actor& killer,Actor& victim) override {
            ++calls;check(&killer==&expected_killer && &victim==&expected_victim,"wrong player kill actors");
            check(runtime.active().front()->packet.counters[0]==calls && services.replica.active().front().counters[0]==calls,"kill bookkeeping ran before quest progress");
            if(fail) throw std::runtime_error("kill bookkeeping unavailable");
        }
    } kill_continuation(*actor,*item_actor,*pc_runtime,pc_services);
    bool wrong_killer=false;try {vertical_quests_kill_pc_to(*item_actor,*actor,pc_quests,kill_continuation);} catch(const std::invalid_argument&) {wrong_killer=true;}
    check(wrong_killer && kill_continuation.calls==0,"wrong player kill owner accepted");
    check(pc_quests.start(1,99),"player-kill quest start failed");vertical_quests_kill_pc_to(*actor,*item_actor,pc_quests,kill_continuation);
    kill_continuation.fail=true;bool continuation_failed=false;
    try {vertical_quests_kill_pc_to(*actor,*item_actor,pc_quests,kill_continuation);} catch(const std::runtime_error&) {continuation_failed=true;}
    check(continuation_failed && kill_continuation.calls==2,"kill continuation failure was hidden");
    check(pc_runtime->active().front()->packet.counters[0]==2 && pc_services.replica.active().front().counters[0]==2,"player-kill progress did not reach real quest owners");
    check(pc_runtime->player_kill_quests()==1,"player-kill quest disappeared before turn-in");pc_quests.end(1,99);
    check(pc_runtime->active().empty() && pc_runtime->finished().size()==1 && pc_runtime->player_kill_quests()==0 && pc_services.experience==20,"player-kill quest did not complete");
    check(pc_services.db==std::vector<unsigned>({0xa01,0xa05,0xa05,0xa02}) && pc_services.locks==0,"player-kill persistence order differs");
    // CZone::UpdateQuest walks the live zone list and retains each user.
    std::array<Scene,3> timer_scenes;std::array<std::shared_ptr<Actor>,3> timer_actors;
    std::array<std::shared_ptr<QuestRuntime>,3> timer_runtimes;std::array<std::unique_ptr<Services>,3> timer_services;
    std::array<std::shared_ptr<VerticalQuests>,3> zone_quests;
    ZoneQuestUsers zone_owners(timer_actors,zone_quests);ZoneUsers timer_zone(1,1,0);
    for(std::size_t i=0;i<timer_actors.size();++i) {
        timer_actors[i]=std::make_shared<Actor>(static_cast<std::uint32_t>(9031+i),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},timer_scenes[i],ActorEquipmentRules{*items,enchant,mobs});
        timer_actors[i]->account_id()=static_cast<std::uint32_t>(101+i);timer_actors[i]->status().level=20;timer_actors[i]->appearance().identity.job=0;
        timer_runtimes[i]=std::make_shared<QuestRuntime>(timer_actors[i]->account_id(),*pool);
        timer_services[i]=std::make_unique<Services>(*timer_actors[i],*timer_runtimes[i],timer_scenes[i]);timer_services[i]->check_success_order=false;
        zone_quests[i]=std::make_shared<VerticalQuests>(QuestStartItemsActorWorldOwners{items,item_pool,ids,timed_defs,pool,timer_actors[i],timer_runtimes[i]},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,*timer_services[i]);
        check(zone_quests[i]->start(1,99),"zone timer quest start failed");
    }
    bool wrong_timer_actor=false;
    try {zone_quests[0]->update_timers_for(*timer_actors[1],60100);} catch(const std::logic_error&) {wrong_timer_actor=true;}
    check(wrong_timer_actor && timer_runtimes[0]->active().size()==1 && timer_runtimes[1]->active().size()==1 && timer_services[0]->db==std::vector<unsigned>({0xa01}) && timer_services[1]->db==std::vector<unsigned>({0xa01}),"direct timer phase accepted another actor");
    zone_quests[0]->update_timers_for(*timer_actors[0],60099);
    check(timer_runtimes[0]->active().size()==1 && timer_services[0]->locks==0,"direct timer phase changed unexpired quest");
    timer_zone.attach(*timer_actors[0],0,0,{});timer_zone.attach(*timer_actors[1],0,0,{});
    zone_owners.wrong=true;bool zone_rejected=false;
    try {vertical_quests_update_zone(timer_zone,60100,zone_owners);} catch(const std::logic_error&) {zone_rejected=true;}
    check(zone_rejected && timer_runtimes[0]->active().size()==1 && timer_runtimes[1]->active().size()==1,"zone timer accepted mismatched quest owner");
    zone_owners.wrong=false;zone_owners.visits.clear();std::vector<std::uint32_t> expired;
    for(std::size_t i=0;i<timer_actors.size();++i) timer_services[i]->on_remove=[&,i] {
        expired.push_back(timer_actors[i]->id());
        if(i==0) {check(timer_zone.detach(timer_actors[0]->id()),"zone timer detach failed");timer_zone.attach(*timer_actors[2],0,0,{});}
    };
    vertical_quests_update_zone(timer_zone,60099,zone_owners);
    check(expired.empty(),"zone timer expired before deadline");zone_owners.visits.clear();
    vertical_quests_update_zone(timer_zone,60100,zone_owners);
    check(expired==std::vector<std::uint32_t>({9031,9032,9033}) && zone_owners.visits==expired,"zone timer lost live traversal order");
    for(std::size_t i=0;i<timer_runtimes.size();++i) {
        check(timer_runtimes[i]->active().empty() && timer_runtimes[i]->finished().size()==1 && timer_services[i]->replica.active().empty(),"zone timer failed to remove a real quest");
        check(timer_services[i]->db==std::vector<unsigned>({0xa01,0xa02}) && timer_services[i]->locks==0,"zone timer persistence or quest lock differs");
    }
    check(timer_zone.size()==2 && !timer_actors[0]->status().in_zone,"zone timer membership mutation differs");
    // PacketNpc shares the retained interaction with movement cleanup.
    auto* other_npc=npc_pool.allocate();other_npc->id=102;other_npc->type=2;other_npc->type_id=9;other_npc->definition=&npc_definition;other_npc->position={10,0,10};
    check(zone.enter(*other_npc),"second NPC entry failed");VerticalNpcInteraction interaction;NpcPacketActions npc_actions(*actor);
    check(vertical_npc_request(*actor,net::Packet{4,14,99,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled && interaction.npc==replacement && replacement->users_using==1,"NPC packet did not acquire interaction");
    check(vertical_npc_request(*actor,net::Packet{4,14,99,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled && replacement->users_using==1,"repeated NPC use changed count");
    check(vertical_npc_request(*actor,net::Packet{4,14,98,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled && interaction.npc==replacement && replacement->users_using==1,"missing NPC changed interaction");
    other_npc->users_using=std::numeric_limits<std::int32_t>::max();
    check(vertical_npc_request(*actor,net::Packet{4,14,102,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled && interaction.npc==other_npc && replacement->users_using==0 && other_npc->users_using==std::numeric_limits<std::int32_t>::min(),"NPC switch or count wrapping differs");
    check(vertical_npc_request(*actor,net::Packet{4,14,102},zone,interaction,npc_actions)==VerticalNpcPacketResult::malformed && interaction.npc==other_npc,"short NPC packet changed interaction");
    check(vertical_npc_request(*actor,net::Packet{5,14},zone,interaction,npc_actions)==VerticalNpcPacketResult::closed && npc_actions.closes==1,"invalid NPC opcode did not close");
    check(vertical_npc_request(*actor,net::Packet{4,9},zone,interaction,npc_actions)==VerticalNpcPacketResult::unhandled && npc_actions.closes==1,"other opcode family consumed by NPC handler");
    NpcMoveServices move_services(*actor);vertical_move_interactions(*actor,interaction,false,move_services);
    check(!interaction.npc && other_npc->users_using==std::numeric_limits<std::int32_t>::max() && move_services.pvp==1,"movement did not release the same NPC interaction");
    // The same use counter gates real NPC patrol and movement away releases it.
    other_npc->users_using=0;other_npc->next_move=0;other_npc->next_position=0;other_npc->forward=true;
    npc_definition.record[8]=4;npc_definition.record[12]=7;MapNpc patrol;patrol.positions={{{11,0,10},0},{{12,0,10},0}};other_npc->source=&patrol;
    const auto broadcasts_before=npc_services.broadcasts.size();const auto deliveries_before=npc_services.deliveries;
    check(vertical_npc_request(*actor,net::Packet{4,14,102,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled && other_npc->users_using==1,"patrol NPC interaction failed");
    MicrosoftRandom patrol_random(1);const auto seed_before=patrol_random.state();update_npc_patrol(*other_npc,100,patrol_random,zone);
    check(patrol_random.state()==seed_before && other_npc->position.x==10 && other_npc->next_move==0 && npc_services.broadcasts.size()==broadcasts_before,"active NPC interaction did not pause patrol");
    vertical_move_interactions(*actor,interaction,false,move_services);
    check(!interaction.npc && other_npc->users_using==0 && move_services.pvp==2,"movement failed to clear patrol interaction");
    MicrosoftRandom expected_random(1);const auto pause=expected_random.next()%10000;
    update_npc_patrol(*other_npc,100,patrol_random,zone);
    check(other_npc->position.x==11 && other_npc->next_position==1 && other_npc->next_move==100+7+10000+static_cast<std::uint32_t>(pause) && patrol_random.state()==expected_random.state(),"patrol did not resume after interaction ended");
    check(npc_services.broadcasts.size()==broadcasts_before+1 && word(npc_services.broadcasts.back(),0,2)==0xe03 && npc_services.deliveries>deliveries_before,"resumed patrol did not broadcast real NPC movement");
    // LeaveZone must release only the departing user's share of a shared NPC.
    VerticalNpcInteraction second_interaction;NpcPacketActions second_actions(*item_actor);
    check(vertical_npc_request(*actor,net::Packet{4,14,102,0,0,0},zone,interaction,npc_actions)==VerticalNpcPacketResult::handled,"first shared NPC selection failed");
    check(vertical_npc_request(*item_actor,net::Packet{4,14,102,0,0,0},zone,second_interaction,second_actions)==VerticalNpcPacketResult::handled && other_npc->users_using==2,"second shared NPC selection failed");
    const auto resume_tick=other_npc->next_move;const auto random_before_leave=patrol_random.state();const auto before_leave_broadcast=npc_services.broadcasts.size();
    update_npc_patrol(*other_npc,resume_tick,patrol_random,zone);
    check(other_npc->position.x==11 && patrol_random.state()==random_before_leave && npc_services.broadcasts.size()==before_leave_broadcast,"shared NPC moved while both users were present");
    ZoneLeaveShopState first_shop,second_shop;NpcLeaveServices first_leave(*actor,interaction,*other_npc,1),second_leave(*item_actor,second_interaction,*other_npc,0);
    zone_leave_interactions(*actor,interaction,first_shop,first_leave);
    check(first_leave.shutdowns==1 && !interaction.npc && second_interaction.npc==other_npc && other_npc->users_using==1,"first zone leave released wrong NPC state");
    update_npc_patrol(*other_npc,resume_tick,patrol_random,zone);
    check(other_npc->position.x==11 && patrol_random.state()==random_before_leave && npc_services.broadcasts.size()==before_leave_broadcast,"shared NPC resumed before last user left");
    zone_leave_interactions(*item_actor,second_interaction,second_shop,second_leave);
    check(second_leave.shutdowns==1 && !second_interaction.npc && other_npc->users_using==0,"last zone leave did not release NPC state");
    const auto second_pause=expected_random.next()%10000;update_npc_patrol(*other_npc,resume_tick,patrol_random,zone);
    check(other_npc->position.x==12 && other_npc->next_position==0 && !other_npc->forward && other_npc->next_move==resume_tick+7+10000+static_cast<std::uint32_t>(second_pause),"shared NPC patrol did not resume after last leave");
    check(patrol_random.state()==expected_random.state() && npc_services.broadcasts.size()==before_leave_broadcast+1 && word(npc_services.broadcasts.back(),0,2)==0xe03,"shared NPC resumed movement differs");
    check(zone.leave(*other_npc),"second NPC leave failed");npc_pool.release(*other_npc);
    // Materialize the character loader's actual restored quest projection.
    auto two_records=quest_data();const auto record_size=two_records.size()-4;
    const net::Packet second_record(two_records.begin()+4,two_records.end());
    two_records.insert(two_records.end(),second_record.begin(),second_record.end());two_records[0]=2;two_records[4+record_size]=2;
    auto restored_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(two_records));
    PlayerSnapshot snapshot;snapshot.quests={{1,3,{1,0,0}},{99,0,{0,0,0}}};snapshot.finished_quests={{2,1},{99,1}};
    const auto loaded=restore_quests(snapshot,*restored_defs,200000);
    check(loaded.active.size()==1 && loaded.finished.size()==1 && loaded.invalid_active==std::vector<std::uint16_t>({99}),"character quest projection differs");
    auto restored=vertical_quests_restore_runtime(loaded,105,pool,restored_defs);
    check(restored.runtime && !restored.skipped_active && !restored.skipped_finished && restored.runtime->active().size()==1 && restored.runtime->finished().size()==1,"runtime quest materialization failed");
    check(restored.runtime->mob_kill_quests()==1 && restored.runtime->active().front()->start_tick==20000 && restored.runtime->active().front()->packet.counters[0]==1 && restored.runtime->finished().front()->success==1,"restored runtime state differs");
    const auto initial_rows=vertical_quests_entry_rows(*restored.runtime);
    const auto active_entry=character_quests(initial_rows.active,200000);const auto finished_entry=character_finished_quests(initial_rows.finished);
    check(active_entry.size()==1 && active_entry.front()==net::Packet({1,9,1,1,0,0,0,1,0,0}) && finished_entry.size()==1 && finished_entry.front()==net::Packet({6,9,1,2,0,1}),"restored entry packets differ");
    Scene restored_scene;auto restored_actor=std::make_shared<Actor>(9041,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},restored_scene,ActorEquipmentRules{*items,enchant,mobs});
    restored_actor->account_id()=105;restored_actor->status().level=20;restored_actor->appearance().identity.job=0;
    Services restored_services(*restored_actor,*restored.runtime,restored_scene);restored_services.check_success_order=false;
    VerticalQuestRewardBinding reward_binding(*restored_actor,*restored.runtime,*pool,restored_services);
    restored_services.reward_override=&reward_binding;restored_services.forbid_reward_remove=true;
    bool reward_account_rejected=false,reward_pool_rejected=false;
    try {VerticalQuestRewardBinding bad(*actor,*restored.runtime,*pool,restored_services);} catch(const std::invalid_argument&) {reward_account_rejected=true;}
    try {VerticalQuestRewardBinding bad(*restored_actor,*restored.runtime,*other_pool,restored_services);} catch(const std::invalid_argument&) {reward_pool_rejected=true;}
    check(reward_account_rejected && reward_pool_rejected,"reward binding accepted mismatched owners");
    check(restored_services.replica.load(snapshot.quests[0]) && restored_services.replica.load_finished(snapshot.finished_quests[0]),"restored DB replica setup failed");
    VerticalQuests restored_quests({items,item_pool,ids,restored_defs,pool,restored_actor,restored.runtime},{last,deadline},zone,npc_pool,restored_services);
    struct FinishedEntryLock final:VerticalQuestEntryFinishedLock {
        Actor& actor;unsigned locks=0;bool held=false,change_account=false;
        explicit FinishedEntryLock(Actor& value):actor(value) {}
        void lock_finished(Actor& value) override {check(&value==&actor && !held,"wrong finished quest lock");held=true;++locks;if(change_account) actor.account_id()=106;}
        void unlock_finished(Actor& value) noexcept override {if(&value!=&actor || !held) std::terminate();held=false;}
    } finished_entry_lock(*restored_actor);
    const auto entry_locks=restored_services.lock_calls;
    check(!restored_quests.send_entry_step(CharacterEntryStep::items,finished_entry_lock) && restored_scene.packets.empty() && restored_services.lock_calls==entry_locks && finished_entry_lock.locks==0,"unrelated entry step invoked quest owner");
    check(restored_quests.send_entry_step(CharacterEntryStep::quests,finished_entry_lock) && restored_quests.send_entry_step(CharacterEntryStep::finished_quests,finished_entry_lock),"bound quest entry steps not handled");
    check(restored_scene.packets==std::vector<net::Packet>({active_entry.front(),finished_entry.front()}) && restored_services.lock_calls==entry_locks+1 && restored_services.locks==0 && finished_entry_lock.locks==1 && !finished_entry_lock.held,"bound quest entry packets or locks differ");
    const auto entry_packets_before=restored_scene.packets.size();restored_services.on_lock=[&]{restored_actor->account_id()=106;};bool changed_active_entry=false;
    try {(void)restored_quests.send_entry_step(CharacterEntryStep::quests,finished_entry_lock);} catch(const std::logic_error&) {changed_active_entry=true;}
    check(changed_active_entry && restored_services.locks==0 && restored_scene.packets.size()==entry_packets_before,"active entry lock account change leaked lock or sent packet");restored_actor->account_id()=105;restored_services.on_lock={};
    finished_entry_lock.change_account=true;bool changed_finished_entry=false;
    try {(void)restored_quests.send_entry_step(CharacterEntryStep::finished_quests,finished_entry_lock);} catch(const std::logic_error&) {changed_finished_entry=true;}
    check(changed_finished_entry && !finished_entry_lock.held && restored_scene.packets.size()==entry_packets_before,"finished entry lock account change leaked lock or sent packet");restored_actor->account_id()=105;finished_entry_lock.change_account=false;
    check(!restored_quests.start(2,99),"restored finished quest admitted again");
    bool wrong_quest_rejected=false;
    try {reward_binding.remove_quest(*runtime->finished().front(),true);} catch(const std::invalid_argument&) {wrong_quest_rejected=true;}
    check(wrong_quest_rejected && restored.runtime->active().size()==1 && restored_services.db.empty(),"reward binding removed a quest from another runtime");
    restored_quests.end(1,99);
    check(restored.runtime->active().empty() && restored.runtime->finished().size()==2 && restored_services.replica.active().empty() && restored_services.replica.finished().size()==2,"restored quest did not complete through live owners");
    check(restored_services.db==std::vector<unsigned>({0xa02}) && restored_services.experience==20,"restored quest completion side effects differ");
    restored_actor->account_id()=106;bool reward_changed_rejected=false;
    try {reward_binding.add_experience(0,99,true);} catch(const std::logic_error&) {reward_changed_rejected=true;}
    restored_actor->account_id()=105;
    check(reward_changed_rejected && restored_services.experience==20,"reward binding used a changed account");
    // DB-agent allocation failures must not resurrect SQL snapshot rows when
    // the game-server runtime is restored from the actual retained cache.
    PlayerSnapshot db_rows;db_rows.core.user_id=116;db_rows.core.character.id=9042;db_rows.core.character.level=20;db_rows.catalogs["PointLog"]={};
    db_rows.quests={{1,0,{0,0,0}},{2,3,{1,0,0}},{99,0,{0,0,0}}};db_rows.finished_quests={{1,1}};
    unsigned db_attempts=0;QuestReplicaPool db_quest_pool([&]()->std::unique_ptr<CachedQuest>{return ++db_attempts==1?nullptr:std::make_unique<CachedQuest>();});
    InventoryReplicaPool db_items;auto db_character=vertical_persistence_restore(db_rows,4,db_items,nullptr,&db_quest_pool);
    check(db_character->quests().active().size()==2 && db_character->quests().active()[0].id==2 && restore_quests(db_rows,*restored_defs,200000).active.size()==2,"DB-agent skipped-row fixture differs");
    auto db_restore=vertical_quests_restore_replica(*db_character,200000,pool,restored_defs);
    check(db_restore.invalid_active==std::vector<std::uint16_t>({99}) && db_restore.restored.runtime->bound_to(116,*pool) && db_restore.restored.runtime->active().size()==1 && db_restore.restored.runtime->active().front()->packet.id==2 && db_restore.restored.runtime->active().front()->packet.counters[0]==1 && db_restore.restored.runtime->active().front()->start_tick==20000 && db_restore.restored.runtime->finished().size()==1 && db_restore.restored.runtime->finished().front()->packet.id==1,"runtime resurrected skipped DB row or changed retained payload");
    Scene db_scene;auto db_actor=std::make_shared<Actor>(9042,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},db_scene,ActorEquipmentRules{*items,enchant,mobs});
    db_actor->account_id()=116;db_actor->status().level=20;db_actor->appearance().identity.job=0;
    Services db_services(*db_actor,*db_restore.restored.runtime,db_scene);db_services.check_success_order=false;
    check(db_services.replica.load({2,3,{1,0,0}}) && db_services.replica.load_finished({1,1}),"DB restoration mirror setup failed");
    std::recursive_mutex db_lock;CharacterReplicaDelivery db_delivery(*db_character,db_lock);db_services.character_delivery=&db_delivery;
    VerticalQuestRewardBinding db_reward(*db_actor,*db_restore.restored.runtime,*pool,db_services);db_services.reward_override=&db_reward;db_services.forbid_reward_remove=true;
    VerticalQuests db_quests({items,item_pool,ids,restored_defs,pool,db_actor,db_restore.restored.runtime},{last,deadline},zone,npc_pool,db_services);
    FinishedEntryLock db_finished_lock(*db_actor);
    check(db_quests.send_entry_step(CharacterEntryStep::quests,db_finished_lock) && db_quests.send_entry_step(CharacterEntryStep::finished_quests,db_finished_lock) && db_scene.packets==std::vector<net::Packet>({{1,9,1,2,0,0,0,1,0,0},{6,9,1,1,0,1}}),"replica-restored quest entry included skipped or invalid rows");
    db_quests.end(2,99);
    check(db_restore.restored.runtime->active().empty() && db_restore.restored.runtime->finished().size()==2 && db_character->quests().active().size()==1 && db_character->quests().active().front().id==99 && db_character->quests().finished().size()==2 && db_services.db==std::vector<unsigned>({0xa02}),"replica-restored quest completion diverged");
    // Bind the same restored DB cache to the actual selected session actor;
    // the Actor alias retains the session rather than inventing a second owner.
    PlayerSnapshot selected;selected.core.user_id=117;selected.core.character.id=9043;selected.core.character.level=20;selected.catalogs["PointLog"]={};
    selected.quests={{1,0,{0,0,0}},{2,3,{1,0,0}}};selected.finished_quests={{1,1}};
    unsigned selected_attempts=0;QuestReplicaPool selected_quest_pool([&]()->std::unique_ptr<CachedQuest>{return ++selected_attempts==1?nullptr:std::make_unique<CachedQuest>();});
    InventoryReplicaPool selected_items;auto selected_replica=vertical_persistence_restore(selected,4,selected_items,nullptr,&selected_quest_pool);
    const StatusRules selected_status;const SkillDefinitions selected_skills;const ActorLoadRules selected_rules{selected_status,*items,enchant,selected_skills,*restored_defs,mobs};
    ActorLoadOptions selected_options;selected_options.tick_ms=200000;selected_options.inventory_pool=item_pool.get();Scene selected_scene;
    auto selected_session=std::make_shared<VerticalSessionCharacter>(VerticalPersistenceSelection{std::move(selected),std::move(selected_replica)},selected_rules,selected_options,selected_scene);
    struct SelectedOwner {
        std::shared_ptr<VerticalSessionCharacter> selected;std::uint32_t account=117;
        VerticalSessionCharacter* character() noexcept {return selected.get();}
        std::uint32_t account_id() const noexcept {return account;}
    };
    auto selected_owner=std::make_shared<SelectedOwner>();selected_owner->selected=selected_session;
    auto* selected_actor=selected_session->pipeline().result().actor.get();auto* selected_cache=&selected_session->replica();std::recursive_mutex selected_mutex;
    check(selected_actor && selected_cache->quests().active().size()==1 && selected_cache->quests().active()[0].id==2,"selected session fixture differs");
    selected_actor->account_id()=118;const auto attempts_before_session=pool->construction_attempts();bool stale_session_rejected=false;
    try {(void)vertical_quests_restore_selected_session(selected_owner,selected_mutex,200000,pool,restored_defs);} catch(const std::invalid_argument&) {stale_session_rejected=true;}
    check(stale_session_rejected && pool->construction_attempts()==attempts_before_session,"stale session allocated quest runtime");selected_actor->account_id()=117;
    selected_owner->account=118;bool wrong_session_account=false;
    try {(void)vertical_quests_restore_selected_session(selected_owner,selected_mutex,200000,pool,restored_defs);} catch(const std::invalid_argument&) {wrong_session_account=true;}
    check(wrong_session_account && pool->construction_attempts()==attempts_before_session,"session account mismatch allocated quest runtime");selected_owner->account=117;
    std::shared_ptr<SelectedOwner> absent_owner;bool absent_session=false;
    try {(void)vertical_quests_restore_selected_session(absent_owner,selected_mutex,200000,pool,restored_defs);} catch(const std::invalid_argument&) {absent_session=true;}
    auto unselected=std::make_shared<SelectedOwner>();bool absent_character=false;
    try {(void)vertical_quests_restore_selected_session(unselected,selected_mutex,200000,pool,restored_defs);} catch(const std::invalid_argument&) {absent_character=true;}
    check(absent_session && absent_character && pool->construction_attempts()==attempts_before_session,"missing selected session allocated quest runtime");
    auto selected_restore=vertical_quests_restore_selected_session(selected_owner,selected_mutex,200000,pool,restored_defs);
    check(selected_restore.actor.get()==selected_actor && selected_restore.quests.restored.runtime->active().size()==1 && selected_restore.quests.restored.runtime->active().front()->packet.id==2 && selected_restore.quests.restored.runtime->finished().size()==1,"session quest restoration used raw skipped row");
    std::weak_ptr<VerticalSessionCharacter> weak_selected=selected_session;std::weak_ptr<SelectedOwner> weak_owner=selected_owner;
    {
        Services selected_services(*selected_restore.actor,*selected_restore.quests.restored.runtime,selected_scene);selected_services.check_success_order=false;
        check(selected_services.replica.load({2,3,{1,0,0}}) && selected_services.replica.load_finished({1,1}),"session mirror setup failed");
        CharacterReplicaDelivery selected_delivery(*selected_cache,selected_mutex);selected_services.character_delivery=&selected_delivery;
        VerticalQuestRewardBinding selected_reward(*selected_restore.actor,*selected_restore.quests.restored.runtime,*pool,selected_services);selected_services.reward_override=&selected_reward;selected_services.forbid_reward_remove=true;
        VerticalQuests selected_quests({items,item_pool,ids,restored_defs,pool,selected_restore.actor,selected_restore.quests.restored.runtime},{last,deadline},zone,npc_pool,selected_services);
        selected_session.reset();selected_owner.reset();selected_restore.actor.reset();check(!weak_selected.expired() && !weak_owner.expired(),"quest Actor alias did not retain session");
        FinishedEntryLock selected_finished(*selected_actor);
        check(selected_quests.send_entry_step(CharacterEntryStep::quests,selected_finished) && selected_quests.send_entry_step(CharacterEntryStep::finished_quests,selected_finished) && selected_scene.packets.size()==2 && word(selected_scene.packets[0],3,2)==2,"selected session entry did not use retained runtime");
        selected_quests.end(2,99);
        check(selected_cache->quests().active().empty() && selected_cache->quests().finished().size()==2 && selected_services.db==std::vector<unsigned>({0xa02}),"selected session quest completion diverged from its cache");
    }
    check(weak_selected.expired() && weak_owner.expired(),"quest Actor alias retained the session after its owner was released");
    // Failed CQuest allocations are skipped per original 0506/0507 handlers.
    PatternPoolFactory limited({false,true,false});auto limited_pool=std::make_shared<QuestRuntimePool>(limited);
    PlayerSnapshot failures;failures.quests={{1,0,{0,0,0}},{2,0,{0,0,0}}};failures.finished_quests={{1,0}};
    auto limited_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(two_records));
    bool wrong_definitions=false;try {(void)vertical_quests_restore_runtime(loaded,106,limited_pool,limited_defs);} catch(const std::invalid_argument&) {wrong_definitions=true;}
    check(wrong_definitions && limited.next==0,"mismatched quest definitions were not rejected before allocation");
    auto failed_rows=restore_quests(failures,*limited_defs,100);
    auto partial=vertical_quests_restore_runtime(failed_rows,106,limited_pool,limited_defs);
    check(partial.skipped_active==1 && partial.skipped_finished==1 && partial.runtime->active().size()==1 && partial.runtime->active().front()->packet.id==2 && partial.runtime->mob_kill_quests()==1 && partial.runtime->finished().empty(),"quest restore allocation failure behavior differs");
    const auto entry_rows=vertical_quests_entry_rows(*partial.runtime);
    check(entry_rows.active.size()==1 && entry_rows.active.front().id==2 && entry_rows.finished.empty(),"entry includes a quest skipped by allocation");
    const auto partial_entry=character_quests(entry_rows.active,100);
    check(partial_entry.size()==1 && partial_entry.front()==net::Packet({1,9,1,2,0,0,0,0,0,0}) && character_finished_quests(entry_rows.finished).empty(),"entry packets include allocation failures");
    EntryTrace partial_trace;
    check(vertical_quests_send_entry_step(*partial.runtime,CharacterEntryStep::quests,partial_trace) && partial_trace.events==std::vector<char>({'T','A','a','s'}) && partial_trace.packets==partial_entry,"actual runtime entry step differs");
    partial_trace.events.clear();partial_trace.packets.clear();
    check(vertical_quests_send_entry_step(*partial.runtime,CharacterEntryStep::finished_quests,partial_trace) && partial_trace.events==std::vector<char>({'F','f'}) && partial_trace.packets.empty(),"empty finished entry differs");
    std::weak_ptr<QuestRuntimePool> weak_pool=limited_pool;std::weak_ptr<const QuestDefinitions> weak_defs=limited_defs;
    limited_pool.reset();limited_defs.reset();check(!weak_pool.expired() && !weak_defs.expired(),"restored runtime did not retain dependencies");
    partial.runtime.reset();check(weak_pool.expired() && weak_defs.expired(),"restored runtime kept dependencies after release");
    // Full 255-entry batches are sent under the respective list lock; the
    // residual batch is sent after unlock, as in the two original routines.
    PoolFactory bulk_factory;auto bulk_pool=std::make_shared<QuestRuntimePool>(bulk_factory);QuestRuntime bulk_runtime(108,*bulk_pool);
    RestoredQuest bulk_seed;bulk_seed.definition=restored_defs->find(1);bulk_seed.packet.id=1;bulk_seed.packet.timed=true;bulk_seed.packet.end_tick=60100;bulk_seed.packet.counters={1,2,3};bulk_seed.success=1;
    for(unsigned i=0;i<256;++i) {check(bulk_runtime.load_active(bulk_seed),"bulk active allocation failed");check(bulk_runtime.load_finished(bulk_seed),"bulk finished allocation failed");}
    EntryTrace bulk_trace;
    check(!vertical_quests_send_entry_step(bulk_runtime,CharacterEntryStep::items,bulk_trace) && bulk_trace.events.empty(),"non-quest entry step was consumed");
    check(vertical_quests_send_entry_step(bulk_runtime,CharacterEntryStep::quests,bulk_trace),"active quest entry not handled");
    check(bulk_trace.events==std::vector<char>({'T','A','S','a','s'}) && bulk_trace.packets.size()==2 && bulk_trace.packets[0].size()==1788 && bulk_trace.packets[0][2]==255 && bulk_trace.packets[1].size()==10 && bulk_trace.packets[1][2]==1 && word(bulk_trace.packets[0],5,2)==0,"active entry batching, lock or definition timer guard differs");
    bulk_trace.events.clear();bulk_trace.packets.clear();
    check(vertical_quests_send_entry_step(bulk_runtime,CharacterEntryStep::finished_quests,bulk_trace),"finished quest entry not handled");
    check(bulk_trace.events==std::vector<char>({'F','S','f','s'}) && bulk_trace.packets.size()==2 && bulk_trace.packets[0].size()==768 && bulk_trace.packets[0][2]==255 && bulk_trace.packets[1].size()==6 && bulk_trace.packets[1][2]==1,"finished entry batching or lock differs");
    bulk_trace.events.clear();bulk_trace.packets.clear();bulk_trace.fail_full=true;bool entry_failed=false;
    try {(void)vertical_quests_send_entry_step(bulk_runtime,CharacterEntryStep::quests,bulk_trace);} catch(const std::runtime_error&) {entry_failed=true;}
    check(entry_failed && !bulk_trace.active && bulk_trace.events==std::vector<char>({'T','A','S','a'}),"entry failure leaked active quest lock");
    QuestRuntime empty_runtime(109,*bulk_pool);EntryTrace empty_trace;
    check(vertical_quests_send_entry_step(empty_runtime,CharacterEntryStep::quests,empty_trace) && empty_trace.events==std::vector<char>({'T','A','a'}) && empty_trace.packets.empty(),"empty active entry sampling differs");
    PoolFactory recycling;auto recycling_pool=std::make_shared<QuestRuntimePool>(recycling);
    auto* old_state=recycling_pool->allocate();check(old_state,"cannot prepare recycled quest");
    old_state->start_tick=123;old_state->success_tick=456;old_state->packet.end_tick=789;old_state->packet.counters={7,8,9};recycling_pool->release(*old_state);
    PlayerSnapshot completed_only;completed_only.finished_quests={{2,1}};
    const auto completed_rows=restore_quests(completed_only,*restored_defs,100);
    auto recycled_finish=vertical_quests_restore_runtime(completed_rows,107,recycling_pool,restored_defs);
    const auto* finished_state=recycled_finish.runtime->finished().front();
    check(finished_state==old_state && finished_state->definition==restored_defs->find(2) && finished_state->packet.id==2 && finished_state->success==1 && finished_state->start_tick==123 && finished_state->success_tick==456 && finished_state->packet.end_tick==789 && finished_state->packet.counters==std::array<std::uint8_t,3>({7,8,9}),"restored finished quest overwrote recycled payload");
    // Runtime save commands feed the same DB-agent quest replica and SQL plan.
    Scene save_scene;auto save_actor=std::make_shared<Actor>(9051,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},save_scene,ActorEquipmentRules{*items,enchant,mobs});
    save_actor->account_id()=111;save_actor->status().level=20;save_actor->appearance().identity.job=0;
    auto save_runtime=std::make_shared<QuestRuntime>(111,*pool);Services save_services(*save_actor,*save_runtime,save_scene);save_services.check_success_order=false;
    VerticalQuests save_quests({items,item_pool,ids,timed_defs,pool,save_actor,save_runtime},{last,deadline},zone,npc_pool,save_services);
    check(save_quests.start(1,99),"save quest start failed");save_quests.kill_mob(3,links);
    std::vector<net::Packet> save_packets;save_services.on_database=[&](const net::Packet& packet){check(save_services.locks==0,"partial save packet sent before unlock");save_packets.push_back(packet);};
    auto previous_save_locks=save_services.lock_calls;save_actor->account_id()=112;bool changed_save_account=false;
    try {save_quests.save_runtime(100);} catch(const std::logic_error&) {changed_save_account=true;}
    check(changed_save_account && save_services.lock_calls==previous_save_locks && save_packets.empty(),"save accepted changed account before lock");save_actor->account_id()=111;
    save_services.on_lock=[&]{save_actor->account_id()=112;};bool changed_during_lock=false;
    try {save_quests.save_runtime(100);} catch(const std::logic_error&) {changed_during_lock=true;}
    check(changed_during_lock && save_services.locks==0 && save_packets.empty(),"save failed to release lock after account changed");save_actor->account_id()=111;save_services.on_lock={};
    previous_save_locks=save_services.lock_calls;save_quests.save_runtime(100);save_services.on_database={};
    check(save_services.lock_calls==previous_save_locks+1 && save_services.locks==0 && save_packets==std::vector<net::Packet>({{4,10,111,0,0,0,1,1,0,1,0,1,0,0}}),"runtime quest save packet or ordering differs");
    check(save_services.replica.active().size()==1 && save_services.replica.active().front().delay==1 && save_services.replica.active().front().counters[0]==1,"runtime save did not update DB-agent replica");
    auto save_plan=quest_save_plan(save_services.replica,save_actor->id());
    check(save_plan.size()==1 && save_plan[0].procedure=="usp_Save_Char_Quest_Add_E" && save_plan[0].parameters==std::vector<storage::Parameter>({std::int64_t{9051},std::int64_t{1},std::int64_t{1},std::int64_t{1},std::int64_t{0},std::int64_t{0}}),"active quest SQL plan differs");
    save_quests.end(1,99);check(save_runtime->active().empty() && save_services.replica.active().empty() && save_services.replica.finished().size()==1,"save quest completion diverged from replica");
    save_plan=quest_save_plan(save_services.replica,save_actor->id());
    check(save_plan.size()==1 && save_plan[0].procedure=="usp_Save_Char_FinishedQuest_E" && save_plan[0].parameters==std::vector<storage::Parameter>({std::int64_t{9051},std::int64_t{1},std::int64_t{1},std::int64_t{1}}),"finished quest SQL plan differs");
    check(save_services.db==std::vector<unsigned>({0xa01,0xa05,0xa04,0xa02}) && save_services.locks==0,"quest save lifecycle persistence order differs");
    // Reward item uses the inner QuestAddItem and actual retained Actor inventory.
    auto reward_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,2,true,42)));
    Scene reward_scene;auto reward_actor=std::make_shared<Actor>(9061,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},reward_scene,ActorEquipmentRules{*items,enchant,mobs});
    reward_actor->account_id()=112;reward_actor->money()=7;reward_actor->status().level=20;reward_actor->status().skills.max_bags=1;reward_actor->appearance().identity.job=0;
    auto reward_runtime=std::make_shared<QuestRuntime>(112,*pool);Services reward_services(*reward_actor,*reward_runtime,reward_scene);reward_services.check_success_order=false;
    QuestStartItemsActor reward_grants({items,item_pool,ids,reward_actor},reward_services);unsigned item_names=0;
    VerticalQuestRewardItems reward_items(*reward_actor,reward_grants,[&](const ItemDefinition& definition) {check(&definition==items->find(5,1),"wrong reward item definition");++item_names;return std::string("Reward");});
    bool wrong_reward_actor=false;try {VerticalQuestRewardItems bad(*actor,reward_grants,[](const ItemDefinition&){return std::string{};});} catch(const std::invalid_argument&) {wrong_reward_actor=true;}
    check(wrong_reward_actor,"reward item adapter accepted wrong Actor");
    VerticalQuestRewardBinding reward_owner(*reward_actor,*reward_runtime,*pool,reward_services);reward_services.reward_override=&reward_owner;reward_services.item_reward=&reward_items;reward_services.forbid_reward_remove=true;
    VerticalQuests item_reward_quests({items,item_pool,ids,reward_defs,pool,reward_actor,reward_runtime},{last,deadline},zone,npc_pool,reward_services);
    check(item_reward_quests.start(1,99),"item reward quest start failed");item_reward_quests.kill_mob(3,links);item_reward_quests.end(1,99);
    check(reward_runtime->active().empty() && reward_runtime->finished().size()==1 && reward_services.replica.active().empty(),"item reward did not complete the actual quest");
    const auto reward_held=reward_actor->retained_inventory_item(1,0);const auto saved_items=reward_services.inventory.inventory();
    check(reward_held && reward_held->pool==item_pool.get() && reward_held->item->definition==items->find(5,1) && reward_held->item->item.count==1 && saved_items.size()==1 && saved_items.front().item.uid==reward_held->item->item.uid && item_names==1,"reward item did not reach actual Actor and DB replica");
    check(reward_actor->money()==49 && reward_services.money.money()==49 && reward_services.db==std::vector<unsigned>({0xa01,0xa05,0x603,0x701,0xa02}),"reward money ownership, replica or database order differs");
    check(reward_scene.packets.size()==3 && word(reward_scene.packets[2],0,2)==0x903 && reward_scene.packets[2].size()==22 && reward_scene.packets[2][18]==1 && reward_scene.packets[2][19]==0 && reward_scene.packets[2][20]==5 && reward_scene.packets[2][21]==1,"reward success receipt differs or sent an extra client packet");
    check(word(reward_scene.packets[2],14,4)==42 && reward_services.game_logs.size()==1 && word(reward_services.game_logs[0],109,4)==42 && word(reward_services.game_logs[0],113,4)==items->find(5,1)->id && reward_services.game_logs[0][117]==1 && reward_services.game_logs[0][118]=='R',"reward money or item log differs");
    const auto before_failed_reward=reward_services.db.size();reward_actor->status().skills.max_bags=0;
    check(!reward_items.receive(5,1,1) && !reward_items.receive(0,0,1) && reward_services.db.size()==before_failed_reward && reward_scene.packets.size()==3 && item_names==1,"failed reward grant changed ownership or sent packets");
    reward_actor->status().skills.max_bags=1;
    auto capped_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,2,false,42)));
    Scene capped_scene;auto capped_actor=std::make_shared<Actor>(9063,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},capped_scene,ActorEquipmentRules{*items,enchant,mobs});
    capped_actor->account_id()=114;capped_actor->money()=std::numeric_limits<std::uint32_t>::max()-10;capped_actor->status().level=20;capped_actor->appearance().identity.job=0;
    auto capped_runtime=std::make_shared<QuestRuntime>(114,*pool);Services capped_services(*capped_actor,*capped_runtime,capped_scene);capped_services.check_success_order=false;
    VerticalQuestRewardBinding capped_owner(*capped_actor,*capped_runtime,*pool,capped_services);capped_services.reward_override=&capped_owner;capped_services.forbid_reward_remove=true;
    VerticalQuests capped_quests({items,item_pool,ids,capped_defs,pool,capped_actor,capped_runtime},{last,deadline},zone,npc_pool,capped_services);
    check(capped_quests.start(1,99),"capped money quest start failed");capped_quests.kill_mob(3,links);capped_quests.end(1,99);
    check(capped_actor->money()==std::numeric_limits<std::uint32_t>::max() && capped_services.money.money()==capped_actor->money() && capped_services.db==std::vector<unsigned>({0xa01,0xa05,0x603,0xa02}) && word(capped_scene.packets.back(),14,4)==42 && word(capped_services.game_logs.back(),109,4)==42,"capped quest reward money or advertised amount differs");
    // Complete another real quest through Actor::gain_experience, including
    // original modifier bypass and the live seven-member party bonus.
    storage::ResultSet exp_rows;exp_rows.columns={"Level","Grow","Exp"};
    for(unsigned grow=1;grow<=4;++grow) for(unsigned level=1;level<=60;++level) exp_rows.rows.push_back({std::to_string(level),std::to_string(grow),"1000"});
    const auto curve=Experience::load(exp_rows);storage::ResultSet status_rows;status_rows.columns={"Job","Level","HP","MP","SP"};const auto rules=StatusRules::load(status_rows);
    {
        SkillDefinitions lease_skills;ActorLoadRules lease_rules{rules,*items,enchant,lease_skills,*defs,mobs};
        PlayerSnapshot lease_snapshot;lease_snapshot.core.user_id=125;lease_snapshot.core.character.id=9090;
        lease_snapshot.core.character.hp=1;lease_snapshot.core.character.mp=1;lease_snapshot.core.character.sp=1;
        InventoryReplicaPool lease_replica_pool;auto replica=std::make_unique<CharacterReplica>(lease_snapshot.core,lease_replica_pool);
        auto lease_scene=std::make_shared<Scene>();std::weak_ptr<Scene> weak_scene=lease_scene;
        auto lease_character=std::make_shared<VerticalSessionCharacter>(VerticalPersistenceSelection{std::move(lease_snapshot),std::move(replica)},lease_rules,ActorLoadOptions{},*lease_scene);
        std::weak_ptr<VerticalSessionCharacter> weak_character=lease_character;
        auto* actual=lease_character->pipeline().result().actor.get();auto actor_lease=vertical_world_quest_actor_lease(lease_character,lease_scene);
        check(actor_lease.get()==actual && actual->id()==9090 && actual->account_id()==125,"quest Actor lease differs from loaded pipeline");
        lease_character.reset();lease_scene.reset();check(!weak_character.expired() && !weak_scene.expired(),"quest Actor lease lost character or scene");
        actor_lease.reset();check(weak_character.expired() && weak_scene.expired(),"quest Actor lease retained character or scene after release");
        bool missing_lease=false;try {(void)vertical_world_quest_actor_lease({},{});} catch(const std::invalid_argument&) {missing_lease=true;}
        check(missing_lease,"quest Actor lease accepted missing owners");
    }
    Scene exp_scene;auto exp_actor=std::make_shared<Actor>(9062,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},exp_scene,ActorEquipmentRules{*items,enchant,mobs});
    exp_actor->account_id()=113;exp_actor->status().level=20;exp_actor->appearance().identity.job=0;
    PlayerCore exp_player;exp_player.user_id=113;exp_player.character.id=9062;exp_player.character.level=20;exp_player.character.grow=1;
    struct ExpPorts final:ExperienceAwardPorts {
        Actor& actor;std::vector<net::Packet> database;CharacterReplicaDelivery* character_delivery=nullptr;std::vector<unsigned>* order=nullptr;explicit ExpPorts(Actor& value):actor(value) {}
        void send_database(const net::Packet& p) override {if(character_delivery) check(character_delivery->apply(p)==InventoryReplicaResult::applied,"character replica rejected experience");database.push_back(p);if(order) order->push_back(word(p,0,2));}
        void send_self(const net::Packet& p) override {actor.send_self(p);}
        void level_up(PlayerCore&,BaseStatus&,std::uint32_t,std::uint32_t) override {throw std::logic_error("unexpected level up");}
    } exp_ports(*exp_actor);
    unsigned option_queries=0;VerticalQuestRewardExperience exp_reward(*exp_actor,exp_player,rules,curve,exp_ports,[&]{++option_queries;return ExperienceAwardOptions{false,true,true,true,3};});
    auto exp_runtime=std::make_shared<QuestRuntime>(113,*pool);Services exp_services(*exp_actor,*exp_runtime,exp_scene);exp_services.check_success_order=false;exp_services.experience_reward=&exp_reward;
    VerticalQuestRewardBinding exp_owner(*exp_actor,*exp_runtime,*pool,exp_services);exp_services.reward_override=&exp_owner;exp_services.forbid_reward_remove=true;
    VerticalQuests exp_quests({items,item_pool,ids,restored_defs,pool,exp_actor,exp_runtime},{last,deadline},zone,npc_pool,exp_services);
    PickupWorld pickup_services;VerticalQuestPickupWorld pickup(*exp_actor,exp_quests,pickup_services);MapGeometry pickup_geometry(160);ZoneItems pickup_zone(pickup_geometry,pickup_services);
    ItemDefinition quest_item;quest_item.type=5;quest_item.type_id=1;quest_item.real_type=27;GroundItem ground;ground.id=123;ground.item.type=5;ground.definition=&quest_item;ground.position={10,0,10};
    check(pickup_zone.enter(ground,1),"quest pickup item entry failed");const auto pickup_links=linked_item();std::int32_t error=-1;
    const auto locks_before_pickup=exp_services.lock_calls;
    check(!pickup_zone.pick(*exp_actor,123,error,true,pickup_links,pickup) && error==2 && pickup_zone.size()==1,"inactive quest item pickup accepted");
    check(exp_services.lock_calls==locks_before_pickup+1 && exp_services.locks==0,"inactive quest pickup did not lock and unlock lookup");
    bool wrong_pickup=false;try {VerticalQuestPickupWorld bad(*actor,exp_quests,pickup_services);} catch(const std::invalid_argument&) {wrong_pickup=true;}
    check(wrong_pickup,"pickup accepted wrong quest actor");
    check(exp_quests.start(1,99),"experience quest start failed");error=-1;
    const auto locks_before_active_pickup=exp_services.lock_calls;
    check(pickup_zone.pick(*exp_actor,123,error,true,pickup_links,pickup)==&ground && error==-1 && pickup_zone.size()==0 && pickup_services.queries==0,"active live quest did not authorize real zone pickup");
    check(exp_services.lock_calls==locks_before_active_pickup+1 && exp_services.locks==0,"active quest pickup did not lock and unlock lookup");
    exp_quests.kill_mob(3,links);exp_quests.end(1,99);
    const auto locks_before_finished=exp_services.lock_calls;
    check(!exp_quests.active(1) && exp_services.lock_calls==locks_before_finished+1 && exp_services.locks==0,"finished quest remained eligible or skipped lookup lock");
    exp_actor->account_id()=114;bool wrong_pickup_account=false;
    try {pickup.pick_quest_active(*exp_actor,1);} catch(const std::logic_error&) {wrong_pickup_account=true;}
    check(wrong_pickup_account && exp_services.lock_calls==locks_before_finished+1 && exp_services.locks==0,"changed pickup account reached quest lock");exp_actor->account_id()=113;
    check(exp_player.experience==40 && option_queries==1 && exp_ports.database.size()==1 && word(exp_ports.database[0],0,2)==0x602 && word(exp_ports.database[0],6,4)==40,"real quest experience or party bonus differs");
    check(exp_scene.packets.size()==4 && word(exp_scene.packets[2],0,2)==0x207 && word(exp_scene.packets[2],2,4)==20 && word(exp_scene.packets[2],6,4)==20 && word(exp_scene.packets[3],0,2)==0x903 && exp_runtime->active().empty(),"quest experience client ordering differs");
    exp_player.user_id=114;bool wrong_exp_owner=false;try {exp_reward.award(0,1,true);} catch(const std::invalid_argument&) {wrong_exp_owner=true;}
    check(wrong_exp_owner && option_queries==1 && exp_ports.database.size()==1,"changed experience owner was accepted");
    exp_player.user_id=113;VerticalQuestRewardExperience changing_options(*exp_actor,exp_player,rules,curve,exp_ports,[&]{exp_player.user_id=114;return ExperienceAwardOptions{};});
    bool changed_during_options=false;try {changing_options.award(0,1,true);} catch(const std::invalid_argument&) {changed_during_options=true;}
    check(changed_during_options && exp_player.experience==40 && exp_ports.database.size()==1,"experience options rebound the player before award");exp_player.user_id=113;
    // One reward composition delivers real experience, money, inventory and
    // quest removal to the same Actor and their actual replica owners.
    Scene combined_scene;auto combined_actor=std::make_shared<Actor>(9064,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},combined_scene,ActorEquipmentRules{*items,enchant,mobs});
    combined_actor->account_id()=115;combined_actor->money()=7;combined_actor->status().level=20;combined_actor->status().skills.max_bags=1;combined_actor->appearance().identity.job=0;
    auto combined_runtime=std::make_shared<QuestRuntime>(115,*pool);Services combined_services(*combined_actor,*combined_runtime,combined_scene);combined_services.check_success_order=false;
    PlayerCore combined_player;combined_player.user_id=115;combined_player.character.id=9064;combined_player.character.level=20;combined_player.character.grow=1;
    combined_player.money=7;InventoryReplicaPool combined_cache_pool;CharacterReplica combined_cache(combined_player,combined_cache_pool);std::recursive_mutex combined_cache_lock;CharacterReplicaDelivery combined_delivery(combined_cache,combined_cache_lock);
    std::vector<unsigned> combined_database_order;combined_services.character_delivery=&combined_delivery;combined_services.on_database=[&](const net::Packet& packet){combined_database_order.push_back(word(packet,0,2));};
    ExpPorts combined_exp_ports(*combined_actor);combined_exp_ports.character_delivery=&combined_delivery;combined_exp_ports.order=&combined_database_order;
    VerticalQuestRewardExperience combined_exp(*combined_actor,combined_player,rules,curve,combined_exp_ports,[]{return ExperienceAwardOptions{};});
    QuestStartItemsActor combined_grants({items,item_pool,ids,combined_actor},combined_services);
    VerticalQuestRewardItems combined_items(*combined_actor,combined_grants,[](const ItemDefinition&){return std::string("Reward");});
    bool wrong_combined_exp=false,wrong_combined_item=false;
    try {VerticalQuestRewardComposition bad(*combined_actor,exp_reward,combined_items,combined_services);} catch(const std::invalid_argument&) {wrong_combined_exp=true;}
    try {VerticalQuestRewardComposition bad(*combined_actor,combined_exp,reward_items,combined_services);} catch(const std::invalid_argument&) {wrong_combined_item=true;}
    check(wrong_combined_exp && wrong_combined_item && combined_exp_ports.database.empty() && combined_services.db.empty(),"mixed actor reward components were accepted");
    VerticalQuestRewardComposition combined_rewards(*combined_actor,combined_exp,combined_items,combined_services);
    Scene stale_scene;Actor stale_actor(combined_actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},stale_scene,ActorEquipmentRules{*items,enchant,mobs});stale_actor.account_id()=115;
    VerticalQuestRewardServices& erased_rewards=combined_rewards;bool stale_binding_rejected=false;
    try {VerticalQuestRewardBinding bad(stale_actor,*combined_runtime,*pool,erased_rewards);} catch(const std::invalid_argument&) {stale_binding_rejected=true;}
    check(stale_binding_rejected && stale_scene.packets.empty() && combined_exp_ports.database.empty() && combined_services.db.empty(),"reward binding accepted another actor with the same account and character ID");
    combined_services.forbid_reward_world=true;combined_services.forbid_reward_remove=true;
    VerticalNpcInteraction combined_interaction;PacketActions combined_actions(*combined_actor);NpcPacketActions combined_npc_actions(*combined_actor);
    auto wrong_combined_player=combined_player;wrong_combined_player.user_id=999;bool wrong_bundle_player=false;
    try {VerticalQuestSession bad({items,item_pool,ids,reward_defs,pool,combined_actor,combined_runtime},{last,deadline},zone,npc_pool,combined_services,wrong_combined_player,rules,curve,combined_exp_ports,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},combined_interaction,combined_actions,combined_npc_actions);} catch(const std::invalid_argument&) {wrong_bundle_player=true;}
    check(wrong_bundle_player && combined_services.db.empty() && combined_exp_ports.database.empty(),"quest session accepted mismatched player or published during construction");
    auto combined_session=std::make_shared<VerticalQuestSession>(QuestStartItemsActorWorldOwners{items,item_pool,ids,reward_defs,pool,combined_actor,combined_runtime},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,combined_services,combined_player,rules,curve,combined_exp_ports,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},combined_interaction,combined_actions,combined_npc_actions);
    auto& combined_quests=combined_session->quests();auto& combined_packets=combined_session->packets();
    struct SessionResolver final:VerticalQuestSessionResolver {
        std::shared_ptr<VerticalQuestSession> session;
        std::shared_ptr<VerticalQuestSession> retain(Actor&) override {return session;}
    } session_resolver;
    session_resolver.session=combined_session;VerticalQuestSessionZoneUsers session_zone_users(session_resolver);
    bool wrong_zone_session=false;try {(void)session_zone_users.retain_quests(*actor);} catch(const std::logic_error&) {wrong_zone_session=true;}
    check(wrong_zone_session && combined_services.db.empty(),"zone session resolver accepted another actor");
    auto retained_quests=session_zone_users.retain_quests(*combined_actor);std::weak_ptr<VerticalQuestSession> weak_combined=combined_session;
    session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired() && retained_quests.get()==&combined_quests,"quest alias did not retain composed session");
    combined_session=weak_combined.lock();session_resolver.session=combined_session;retained_quests.reset();
    ZoneUsers combined_users(1,1,0);combined_users.attach(*combined_actor,0,0,{});
    FinishedEntryLock combined_finished_lock(*combined_actor);
    bool wrong_entry_session=false;try {(void)vertical_quests_session_send_entry_step(*actor,CharacterEntryStep::quests,combined_finished_lock,session_resolver);} catch(const std::logic_error&) {wrong_entry_session=true;}
    check(wrong_entry_session && !vertical_quests_session_send_entry_step(*actor,CharacterEntryStep::items,combined_finished_lock,session_resolver),"entry accepted mismatched session or unrelated step");
    std::shared_ptr<VerticalQuestSession> recovered_session;
    combined_services.on_lock=[&]{session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired(),"quest entry lost session in callback");recovered_session=weak_combined.lock();};
    check(vertical_quests_session_send_entry_step(*combined_actor,CharacterEntryStep::quests,combined_finished_lock,session_resolver),"active quest entry not handled");
    combined_services.on_lock={};combined_session=std::move(recovered_session);session_resolver.session=combined_session;
    check(vertical_quests_session_send_entry_step(*combined_actor,CharacterEntryStep::finished_quests,combined_finished_lock,session_resolver) && combined_scene.packets.empty(),"empty combined quest entry differs");
    check(vertical_quests_session_receive(*combined_actor,net::Packet{4,14,99,0,0,0},session_resolver)==VerticalQuestNpcPacketResult::handled && combined_interaction.npc==replacement,"combined NPC selection failed");
    bool wrong_packet_session=false;try {(void)vertical_quests_session_receive(*actor,net::Packet{2,9,99,0,0,0,1,0},session_resolver);} catch(const std::logic_error&) {wrong_packet_session=true;}
    session_resolver.session.reset();bool missing_packet_session=false;try {(void)vertical_quests_session_receive(*combined_actor,net::Packet{2,9,99,0,0,0,1,0},session_resolver);} catch(const std::logic_error&) {missing_packet_session=true;}
    check(wrong_packet_session && missing_packet_session && combined_actions.calls.empty() && combined_runtime->active().empty(),"quest packet accepted missing or mismatched session");
    check(vertical_quests_session_receive(*combined_actor,net::Packet{1},session_resolver)==VerticalQuestNpcPacketResult::malformed && vertical_quests_session_receive(*combined_actor,net::Packet{1,2},session_resolver)==VerticalQuestNpcPacketResult::unhandled,"quest dispatch changed unrelated framing");
    session_resolver.session=combined_session;
    combined_actions.after_shop=[&]{session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired(),"quest packet lost session in callback");recovered_session=weak_combined.lock();};
    check(vertical_quests_session_receive(*combined_actor,net::Packet{2,9,99,0,0,0,1,0},session_resolver)==VerticalQuestNpcPacketResult::handled && combined_runtime->active().size()==1,"combined reward quest start failed");
    combined_actions.after_shop={};combined_session=std::move(recovered_session);session_resolver.session=combined_session;
    // Quest-item drop eligibility consults each actual party member's runtime.
    {
        ItemDefinition loot_definition=*items->find(5,1);loot_definition.real_type=27;loot_definition.quest_start=false;loot_definition.quality[0]=100;
        Scene loot_scene;auto loot_actor=std::make_shared<Actor>(9080,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},loot_scene,ActorEquipmentRules{*items,enchant,mobs});
        loot_actor->account_id()=121;loot_actor->status().level=20;loot_actor->status().skills.max_bags=1;auto loot_runtime=std::make_shared<QuestRuntime>(121,*pool);Services loot_services(*loot_actor,*loot_runtime,loot_scene);loot_services.check_success_order=false;
        PlayerCore loot_player;loot_player.user_id=121;loot_player.character.id=9080;loot_player.character.level=20;loot_player.character.grow=1;ExpPorts loot_exp(*loot_actor);
        VerticalNpcInteraction loot_npc;PacketActions loot_actions(*loot_actor);NpcPacketActions loot_npc_actions(*loot_actor);
        auto loot_session=std::make_shared<VerticalQuestSession>(QuestStartItemsActorWorldOwners{items,item_pool,ids,reward_defs,pool,loot_actor,loot_runtime},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,loot_services,loot_player,rules,curve,loot_exp,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},loot_npc,loot_actions,loot_npc_actions);
        MobLootUser loot_user;loot_user.id=loot_actor->id();MobLootUser combined_user;combined_user.id=combined_actor->id();
        struct LootActors final:VerticalQuestLootActorResolver {
            MobLootUser& one;MobLootUser& two;std::shared_ptr<Actor> a,b;bool wrong=false;unsigned lookups=0;
            LootActors(MobLootUser& x,MobLootUser& y,std::shared_ptr<Actor> first,std::shared_ptr<Actor> second):one(x),two(y),a(std::move(first)),b(std::move(second)) {}
            std::shared_ptr<Actor> retain(MobLootUser& user) override {++lookups;if(&user==&one)return wrong?b:a;if(&user==&two)return b;throw std::logic_error("unknown loot user");}
        } loot_actors(loot_user,combined_user,loot_actor,combined_actor);
        struct LootSessions final:VerticalQuestSessionResolver {
            Actor& a;Actor& b;std::shared_ptr<VerticalQuestSession> first,second;bool wrong=false;
            LootSessions(Actor& x,Actor& y,std::shared_ptr<VerticalQuestSession> one,std::shared_ptr<VerticalQuestSession> two):a(x),b(y),first(std::move(one)),second(std::move(two)) {}
            std::shared_ptr<VerticalQuestSession> retain(Actor& actor) override {if(&actor==&a)return wrong?second:first;if(&actor==&b)return second;throw std::logic_error("unknown loot session actor");}
        } loot_sessions(*loot_actor,*combined_actor,loot_session,combined_session);
        const auto loot_links=linked_item();VerticalQuestSessionLoot quest_loot(loot_actors,loot_sessions,loot_links);
        struct LootWorld final:MobLootWorld {
            MobLootItem item_value;unsigned delivers=0;MobLootUser* recipient=nullptr;MobLootParty* party=nullptr;std::function<void(const MobLootItem&,MobLootUser*,MobLootParty*)> on_deliver;
            LootWorld() {item_value.type=5;item_value.type_id=1;item_value.quality=100;item_value.real_type=27;}
            [[noreturn]] static void unexpected() {throw std::logic_error("unexpected loot service");}
            const MobLootItem* item(std::uint8_t type,std::uint8_t id) override {check(type==5 && id==1,"wrong quest drop item");return &item_value;}
            std::span<const std::uint16_t> end_quests(const MobLootItem&) override {unexpected();}
            bool has_quest(MobLootUser&,std::uint16_t) override {unexpected();}
            const MobLootItem* grade_item(std::uint16_t) override {unexpected();}const MobLootItem* sequence_item(std::uint16_t,std::uint32_t) override {unexpected();}
            void deliver(const MobLootItem& item,MobLootUser* user,MobLootParty* group,bool sequence) override {check(&item==&item_value && !sequence,"wrong quest drop delivery");++delivers;recipient=user;party=group;if(on_deliver) on_deliver(item,user,group);}
            void guild_ranking(MobLootUser&,std::uint16_t) override {unexpected();}void log_money(std::int32_t) override {unexpected();}void ground_money(std::uint32_t,std::int32_t) override {unexpected();}
            void give_money(MobLootUser&,std::int32_t) override {unexpected();}bool zone_item(MobLootUser&,MobLootUser*) override {unexpected();}
        } loot_world;
        VerticalQuestMobLootWorld quest_loot_world(loot_world,quest_loot);
        struct ZeroRandom final:RandomSource {std::int32_t next() override {return 0;}} loot_random;ServerRandom loot_server;
        MobLootContext loot_context;loot_context.quest_type=5;loot_context.quest_type_id=1;
        const auto first_loot_lock=loot_services.lock_calls;mob_loot(loot_context,&loot_user,nullptr,loot_random,loot_server,quest_loot_world);
        check(loot_world.delivers==0 && loot_actors.lookups==1 && loot_services.lock_calls==first_loot_lock+1 && loot_services.locks==0,"inactive quest item drop was accepted or skipped lock");
        MobLootParty loot_party;loot_party.members={&loot_user,&combined_user};const auto combined_loot_locks=combined_services.lock_calls;
        mob_loot(loot_context,&loot_user,&loot_party,loot_random,loot_server,quest_loot_world);
        check(loot_world.delivers==1 && loot_world.recipient==&loot_user && loot_world.party==&loot_party && loot_actors.lookups==3 && combined_services.lock_calls==combined_loot_locks+1 && combined_services.locks==0,"party quest item drop ignored another member's active quest");
        loot_world.on_deliver=[](const MobLootItem&,MobLootUser*,MobLootParty*){throw std::runtime_error("delivery unavailable");};
        bool delivery_failed=false;try {mob_loot(loot_context,&combined_user,nullptr,loot_random,loot_server,quest_loot_world);} catch(const std::runtime_error&) {delivery_failed=true;}
        check(delivery_failed,"loot world swallowed delivery failure");loot_world.on_deliver={};
        MobLootItem unlinked;unlinked.type=5;unlinked.type_id=99;check(quest_loot.end_quests(unlinked).empty(),"unknown loot item returned quest links");
        loot_actors.wrong=true;bool wrong_loot_actor=false;try {(void)quest_loot.has_quest(loot_user,1);} catch(const std::logic_error&) {wrong_loot_actor=true;}loot_actors.wrong=false;
        loot_sessions.wrong=true;bool wrong_loot_session=false;try {(void)quest_loot.has_quest(loot_user,1);} catch(const std::logic_error&) {wrong_loot_session=true;}
        check(wrong_loot_actor && wrong_loot_session && loot_world.delivers==2 && loot_runtime->active().empty(),"quest loot accepted mismatched owners");
        loot_sessions.wrong=false;check(loot_session->quests().start(1,99),"loot user's quest start failed");
        struct GroundWorld final:ItemPickupWorld,ZoneItemServices {
            Actor& actor;Services& services;GroundItemPool& pool;ZoneItems* zone=nullptr;unsigned logs=0;std::vector<net::Packet> views;
            GroundWorld(Actor& a,Services& s,GroundItemPool& p):actor(a),services(s),pool(p) {}
            [[noreturn]] static void unexpected() {throw std::logic_error("unexpected quest ground service");}
            std::uint32_t pick_tick_ms() override {return 100;}std::uint32_t pick_party_id(Actor& a) override {check(&a==&actor,"wrong pickup actor");return 0;}
            UserPosition pick_position(const Actor& a) override {check(&a==&actor,"wrong pickup position actor");return {10,0,10};}
            bool pick_quest_active(Actor&,std::uint16_t) override {unexpected();}
            std::uint32_t lock_inventory_party(Actor& a) override {check(&a==&actor,"wrong inventory actor");return 0;}
            void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}std::optional<bool> inventory_party_kind(const Actor&) override {return {};}
            void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}void send_inventory_database(const net::Packet& p) override {services.send_database(actor,p);}
            std::string inventory_item_name(const ItemDefinition&) override {return "Quest item";}std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();}
            bool return_inventory_item(Actor& a,GroundItem& item) override {check(&a==&actor && zone,"wrong returned item owner");return zone->enter(item,1);}
            void send_money_database(const net::Packet&) override {unexpected();}GameLogSubject money_log_subject(const Actor& a) override {check(&a==&actor,"wrong loot log actor");GameLogSubject s;s.account_id=a.account_id();s.character_id=a.id();s.account_name="quest";s.character_name="quest";return s;}
            std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/5};}
            void send_money_log(const net::Packet& p) override {check(word(p,0,2)==0x401,"wrong pickup log");++logs;}
            std::uint32_t tick_ms() override {return 100;}void send_cell_view(CellCoordinates,const net::Packet& p) override {views.push_back(p);}void release(GroundItem& item) override {pool.release(item);}
        } ground_world(*loot_actor,loot_services,*item_pool);
        MapGeometry ground_geometry(160);ZoneItems loot_ground(ground_geometry,ground_world);ground_world.zone=&loot_ground;
        struct DropServices final:MobDropDeliveryServices {
            ItemDefinition& definition_value;ItemIds& ids;Services& services;Actor& actor;
            DropServices(ItemDefinition& d,ItemIds& i,Services& s,Actor& a):definition_value(d),ids(i),services(s),actor(a) {}
            const ItemDefinition& definition(const MobLootItem&) override {return definition_value;}
            std::uint32_t make_time() override {return services.local_make_time(actor);}std::uint64_t raw_unique_id() override {return ids.raw_next();}
            MobDropInventoryTarget recipient(MobLootUser&,GroundItem&) override {throw std::logic_error("quest drop unexpectedly granted directly");}
            void sequence_log(GroundItem&,MobLootUser*) override {throw std::logic_error("unexpected sequence log");}
            void log_item(GroundItem&,MobLootUser*) override {throw std::logic_error("unexpected drop log");}
        } drop_services(loot_definition,*ids,loot_services,*loot_actor);
        loot_world.item_value.id=loot_definition.id;MobDropDeliveryResult actual_drop;
        loot_world.on_deliver=[&](const MobLootItem& item,MobLootUser* user,MobLootParty* party){actual_drop=deliver_mob_drop(&item,user,party,false,false,{{3,3,10,10},0},*item_pool,loot_ground,loot_random,drop_services);};
        mob_loot(loot_context,&loot_user,nullptr,loot_random,loot_server,quest_loot_world);
        check(actual_drop.destination==MobDropDestination::ground && actual_drop.item && actual_drop.item->user_id==loot_actor->id() && loot_ground.size()==1,"eligible quest drop did not reach actual zone");
        const auto dropped_id=actual_drop.item->id;const auto dropped_uid=actual_drop.item->item.uid;
        struct PickupActions final:VerticalInventoryActions {
            Actor& actor;std::string events;std::function<void()> on_cancel;explicit PickupActions(Actor& a):actor(a) {}
            void cancel_action_exchange(Actor& a) override {check(&a==&actor,"wrong exchange actor");events+='C';if(on_cancel) on_cancel();}
            void end_shop(Actor& a) override {check(&a==&actor,"wrong shop actor");events+='S';}
            bool pickup_checks() override {events+='P';return true;}
        } pickup_actions(*loot_actor);
        std::array<std::uint8_t,6> pickup_packet{0x05,0x02};
        for(unsigned i=0;i<4;++i) pickup_packet[2+i]=static_cast<std::uint8_t>(dropped_id>>(i*8));
        const auto truncated=vertical_quests_session_pickup_request(*loot_actor,std::span(pickup_packet).first(5),loot_ground,loot_links,ground_world,*item_pool,pickup_actions,loot_sessions);
        check(truncated.request==VerticalInventoryRequest::malformed && pickup_actions.events.empty() && loot_ground.size()==1,"truncated pickup affected quest drop");
        loot_sessions.wrong=true;bool wrong_pickup_session=false;
        try {(void)vertical_quests_session_pickup_request(*loot_actor,pickup_packet,loot_ground,loot_links,ground_world,*item_pool,pickup_actions,loot_sessions);} catch(const std::logic_error&) {wrong_pickup_session=true;}
        loot_sessions.wrong=false;loot_sessions.first.reset();bool missing_pickup_session=false;
        loot_actor->attack_state().mode=1;
        const auto ignored_pickup=vertical_quests_session_pickup_request(*loot_actor,pickup_packet,loot_ground,loot_links,ground_world,*item_pool,pickup_actions,loot_sessions);
        check(ignored_pickup.request==VerticalInventoryRequest::ignored && !ignored_pickup.receipt && pickup_actions.events.empty() && loot_ground.size()==1,"attack-state pickup gate resolved a missing session or caused effects");
        loot_actor->attack_state().mode=0;
        try {(void)vertical_quests_session_pickup_request(*loot_actor,pickup_packet,loot_ground,loot_links,ground_world,*item_pool,pickup_actions,loot_sessions);} catch(const std::logic_error&) {missing_pickup_session=true;}
        check(wrong_pickup_session && missing_pickup_session && pickup_actions.events.empty() && loot_ground.size()==1,"pickup accepted missing or mismatched quest session");
        loot_sessions.first=loot_session;std::weak_ptr<VerticalQuestSession> pickup_lease=loot_session;
        pickup_actions.on_cancel=[&]{loot_sessions.first.reset();loot_session.reset();check(!pickup_lease.expired(),"pickup lost quest session in callback");};
        const auto pickup=vertical_quests_session_pickup_request(*loot_actor,pickup_packet,loot_ground,loot_links,ground_world,*item_pool,pickup_actions,loot_sessions);
        pickup_actions.on_cancel={};check(pickup_lease.expired(),"pickup retained quest session after request");
        check(pickup.request==VerticalInventoryRequest::pickup && pickup.receipt==InventoryReceipt::stored && pickup_actions.events=="CSP" && loot_ground.size()==0,"quest item packet did not reach actual Actor inventory");
        const auto held_loot=loot_actor->retained_inventory_item(1,0);const auto stored_loot=loot_services.inventory.inventory();
        check(held_loot && held_loot->item->item.uid==dropped_uid && stored_loot.size()==1 && stored_loot.front().item.uid==dropped_uid && loot_services.db==std::vector<unsigned>({0xa01,0x701}) && ground_world.logs==1 && loot_runtime->active().front()->packet.counters[0]==0,"actual quest drop/pickup ownership, persistence or quest counter differs");
    }
    const auto combined_timer_locks=combined_services.lock_calls;vertical_quests_update_zone(combined_users,100,session_zone_users);
    check(combined_services.lock_calls>combined_timer_locks && combined_runtime->active().size()==1 && combined_services.locks==0,"zone timer did not use composed quest session");
    struct CreditedActorResolver final:VerticalQuestMobActorResolver {
        std::shared_ptr<Actor> actor;std::shared_ptr<Actor> retain(MobDeathUser&) override {return actor;}
    } credited_actor;credited_actor.actor=combined_actor;
    VerticalQuestSessionMobCredit mob_credit(credited_actor,session_resolver,links);DeathServices combined_death(combined_quests,combined_actor->id());VerticalQuestMobDeath combined_death_bridge(combined_death,mob_credit);
    MobRuntime combined_mob;combined_mob.type=3;combined_mob.definition=&mob_definition;combined_mob.damage=std::make_unique<MobDamageLedger>(damage.pool);
    Scene fake_scene;auto fake_credited=std::make_shared<Actor>(combined_actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},fake_scene,ActorEquipmentRules{*items,enchant,mobs});
    credited_actor.actor=fake_credited;bool stale_mob_actor=false;
    try {mob_credit.apply(combined_death.user,combined_mob);} catch(const std::logic_error&) {stale_mob_actor=true;}
    check(stale_mob_actor && combined_runtime->active().front()->packet.counters[0]==0,"mob credit accepted another actor with the same ID");credited_actor.actor=combined_actor;
    session_resolver.session.reset();bool missing_mob_session=false;
    try {mob_credit.apply(combined_death.user,combined_mob);} catch(const std::logic_error&) {missing_mob_session=true;}
    check(missing_mob_session && combined_runtime->active().front()->packet.counters[0]==0,"mob credit accepted a missing quest session");session_resolver.session=combined_session;
    combined_mob.damage->add(1,combined_actor->id(),10,0,0);combined_services.on_progress=[&]{combined_death.events.push_back('P');};
    mob_death(combined_mob,1,combined_actor->id(),MobDeathExperience{},combined_death_bridge);combined_services.on_progress={};
    check(combined_death.events==std::vector<char>({'E','D','P','C'}) && combined_runtime->active().front()->packet.counters[0]==1,"mob death did not credit the retained quest session");
    bool wrong_save_session=false;try {vertical_quests_session_save_runtime(*actor,100,session_resolver);} catch(const std::logic_error&) {wrong_save_session=true;}
    check(wrong_save_session,"quest save accepted mismatched session");
    auto prior_database=combined_services.on_database;
    combined_services.on_database=[&](const net::Packet& packet){prior_database(packet);session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired(),"quest save lost session in callback");recovered_session=weak_combined.lock();};
    vertical_quests_session_save_runtime(*combined_actor,100,session_resolver);combined_services.on_database=std::move(prior_database);combined_session=std::move(recovered_session);session_resolver.session=combined_session;
    check(combined_cache.quests().active().size()==1 && combined_cache.quests().active().front().counters[0]==1,"combined quest save did not reach character replica");
    check(vertical_quests_session_receive(*combined_actor,end_packet,session_resolver)==VerticalQuestNpcPacketResult::handled,"combined reward completion packet failed");
    NpcMoveServices combined_move(*combined_actor);bool wrong_move_session=false;
    try {vertical_quests_session_move_interactions(*actor,false,combined_move,session_resolver);} catch(const std::logic_error&) {wrong_move_session=true;}
    check(wrong_move_session && combined_interaction.npc==replacement && replacement->users_using==1,"movement accepted mismatched session");
    combined_move.on_pvp=[&]{session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired(),"movement lost quest session in callback");recovered_session=weak_combined.lock();};
    vertical_quests_session_move_interactions(*combined_actor,false,combined_move,session_resolver);combined_move.on_pvp={};combined_session=std::move(recovered_session);session_resolver.session=combined_session;
    check(!combined_interaction.npc && replacement->users_using==0,"combined NPC interaction cleanup failed");
    check(vertical_quests_session_receive(*combined_actor,net::Packet{4,14,99,0,0,0},session_resolver)==VerticalQuestNpcPacketResult::handled && combined_interaction.npc==replacement,"NPC selection before leave failed");
    ZoneLeaveShopState combined_shop;NpcLeaveServices combined_leave(*combined_actor,combined_interaction,*replacement,0);
    session_resolver.session.reset();bool missing_leave_session=false;try {vertical_quests_session_leave_interactions(*combined_actor,combined_shop,combined_leave,session_resolver);} catch(const std::logic_error&) {missing_leave_session=true;}
    check(missing_leave_session && combined_interaction.npc==replacement && replacement->users_using==1 && combined_leave.shutdowns==0,"leave accepted missing session");
    session_resolver.session=combined_session;combined_leave.on_shutdown=[&]{session_resolver.session.reset();combined_session.reset();check(!weak_combined.expired(),"leave lost quest session in callback");recovered_session=weak_combined.lock();};
    vertical_quests_session_leave_interactions(*combined_actor,combined_shop,combined_leave,session_resolver);combined_leave.on_shutdown={};combined_session=std::move(recovered_session);session_resolver.session=combined_session;
    check(!combined_interaction.npc && replacement->users_using==0 && combined_leave.shutdowns==1,"combined leave cleanup failed");
    check(combined_player.experience==20 && combined_exp_ports.database.size()==1 && word(combined_exp_ports.database[0],0,2)==0x602 && combined_actor->money()==49 && combined_services.money.money()==49 && combined_actor->retained_inventory_item(1,0) && combined_services.inventory.inventory().size()==1 && combined_runtime->active().empty() && combined_services.replica.active().empty(),"combined actual reward owners diverged");
    check(combined_services.db==std::vector<unsigned>({0xa01,0xa05,0xa04,0x603,0x701,0xa02}) && combined_database_order==std::vector<unsigned>({0xa01,0xa05,0xa04,0x602,0x603,0x701,0xa02}) && combined_scene.packets.size()==4 && word(combined_scene.packets[2],0,2)==0x207 && word(combined_scene.packets[3],0,2)==0x903 && combined_services.game_logs.size()==1,"combined reward event ordering differs");
    check(combined_cache.core().experience==20 && combined_cache.core().money==49 && combined_cache.inventory().inventory().size()==1 && combined_cache.quests().active().empty() && combined_cache.quests().finished().size()==1,"combined character replica diverged from live owners");
    const auto combined_plan=quest_save_plan(combined_cache.quests(),combined_actor->id());
    check(combined_plan.size()==1 && combined_plan.front().procedure=="usp_Save_Char_FinishedQuest_E" && combined_plan.front().parameters==std::vector<storage::Parameter>({std::int64_t{9064},std::int64_t{1},std::int64_t{1},std::int64_t{1}}),"combined finished quest procedure plan differs");
    check(combined_users.detach(combined_actor->id()),"combined quest user detach failed");
    // Real party death traversal now resolves three independent composed quest
    // sessions, with each credited session retained through its callback.
    {
        std::array<PlayerCore,3> party_players;std::array<VerticalNpcInteraction,3> party_npc_interactions;
        std::array<std::unique_ptr<PacketActions>,3> party_actions;std::array<std::unique_ptr<NpcPacketActions>,3> party_npc_actions;
        std::array<std::unique_ptr<ExpPorts>,3> party_experience_ports;std::array<std::shared_ptr<VerticalQuestSession>,3> party_sessions;
        for(std::size_t i=0;i<party_sessions.size();++i) {
            party_players[i].user_id=party_actors[i]->account_id();party_players[i].character.id=party_actors[i]->id();party_players[i].character.level=20;party_players[i].character.grow=1;
            party_actions[i]=std::make_unique<PacketActions>(*party_actors[i]);party_npc_actions[i]=std::make_unique<NpcPacketActions>(*party_actors[i]);party_experience_ports[i]=std::make_unique<ExpPorts>(*party_actors[i]);
            party_sessions[i]=std::make_shared<VerticalQuestSession>(QuestStartItemsActorWorldOwners{items,item_pool,ids,defs,pool,party_actors[i],party_runtimes[i]},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,*party_ports[i],party_players[i],rules,curve,*party_experience_ports[i],[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},party_npc_interactions[i],*party_actions[i],*party_npc_actions[i]);
        }
        struct PartyActorResolver final:VerticalQuestMobActorResolver {
            PartyDeathServices& death;std::array<std::shared_ptr<Actor>,3>& actors;
            PartyActorResolver(PartyDeathServices& d,std::array<std::shared_ptr<Actor>,3>& a):death(d),actors(a) {}
            std::shared_ptr<Actor> retain(MobDeathUser& user) override {check(death.locked,"party actor resolved outside lock");for(std::size_t i=0;i<actors.size();++i) if(&user==&death.users[i]) return actors[i];throw std::logic_error("unknown credited party actor");}
        } party_actor_resolver(party_death,party_actors);
        struct PartySessionResolver final:VerticalQuestSessionResolver {
            PartyDeathServices& death;std::array<std::shared_ptr<Actor>,3>& actors;std::array<std::shared_ptr<VerticalQuestSession>,3>& sessions;bool wrong=false;
            PartySessionResolver(PartyDeathServices& d,std::array<std::shared_ptr<Actor>,3>& a,std::array<std::shared_ptr<VerticalQuestSession>,3>& s):death(d),actors(a),sessions(s) {}
            std::shared_ptr<VerticalQuestSession> retain(Actor& actor) override {check(death.locked,"party quest session resolved outside lock");for(std::size_t i=0;i<actors.size();++i) if(&actor==actors[i].get()) return sessions[wrong && i==1?0:i];throw std::logic_error("unknown party session actor");}
        } party_session_resolver(party_death,party_actors,party_sessions);
        VerticalQuestSessionMobCredit party_session_credit(party_actor_resolver,party_session_resolver,links);VerticalQuestMobDeath party_death_bridge(party_death,party_session_credit);party_death.wrong_owner=false;
        party_death.events.clear();party_session_resolver.wrong=true;party_mob.damage->add(1,party_death.users[0].id,10,0,0);bool rejected_session=false;
        try {mob_death(party_mob,1,party_death.users[0].id,MobDeathExperience{},party_death_bridge);} catch(const std::logic_error&) {rejected_session=true;}
        check(rejected_session && !party_death.locked && party_death.events==std::vector<char>({'[','R','E','P','R','E',']'}) && party_runtimes[0]->active().front()->packet.counters[0]==3 && party_runtimes[1]->active().front()->packet.counters[0]==1,"wrong composed party session changed another quest or leaked lock");
        party_session_resolver.wrong=false;std::weak_ptr<VerticalQuestSession> first_party_lease=party_sessions[0];
        party_ports[0]->on_progress=[&]{party_sessions[0].reset();check(!first_party_lease.expired(),"credited quest session was not retained through progress callback");party_death.events.push_back('P');};
        party_death.events.clear();party_mob.damage->add(1,party_death.users[0].id,10,0,0);
        mob_death(party_mob,1,party_death.users[0].id,MobDeathExperience{},party_death_bridge);
        check(!party_death.locked && first_party_lease.expired() && party_death.events==std::vector<char>({'[','R','E','P','R','E','P','R','D',']','C'}),"composed party credit ordering or session lease differs");
        check(party_runtimes[0]->active().front()->packet.counters[0]==4 && party_runtimes[1]->active().front()->packet.counters[0]==2 && party_runtimes[2]->active().front()->packet.counters[0]==0 && party_ports[0]->replica.active().front().counters[0]==4 && party_ports[1]->replica.active().front().counters[0]==2,"composed party quest credit diverged from replicas");
        party_ports[0]->on_progress={};party_death.credit=nullptr;
    }
    // KillPCTo must retain the killer's composed quest session through both
    // quest progress and the real owner's required bookkeeping continuation.
    Scene pc_session_scene;auto pc_session_actor=std::make_shared<Actor>(9070,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},pc_session_scene,ActorEquipmentRules{*items,enchant,mobs});
    pc_session_actor->account_id()=120;pc_session_actor->status().level=20;pc_session_actor->appearance().identity.job=0;
    auto pc_session_runtime=std::make_shared<QuestRuntime>(120,*pool);Services pc_session_services(*pc_session_actor,*pc_session_runtime,pc_session_scene);pc_session_services.check_success_order=false;
    PlayerCore pc_session_player;pc_session_player.user_id=120;pc_session_player.character.id=9070;pc_session_player.character.level=20;pc_session_player.character.grow=1;
    ExpPorts pc_session_experience(*pc_session_actor);VerticalNpcInteraction pc_session_interaction;PacketActions pc_session_actions(*pc_session_actor);NpcPacketActions pc_session_npc_actions(*pc_session_actor);
    auto pc_session=std::make_shared<VerticalQuestSession>(QuestStartItemsActorWorldOwners{items,item_pool,ids,pc_defs,pool,pc_session_actor,pc_session_runtime},QuestStartItemsActorUseState{last,deadline},zone,npc_pool,pc_session_services,pc_session_player,rules,curve,pc_session_experience,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},pc_session_interaction,pc_session_actions,pc_session_npc_actions);
    check(pc_session->quests().start(1,99),"session player-kill quest start failed");
    struct PcSessionResolver final:VerticalQuestSessionResolver {std::shared_ptr<VerticalQuestSession> session;std::shared_ptr<VerticalQuestSession> retain(Actor&) override {return session;}} pc_session_resolver;
    struct PcSessionContinuation final:VerticalQuestKillPcContinuation {
        Actor& killer;Actor& victim;QuestRuntime& runtime;Services& services;std::weak_ptr<VerticalQuestSession>& session;unsigned calls=0;
        PcSessionContinuation(Actor& k,Actor& v,QuestRuntime& r,Services& s,std::weak_ptr<VerticalQuestSession>& lease):killer(k),victim(v),runtime(r),services(s),session(lease) {}
        void after_quest(Actor& k,Actor& v) override {check(&k==&killer && &v==&victim && !session.expired() && runtime.active().front()->packet.counters[0]==1 && services.replica.active().front().counters[0]==1,"player-kill continuation preceded progress or lost owner");++calls;}
    };
    std::weak_ptr<VerticalQuestSession> weak_pc_session=pc_session;PcSessionContinuation pc_continuation(*pc_session_actor,*actor,*pc_session_runtime,pc_session_services,weak_pc_session);
    bool missing_pc_session=false;try {vertical_quests_session_kill_pc_to(*pc_session_actor,*actor,pc_session_resolver,pc_continuation);} catch(const std::logic_error&) {missing_pc_session=true;}
    pc_session_resolver.session=pc_session;Scene stale_pc_scene;Actor stale_pc_actor(pc_session_actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},stale_pc_scene,ActorEquipmentRules{*items,enchant,mobs});stale_pc_actor.account_id()=120;
    bool stale_pc_session=false;try {vertical_quests_session_kill_pc_to(stale_pc_actor,*actor,pc_session_resolver,pc_continuation);} catch(const std::logic_error&) {stale_pc_session=true;}
    check(missing_pc_session && stale_pc_session && pc_session_runtime->active().front()->packet.counters[0]==0 && pc_continuation.calls==0,"invalid player-kill session changed quest state");
    pc_session_services.on_progress=[&]{pc_session_resolver.session.reset();pc_session.reset();check(!weak_pc_session.expired(),"player-kill session lost during quest progress");};
    vertical_quests_session_kill_pc_to(*pc_session_actor,*actor,pc_session_resolver,pc_continuation);pc_session_services.on_progress={};
    check(pc_continuation.calls==1 && weak_pc_session.expired() && pc_session_services.db==std::vector<unsigned>({0xa01,0xa05}) && pc_session_services.locks==0,"player-kill session lease or database order differs");
    // Exercise the actual selected-world-session preparation and its existing
    // registry/IO fences; no entry or gameplay service reports fake success.
    {
        asio::io_context io;auto work=asio::make_work_guard(io);auto strand=asio::make_strand(io);asio::thread_pool database(1);std::jthread worker([&]{io.run();});
        auto login_registry=std::make_shared<session::Registry>();session::LoginSession login{};login.user_id=140;login.session_id=1;
        login_registry->add(login);check(login_registry->select(140,1,0,1),"world claim preparation failed");
        auto claim=login_registry->claim_world(140,login.token,0,1,2);check(claim.session.has_value(),"world claim missing");
        VerticalWorldSession::Registry world_registry;world_registry.bind_network_registry(login_registry,0);QuestAdmissionFixture admission_services;WorldAdmission admission(admission_services);
        MapGeometry pickup_geometry(81);RuntimeZone world_zone;world_zone.users=std::make_unique<ZoneUsers>(1,1,0);world_zone.npcs=std::make_unique<ZoneNpcs>(pickup_geometry,npc_services);
        RuntimeZone destination_zone;destination_zone.users=std::make_unique<ZoneUsers>(1,1,0);destination_zone.npcs=std::make_unique<ZoneNpcs>(pickup_geometry,npc_services);
        struct WorldPickupActions final:VerticalInventoryActions {std::string events;void cancel_action_exchange(Actor&) override {events+='C';}void end_shop(Actor&) override {events+='S';}bool pickup_checks() override {events+='P';return true;}};
        struct WorldPickupWorld final:ItemPickupWorld,ZoneItemServices {
            GroundItemPool& pool;Actor* actor=nullptr;CharacterReplicaDelivery* database=nullptr;ZoneItems* zone=nullptr;unsigned logs=0;
            explicit WorldPickupWorld(GroundItemPool& p):pool(p) {}[[noreturn]] static void unexpected() {throw std::logic_error("unexpected world pickup service");}
            std::uint32_t pick_tick_ms() override {return 100;}std::uint32_t pick_party_id(Actor& a) override {check(&a==actor,"wrong pickup actor");return 0;}
            UserPosition pick_position(const Actor& a) override {check(&a==actor,"wrong pickup position actor");return {10,0,10};}bool pick_quest_active(Actor&,std::uint16_t) override {unexpected();}
            std::uint32_t lock_inventory_party(Actor& a) override {check(&a==actor,"wrong pickup inventory actor");return 0;}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
            std::optional<bool> inventory_party_kind(const Actor&) override {return {};}void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}
            void send_inventory_database(const net::Packet& p) override {check(database && database->apply(p)==InventoryReplicaResult::applied,"pickup replica rejected item");}
            std::string inventory_item_name(const ItemDefinition&) override {return "Quest item";}std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();}
            bool return_inventory_item(Actor& a,GroundItem& item) override {check(&a==actor && zone,"wrong returned item");return zone->enter(item,1);}
            void send_money_database(const net::Packet&) override {unexpected();}GameLogSubject money_log_subject(const Actor& a) override {check(&a==actor,"wrong pickup log actor");GameLogSubject s;s.account_id=a.account_id();s.character_id=a.id();s.account_name="quest";s.character_name="quest";return s;}
            std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/5};}void send_money_log(const net::Packet& p) override {check(word(p,0,2)==0x401,"wrong pickup log");++logs;}
            std::uint32_t tick_ms() override {return 100;}void send_cell_view(CellCoordinates,const net::Packet&) override {}void release(GroundItem& item) override {pool.release(item);}
        };
        struct WorldPickupFixture {WorldPickupWorld world;ItemQuestLinks links;std::shared_ptr<GroundItemPool> pool;ItemDefinition definition;WorldPickupActions actions;
            WorldPickupFixture(std::shared_ptr<GroundItemPool> p,const ItemDefinition& item):world(*p),links(linked_item()),pool(std::move(p)),definition(item) {definition.real_type=27;definition.quest_start=false;}};
        auto pickup_fixture=std::make_shared<WorldPickupFixture>(item_pool,*items->find(5,1));world_zone.items=std::make_unique<ZoneItems>(pickup_geometry,pickup_fixture->world);pickup_fixture->world.zone=world_zone.items.get();
        auto* world_npc=npc_pool.allocate();world_npc->id=99;world_npc->type=2;world_npc->type_id=9;world_npc->definition=&replacement_definition;check(world_zone.npcs->enter(*world_npc),"world quest NPC entry failed");
        auto* destination_npc=npc_pool.allocate();destination_npc->id=102;destination_npc->type=2;destination_npc->type_id=9;destination_npc->definition=&replacement_definition;check(destination_zone.npcs->enter(*destination_npc),"destination quest NPC entry failed");
        auto world_defs=std::make_shared<QuestDefinitions>(QuestDefinitions::decode(quest_data(false,false,2,true,42,true)));
        SkillDefinitions world_skills;ActorLoadRules world_rules{rules,*items,enchant,world_skills,*world_defs,mobs};std::promise<void> ready;auto ready_future=ready.get_future();
        QuestWorldSessionFixture world_services(world_rules,ready);world_services.schedule=[&](std::function<void()> done){asio::post(strand,std::move(done));};std::shared_ptr<VerticalWorldSession> live;std::shared_ptr<VerticalQuestSession> pinned;std::unique_ptr<VerticalWorldSessionPacketRouter> packet_router;
        std::shared_ptr<QuestRuntime> world_runtime,save_pin;std::unique_ptr<Services> world_quest_ports;std::unique_ptr<VerticalWorldQuestReplicaPorts> replica_ports;std::unique_ptr<ExpPorts> world_experience;std::unique_ptr<CharacterReplicaDelivery> world_experience_delivery;std::unique_ptr<PacketActions> world_actions;std::unique_ptr<NpcPacketActions> world_npc_actions;
        PlayerCore world_player;VerticalNpcInteraction world_interaction;VerticalWorldQuestSessions world_quests(world_registry);VerticalWorldQuestPacketDispatch quest_dispatch(world_registry);VerticalWorldPrimaryQuestServices world_phase(world_registry);std::weak_ptr<void> weak_io;auto world_replica_mutex=std::make_shared<std::recursive_mutex>();std::weak_ptr<std::recursive_mutex> weak_replica_mutex=world_replica_mutex;std::uint8_t world_last=0;std::uint32_t world_deadline=0;
        unsigned pickup_lookups=0;bool pickup_incomplete=false,pickup_mutates=false;VerticalWorldQuestPickupDispatch pickup_dispatch(world_registry,[&](VerticalWorldSession& owner,RuntimeZone& z){check(&owner==live.get() && &z==&world_zone,"wrong pickup owners");++pickup_lookups;if(pickup_mutates)owner.world_user()->actor.attack_state().mode=1;return VerticalWorldQuestPickupOwners{pickup_fixture,&pickup_fixture->links,pickup_incomplete?nullptr:&pickup_fixture->world,pickup_fixture->pool.get(),&pickup_fixture->actions};});
        net::WorldChannel channel;channel.retain_io=[&]{auto p=std::make_shared<int>(1);weak_io=p;return p;};channel.pause_input=[](auto done){done(true);};channel.resume_input=[]{};channel.set_admission_dispatch=[](bool){};
        unsigned dispatch_disconnects=0;channel.disconnect=[&]{++dispatch_disconnects;};channel.disconnect_with_reason=[](std::uint32_t){throw std::logic_error("unexpected test disconnect reason");};
        world_services.on_prepare=[&](VerticalWorldSession& owner){
            auto probe=owner.quest_actor_lease();probe->account_id()=141;bool wrong_restore=false;
            try {(void)vertical_world_quests_restore(owner,*world_replica_mutex,100,pool,world_defs);} catch(const std::logic_error&) {wrong_restore=true;}
            probe->account_id()=140;check(wrong_restore,"world quest restore accepted mismatched actor account");probe.reset();
            auto restored=vertical_world_quests_restore(owner,*world_replica_mutex,100,pool,world_defs);auto world_actor=std::move(restored.actor);world_runtime=std::move(restored.quests.restored.runtime);
            check(world_actor.get()==&owner.world_user()->actor && restored.quests.invalid_active.empty() && world_runtime->active().size()==1 && world_runtime->active().front()->packet.counters[0]==1,"prepared quest restore did not use retained replica");
            world_quest_ports=std::make_unique<Services>(*world_actor,*world_runtime,*world_services.scene);world_quest_ports->check_success_order=false;
            replica_ports=std::make_unique<VerticalWorldQuestReplicaPorts>(*world_actor,owner.character()->replica(),world_replica_mutex,*world_quest_ports);
            world_player=owner.character()->snapshot().core;world_experience=std::make_unique<ExpPorts>(*world_actor);world_experience_delivery=std::make_unique<CharacterReplicaDelivery>(owner.character()->replica(),*world_replica_mutex);world_experience->character_delivery=world_experience_delivery.get();world_actions=std::make_unique<PacketActions>(*world_actor);world_npc_actions=std::make_unique<NpcPacketActions>(*world_actor);
            pickup_fixture->world.actor=world_actor.get();pickup_fixture->world.database=world_experience_delivery.get();
            auto make_quests=[&](std::shared_ptr<Actor> a){return std::make_unique<VerticalQuestSession>(QuestStartItemsActorWorldOwners{items,item_pool,ids,world_defs,pool,std::move(a),world_runtime},QuestStartItemsActorUseState{world_last,world_deadline},*world_zone.npcs,npc_pool,*replica_ports,world_player,rules,curve,*world_experience,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},world_interaction,*world_actions,*world_npc_actions);};
            auto cyclic=std::shared_ptr<Actor>(owner.shared_from_this(),world_actor.get());bool cycle_rejected=false;
            try {owner.install_quests(make_quests(std::move(cyclic)));} catch(const std::logic_error&) {cycle_rejected=true;}
            check(cycle_rejected,"quest install accepted session-owned actor alias");
            bool wrong_ports=false;try {owner.install_quests(make_quests(world_actor),std::make_unique<VerticalWorldQuestReplicaPorts>(*world_actor,owner.character()->replica(),world_replica_mutex,*world_quest_ports));} catch(const std::invalid_argument&) {wrong_ports=true;}
            check(wrong_ports,"quest install accepted a different owned ports object");
            auto cyclic_mutex=std::shared_ptr<std::recursive_mutex>(owner.shared_from_this(),world_replica_mutex.get());bool mutex_cycle_rejected=false;
            try {vertical_world_install_replica_quests(owner,{{items,item_pool,ids,world_defs,pool,world_actor,world_runtime},{world_last,world_deadline},*world_zone.npcs,npc_pool,cyclic_mutex,*world_quest_ports,
                world_player,rules,curve,*world_experience,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},world_interaction,*world_actions,*world_npc_actions});} catch(const std::invalid_argument&) {mutex_cycle_rejected=true;}
            cyclic_mutex.reset();check(mutex_cycle_rejected,"quest install accepted a session-owned mutex alias");
            vertical_world_install_replica_quests(owner,{{items,item_pool,ids,world_defs,pool,world_actor,world_runtime},{world_last,world_deadline},*world_zone.npcs,npc_pool,world_replica_mutex,*world_quest_ports,
                world_player,rules,curve,*world_experience,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},world_interaction,*world_actions,*world_npc_actions});
            bool duplicate=false;try {owner.install_quests(make_quests(world_actor));} catch(const std::logic_error&) {duplicate=true;}check(duplicate,"quest session installed twice");
            packet_router=std::make_unique<VerticalWorldSessionPacketRouter>(strand,world_registry,16,1024,
                [](VerticalWorldSession&,RuntimeZone&)->std::shared_ptr<VerticalWorldSessionGameplay>{throw std::logic_error("unexpected non-quest gameplay dispatch");},[&](VerticalWorldSession& s,const net::Packet& p){return quest_dispatch(s,p) || pickup_dispatch(s,p);});
            packet_router->attach(owner.shared_from_this());
            pinned=world_quests.retain(*world_actor);check(pinned && &world_phase.quests(*world_actor)==&pinned->quests(),"world registry did not return attached quest owner");
            VerticalProtocolServices installed_protocol;owner.bind_installed_quest_entry(installed_protocol);
            bool duplicate_installed=false;try {owner.bind_installed_quest_entry(installed_protocol);} catch(const std::logic_error&) {duplicate_installed=true;}
            installed_protocol.quest_entry(CharacterEntryStep::quests,*world_actor);installed_protocol.quest_entry(CharacterEntryStep::finished_quests,*world_actor);
            check(duplicate_installed && !world_services.scene->packets.empty() && world_services.scene->packets.back()==net::Packet({1,9,1,1,0,0,0,1,0,0}),"installed quest entry did not use session runtime");world_services.scene->packets.clear();
            world_runtime->active().front()->packet.counters[0]=2;replica_ports->send_database(*world_actor,net::Packet{5,10,140,0,0,0,1,0,0,2});
            CharacterEntryState entry_state;entry_state.user_id=140;entry_state.character_id=world_actor->id();entry_state.level=owner.character()->snapshot().core.character.level;
            SocialCatalog entry_social(140,world_actor->id());auto entry_catalogs=vertical_restore_entry_catalogs(owner.character()->replica());
            installed_protocol.delivery.send_self=[&](net::Packet p){world_actor->send_self(p);};installed_protocol.delivery.send_peer=[](std::uint32_t,net::Packet){};
            installed_protocol.delivery.social=[](std::uint32_t)->const SocialCatalog* {return nullptr;};installed_protocol.delivery.tick=[] {return 100U;};installed_protocol.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
            installed_protocol.lifecycle=[](CharacterEntryStep,Actor&){};installed_protocol.send_manager=[](net::Packet){};installed_protocol.world_day=[] {return 1U;};installed_protocol.system_time=[] {return 2U;};
            installed_protocol.lock_guild=[] {return false;};installed_protocol.unlock_guild=[] {};
            deliver_vertical_entry(entry_state,owner.character()->pipeline(),owner.character()->snapshot(),entry_social,entry_catalogs,{0,100},"",installed_protocol);
            const net::Packet live_entry{1,9,1,1,0,0,0,2,0,0};
            check(entry_state.ready && std::count(world_services.scene->packets.begin(),world_services.scene->packets.end(),live_entry)==1,"protocol entry did not emit the retained live quest state exactly once");
            world_runtime->active().front()->packet.counters[0]=1;replica_ports->send_database(*world_actor,net::Packet{5,10,140,0,0,0,1,0,0,1});world_services.scene->packets.clear();
            auto world_finished_lock=vertical_world_quests_finished_lock(owner,world_registry);VerticalProtocolServices quest_protocol;
            world_finished_lock->lock_finished(*world_actor);bool finished_blocked=false;
            std::thread finished_probe([&]{finished_blocked=!world_runtime->finished_mutex().try_lock();if(!finished_blocked)world_runtime->finished_mutex().unlock();});finished_probe.join();world_finished_lock->unlock_finished(*world_actor);
            bool missing_entry_lock=false;try {vertical_world_bind_quest_entry(quest_protocol,owner,world_registry,{});} catch(const std::invalid_argument&) {missing_entry_lock=true;}
            auto cyclic_entry_lock=std::shared_ptr<VerticalQuestEntryFinishedLock>(owner.shared_from_this(),world_finished_lock.get());bool cyclic_entry=false;
            try {vertical_world_bind_quest_entry(quest_protocol,owner,world_registry,cyclic_entry_lock);} catch(const std::invalid_argument&) {cyclic_entry=true;}
            cyclic_entry_lock.reset();vertical_world_bind_quest_entry(quest_protocol,owner,world_registry,world_finished_lock);
            bool duplicate_entry=false;try {vertical_world_bind_quest_entry(quest_protocol,owner,world_registry,world_finished_lock);} catch(const std::invalid_argument&) {duplicate_entry=true;}
            quest_protocol.quest_entry(CharacterEntryStep::quests,*world_actor);quest_protocol.quest_entry(CharacterEntryStep::finished_quests,*world_actor);
            check(finished_blocked && missing_entry_lock && cyclic_entry && duplicate_entry && !world_services.scene->packets.empty() && world_services.scene->packets.back()==net::Packet({1,9,1,1,0,0,0,1,0,0}),"session quest entry did not use retained runtime and finished lock");
            world_services.scene->packets.clear();
            Scene stale_scene;Actor stale(world_actor->id(),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},stale_scene,ActorEquipmentRules{*items,enchant,mobs});stale.account_id()=140;
            check(!world_quests.retain(stale) && world_phase.retain(*world_actor),"world registry lost exact Actor identity or phase lease");
            bool stale_save=false;try {(void)pinned->quests().runtime_for(stale);} catch(const std::logic_error&) {stale_save=true;}
            auto save_probe=vertical_world_quests_runtime_for_save(owner,world_registry);check(stale_save && save_probe.get()==world_runtime.get(),"quest save did not resolve exact Actor runtime");save_probe.reset();
            struct EmptySkillServices final:UserSkillRuntimeServices {
                const UserSkillDefinition* definition(std::uint16_t,std::uint8_t) override {throw std::logic_error("unexpected skill definition");}
                UserSkillHandle allocate() override {throw std::logic_error("unexpected skill allocation");}
                void destroy(UserSkillHandle) noexcept override {}void strand(UserSkillHandle) noexcept override {}
                void invalid_definition(std::uint16_t,std::uint8_t) override {throw std::logic_error("unexpected invalid skill");}
                void disconnect(std::uint32_t,std::uint32_t) override {throw std::logic_error("unexpected skill disconnect");}
            } save_skill_services;
            UserSkillRuntime save_skills(world_actor,save_skill_services);
            struct SaveServices final:VerticalSaveCoreSkillServices,VerticalQuestZoneSaveBindingIdentity {
                Actor& actor;CharacterReplicaDelivery delivery;VerticalWorldQuestReplicaPorts& quests;std::recursive_mutex& mutex;std::vector<unsigned> operations;bool change_account=false;
                const void* binding_identity=nullptr;
                SaveServices(Actor& a,CharacterReplica& replica,VerticalWorldQuestReplicaPorts& q,std::recursive_mutex& m):actor(a),delivery(replica,m),quests(q),mutex(m) {}
                UserPosition position(Actor& a) override {check(&a==&actor,"save used wrong actor");if(change_account)++a.account_id();return {};}
                std::uint32_t tick_ms() override {return 100;}
                void lock_effects() override {mutex.lock();}void unlock_effects() override {mutex.unlock();}
                void lock_quests() override {mutex.lock();}void unlock_quests() override {mutex.unlock();}
                void send_database(const net::Packet& p) override {const auto op=word(p,0,2);operations.push_back(op);if(op==0xa04)quests.send_database(actor,p);else check(delivery.apply(p)==InventoryReplicaResult::applied,"session save replica rejected packet");}
                const void* quest_save_binding_identity() const noexcept override {return binding_identity;}
            } save_services(*world_actor,owner.character()->replica(),*replica_ports,*world_replica_mutex);
            std::array<std::uint8_t,775> save_scratch{};save_services.change_account=true;bool changed_save_owner=false;
            try {(void)vertical_world_quests_save_character(owner,world_registry,save_skills,save_services,save_scratch);} catch(const std::logic_error&) {changed_save_owner=true;}
            world_actor->account_id()=140;save_services.change_account=false;
            check(changed_save_owner && save_services.operations==std::vector<unsigned>({0x606}),"save continued after callback changed Actor identity");save_services.operations.clear();
            const auto quest_save_locks=world_quest_ports->lock_calls;world_runtime->active().front()->packet.counters[0]=2;
            check(vertical_world_quests_save_character(owner,world_registry,save_skills,save_services,save_scratch)==100 &&
                std::find(save_services.operations.begin(),save_services.operations.end(),0xa04)==save_services.operations.end() &&
                world_quest_ports->lock_calls>quest_save_locks && owner.character()->replica().quests().active().front().counters[0]==2,"session save did not use retained quest lock and replica port");
            const auto phase_locks=world_quest_ports->lock_calls;world_runtime->active().front()->packet.counters[0]=3;
            std::recursive_mutex wrong_save_mutex;bool wrong_save_binding=false;
            try {vertical_world_quests_save_phase(owner,world_registry,wrong_save_mutex,100);} catch(const std::logic_error&) {wrong_save_binding=true;}
            check(wrong_save_binding && world_quest_ports->lock_calls==phase_locks && owner.character()->replica().quests().active().front().counters[0]==2,"quest save accepted a foreign mutex or mutated replica");
            vertical_world_quests_save_phase(owner,world_registry,*world_replica_mutex,100);
            check(world_quest_ports->lock_calls>phase_locks && owner.character()->replica().quests().active().front().counters[0]==3,"coordinator quest save phase did not use retained owner");
            struct SaveBindingFixture {VerticalWorldSession* owner;std::recursive_mutex* mutex;bool matches(const VerticalWorldSession& value) const {return owner==&value;}std::recursive_mutex& save_mutex() {return *mutex;}};
            using BindingLookup=std::function<std::shared_ptr<SaveBindingFixture>(VerticalWorldSession&)>;
            bool missing_binding_lookup=false;try {(void)vertical_world_quests_save_phase_callback<SaveBindingFixture>(world_registry,BindingLookup{});} catch(const std::invalid_argument&) {missing_binding_lookup=true;}
            auto binding=std::make_shared<SaveBindingFixture>(SaveBindingFixture{nullptr,world_replica_mutex.get()});
            struct ZoneSavePortsFixture {BindingLookup binding;std::function<QuestRuntime&(VerticalWorldSession&)> quests;std::function<void(VerticalWorldSession&,std::uint32_t,VerticalSaveCoreSkillServices&)> save_quests;} zone_save_ports;
            bool missing_zone_binding=false;try {vertical_world_bind_quest_save_phase<SaveBindingFixture>(zone_save_ports,world_registry);} catch(const std::invalid_argument&) {missing_zone_binding=true;}
            zone_save_ports.binding=[&](VerticalWorldSession&){return binding;};vertical_world_bind_quest_save_phase<SaveBindingFixture>(zone_save_ports,world_registry);
            bool duplicate_zone_provider=false;try {vertical_world_bind_quest_save_phase<SaveBindingFixture>(zone_save_ports,world_registry);} catch(const std::invalid_argument&) {duplicate_zone_provider=true;}
            bool wrong_session_binding=false;try {zone_save_ports.save_quests(owner,100,save_services);} catch(const std::logic_error&) {wrong_session_binding=true;}
            check(missing_binding_lookup && missing_zone_binding && duplicate_zone_provider && wrong_session_binding && owner.character()->replica().quests().active().front().counters[0]==3,"quest save accepted an invalid session binding");
            binding->owner=&owner;bool wrong_active_binding=false;try {zone_save_ports.save_quests(owner,100,save_services);} catch(const std::logic_error&) {wrong_active_binding=true;}
            check(wrong_active_binding && owner.character()->replica().quests().active().front().counters[0]==3,"quest save accepted a different active binding");
            save_services.binding_identity=binding.get();world_runtime->active().front()->packet.counters[0]=4;zone_save_ports.save_quests(owner,100,save_services);
            check(owner.character()->replica().quests().active().front().counters[0]==4,"bound coordinator quest save did not update replica");
            world_quest_ports->on_lock=[&]{binding->owner=nullptr;};bool changed_binding=false;
            try {zone_save_ports.save_quests(owner,100,save_services);} catch(const std::logic_error&) {changed_binding=true;}
            world_quest_ports->on_lock={};binding->owner=&owner;
            check(changed_binding && world_quest_ports->locks==0,"quest save accepted a binding changed during its lock callback");
            world_runtime->active().front()->packet.counters[0]=1;replica_ports->send_database(*world_actor,net::Packet{5,10,140,0,0,0,1,0,0,1});
            replica_ports->send_database(*world_actor,net::Packet{2,10,140,0,0,0,1,0,2});
            save_services.operations.clear();bool failed_save=false;
            try {(void)vertical_world_quests_save_character(owner,world_registry,save_skills,save_services,save_scratch);} catch(const std::runtime_error&) {failed_save=true;}
            check(failed_save && world_quest_ports->locks==0 && owner.character()->replica().quests().active().empty() &&
                std::find(save_services.operations.begin(),save_services.operations.end(),0x609)==save_services.operations.end(),"failed quest save continued to quick slots or leaked its lock");
            replica_ports->send_database(*world_actor,net::Packet{1,10,140,0,0,0,1,0,0,0});
            replica_ports->send_database(*world_actor,net::Packet{5,10,140,0,0,0,1,0,0,1});
            bool timer_rebind=false;world_quest_ports->on_lock=[&]{try {pinned->packets().rebind_npc_zone(*world_zone.npcs,*destination_zone.npcs);} catch(const std::logic_error&) {timer_rebind=true;}};
            world_phase.quests(*world_actor).update_timers_for(*world_actor,100);world_quest_ports->on_lock={};
            check(timer_rebind && pinned->quests().owns_npc_zone(*world_zone.npcs),"quest NPC zone changed during timer callback");
            bool bad_packet=false,wrong_actor=false;
            try {replica_ports->send_database(*world_actor,net::Packet{2,10});} catch(const std::runtime_error&) {bad_packet=true;}
            try {replica_ports->send_database(stale,net::Packet{2,10});} catch(const std::logic_error&) {wrong_actor=true;}
            check(bad_packet && wrong_actor && owner.character()->replica().quests().active().size()==1,"quest replica accepted invalid packet or foreign actor");
            bool missing_mutex=false;try {VerticalWorldQuestReplicaPorts invalid(*world_actor,owner.character()->replica(),{},*world_quest_ports);} catch(const std::invalid_argument&) {missing_mutex=true;}
            check(missing_mutex,"quest replica accepted a missing save mutex lease");replica_ports.reset();world_replica_mutex.reset();
            check(!weak_replica_mutex.expired(),"installed quest ports did not retain the save mutex");
            bool not_admitted=false;try {(void)quest_dispatch(owner,net::Packet{8,9,1,0});} catch(const std::logic_error&) {not_admitted=true;}
            bool pickup_not_admitted=false;try {(void)pickup_dispatch(owner,net::Packet{5,2,42,0,0,0});} catch(const std::logic_error&) {pickup_not_admitted=true;}
            check(not_admitted && pickup_not_admitted && pickup_lookups==0 && !quest_dispatch(owner,net::Packet{1,2}) && world_runtime->active().size()==1,"quest or pickup dispatch accepted a non-admitted Actor or consumed unrelated traffic");
            world_zone.users->attach(*world_actor,0,0,{});owner.admission().zone=&world_zone;owner.admission().location=4;
            auto other_npcs=std::make_unique<ZoneNpcs>(MapGeometry(81),npc_services);world_zone.npcs.swap(other_npcs);bool wrong_zone=false;
            try {(void)quest_dispatch(owner,net::Packet{8,9,1,0});} catch(const std::logic_error&) {wrong_zone=true;}
            world_zone.npcs.swap(other_npcs);check(wrong_zone && world_runtime->active().size()==1,"quest dispatch accepted a different NPC zone");
            pickup_incomplete=true;bool rejected_incomplete=false;try {(void)pickup_dispatch(owner,net::Packet{5,2,42,0,0,0});} catch(const std::logic_error&) {rejected_incomplete=true;}pickup_incomplete=false;
            pickup_mutates=true;bool rejected_mutation=false;try {(void)pickup_dispatch(owner,net::Packet{5,2,42,0,0,0});} catch(const std::logic_error&) {rejected_mutation=true;}pickup_mutates=false;world_actor->attack_state().mode=0;
            check(rejected_incomplete && rejected_mutation && pickup_fixture->actions.events.empty(),"pickup accepted incomplete or mutating owners");pickup_lookups=0;
            auto queued=[&](const net::Packet& packet){check(packet_router->try_enqueue(owner,packet),"quest packet queue rejected request");packet_router->update(owner);check(packet_router->pending(owner)==0,"quest packet remained queued");};
            queued(net::Packet{8,9,1,0});check(world_runtime->active().empty() && owner.character()->replica().quests().active().empty() && world_quest_ports->db.empty(),"quest abandon did not reach selected character replica");
            world_actor->status().skills.max_bags=1;
            check(packet_router->try_enqueue(owner,net::Packet{2,9,99,0,0,0,1,0}) && packet_router->try_enqueue(owner,net::Packet{4,14,99,0,0,0}) && packet_router->pending(owner)==2,"world quest/NPC FIFO rejected requests");
            packet_router->update(owner);check(packet_router->pending(owner)==0 && world_runtime->active().size()==1 && owner.character()->replica().quests().active().size()==1,"world NPC quest acceptance diverged from replica");
            check(world_interaction.npc==world_npc && world_npc->users_using==1,"world NPC interaction dispatch failed");
            bool active_rebind=false;try {pinned->packets().rebind_npc_zone(*world_zone.npcs,*destination_zone.npcs);} catch(const std::logic_error&) {active_rebind=true;}
            bool not_transferred=false;try {vertical_world_quests_rebind_zone(owner,world_zone,destination_zone,world_registry);} catch(const std::logic_error&) {not_transferred=true;}
            check(active_rebind && not_transferred && pinned->quests().owns_npc_zone(*world_zone.npcs),"quest NPC zone changed before interaction cleanup or destination admission");
            world_actor->attack_state().mode=1;queued(net::Packet{5,2,42,0,0,0});world_actor->attack_state().mode=0;
            check(pickup_lookups==0 && pickup_fixture->actions.events.empty(),"gated pickup resolved owners or ran actions");
            queued(net::Packet{5,2,42,0,0,0});check(pickup_lookups==1 && pickup_fixture->actions.events=="CSP" && !world_services.scene->packets.empty() && world_services.scene->packets.back()==inventory_failure_packet(3),"queued missing pickup did not reach actual inventory rejection");
            auto* ground=item_pool->allocate();check(ground,"world quest pickup allocation failed");*ground=GroundItem{};ground->id=101;ground->user_id=world_actor->id();ground->drop_type=1;ground->position={10,0,10};ground->definition=&pickup_fixture->definition;
            ground->item.uid=ids->raw_next();ground->item.id=pickup_fixture->definition.id;ground->item.type=5;ground->item.type_id=1;ground->item.count=1;ground->item.quality=100;ground->item.make_type="M";const auto ground_uid=ground->item.uid;
            check(world_zone.items->enter(*ground,1),"world quest pickup item did not enter zone");queued(net::Packet{5,2,101,0,0,0});
            const auto picked=world_actor->retained_inventory_item(1,0);const auto pickup_stored=owner.character()->replica().inventory().inventory();
            check(pickup_lookups==2 && pickup_fixture->actions.events=="CSPCSP","queued quest item pickup actions differed");
            check(world_zone.items->size()==0 && pickup_fixture->world.logs==1,"queued quest item did not leave zone or log pickup: zone="+std::to_string(world_zone.items->size())+" logs="+std::to_string(pickup_fixture->world.logs)+" response="+std::to_string(world_services.scene->packets.back().back()));
            check(picked && picked->item->item.uid==ground_uid,"queued quest item did not reach Actor inventory");
            check(pickup_stored.size()==1 && pickup_stored.front().item.uid==ground_uid,"queued quest item did not reach replica");
            check(world_runtime->active().size()==1 && world_runtime->active().front()->packet.counters[0]==0,"queued quest pickup changed quest state");
            MobLootUser world_loot_user;world_loot_user.id=world_actor->id();bool mutate_loot_identity=false;
            VerticalWorldQuestLootActors world_loot_actor(world_registry,[&](VerticalWorldSession& selected,MobLootUser& user){check(&selected==&owner,"wrong loot session");if(mutate_loot_identity)++user.id;return &user==&world_loot_user;});
            VerticalQuestSessionLoot world_loot(world_loot_actor,world_quests,pickup_fixture->links);
            check(world_loot.has_quest(world_loot_user,1) && !world_loot.has_quest(world_loot_user,65535),"loot did not query the retained quest runtime");
            MobLootUser stale_loot=world_loot_user;check(!world_loot_actor.retain(stale_loot),"loot accepted stale same-ID user");
            mutate_loot_identity=true;bool changed_loot=false;try {(void)world_loot_actor.retain(world_loot_user);} catch(const std::logic_error&) {changed_loot=true;}
            world_loot_user.id=world_actor->id();mutate_loot_identity=false;check(changed_loot,"loot accepted identity mutation during lookup");
            MobDeathUser* live_death_user=nullptr;bool mutate_death_identity=false;
            VerticalWorldQuestMobActors world_mob_actor(world_registry,[&](VerticalWorldSession& selected,MobDeathUser& user){check(&selected==&owner,"wrong mob credit session");if(mutate_death_identity)++user.id;return &user==live_death_user;});
            VerticalQuestSessionMobCredit world_credit(world_mob_actor,world_quests,links);DeathServices world_death(pinned->quests(),world_actor->id());VerticalQuestMobDeath world_death_bridge(world_death,world_credit);live_death_user=&world_death.user;
            MobDeathUser stale_death=world_death.user;check(!world_mob_actor.retain(stale_death),"mob credit accepted stale death-user identity");
            mutate_death_identity=true;bool changed_death=false;try {(void)world_mob_actor.retain(world_death.user);} catch(const std::logic_error&) {changed_death=true;}
            world_death.user.id=world_actor->id();mutate_death_identity=false;check(changed_death,"mob credit accepted identity mutation during lookup");
            auto retained_mob_actor=world_mob_actor.retain(world_death.user);check(retained_mob_actor.get()==world_actor.get(),"mob credit failed to retain selected actor");retained_mob_actor.reset();
            MobRuntime world_mob;world_mob.type=3;world_mob.definition=&mob_definition;world_mob.damage=std::make_unique<MobDamageLedger>(damage.pool);world_mob.damage->add(1,world_actor->id(),10,0,0);
            mob_death(world_mob,1,world_actor->id(),MobDeathExperience{},world_death_bridge);
            check(world_death.events==std::vector<char>({'E','D','C'}) && world_runtime->active().front()->packet.counters[0]==1 && owner.character()->replica().quests().active().front().counters[0]==1,"world mob quest credit diverged from replica");
            queued(end_packet);check(world_runtime->active().empty() && owner.character()->replica().quests().active().empty() && owner.character()->replica().quests().finished().empty(),"world repeatable NPC quest completion diverged from replica");
            const auto held=world_actor->retained_inventory_item(1,1);const auto stored=owner.character()->replica().inventory().inventory();
            check(held && stored.size()==2 && stored.front().item.uid==ground_uid && held->item->item.uid==stored.back().item.uid && world_actor->money()==42 && owner.character()->replica().core().money==42 && world_player.experience==20 && owner.character()->replica().core().experience==20 && world_quest_ports->db.empty(),"world quest rewards diverged from selected character replica");
            NpcMoveServices world_move(*world_actor);vertical_quests_session_move_interactions(*world_actor,false,world_move,world_quests);
            check(!world_interaction.npc && world_npc->users_using==0,"world NPC interaction cleanup failed");
            bool nested_rebind=false;world_actions->after_shop=[&]{try {pinned->packets().rebind_npc_zone(*world_zone.npcs,*destination_zone.npcs);} catch(const std::logic_error&) {nested_rebind=true;}};
            queued(net::Packet{8,9,255,255});world_actions->after_shop={};check(nested_rebind && pinned->quests().owns_npc_zone(*world_zone.npcs),"quest NPC zone changed during packet callback");
            check(world_zone.users->detach(world_actor->id()),"world quest source Actor detach failed");destination_zone.users->attach(*world_actor,0,0,{});owner.admission().zone=&destination_zone;
            bool stale_source=false;try {vertical_world_quests_rebind_zone(owner,destination_zone,destination_zone,world_registry);} catch(const std::logic_error&) {stale_source=true;}
            check(stale_source && pinned->quests().owns_npc_zone(*world_zone.npcs),"quest zone transfer accepted a stale source");
            auto* retained_quest=&pinned->quests();vertical_world_quests_rebind_zone(owner,world_zone,destination_zone,world_registry);
            check(&pinned->quests()==retained_quest && pinned->quests().owns_npc_zone(*destination_zone.npcs),"quest runtime did not follow destination zone");
            queued(net::Packet{4,14,99,0,0,0});check(!world_interaction.npc,"destination resolved NPC from prior zone");
            queued(net::Packet{4,14,102,0,0,0});check(world_interaction.npc==destination_npc && destination_npc->users_using==1,"destination NPC selection failed");
            vertical_quests_session_move_interactions(*world_actor,false,world_move,world_quests);check(!world_interaction.npc && destination_npc->users_using==0,"destination NPC cleanup failed");
            bool failure_reported=false;world_services.on_failure=[&](std::exception_ptr error){try {std::rethrow_exception(error);} catch(const std::logic_error&) {failure_reported=true;}};
            check(packet_router->try_enqueue(owner,net::Packet{4,14,102,0,0,0}),"failure fixture queue failed");
            world_quest_ports->on_lock=[] {throw std::logic_error("timer failure");};
            vertical_primary_quest_update_actor(world_phase,*world_actor,100,[]{return true;});world_quest_ports->on_lock={};
            check(failure_reported && dispatch_disconnects==1 && owner.dispatch_failed() && !packet_router->try_enqueue(owner,net::Packet{4,14,102,0,0,0}),"quest timer failure did not fence and disconnect session");
            bool failed_quest=false,failed_pickup=false,failed_rebind=false;
            try {(void)quest_dispatch(owner,net::Packet{4,14,102,0,0,0});} catch(const std::logic_error&) {failed_quest=true;}
            destination_zone.items=std::make_unique<ZoneItems>(pickup_geometry,pickup_fixture->world);world_actor->attack_state().mode=1;
            try {(void)pickup_dispatch(owner,net::Packet{5,2,42,0,0,0});} catch(const std::logic_error&) {failed_pickup=true;}
            world_actor->attack_state().mode=0;destination_zone.items.reset();
            try {vertical_world_quests_rebind_zone(owner,destination_zone,destination_zone,world_registry);} catch(const std::logic_error&) {failed_rebind=true;}
            check(failed_quest && failed_pickup && failed_rebind && !world_interaction.npc && destination_npc->users_using==0,"failed quest session accepted direct gameplay or zone rebind");
            check(world_phase.retain(*world_actor) && !world_phase.eligible(*world_actor) && world_quests.retain(*world_actor),"failed quest owner lost traversal retention or remained timer eligible");
            unsigned failed_visits=0;destination_zone.users->visit_all([&](Actor& actor){return world_phase.retain(actor);},[&](Actor& actor){++failed_visits;vertical_primary_quest_update_actor(world_phase,actor,100,[]{return true;});});
            check(failed_visits==1 && dispatch_disconnects==1,"failed quest owner aborted traversal or ran timer failure twice");
            const auto failed_entry_packets=world_services.scene->packets.size();VerticalProtocolServices failed_protocol;bool failed_bind=false,failed_entry=false,failed_external=false;
            try {owner.bind_installed_quest_entry(failed_protocol);} catch(const std::logic_error&) {failed_bind=true;}
            try {installed_protocol.quest_entry(CharacterEntryStep::quests,*world_actor);} catch(const std::logic_error&) {failed_entry=true;}
            try {quest_protocol.quest_entry(CharacterEntryStep::quests,*world_actor);} catch(const std::logic_error&) {failed_external=true;}
            check(failed_bind && failed_entry && failed_external && world_services.scene->packets.size()==failed_entry_packets,"failed session delivered or bound quest entry");
            owner.report_dispatch_failure(std::make_exception_ptr(std::logic_error("duplicate failure")));packet_router->update(owner);
            check(dispatch_disconnects==1 && packet_router->pending(owner)==0 && !world_interaction.npc,"failed session processed queued NPC action or disconnected twice");
            check(destination_zone.users->detach(world_actor->id()),"world quest destination Actor detach failed");owner.admission().zone=nullptr;owner.admission().location=1;
        };
        world_services.on_disconnect=[&](VerticalWorldSession& owner){check(owner.closing() && world_quests.retain(owner.world_user()->actor) && world_phase.retain(owner.world_user()->actor) && !world_phase.eligible(owner.world_user()->actor),"closing quest owner lost retention or remained eligible for primary timer");bool rejected=false;try {(void)world_phase.quests(owner.world_user()->actor);} catch(const std::logic_error&) {rejected=true;}check(rejected,"closing quest session accepted primary timer access");save_pin=vertical_world_quests_runtime_for_save(owner,world_registry);check(save_pin.get()==world_runtime.get(),"closing save lost quest runtime");packet_router->stop(owner);};
        CharacterRoster roster;roster.country=0;CharacterSummary selected;selected.id=9095;roster.slots[0]=selected;auto replica_pool=std::make_shared<InventoryReplicaPool>();
        asio::post(strand,[&]{try {live=VerticalWorldSession::create(strand,database.get_executor(),world_registry,admission,world_services,*claim.session,channel,roster,
            [replica_pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;snapshot.core.character.level=20;snapshot.core.character.grow=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*replica_pool);replica->retain_resources(replica_pool);check(replica->quests().load({1,0,{1,0,0}}),"fixture replica quest allocation failed");return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
            live->open();live->receive(net::Packet{4,1,0x87,0x23,0,0});claim.session.reset();
        } catch(...) {ready.set_exception(std::current_exception());}});
        try {ready_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
        std::weak_ptr<VerticalWorldSession> weak_world=live;std::promise<void> closed;auto closed_future=closed.get_future();
        asio::post(strand,[&]{try {check(dispatch_disconnects==1,"failed preparation attempted entry or disconnected twice");live->close();check(world_registry.size()==0 && admission.selection_count()==0 && weak_io.expired() && world_services.unbound && world_services.cancelled_preparations==0,"quest session failed disconnect fences or cancelled completed preparation");live.reset();check(!weak_world.expired() && pinned && !weak_replica_mutex.expired(),"quest lease did not retain closed session and save mutex");pinned.reset();check(!weak_world.expired() && !weak_replica_mutex.expired(),"save lease did not retain quest session");save_pin.reset();check(weak_world.expired() && weak_replica_mutex.expired(),"closed quest session or save mutex remained retained");closed.set_value();} catch(...) {closed.set_exception(std::current_exception());}});
        try {closed_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
        {
            session::LoginSession next_login{};next_login.user_id=141;next_login.session_id=1;login_registry->add(next_login);
            check(login_registry->select(141,1,0,1),"successful entry claim preparation failed");auto next_claim=login_registry->claim_world(141,next_login.token,0,1,3);check(next_claim.session.has_value(),"successful entry claim missing");
            std::promise<void> entered;auto entered_future=entered.get_future();SuccessfulQuestSessionFixture success_services(world_rules,strand,entered);
            success_services.on_disconnect=[&](VerticalWorldSession& owner){auto& actor=owner.world_user()->actor;check(owner.closing() && !owner.dispatch_failed() && world_quests.retain(actor) && world_phase.retain(actor) && !world_phase.eligible(actor),"closing quest owner lost traversal retention or remained timer eligible");bool rejected=false;try {(void)world_phase.quests(actor);} catch(const std::logic_error&) {rejected=true;}check(rejected,"closing quest owner accepted primary timer access");};
            std::shared_ptr<VerticalWorldSession> success_session;std::shared_ptr<QuestRuntime> success_runtime;std::shared_ptr<std::recursive_mutex> success_mutex=std::make_shared<std::recursive_mutex>();
            std::unique_ptr<Services> success_quest_ports;std::unique_ptr<ExpPorts> success_exp;std::unique_ptr<PacketActions> success_actions;std::unique_ptr<NpcPacketActions> success_npc_actions;
            PlayerCore success_player;VerticalNpcInteraction success_interaction;std::uint8_t success_last=0;std::uint32_t success_deadline=0;
            struct PreparedSave {VerticalWorldSession* owner;std::recursive_mutex* mutex;
                bool matches(const VerticalWorldSession::Strand&,const VerticalWorldSession::Registry&,const VerticalWorldSession& candidate) const {return owner==&candidate;}
                std::recursive_mutex& save_mutex() {return *mutex;}
            };
            using Preparation=VerticalWorldSessionPreparation<PreparedSave,PreparedSave>;
            std::shared_ptr<Preparation> preparation;
            success_services.async_prepare=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done) {
                auto selected=owner.shared_from_this();
                preparation=Preparation::create(strand,database.get_executor(),world_registry,selected,
                    [&owner,&success_mutex]{return std::make_shared<PreparedSave>(PreparedSave{&owner,success_mutex.get()});},
                    [](VerticalWorldSession&,std::shared_ptr<PreparedSave> value){return value;},
                    [&](VerticalWorldSession& owner,PreparedSave& save){bool use_wrong_mutex=true;unsigned borrowed_owner=0;auto wrong_mutex=std::make_shared<std::recursive_mutex>();
                auto bind=[&](const VerticalQuestSessionRestoreResult& restored){auto selected_actor=restored.actor;success_runtime=restored.quests.restored.runtime;
                    success_quest_ports=std::make_unique<Services>(*selected_actor,*success_runtime,*success_services.scene);success_quest_ports->check_success_order=false;success_exp=std::make_unique<ExpPorts>(*selected_actor);success_actions=std::make_unique<PacketActions>(*selected_actor);success_npc_actions=std::make_unique<NpcPacketActions>(*selected_actor);success_player=owner.character()->snapshot().core;
                    auto actor_lease=borrowed_owner==1?std::shared_ptr<Actor>(selected_actor.get(),[](Actor*){}):selected_actor;
                    auto runtime_lease=borrowed_owner==2?std::shared_ptr<QuestRuntime>(success_runtime.get(),[](QuestRuntime*){}):success_runtime;
                    auto pool_lease=borrowed_owner==3?std::shared_ptr<QuestRuntimePool>(pool.get(),[](QuestRuntimePool*){}):pool;
                    auto definition_lease=borrowed_owner==4?std::shared_ptr<const QuestDefinitions>(world_defs.get(),[](const QuestDefinitions*){}):std::shared_ptr<const QuestDefinitions>(world_defs);
                    auto mutex_lease=borrowed_owner==5?std::shared_ptr<std::recursive_mutex>(success_mutex.get(),[](std::recursive_mutex*){}):(use_wrong_mutex?wrong_mutex:success_mutex);
                    return VerticalWorldQuestInstall{{items,item_pool,ids,std::move(definition_lease),std::move(pool_lease),std::move(actor_lease),std::move(runtime_lease)},{success_last,success_deadline},*world_zone.npcs,npc_pool,std::move(mutex_lease),*success_quest_ports,success_player,rules,curve,*success_exp,[]{return ExperienceAwardOptions{};},[](const ItemDefinition&){return std::string("Reward");},success_interaction,*success_actions,*success_npc_actions};};
                bool wrong_binding=false;try {(void)vertical_world_prepare_replica_quests(owner,success_mutex,100,pool,world_defs,bind);} catch(const std::logic_error&) {wrong_binding=true;}
                check(wrong_binding && !owner.quests_lease(),"quest preparation accepted a different replica mutex");use_wrong_mutex=false;
                for(borrowed_owner=1;borrowed_owner<=5;++borrowed_owner) {bool rejected=false;try {(void)vertical_world_prepare_replica_quests(owner,success_mutex,100,pool,world_defs,bind);} catch(const std::logic_error&) {rejected=true;}
                    check(rejected && !owner.quests_lease(),"quest preparation accepted a non-owning restored owner");}borrowed_owner=0;
                save.mutex=wrong_mutex.get();
                bool rejected_save=false;try {(void)vertical_world_prepare_bound_replica_quests(owner,strand,world_registry,save,success_mutex,100,pool,world_defs,bind);} catch(const std::invalid_argument&) {rejected_save=true;}
                check(rejected_save && !owner.quests_lease(),"quest preparation accepted a foreign save mutex");
                save.mutex=success_mutex.get();save.owner=nullptr;rejected_save=false;
                try {(void)vertical_world_prepare_bound_replica_quests(owner,strand,world_registry,save,success_mutex,100,pool,world_defs,bind);} catch(const std::invalid_argument&) {rejected_save=true;}
                check(rejected_save && !owner.quests_lease(),"quest preparation accepted a foreign save owner");save.owner=&owner;
                bool tick_called=false,policy_called=false;
                auto sql_provider=vertical_world_sql_quest_after_bind_provider(pool,world_defs,
                    [&](VerticalWorldSession&){return 100U;},
                    [&](VerticalWorldSession&,const VerticalQuestSessionRestoreResult& restored){return bind(restored);},
                    [&](VerticalWorldSession&,const VerticalWorldQuestPrepareResult&){});
                auto sql_hook=sql_provider(strand,world_registry,owner,success_mutex);
                check(bool(sql_hook),"SQL provider did not construct retained quest callback");
                bool missing_sql_mutex=false;try {(void)sql_provider(strand,world_registry,owner,{});} catch(const std::invalid_argument&) {missing_sql_mutex=true;}
                check(missing_sql_mutex,"SQL provider accepted missing quest mutex");
                bool rejected_factory_called=false;auto rejecting_hook=vertical_world_quest_after_bind<PreparedSave>(strand,world_registry,success_mutex,pool,world_defs,
                    [](VerticalWorldSession&){return 100U;},
                    [&](VerticalWorldSession&,const VerticalQuestSessionRestoreResult& restored){rejected_factory_called=true;return bind(restored);},
                    [](VerticalWorldSession&,const VerticalWorldQuestPrepareResult&){throw std::runtime_error("restore policy rejected rows");});
                bool policy_rejected=false;try {rejecting_hook(owner,save);} catch(const std::runtime_error&) {policy_rejected=true;}
                check(policy_rejected && !rejected_factory_called && !owner.quests_lease(),"rejected quest restoration constructed or installed an owner");
                const auto change_counter=[&](std::uint8_t value){std::lock_guard lock(*success_mutex);check(owner.character()->replica().quests().apply(net::Packet{5,10,141,0,0,0,1,0,0,value})==InventoryReplicaResult::applied,"quest fixture failed to update replica");};
                bool stale_factory_called=false;auto stale_policy=vertical_world_quest_after_bind<PreparedSave>(strand,world_registry,success_mutex,pool,world_defs,
                    [](VerticalWorldSession&){return 100U;},
                    [&](VerticalWorldSession&,const VerticalQuestSessionRestoreResult& restored){stale_factory_called=true;return bind(restored);},
                    [&](VerticalWorldSession&,const VerticalWorldQuestPrepareResult&){change_counter(3);});
                bool stale_rejected=false;try {stale_policy(owner,save);} catch(const std::logic_error&) {stale_rejected=true;}
                check(stale_rejected && !stale_factory_called && !owner.quests_lease(),"quest policy installed stale restored rows");change_counter(2);
                auto stale_factory=vertical_world_quest_after_bind<PreparedSave>(strand,world_registry,success_mutex,pool,world_defs,
                    [](VerticalWorldSession&){return 100U;},
                    [&](VerticalWorldSession&,const VerticalQuestSessionRestoreResult& restored){auto result=bind(restored);change_counter(3);return result;},
                    [](VerticalWorldSession&,const VerticalWorldQuestPrepareResult&){});
                stale_rejected=false;try {stale_factory(owner,save);} catch(const std::logic_error&) {stale_rejected=true;}
                check(stale_rejected && !owner.quests_lease(),"quest factory installed stale restored rows");change_counter(2);
                auto hook=vertical_world_quest_after_bind<PreparedSave>(strand,world_registry,success_mutex,pool,world_defs,
                    [&](VerticalWorldSession& candidate){check(&candidate==&owner,"quest tick changed session");tick_called=true;return 100U;},
                    [&](VerticalWorldSession& candidate,const VerticalQuestSessionRestoreResult& restored){check(&candidate==&owner,"quest factory changed session");return bind(restored);},
                    [&](VerticalWorldSession& candidate,const VerticalWorldQuestPrepareResult& result){check(&candidate==&owner && result.skipped_active==0 && result.skipped_finished==0 && result.invalid_active.empty(),"quest preparation lost restored rows");policy_called=true;});
                save.owner=nullptr;rejected_save=false;try {hook(owner,save);} catch(const std::invalid_argument&) {rejected_save=true;}
                check(rejected_save && !tick_called && !policy_called && !owner.quests_lease(),"quest hook used services with a foreign save owner");save.owner=&owner;
                hook(owner,save);check(tick_called && policy_called,"quest preparation did not use real hook services");});
                preparation->start(std::move(done));};
            auto success_pool=std::make_shared<InventoryReplicaPool>();
            asio::post(strand,[&]{try {success_session=VerticalWorldSession::create(strand,database.get_executor(),world_registry,admission,success_services,*next_claim.session,channel,roster,
                [success_pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;snapshot.core.character.level=20;snapshot.core.character.grow=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*success_pool);replica->retain_resources(success_pool);check(replica->quests().load({1,0,{2,0,0}}),"successful entry quest allocation failed");return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});success_session->open();success_session->receive(net::Packet{4,1,0x87,0x23,0,0});next_claim.session.reset();} catch(...) {entered.set_exception(std::current_exception());}});
            try {entered_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
            struct RejectedBinding {std::atomic<bool>& destroyed_on_world;VerticalWorldSession::Strand strand;
                RejectedBinding(std::atomic<bool>& flag,VerticalWorldSession::Strand executor):destroyed_on_world(flag),strand(std::move(executor)) {}
                ~RejectedBinding(){destroyed_on_world=strand.running_in_this_thread();}
            };
            using RejectedPreparation=VerticalWorldSessionPreparation<RejectedBinding,RejectedBinding>;
            std::atomic<bool> rejected_on_world{true};std::shared_ptr<RejectedPreparation> rejected_preparation;
            std::promise<void> rejected_done;auto rejected_future=rejected_done.get_future();
            asio::post(strand,[&]{try {
                rejected_preparation=RejectedPreparation::create(strand,database.get_executor(),world_registry,success_session,
                    [&]{return std::make_shared<RejectedBinding>(rejected_on_world,strand);},
                    [](VerticalWorldSession&,std::shared_ptr<RejectedBinding> value){return value;},
                    [](VerticalWorldSession&,RejectedBinding&){throw std::runtime_error("post-bind failure");});
                rejected_preparation->start([&](std::exception_ptr error){try {check(error && !rejected_preparation->binding() && !rejected_on_world && world_registry.pending_database_work(141)==0,"post-bind failure escaped worker disposal fence");rejected_done.set_value();}catch(...){rejected_done.set_exception(std::current_exception());}});
            }catch(...){rejected_done.set_exception(std::current_exception());}});
            try {rejected_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
            std::atomic<bool> cancelled_on_world{true};std::shared_ptr<RejectedPreparation> cancelled_preparation;bool cancelled_completion=false;
            std::promise<void> cancelled_done;auto cancelled_future=cancelled_done.get_future();
            asio::post(strand,[&]{try {
                cancelled_preparation=RejectedPreparation::create(strand,database.get_executor(),world_registry,success_session,
                    [&]{return std::make_shared<RejectedBinding>(cancelled_on_world,strand);},
                    [](VerticalWorldSession&,std::shared_ptr<RejectedBinding> value){return value;},
                    [&](VerticalWorldSession&,RejectedBinding&){cancelled_preparation->cancel([&]{try {
                        check(!cancelled_completion && !cancelled_preparation->binding() && !cancelled_on_world && world_registry.pending_database_work(141)==0,"post-bind cancellation escaped disposal fence");cancelled_done.set_value();
                    } catch(...){cancelled_done.set_exception(std::current_exception());}},[&](std::exception_ptr error){cancelled_done.set_exception(error);});});
                cancelled_preparation->start([&](std::exception_ptr){cancelled_completion=true;});
            }catch(...){cancelled_done.set_exception(std::current_exception());}});
            try {cancelled_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
            std::promise<void> success_closed;auto success_closed_future=success_closed.get_future();asio::post(strand,[&]{try {const net::Packet expected{1,9,1,1,0,0,0,2,0,0};check(success_session->entry_state() && success_session->entry_state()->ready && admission.waiting_count()==1 && admission_services.registered==1 && std::count(success_services.scene->packets.begin(),success_services.scene->packets.end(),expected)==1,"successful preparation did not send retained quest entry");success_session->close();check(world_registry.size()==0 && admission.waiting_count()==0 && success_services.unbound,"successful entry session did not drain");success_session.reset();preparation.reset();rejected_preparation.reset();cancelled_preparation.reset();success_closed.set_value();} catch(...) {success_closed.set_exception(std::current_exception());}});
            try {success_closed_future.get();} catch(...) {work.reset();io.stop();database.join();worker.join();throw;}
        }
        packet_router.reset();world_actions.reset();world_npc_actions.reset();world_experience.reset();world_experience_delivery.reset();replica_ports.reset();world_quest_ports.reset();world_runtime.reset();world_zone.items.reset();check(world_zone.npcs->leave(*world_npc),"world quest NPC leave failed");npc_pool.release(*world_npc);check(destination_zone.npcs->leave(*destination_npc),"destination quest NPC leave failed");npc_pool.release(*destination_npc);work.reset();database.join();worker.join();
    }
    check(zone.leave(*replacement),"NPC leave failed");npc_pool.release(*replacement);
    auto* failed=npc_pool.allocate();failed->id=101;check(zone.enter(*failed),"failure NPC entry failed");
    auto lease=npc_pool.pin(*failed);npc_services.fail_broadcast=true;bool threw=false;
    try {zone.leave(*failed);} catch(const std::runtime_error&) {threw=true;}
    npc_services.fail_broadcast=false;check(threw,"retirement failure was hidden");lease=NpcPool::Lease{};
    auto* other=npc_pool.allocate();check(other!=failed,"failed retirement recycled NPC");npc_pool.release(*other);
    // ZoneNpcs::leave unlinks before broadcasting and did not clear the cell on failure.
    failed->cell.reset();npc_pool.release(*failed);
    auto* pinned=npc_pool.allocate();auto first=npc_pool.pin(*pinned);auto second=npc_pool.pin(*pinned);
    npc_pool.release(*pinned);bool rejected=false;try {npc_pool.release(*pinned);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"duplicate deferred release accepted");rejected=false;
    try {(void)npc_pool.pin(*pinned);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"released NPC accepted for pin");first=NpcPool::Lease{};
    auto* interim=npc_pool.allocate();check(interim!=pinned,"NPC recycled before final lease");
    second=NpcPool::Lease{};check(npc_pool.allocate()==pinned,"last lease did not restore pool reuse");
    npc_pool.release(*pinned);npc_pool.release(*interim);
    std::cout<<"Actual Actor quest acceptance, mob progress and NPC completion passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
