#define ETERNITY_HANDOFF_FIXTURE_ONLY
#include "vertical_protocol_handoff_tcp_unit.cpp"
#include "eternity/game/vertical_disconnect.hpp"
#include "eternity/game/zone_logout_update.hpp"
#include "eternity/game/vertical_database_disconnect.hpp"
#include "eternity/game/vertical_database_ack.hpp"
#include "eternity/game/zone_actor_leave.hpp"
#include "eternity/game/zone_leave_lifecycle.hpp"
#include "eternity/game/vertical_save_core_skills.hpp"
#include "eternity/game/vertical_save_job_bridge.hpp"
#include "eternity/game/vertical_social_lifecycle.hpp"
#include "eternity/game/vertical_social_selection.hpp"
#include <filesystem>

QuestDefinitions fixture_quest_definitions(){
 std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"data/quests-oracle.tsv");std::string line,hex;
 while(std::getline(file,line))if(line.starts_with("S\t")){std::istringstream row(line);char tag;row>>tag>>hex;break;}
 check(!hex.empty(),"quest fixture missing");net::Packet data;for(std::size_t i=0;i<hex.size();i+=2)data.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));
 for(unsigned i=0;i<4;++i){data[48+i]=static_cast<std::uint8_t>(3U>>(8*i));data[52+i]=static_cast<std::uint8_t>(7U>>(8*i));}return QuestDefinitions::decode(data);
}
// This fixture retains the complete handoff object (including its Actor, scene,
// replica and pools), rather than allowing WorldServer's connection to own it.
// Lifecycle follows the native composition; persistence, transport recipients
// and inactive creation/duel providers remain test fixtures.
struct RetainedSession {
 HandoffApp app;WorldUsers users;WorldUser user;std::recursive_mutex user_mutex,zone_mutex;
 SocialCatalog social{11,42};VerticalSocialReplica social_db{11};
 struct SocialDatabase {
  bool active=false;unsigned commits=0;std::vector<std::pair<std::string,std::vector<storage::Parameter>>> calls;
  bool transaction_active() const{return active;}void begin(){active=true;}void commit(){active=false;++commits;}void rollback(){active=false;}
  void query(std::string sql,std::span<const storage::Parameter> args){calls.emplace_back(std::move(sql),std::vector<storage::Parameter>(args.begin(),args.end()));}
 } social_database;
 VerticalSocialLifecycle<SocialDatabase> social_lifecycle{social_db,app.character.replica(),social_database,user_mutex,"fixture."};
 std::vector<std::string> events;std::vector<std::uint32_t> jobs;std::vector<net::Packet> notifications;std::function<void(std::uint32_t)> on_db_post;unsigned simulated_timeout_attempts=0;
 UserLifecycleState db_state{11,2,0,true};
 struct Store {
  RetainedSession& s;
  UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>&){throw std::logic_error("unexpected periodic save");}
  UserCharacterSaveResult save_exit(CharacterReplica&,bool shutdown,std::string_view,const CharacterSaveCallbacks& callbacks,const std::function<void(std::string_view)>&){
   UserCharacterExitPort e;e.purchases_pending=[] {return false;};e.save_purchases=[] {return true;};
   e.attempt=[this](bool quick,bool recovery,bool& timeout){check(quick,"unexpected exit attempt");s.events.emplace_back("db-save");if(s.simulated_timeout_attempts){--s.simulated_timeout_attempts;timeout=true;return false;}check(!recovery,"unexpected successful recovery attempt");return true;};
   e.save_friends=callbacks.save_friends;e.save_blocks=callbacks.save_blocks;e.queue_exit=callbacks.queue_exit;e.exit=callbacks.exit;
   e.save_leave=[this]{s.events.emplace_back("save-leave");};return execute_user_character_exit(shutdown,e);
  }
 } store{*this};
 VerticalSaveJobBridge<Store> lifecycle;
 CharacterSaveLifecyclePorts lifecycle_ports(){
  CharacterSaveLifecyclePorts p;p.post=[this](std::uint32_t job){jobs.push_back(job);events.emplace_back("db-post");auto post=on_db_post;if(post)post(job);};
  p.send=[this](net::Packet packet){notifications.push_back(std::move(packet));};p.leave=[this]{events.emplace_back("db-leave");};
  p.reset=[]{};p.roster=[](bool){};p=social_lifecycle.bind(std::move(p));
  auto friends=p.save_friends,blocks=p.save_blocks;p.save_friends=[this,friends]{events.emplace_back("friends");friends();};p.save_blocks=[this,blocks]{events.emplace_back("blocks");blocks();};return p;
 }
 struct Skills final:UserSkillRuntimeServices {
  SkillDefinition definition_value;UserSkillDefinition definition_info{definition_value,"fixture"};
  Skills(){definition_info.reset_seconds=60;definition_value.id=11;definition_value.level=1;definition_value.effect=3;definition_value.keep_time_seconds=60;}
  const UserSkillDefinition* definition(std::uint16_t id,std::uint8_t level) override{return id==11 && level==1?&definition_info:nullptr;}
  UserSkillHandle allocate() override{return std::make_shared<UserSkillRuntimeSkill>();}
  void destroy(UserSkillHandle) noexcept override{} void strand(UserSkillHandle) noexcept override{}
  void invalid_definition(std::uint16_t,std::uint8_t) override{throw std::logic_error("unexpected skill definition");}
  void disconnect(std::uint32_t,std::uint32_t) override{throw std::logic_error("unexpected skill disconnect");}
 } skill_services;
 std::unique_ptr<UserSkillRuntime> skills;
 struct QuestPoolProvider final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override{return std::make_unique<RestoredQuest>();}} quest_pool_provider;
 QuestRuntimePool quest_pool{quest_pool_provider};QuestDefinitions quest_definitions=fixture_quest_definitions();QuestRuntime quests{11,quest_pool};
 VerticalNpcInteraction npc;ZoneLeaveShopState shop;std::uint32_t trade_timer=0;PartyRequestState party;ZoneLeaveRequestState requests;ZoneLeaveTailState tail;ZoneGuildCreation* creation=nullptr;
 struct Owners final:ZoneLeaveBoundOwners {
  RetainedSession& s;explicit Owners(RetainedSession& owner):s(owner){}
  std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
  ZoneLeaveShopState& shop(Actor&) override{return s.shop;}
  std::uint32_t& exchange_timer(Actor&) override{return s.trade_timer;}
 } owners;
 struct Creation final:ZoneGuildCreationServices {
  RetainedSession& s;explicit Creation(RetainedSession& owner):s(owner){}
  ZoneGuildCreation*& current(Actor&) override{return s.creation;}
  void broadcast(ZoneGuildCreation&,const net::Packet&) override{throw std::logic_error("unexpected creation");}
  void release(ZoneGuildCreation*) override{throw std::logic_error("unexpected creation release");}
 } creation_services;
 struct Duel final:DuelWorld {
  Actor* find_duel_user(Actor&,std::uint32_t) override{throw std::logic_error("unexpected duel");}
  UserPosition duel_position(const Actor&) override{throw std::logic_error("unexpected duel position");}
  std::int32_t drop_random() override{throw std::logic_error("unexpected duel random");}
  DuelDropItem* allocate_drop_item() override{throw std::logic_error("unexpected duel item");}
  bool enter_duel_item(Actor&,DuelDropItem&,bool) override{throw std::logic_error("unexpected duel item entry");}
  GameLogSubject duel_log_subject(const Actor&) override{throw std::logic_error("unexpected duel log");}
  std::chrono::sys_seconds local_calendar() override{throw std::logic_error("unexpected duel clock");}
  void send_duel_log(const net::Packet&) override{throw std::logic_error("unexpected duel log send");}
 } duel;
 struct Guild final:ZoneLeaveGuildDuelServices {
  std::shared_ptr<void> capture_guild_lock(Actor&) override{throw std::logic_error("unexpected guild");}
  void* current_guild(Actor&) override{throw std::logic_error("unexpected guild");}
  std::int32_t guild_duel_status(void*) override{throw std::logic_error("unexpected guild");}
  void leave_guild_duel(void*,Actor&,std::uint8_t) override{throw std::logic_error("unexpected guild");}
  GuildDuelState& duel_state(void*) override{throw std::logic_error("unexpected guild");}
  VerticalGuildDuelMovement& movement_state(Actor&) override{throw std::logic_error("unexpected guild");}
  Actor* find_in_zone(Actor&,std::uint32_t) override{throw std::logic_error("unexpected guild");}
  std::shared_ptr<void> retain(Actor&) override{throw std::logic_error("unexpected guild");}
 } guild;
 struct Disconnect final:VerticalDisconnectServices {
  RetainedSession& s;explicit Disconnect(RetainedSession& owner):s(owner){}
  std::recursive_mutex& user_mutex(WorldUser&) override{return s.user_mutex;}
  std::recursive_mutex& zone_mutex(RuntimeZone&) override{return s.zone_mutex;}
  void dead_disconnect(WorldUser&) override{s.events.emplace_back("dead");}
  std::uint32_t tick_ms() override{return 1234;}
  void prepare_save(WorldUser& user) override{
   s.events.emplace_back("prepare");
   struct Core final:VerticalSaveCoreSkillServices {
    RetainedSession& s;explicit Core(RetainedSession& owner):s(owner){}
    UserPosition position(Actor& actor) override{auto p=s.app.zone.users->position(actor.id());check(p.has_value(),"save lost live position");return *p;}
    std::uint32_t tick_ms() override{return 1234;}
    void lock_effects() override{s.user_mutex.lock();}
    void unlock_effects() override{s.user_mutex.unlock();}
    void lock_quests() override{s.user_mutex.lock();}
    void unlock_quests() override{s.user_mutex.unlock();}
    void send_database(const net::Packet& packet) override{check(s.lifecycle.apply(packet)==InventoryReplicaResult::applied,"replica rejected save update");}
   } core(s);
   std::array<std::uint8_t,775> scratch{};check(vertical_save_character(user.actor,s.app.pos,*s.skills,s.quests,s.app.character.quick_slots(),core,scratch)==1234,"save clock differs");
  }
  void request_save(WorldUser& u) override{s.events.emplace_back("save-request");auto packet=vertical_database_disconnect_packet(u.actor.account_id());check(s.lifecycle.disconnect(packet),"DB disconnect dispatch failed");}
  void leave_zone(WorldUser& u,RuntimeZone& z) override{
   ZoneLeaveBound binding(z,u,s.app.pos,s.owners,s.app.move_view);ZoneActorLeaveServices ports;
   ports.user_leave_lifecycle=[&](Actor& actor){check(&actor==&u.actor && actor.status().in_zone && !z.users->find_actor(actor.id()),"lifecycle membership order");
    zone_leave_user_lifecycle(actor,binding,s.npc,s.party,s.requests,s.creation_services,{},s.tail,s.duel,s.guild,[&](Actor& peer){return s.owners.retain(peer);},{});
    check(!s.npc.npc && s.app.npc.users_using==0 && actor.appearance().vehicle_mode==0 && actor.actions().vehicle_request==0 && s.tail.pending_147c==0 && s.tail.associated_1480==0,"session lifecycle state");s.events.emplace_back("zone-lifecycle");};
   ports.send_view=[&](CellCoordinates cell,const net::Packet& packet){check(cell.x==1 && cell.z==1 && packet==net::Packet({2,2,42,0,0,0}) && u.admission.zone==&z && u.actor.status().in_zone,"leave broadcast order");s.events.emplace_back("zone-view");};
   check(leave_zone_actor(z,u,s.app.pos,ports),"zone leave failed");s.events.emplace_back("zone");
  }
  void finish_final(WorldUser&) override{s.events.emplace_back("finish");}
 } disconnect;
 VerticalDatabaseAckUser ack{user,user_mutex};
 struct AckServices final:VerticalDatabaseAckServices {
  RetainedSession& s;explicit AckServices(RetainedSession& owner):s(owner){}
  VerticalDatabaseAckUser* find(std::uint32_t account) override{return account==s.app.actor.account_id()?&s.ack:nullptr;}
  void close(VerticalDatabaseAckUser&,std::uint32_t) override{throw std::runtime_error("unexpected open socket");}
  void leave(VerticalDatabaseAckUser& u) override{check(vertical_disconnect_in_zone(s.users,u.user,true,s.disconnect)==VerticalDisconnectResult::finalized,"final disconnect failed");}
 } ack_services;
 RetainedSession(std::shared_ptr<HandoffState> state,net::WorldChannel channel)
  :app(std::move(state),std::move(channel)),users({[](WorldUser&){},[this](WorldUser&){events.emplace_back("world");}}),user{app.actor,app.user},lifecycle(db_state,app.character.replica(),store,user_mutex,"native",lifecycle_ports()),owners(*this),creation_services(*this),disconnect(*this),ack_services(*this){
   auto loaded=vertical_load_social_selection(app.character.snapshot(),social_db);check(loaded.friends_loaded && loaded.blocks_loaded,"social fixture load failed");social=std::move(loaded.catalog);
   skills=std::make_unique<UserSkillRuntime>(std::shared_ptr<Actor>(&app.actor,[](Actor*){}),skill_services);
   check(skills->load(net::Packet{4,5,11,0,0,0,1,11,0,1,0,10,0},0)==UserSkillLoadResult::loaded,"skill setup failed");
   const std::array<PlayerSkill,1> learned{{PlayerSkill{11,10,1,0,0}}};const std::array<PersistedEffect,1> effects{{PersistedEffect{11,1,30}}};check(app.character.replica().skills().load(learned,effects),"replica skill setup failed");
   EffectTimer effect;effect.effect=effects[0];effect.end=11234;app.actor.effects().append(1,effect,skill_services.definition_value);
   RestoredQuest quest;quest.definition=quest_definitions.find(1);quest.packet.id=1;quest.packet.end_tick=121234;quest.packet.counters={3,4,5};check(quests.load_active(quest),"runtime quest setup failed");check(app.character.replica().quests().load({1,9,{0,0,0}}),"replica quest setup failed");
   npc.npc=&app.npc;app.npc.users_using=1;app.actor.appearance().vehicle_mode=1;app.actor.actions().vehicle_request=77;tail={88,99};
  }
};
struct Retention {std::shared_ptr<RetainedSession> owner;std::weak_ptr<RetainedSession> weak;bool forwarder_destroyed=false;};
struct Forwarder final:net::WorldApplication {
 std::shared_ptr<RetainedSession> session;std::shared_ptr<Retention> retention;
 Forwarder(std::shared_ptr<RetainedSession> s,std::shared_ptr<Retention> r):session(std::move(s)),retention(std::move(r)){}
 ~Forwarder() override {std::lock_guard lock(session->app.state->mutex);retention->forwarder_destroyed=true;session->app.state->cv.notify_all();}
 void open() override{session->app.open();}
 void receive(const net::Packet& packet) override{
  if(packet.size()>=2 && packet[0]==0x0b && packet[1]==1){
   check(session->app.character.quick_slots().receive(packet,session->app.actor.account_id(),[&](const net::Packet& p){check(session->lifecycle.apply(p)==InventoryReplicaResult::applied,"quick update rejected");},[&](std::uint32_t){session->app.channel.disconnect();})==QuickSlotUpdate::accepted,"quick packet failed");
   session->app.channel.send({0x7c,0x77});return;
  }
  try {session->app.receive(packet);} catch(const std::exception& e){std::cerr<<"application receive: "<<e.what()<<"\n";throw;}
  if(packet==net::Packet{4,1,42,0,0,0})session->users.enter(session->user);
 }
 void close() override{
  check(session->app.user.location==4,"fixture closed before zone entry");
  retention->owner=session;
  check(vertical_disconnect_in_zone(session->users,session->user,false,session->disconnect)==VerticalDisconnectResult::deferred,"disconnect did not defer");
  std::lock_guard lock(session->app.state->mutex);session->app.state->closed=true;session->app.state->cv.notify_all();
 }
};

#ifndef ETERNITY_DISCONNECT_FIXTURE_ONLY
int main(){try{
 asio::io_context io;auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<HandoffState>();auto retained=std::make_shared<Retention>();net::WorldServerOptions options;options.port=0;
 net::WorldServer server(io,options,registry,[state,retained](const session::LoginSession&,net::WorldChannel c){
  auto owner=std::make_shared<RetainedSession>(state,std::move(c));retained->weak=owner;return std::make_unique<Forwarder>(std::move(owner),retained);
 });server.start();std::jthread thread([&]{io.run();});
 struct Guard {net::WorldServer& server;std::jthread& thread;std::shared_ptr<HandoffState> state;~Guard(){{std::lock_guard lock(state->mutex);state->pause=true;state->first=true;state->second=true;state->cv.notify_all();}server.stop();if(thread.joinable())thread.join();}} guard{server,thread,state};
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});
 auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20,"greeting failed");
 auto send=[&](net::Packet p){outgoing.crypt(p);peer.write(net::frame(p));};send({4,1,42,0,0,0});auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0,42,0,0,0}),"selection ack");
 state->wait([&]{return state->waiting;});bool saw_data=false,saw_time=false;for(unsigned i=1;i<state->entry_packets;++i){auto packet=peer.packet();incoming.crypt(packet);check(packet.size()>=2,"truncated initial character packet");saw_data|=packet[0]==5 && packet[1]==1;saw_time|=packet[0]==0x14 && packet[1]==1;}check(saw_data && saw_time,"initial character data or clock not delivered");send({1,2});state->release(state->first);auto barrier=peer.packet();incoming.crypt(barrier);check(barrier==net::Packet({0x77,0x77,0}),"handoff barrier");
 for(int i=0;i<5;++i){auto packet=peer.packet();incoming.crypt(packet);}auto entered=peer.packet();incoming.crypt(entered);check(entered==net::Packet({0x79,0x77}),"entry failed");
 send({0x0b,1,1,2,3,4,0x34,0x12});auto quick_ack=peer.packet();incoming.crypt(quick_ack);check(quick_ack==net::Packet({0x7c,0x77}),"quick-slot TCP dispatch failed");
 asio::error_code ignored;peer.socket.close(ignored);state->wait([&]{return state->closed && retained->forwarder_destroyed;});
 auto owner=retained->owner;check(owner && owner->users.find(42)==&owner->user && owner->app.zone.users->find_actor(42)==&owner->app.actor && owner->app.user.logout_tick==11234,"closed socket lost retained actor");
 auto poll=[&](std::uint32_t tick){zone_update_logout_deadlines(*owner->app.zone.users,tick,[&](Actor&){return std::shared_ptr<void>(owner);},[&](Actor&)->WorldAdmissionUser&{return owner->app.user;},[&](Actor&,WorldAdmissionUser&){check(vertical_disconnect_in_zone(owner->users,owner->user,false,owner->disconnect)==VerticalDisconnectResult::save_requested,"save not requested");});};
 owner->app.actor.replace_resources_raw({123,456,789});
 poll(11233);check(owner->events.empty(),"early logout");poll(11234);const auto saved_core=owner->app.character.replica().core();check(saved_core.character.hp==123 && saved_core.character.mp==456 && saved_core.character.sp==789 && saved_core.character.map==20 && saved_core.x==40 && saved_core.z==40,"live save core did not reach replica");const auto saved_skills=owner->app.character.replica().skills().learned();check(saved_skills.size()==1 && saved_skills[0].delay==8,"live skill cooldown did not reach replica");const auto saved_effects=owner->app.character.replica().skills().effects();check(saved_effects.size()==1 && saved_effects[0].delay==10,"live effect timer did not reach replica");const auto saved_quests=owner->app.character.replica().quests().active();check(saved_quests.size()==1 && saved_quests[0].delay==2 && saved_quests[0].counters==std::array<std::uint8_t,3>{3,4,5},"live quest state did not reach replica");const auto quick=owner->app.character.replica().interface().quick_slots();check(quick.size()==1 && quick[0].bar==2 && quick[0].slot==3 && quick[0].bag==4 && quick[0].number==0x1234,"quick slots did not survive save");check(owner->events==std::vector<std::string>({"prepare","save-request","db-post"}) && owner->jobs==std::vector<std::uint32_t>{9},"save request ordering");
 check(owner->lifecycle.dispatch(owner->jobs.front()),"DB exit job rejected");
 check(owner->notifications==std::vector<net::Packet>({{2,3,11,0,0,0},{3,3,11,0,0,0}}),"DB exit notifications differ");
 check(vertical_database_disconnect_ack(owner->notifications.front(),owner->ack_services),"DB acknowledgment rejected");
 check(owner->events==std::vector<std::string>({"prepare","save-request","db-post","db-save","friends","blocks","save-leave","db-leave","world","zone-lifecycle","zone-view","zone","finish"}) && !owner->users.find(42) && owner->app.zone.users->size()==0,"final cleanup order");
 retained->owner.reset();owner.reset();check(retained->weak.expired(),"fixture session retained after final cleanup");
 std::cout<<"TCP close retains session through deadline, DB exit orchestration and final cleanup\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif
