#include "eternity/game/vertical_world_session_runtime.hpp"
#include "world_claim_fixture.hpp"
#define main unused_gate_main
#include "vertical_protocol_gate_unit.cpp"
#undef main
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_owned_selection.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_world_session_admission.hpp"
#include "eternity/game/vertical_world_session_ack.hpp"
#include "eternity/game/vertical_world_session_save_binding.hpp"
#include "eternity/game/vertical_world_session_sql_save.hpp"
#include "eternity/game/vertical_world_session_resource_open.hpp"
#include "eternity/game/vertical_world_session_preparation.hpp"
#include "eternity/game/vertical_world_session_save_teardown.hpp"
#include "eternity/game/vertical_social_world_owner.hpp"
#include "eternity/game/zone_actor_leave.hpp"
#include <asio/thread_pool.hpp>
#include <future>
#include <thread>
struct Owner {
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;EffectStatus status;
 Actor actor;
 Owner(std::uint32_t account,std::uint32_t character):actor(character,status,{1,1,1},{},scene,{items,enchant,mobs}){actor.account_id()=account;}
};

struct WorldSessionTestServices final:VerticalWorldSessionServices {
 std::promise<void>& closed;explicit WorldSessionTestServices(std::promise<void>& value):closed(value){}
 const ActorLoadRules& actor_rules() override {throw std::logic_error("unexpected actor rules");}
 ActorLoadOptions actor_options(const PlayerSnapshot&) override {throw std::logic_error("unexpected actor options");}
 VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot&) override {throw std::logic_error("unexpected actor scene");}
 void configure_admission(VerticalWorldSession&,WorldAdmissionUser&,const PlayerSnapshot&) override {throw std::logic_error("unexpected admission");}
 void prepare(VerticalWorldSession&,std::function<void(std::exception_ptr)>) override {throw std::logic_error("unexpected preparation");}
 void cancel_preparation(VerticalWorldSession&,std::function<void()>) override {throw std::logic_error("unexpected preparation cancellation");}
 CharacterEntryState entry_state(VerticalWorldSession&) override {throw std::logic_error("unexpected entry state");}
 const SocialCatalog& social(VerticalWorldSession&) override {throw std::logic_error("unexpected social");}
 ExperienceBounds experience(VerticalWorldSession&) override {throw std::logic_error("unexpected experience");}
 std::string guild_name(VerticalWorldSession&) override {throw std::logic_error("unexpected guild");}
 VerticalProtocolServices protocol(VerticalWorldSession&) override {throw std::logic_error("unexpected protocol");}
 void waiting(VerticalWorldSession&) override {throw std::logic_error("unexpected waiting");}
 void receive(VerticalWorldSession&,const eternity::net::Packet&) override {throw std::logic_error("unexpected packet");}
 void receive_nprotect(VerticalWorldSession&,const eternity::net::Packet&) override {throw std::logic_error("unexpected nprotect");}
 void unavailable(VerticalWorldSession&) override {throw std::logic_error("cancelled result delivered");}
 void failure(VerticalWorldSession&,std::exception_ptr) override {throw std::logic_error("unexpected failure");}
 void disconnect(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done) override {done();closed.set_value();}
};

struct WorldSessionSuccessServices final:VerticalWorldSessionServices {
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;StatusRules statuses;SkillDefinitions skills;
 QuestDefinitions quests=QuestDefinitions::decode(eternity::net::Packet{0,0,0,0});ActorLoadRules rules{statuses,items,enchant,skills,quests,mobs};
 SocialCatalog social_catalog{11,43};std::vector<eternity::net::Packet> sent;std::promise<void>& entered;std::promise<void>& closed;
 std::function<void(VerticalWorldSession&,std::function<void()>)> disconnect_override;
 std::function<void(VerticalWorldSession&,std::function<void(std::exception_ptr)>)> prepare_override;
 std::function<void(VerticalWorldSession&,std::function<void()>)> cancel_override;
 WorldSessionSuccessServices(std::promise<void>& a,std::promise<void>& b):entered(a),closed(b){}
 const ActorLoadRules& actor_rules() override{return rules;} ActorLoadOptions actor_options(const PlayerSnapshot&) override {ActorLoadOptions o;o.country=0;return o;}
 VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot&) override{return {{&scene,[](ActorScene*){}},[](Actor&){},[]{}};}
 void configure_admission(VerticalWorldSession&,WorldAdmissionUser& user,const PlayerSnapshot& p) override {check(user.map==p.core.character.map,"admission map missing");}
 void prepare(VerticalWorldSession& session,std::function<void(std::exception_ptr)> done) override {if(prepare_override)prepare_override(session,std::move(done));else done({});}
 void cancel_preparation(VerticalWorldSession& session,std::function<void()> done) override {if(cancel_override)cancel_override(session,std::move(done));else done();}
 CharacterEntryState entry_state(VerticalWorldSession& session) override {CharacterEntryState state;state.user_id=session.account_id();state.character_id=43;state.level=session.character()->snapshot().core.character.level;return state;}
 const SocialCatalog& social(VerticalWorldSession&) override{return social_catalog;} ExperienceBounds experience(VerticalWorldSession&) override{return {0,100};}
 std::string guild_name(VerticalWorldSession&) override{return {};}
 VerticalProtocolServices protocol(VerticalWorldSession&) override {VerticalProtocolServices p;
  p.delivery.send_self=[this](eternity::net::Packet packet){sent.push_back(std::move(packet));};p.delivery.send_peer=[](std::uint32_t,eternity::net::Packet){};
  p.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};p.delivery.tick=[]{return 1U;};p.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
  p.lifecycle=[](CharacterEntryStep,Actor&){};p.send_manager=[](eternity::net::Packet){};p.world_day=[]{return 0U;};p.system_time=[]{return 0U;};p.lock_guild=[]{return false;};p.unlock_guild=[]{};return p;}
 void waiting(VerticalWorldSession&) override {entered.set_value();} void receive(VerticalWorldSession&,const eternity::net::Packet&) override {throw std::logic_error("unexpected packet");}
 void receive_nprotect(VerticalWorldSession&,const eternity::net::Packet&) override {throw std::logic_error("unexpected nprotect");}
 void unavailable(VerticalWorldSession&) override {entered.set_exception(std::make_exception_ptr(std::runtime_error("selection unavailable")));}
 void failure(VerticalWorldSession&,std::exception_ptr error) override {entered.set_exception(error);}
 void disconnect(VerticalWorldSession& session,std::optional<std::uint32_t>,std::function<void()> done) override {if(disconnect_override){disconnect_override(session,std::move(done));return;}done();closed.set_value();}
};

struct LiveZoneDisconnect final:VerticalDisconnectServices {
 ZoneEntryState& position;std::recursive_mutex user_lock,zone_lock;unsigned saves=0,leaves=0,finalized=0;std::function<void()> on_save;
 explicit LiveZoneDisconnect(ZoneEntryState& p):position(p){}
 std::recursive_mutex& user_mutex(WorldUser&) override{return user_lock;}std::recursive_mutex& zone_mutex(RuntimeZone&) override{return zone_lock;}
 void dead_disconnect(WorldUser&) override{throw std::logic_error("unexpected dead disconnect");}std::uint32_t tick_ms() override{return 100;}
 void prepare_save(WorldUser&) override {}void request_save(WorldUser&) override {++saves;if(on_save)on_save();}
 void leave_zone(WorldUser& user,RuntimeZone& zone) override {ZoneActorLeaveServices ports;ports.user_leave_lifecycle=[](Actor&){};ports.send_view=[](CellCoordinates,const eternity::net::Packet&){};check(leave_zone_actor(zone,user,position,ports),"zone leave failed");++leaves;}
 void finish_final(WorldUser&) override {++finalized;}
};

struct SessionSocialDb {
 bool active=false;std::vector<std::string> calls;
 bool transaction_active() const{return active;}void begin(){active=true;}void commit(){active=false;}void rollback(){active=false;}
 void query(std::string sql,std::span<const eternity::storage::Parameter>){calls.push_back(std::move(sql));}
};
// Compile the concrete runtime installation against a real social owner and
// its matching save binding; live dispatch remains covered separately.
[[maybe_unused]] static void runtime_social_install_compile(
 VerticalWorldSessionRuntime<SessionSocialDb>& runtime,VerticalWorldSession& session,
 std::shared_ptr<VerticalSocialWorldOwner<SessionSocialDb>> owner) {
 runtime.install_social(session,std::move(owner));
 (void)runtime.social_lease<SessionSocialDb>(session);
}
// Compile the SQL resource path against the same real social/session types;
// opening SQL is deliberately outside this fixture.
[[maybe_unused]] static std::shared_ptr<VerticalWorldSessionSqlSave::Binding> sql_social_bind_compile(
 VerticalWorldSessionSqlSave& resource,VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
 std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,UserLifecycleState state,
 CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,
 VerticalWorldSessionSqlSave::Binding::Other other,VerticalWorldSessionSqlSave::Binding::Failure failure,
 std::shared_ptr<VerticalSocialSession<VerticalWorldSession,SessionSocialDb>> social,
 std::shared_ptr<VerticalSocialPacketJobs<VerticalWorldSession,SessionSocialDb>> jobs) {
 return resource.bind_social(strand,registry,std::move(session),dispatcher,state,"fixture",std::move(ports),
   std::move(router),std::move(other),std::move(failure),std::move(social),std::move(jobs));
}
[[maybe_unused]] static std::shared_ptr<VerticalWorldSessionSqlPreparation> sql_social_preparation_compile(
 VerticalWorldSession::Strand strand,asio::any_io_executor executor,VerticalWorldSession::Registry& registry,
 std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,eternity::storage::SqlServerConfig config,
 UserLifecycleState state,CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,
 VerticalWorldSessionSqlSave::Binding::Other other,VerticalWorldSessionSqlSave::Binding::Failure failure,
 std::function<VerticalPreparedSocialOwners<SessionSocialDb>(VerticalWorldSession&,std::shared_ptr<VerticalWorldSessionSqlSave>)> social) {
 return make_vertical_world_session_sql_social_preparation<SessionSocialDb>(strand,std::move(executor),registry,std::move(session),
  dispatcher,std::move(config),1,"fixture","PS_GameData",state,"fixture",std::move(ports),std::move(router),std::move(other),std::move(failure),std::make_shared<std::recursive_mutex>(),std::move(social));
}
struct SessionSaveStore {
 std::atomic<unsigned> exits{0};
 UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>&){throw std::logic_error("unexpected periodic save");}
 UserCharacterSaveResult save_exit(CharacterReplica&,bool,std::string_view,const CharacterSaveCallbacks& callbacks,const std::function<void(std::string_view)>&) {
  callbacks.save_friends();callbacks.save_blocks();if(++exits==1)callbacks.queue_exit();else callbacks.exit();return {};
 }
};

struct OpenedSessionResource {std::atomic<unsigned>& destroyed;std::function<void()> on_destroy;explicit OpenedSessionResource(std::atomic<unsigned>& value,std::function<void()> hook={}):destroyed(value),on_destroy(std::move(hook)){}~OpenedSessionResource(){if(on_destroy)on_destroy();++destroyed;}};

#ifndef ETERNITY_SESSION_REGISTRY_FIXTURE_ONLY
int main(){try {
 bool missing_path=false;try{(void)VerticalWorldSessionSqlSave::open({},1,{});}catch(const std::invalid_argument&){missing_path=true;}check(missing_path,"empty recovery path accepted");
 VerticalSessionRegistry<Owner> registry;auto first=std::make_shared<Owner>(11,42),second=std::make_shared<Owner>(11,43),same_character=std::make_shared<Owner>(12,42);
 std::weak_ptr<Owner> weak=first;registry.retain(first,first->actor);check(registry.find_account(11)==first && registry.retain_actor(first->actor)==first,"registered owner missing");
 bool rejected=false;try{registry.retain(second,second->actor);}catch(const std::logic_error&){rejected=true;}check(rejected && registry.size()==1,"duplicate account accepted");
 rejected=false;try{registry.retain(same_character,same_character->actor);}catch(const std::logic_error&){rejected=true;}check(rejected && registry.size()==1,"duplicate actor accepted");
 rejected=false;try{registry.retain(first,second->actor);}catch(const std::logic_error&){rejected=true;}check(rejected && registry.size()==1,"duplicate session accepted");
 check(!registry.retain_actor(same_character->actor),"foreign actor with same id retained");
 check(registry.begin_database_work(11,*first) && registry.begin_database_work(11,*first) && registry.pending_database_work(11)==2,"DB work was not tracked");
 check(!registry.release(11,*first) && !registry.finish_database_work(11,*second),"in-flight or foreign work released session");
 check(registry.finish_database_work(11,*first) && !registry.release(11,*first) && registry.finish_database_work(11,*first) && !registry.finish_database_work(11,*first),"DB work drain differs");
 auto pin=registry.find_account(11);check(registry.release(11,*first),"first owner release failed");registry.retain(second,second->actor);
 check(!registry.release(11,*first) && registry.find_account(11)==second,"stale owner removed replacement");first.reset();check(!weak.expired(),"callback pin did not retain old session");pin.reset();check(weak.expired(),"released session still retained");
 check(registry.release(11,*second) && !registry.find_account(11) && registry.size()==0,"replacement release failed");
 registry.reserve(11,second);check(registry.find_account(11)==second && !registry.retain_actor(second->actor),"preselection owner not reserved");
 rejected=false;try{registry.reserve(11,same_character);}catch(const std::logic_error&){rejected=true;}check(rejected,"pending selection allowed replacement");
 check(registry.begin_database_work(11,*second) && !registry.release(11,*second),"pending selection did not fence account");
 rejected=false;try{registry.bind_actor(11,*second,same_character->actor);}catch(const std::logic_error&){rejected=true;}check(rejected,"foreign actor bound to reserved account");
 registry.bind_actor(11,*second,second->actor);check(registry.retain_actor(second->actor)==second && !registry.release(11,*second),"actor binding lost selection fence");
 check(registry.finish_database_work(11,*second) && registry.release(11,*second),"drained selected owner did not release");
 registry.reserve(12,same_character);check(registry.release(12,*same_character),"cancelled preselection owner did not release");
 asio::io_context io;auto work=asio::make_work_guard(io);auto strand=asio::make_strand(io);asio::thread_pool db(1);std::jthread thread([&]{io.run();});
 CharacterRoster roster;roster.country=0;CharacterSummary item;item.id=43;roster.slots[0]=item;
 std::promise<void> started,release_worker,cancelled,drained;auto started_future=started.get_future();auto worker_gate=release_worker.get_future();auto cancelled_future=cancelled.get_future();auto drained_future=drained.get_future();
 std::shared_ptr<VerticalOwnedSelection<Owner>> selection;
 asio::post(strand,[&]{registry.reserve(11,second);selection=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
  [&](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{started.set_value();worker_gate.wait();return {};},
  [](Owner&,VerticalPersistenceSelection){throw std::runtime_error("cancelled selection delivered");},[](Owner&){throw std::runtime_error("cancelled selection unavailable");},[](Owner&,std::uint32_t){throw std::runtime_error("selection closed");},[](Owner&,std::exception_ptr){throw std::runtime_error("selection failed");});
  (void)selection->receive(eternity::net::Packet{4,1,43,0,0,0});});started_future.wait();
 asio::post(strand,[&]{check(!registry.release(11,*second),"cancel released active SQL selection");bool duplicate=false;try{registry.reserve(11,same_character);}catch(const std::logic_error&){duplicate=true;}check(duplicate,"reconnect overtook active selection");selection->cancel_and_drain([&]{check(registry.release(11,*second),"selection fence did not drain");registry.reserve(11,same_character);check(registry.release(11,*same_character),"replacement account not released");drained.set_value();});cancelled.set_value();});
 cancelled_future.wait();release_worker.set_value();drained_future.wait();
 std::promise<void> rejected_post;auto rejected_post_future=rejected_post.get_future();
 asio::post(strand,[&]{try {
  registry.reserve(11,second);
  auto rejected=std::make_shared<VerticalOwnedSelection<Owner>>(strand,asio::any_io_executor{},registry,second,CharacterSelectionState{11,0,true},roster,
   [](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::runtime_error("rejected post ran loader");},
   [](Owner&,VerticalPersistenceSelection){throw std::runtime_error("rejected post delivered");},[](Owner&){throw std::runtime_error("rejected post unavailable");},[](Owner&,std::uint32_t){throw std::runtime_error("rejected post closed");},[](Owner&,std::exception_ptr){throw std::runtime_error("rejected post fence failed");});
  bool failed=false;try{(void)rejected->receive(eternity::net::Packet{4,1,43,0,0,0});}catch(const asio::execution::bad_executor&){failed=true;}
  check(failed && !rejected->pending() && registry.pending_database_work(11)==0,"rejected post leaked selection fence");
  rejected->cancel_and_drain([&]{check(registry.release(11,*second),"rejected post blocked release");});rejected_post.set_value();
 }catch(...){rejected_post.set_exception(std::current_exception());}});rejected_post_future.get();
 std::promise<void> reentrant_done;auto reentrant_future=reentrant_done.get_future();
 asio::post(strand,[&]{try {
  registry.reserve(11,second);std::shared_ptr<VerticalOwnedSelection<Owner>> reentrant;
  auto token=std::make_shared<int>(1);std::weak_ptr<int> lifetime=token;
  reentrant=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
   [token](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::runtime_error("unlisted character loaded");},
   [](Owner&,VerticalPersistenceSelection){throw std::runtime_error("unlisted character delivered");},
   [&](Owner& owner){reentrant.reset();check(!lifetime.expired(),"selection destroyed during callback");check(registry.release(11,owner),"unlisted selection blocked release");},
   [](Owner&,std::uint32_t){throw std::runtime_error("unexpected close");},[](Owner&,std::exception_ptr){throw std::runtime_error("unexpected failure");});
  token.reset();auto* raw=reentrant.get();check(raw->receive(eternity::net::Packet{4,1,44,0,0,0})==CharacterSelectionResult::requested,"unlisted request differed");
  check(!reentrant && lifetime.expired(),"selection survived synchronous teardown");
  registry.reserve(11,second);token=std::make_shared<int>(1);lifetime=token;
  reentrant=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
   [token](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{return {};},
   [](Owner&,VerticalPersistenceSelection){throw std::runtime_error("idle selection delivered");},[](Owner&){throw std::runtime_error("idle selection unavailable");},[](Owner&,std::uint32_t){throw std::runtime_error("idle selection closed");},[](Owner&,std::exception_ptr){throw std::runtime_error("idle selection failed");});
  token.reset();raw=reentrant.get();raw->cancel_and_drain([&]{reentrant.reset();check(!lifetime.expired(),"selection destroyed during drain");check(registry.release(11,*second),"idle selection blocked release");});
  check(!reentrant && lifetime.expired(),"selection survived inline drain");reentrant_done.set_value();
 }catch(...){reentrant_done.set_exception(std::current_exception());}});reentrant_future.get();
 RuntimeZone runtime_zone;ZoneEntryState position;ZoneActorEntryServices entry;Services admission_services(runtime_zone,second->actor,position,entry);WorldAdmission world(admission_services);
 VerticalWorldSession::Registry sessions;sessions.bind_network_registry(world_claim_registry(),0);std::promise<void> world_started,world_release,world_closed,world_cancelled;auto world_started_future=world_started.get_future();auto world_release_future=world_release.get_future();auto world_closed_future=world_closed.get_future();auto world_cancelled_future=world_cancelled.get_future();
 WorldSessionTestServices session_services(world_closed);std::shared_ptr<VerticalWorldSession> live;
 std::weak_ptr<void> io_lease;
 std::vector<eternity::net::Packet> social_channel_sent;eternity::net::WorldChannel channel;channel.send=[&](eternity::net::Packet packet){social_channel_sent.push_back(std::move(packet));};channel.retain_io=[&]{auto lease=std::make_shared<int>(1);io_lease=lease;return lease;};channel.pause_input=[](auto callback){callback(true);};channel.resume_input=[]{};channel.set_admission_dispatch=[](bool){};channel.disconnect=[]{throw std::logic_error("unexpected disconnect");};channel.disconnect_with_reason=[](std::uint32_t){throw std::logic_error("unexpected close");};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,session_services,account,channel,roster,
   [&](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{world_started.set_value();world_release_future.wait();return {};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){world_started.set_exception(std::current_exception());}});world_started_future.get();
 asio::post(strand,[&]{try {live->close();check(sessions.pending_database_work(11)==1 && sessions.size()==1 && world.selection_count()==1 && !io_lease.expired(),"session released during SQL");world_cancelled.set_value();}catch(...){world_cancelled.set_exception(std::current_exception());}});world_cancelled_future.get();
 world_release.set_value();world_closed_future.get();std::promise<void> world_checked;auto world_checked_future=world_checked.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && world.selection_count()==0 && io_lease.expired(),"session drain retained account, admission user or IO lease");live.reset();world_checked.set_value();}catch(...){world_checked.set_exception(std::current_exception());}});world_checked_future.get();
 std::promise<void> entered_success,closed_success,zone_published;auto entered_success_future=entered_success.get_future();auto closed_success_future=closed_success.get_future();auto zone_published_future=zone_published.get_future();
 WorldSessionSuccessServices success_services(entered_success,closed_success);auto pool=std::make_shared<InventoryReplicaPool>();bool resumed=false,admission_dispatch=false;
 std::promise<void> preparation_started,preparation_release,preparation_checked;auto preparation_started_future=preparation_started.get_future();auto preparation_release_future=preparation_release.get_future();auto preparation_checked_future=preparation_checked.get_future();
 std::promise<void> binding_disposal_started,binding_disposal_release,binding_disposal_checked,binding_disposed,binding_alias_rejected;auto binding_disposal_started_future=binding_disposal_started.get_future();auto binding_disposal_release_future=binding_disposal_release.get_future();auto binding_disposal_checked_future=binding_disposal_checked.get_future();auto binding_disposed_future=binding_disposed.get_future();auto binding_alias_rejected_future=binding_alias_rejected.get_future();
 std::atomic<unsigned> prepared_destroyed{0};std::atomic<bool> binding_destroyed_on_world{true};std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> preparation;std::shared_ptr<OpenedSessionResource> binding_alias;
 success_services.prepare_override=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done){
  preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,owner.shared_from_this(),
   [&]{preparation_started.set_value();preparation_release_future.wait();return std::make_shared<OpenedSessionResource>(prepared_destroyed,[&]{binding_destroyed_on_world=strand.running_in_this_thread();binding_disposal_started.set_value();binding_disposal_release_future.wait();});},
   [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource> value){return value;});preparation->start(std::move(done));};
 success_services.cancel_override=[&](VerticalWorldSession&,std::function<void()> done){preparation->cancel(std::move(done),[](std::exception_ptr){throw std::logic_error("unexpected preparation cancellation failure");});};
 channel.resume_input=[&]{resumed=true;};channel.set_admission_dispatch=[&](bool enabled){admission_dispatch=enabled;};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,success_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;
    auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){entered_success.set_exception(std::current_exception());}});preparation_started_future.get();
 asio::post(strand,[&]{try {check(live->character() && !live->entry_state() && world.selection_count()==1 && world.waiting_count()==0 && success_services.sent.empty() && sessions.pending_database_work(11)==1,"entry escaped pending resource preparation");preparation_checked.set_value();}catch(...){preparation_checked.set_exception(std::current_exception());}});preparation_checked_future.get();preparation_release.set_value();entered_success_future.get();
 std::promise<void> preparation_failure;auto preparation_failure_future=preparation_failure.get_future();std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> failed_preparation;
 asio::post(strand,[&]{try {failed_preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,live,
  []()->std::shared_ptr<OpenedSessionResource>{throw std::runtime_error("fixture open failure");},
  [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>)->std::shared_ptr<OpenedSessionResource>{throw std::logic_error("failed open bound resource");});
  failed_preparation->start([&](std::exception_ptr error){try {check(bool(error) && sessions.pending_database_work(11)==0 && !failed_preparation->binding(),"preparation failure escaped fence or bound resource");preparation_failure.set_value();}catch(...){preparation_failure.set_exception(std::current_exception());}});
 }catch(...){preparation_failure.set_exception(std::current_exception());}});preparation_failure_future.get();
 std::promise<void> rejected_social_preparation;auto rejected_social_preparation_future=rejected_social_preparation.get_future();
 std::atomic<unsigned> rejected_social_resource_destroyed{0};
 using RejectedSocialBinding=VerticalWorldSessionSaveBinding<SessionSaveStore>;
 std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,RejectedSocialBinding>> rejected_social;
 asio::post(strand,[&]{try {
  rejected_social=make_vertical_world_session_social_preparation<OpenedSessionResource,SessionSaveStore,SessionSocialDb>(strand,db.get_executor(),sessions,live,
   [&]{return std::make_shared<OpenedSessionResource>(rejected_social_resource_destroyed);},
   [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>){return VerticalPreparedSocialOwners<SessionSocialDb>{};},
   [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>,VerticalPreparedSocialOwners<SessionSocialDb>)->std::shared_ptr<RejectedSocialBinding>{throw std::logic_error("invalid social owner reached binding");});
  rejected_social->start([&](std::exception_ptr error){try{check(bool(error) && !rejected_social->binding() && sessions.pending_database_work(11)==0 && rejected_social_resource_destroyed==1,"invalid social preparation escaped resource drain");rejected_social_preparation.set_value();}catch(...){rejected_social_preparation.set_exception(std::current_exception());}});
 }catch(...){rejected_social_preparation.set_exception(std::current_exception());}});rejected_social_preparation_future.get();
 std::promise<void> resource_started,resource_release,resource_cancelled,resource_drained;auto resource_started_future=resource_started.get_future();auto resource_release_future=resource_release.get_future();auto resource_cancelled_future=resource_cancelled.get_future();auto resource_drained_future=resource_drained.get_future();
 std::promise<void> disposal_started,disposal_release,disposal_checked;auto disposal_started_future=disposal_started.get_future();auto disposal_release_future=disposal_release.get_future();auto disposal_checked_future=disposal_checked.get_future();
 std::atomic<unsigned> resources_destroyed{0};std::atomic<bool> resource_destroyed_on_world{true};std::shared_ptr<VerticalWorldSessionResourceOpen<OpenedSessionResource>> resource_open;
 asio::post(strand,[&]{try {
  auto rejected=VerticalWorldSessionResourceOpen<OpenedSessionResource>::create(strand,asio::any_io_executor{},sessions,live,
   []()->std::shared_ptr<OpenedSessionResource>{throw std::runtime_error("rejected open ran");},
   [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>){throw std::runtime_error("rejected open delivered");},
   [](VerticalWorldSession&,std::exception_ptr){throw std::runtime_error("rejected open fence failed");});
  bool rejected_post=false;try{rejected->start();}catch(const asio::execution::bad_executor&){rejected_post=true;}
  check(rejected_post && !rejected->pending() && sessions.pending_database_work(11)==0,"rejected resource post leaked fence");bool rejected_drained=false;rejected->cancel_and_drain([&]{rejected_drained=true;});check(rejected_drained,"rejected resource post did not drain");
  resource_open=VerticalWorldSessionResourceOpen<OpenedSessionResource>::create(strand,db.get_executor(),sessions,live,
  [&]{resource_started.set_value();resource_release_future.wait();return std::make_shared<OpenedSessionResource>(resources_destroyed,[&]{resource_destroyed_on_world=strand.running_in_this_thread();disposal_started.set_value();disposal_release_future.wait();});},
  [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>){throw std::runtime_error("cancelled resource delivered");},
  [](VerticalWorldSession&,std::exception_ptr){throw std::runtime_error("resource open failed");});resource_open->start();
 }catch(...){resource_started.set_exception(std::current_exception());}});resource_started_future.get();
 asio::post(strand,[&]{try {resource_open->cancel_and_drain([&]{try {check(!resource_open->pending() && sessions.pending_database_work(11)==0 && resources_destroyed==1 && !resource_destroyed_on_world,"resource drain preceded worker discard or fence release");resource_drained.set_value();}catch(...){resource_drained.set_exception(std::current_exception());}});
  check(resource_open->pending() && sessions.pending_database_work(11)==1 && !sessions.release(11,*live),"cancelled resource open lost account fence");resource_cancelled.set_value();
 }catch(...){resource_cancelled.set_exception(std::current_exception());}});resource_cancelled_future.get();resource_release.set_value();disposal_started_future.get();
 asio::post(strand,[&]{try {check(resource_open->pending() && sessions.pending_database_work(11)==1 && resources_destroyed==0,"resource disposal did not retain fence");disposal_checked.set_value();}catch(...){disposal_checked.set_exception(std::current_exception());}});disposal_checked_future.get();disposal_release.set_value();resource_drained_future.get();
 asio::post(strand,[&]{try {binding_alias=preparation->binding();check(bool(binding_alias),"prepared binding missing");
  preparation->release_binding([&](std::exception_ptr error){try{check(bool(error) && preparation->binding()==binding_alias && sessions.pending_database_work(11)==0,"binding release accepted external alias");binding_alias_rejected.set_value();}catch(...){binding_alias_rejected.set_exception(std::current_exception());}});
 }catch(...){binding_alias_rejected.set_exception(std::current_exception());}});binding_alias_rejected_future.get();
 asio::post(strand,[&]{try {binding_alias.reset();preparation->release_binding([&](std::exception_ptr error){try{if(error)std::rethrow_exception(error);check(sessions.pending_database_work(11)==0 && prepared_destroyed==1 && !binding_destroyed_on_world && !preparation->binding(),"binding release completed before worker disposal");binding_disposed.set_value();}catch(...){binding_disposed.set_exception(std::current_exception());}});}catch(...){binding_disposed.set_exception(std::current_exception());}});binding_disposal_started_future.get();
 asio::post(strand,[&]{try{check(sessions.pending_database_work(11)==1 && !sessions.release(11,*live) && prepared_destroyed==0,"binding disposal lost its account fence");binding_disposal_checked.set_value();}catch(...){binding_disposal_checked.set_exception(std::current_exception());}});binding_disposal_checked_future.get();binding_disposal_release.set_value();binding_disposed_future.get();
 asio::post(strand,[&]{try {check(live->entry_state() && live->entry_state()->ready && live->admission().location==2 && world.waiting_count()==1,"entry state or world wait not retained");
  auto* actor=live->character()->pipeline().result().actor.get();check(actor && sessions.retain_actor(*actor)==live && live->world_user() && &live->world_user()->actor==actor && &live->world_user()->admission==&live->admission(),"actor or world user was not bound to session");
  check(!resumed && !admission_dispatch,"input resumed before zone publication");world.update_packets();check(live->admission().location==3 && world.zone_waiting_count(runtime_zone)==1,"zone publication failed");live->zone_ready();check(resumed && admission_dispatch,"zone-ready handoff failed");
  check(!success_services.sent.empty() && success_services.sent.front()==eternity::net::Packet({4,1,0,43,0,0,0}),"selection acknowledgement missing");live->close();zone_published.set_value();
 }catch(...){zone_published.set_exception(std::current_exception());}});zone_published_future.get();closed_success_future.get();
 std::promise<void> success_checked;auto success_checked_future=success_checked.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && world.zone_waiting_count(runtime_zone)==0,"successful session did not drain");resource_open.reset();failed_preparation.reset();rejected_social.reset();check(!preparation->binding() && prepared_destroyed==1,"prepared resource not released");preparation.reset();live.reset();success_checked.set_value();}catch(...){success_checked.set_exception(std::current_exception());}});success_checked_future.get();
 std::promise<void> cancel_entered,cancel_closed,cancel_opened,cancel_release,cancel_checked,cancel_cleanup;auto cancel_closed_future=cancel_closed.get_future();auto cancel_opened_future=cancel_opened.get_future();auto cancel_release_future=cancel_release.get_future();auto cancel_checked_future=cancel_checked.get_future();auto cancel_cleanup_future=cancel_cleanup.get_future();
 std::atomic<unsigned> cancel_destroyed{0};std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> cancelled_preparation;
 WorldSessionSuccessServices cancelling_services(cancel_entered,cancel_closed);
 cancelling_services.prepare_override=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done){
  cancelled_preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,owner.shared_from_this(),
   [&]{cancel_opened.set_value();cancel_release_future.wait();return std::make_shared<OpenedSessionResource>(cancel_destroyed);},
   [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource> value){return value;});cancelled_preparation->start(std::move(done));};
 cancelling_services.cancel_override=[&](VerticalWorldSession&,std::function<void()> done){cancelled_preparation->cancel(std::move(done),[](std::exception_ptr){throw std::logic_error("unexpected preparation cancellation failure");});};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,cancelling_services,account,channel,roster,
  [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){cancel_opened.set_exception(std::current_exception());}});cancel_opened_future.get();
 asio::post(strand,[&]{try {live->close();check(sessions.pending_database_work(11)==1 && sessions.size()==1 && world.selection_count()==1 && cancelling_services.sent.empty(),"disconnect bypassed preparation drain");cancel_checked.set_value();}catch(...){cancel_checked.set_exception(std::current_exception());}});cancel_checked_future.get();cancel_release.set_value();cancel_closed_future.get();
 asio::post(strand,[&]{try {check(cancel_destroyed==1 && sessions.size()==0 && world.selection_count()==0 && cancelling_services.sent.empty(),"cancelled preparation delivered or leaked owner");cancelled_preparation.reset();live.reset();cancel_cleanup.set_value();}catch(...){cancel_cleanup.set_exception(std::current_exception());}});cancel_cleanup_future.get();
 ConfiguredMap live_map;live_map.settings.id=20;live_map.definition.size=81;RuntimeZone live_zone;live_zone.map=&live_map;live_zone.users=std::make_unique<ZoneUsers>(live_map.definition.geometry(),0);live_zone.environment=ZoneEnvironment::prepare(live_map);
 WorldUsers world_users({[](WorldUser&){},[](WorldUser&){}});
 ZoneActorEntryServices live_entry;live_entry.send_view=[](CellCoordinates,eternity::net::Packet){};live_entry.cell_info=[](CellCoordinates){};live_entry.obelisks=[]{};live_entry.nprotect=[]{};
 live_entry.diagnostic=[]{throw std::logic_error("unexpected zone diagnostic");};live_entry.close=[](std::uint32_t){throw std::logic_error("unexpected zone close");};
 unsigned admission_packets=0;bool close_during_pass=false,close_during_select=false;VerticalWorldSessionAdmissionPorts admission_ports;
 admission_ports.packets=[&](VerticalWorldSession& owner){check(&owner==live.get(),"wrong packet owner");++admission_packets;if(close_during_pass){owner.close();check(sessions.find_account(11).get()==&owner,"session released during admission callback");}};
 admission_ports.select_zone=[&](VerticalWorldSession& owner){check(&owner==live.get(),"wrong zone owner");if(close_during_select){owner.close();check(sessions.find_account(11).get()==&owner,"selection callback released session during pass");}return &live_zone;};
 admission_ports.entry_services=[&](VerticalWorldSession& owner,RuntimeZone& zone){check(&owner==live.get() && &zone==&live_zone,"wrong entry services");return live_entry;};
 admission_ports.diagnostic=[](VerticalWorldSession&,WorldAdmissionError){throw std::logic_error("unexpected admission diagnostic");};
 admission_ports.rank_window=[](RuntimeZone&)->std::optional<std::uint32_t>{throw std::logic_error("unexpected rank");};
 admission_ports.lock_guild=[](VerticalWorldSession&)->bool{throw std::logic_error("unexpected guild");};admission_ports.unlock_guild=[](VerticalWorldSession&){throw std::logic_error("unexpected guild");};
 VerticalWorldSessionAdmissionServices live_admission(strand,sessions,world_users,std::move(admission_ports));WorldAdmission live_world(live_admission);std::unique_ptr<LiveZoneDisconnect> disconnect_services;
 std::promise<void> entered_zone,closed_zone,zone_entered,zone_finished;auto entered_zone_future=entered_zone.get_future();auto closed_zone_future=closed_zone.get_future();auto zone_entered_future=zone_entered.get_future();auto zone_finished_future=zone_finished.get_future();
 WorldSessionSuccessServices live_services(entered_zone,closed_zone);std::function<void()> disconnect_done;
 live_services.disconnect_override=[&](VerticalWorldSession& session,std::function<void()> done){check(session.disconnect_in_zone(world_users,false,*disconnect_services)==VerticalDisconnectResult::deferred,"world logout did not defer");disconnect_done=std::move(done);};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,live_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;
    auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){entered_zone.set_exception(std::current_exception());}});entered_zone_future.get();
 asio::post(strand,[&]{try {disconnect_services=std::make_unique<LiveZoneDisconnect>(live->zone_entry());WorldAdmissionUser foreign;foreign.user_id=11;bool rejected_foreign=false;try{(void)live_admission.select_zone(foreign);}catch(const std::logic_error&){rejected_foreign=true;}check(rejected_foreign,"foreign admission user accepted");
  live_admission.update_world(live_world);live_admission.packets(live->admission());check(admission_packets==1,"scheduler packet dispatch lost owner");live->zone_ready();check(live->receive_zone_wait(eternity::net::Packet{1,2})==VerticalEntryRequest::entered,"session zone entry failed");
  check(live->admission().location==4 && live_zone.users->find_actor(43)==&live->world_user()->actor && world_users.find(43)==live->world_user(),"world and zone indexes lost actual user");
  live->close();check(disconnect_done && sessions.size()==1 && live->admission().logout_tick==10100 && live_zone.users->find_actor(43),"deferred logout released owner");zone_entered.set_value();
 }catch(...){zone_entered.set_exception(std::current_exception());}});zone_entered_future.get();
 DbJobDispatcher save_dispatcher(false,1);using SessionBinding=VerticalWorldSessionSaveBinding<SessionSaveStore>;using SavePreparation=VerticalWorldSessionPreparation<SessionSaveStore,SessionBinding>;using SaveTeardown=VerticalWorldSessionSaveTeardown<SessionSaveStore,SessionSaveStore>;
 std::shared_ptr<SavePreparation> save_preparation;std::shared_ptr<SaveTeardown> save_teardown;
 std::shared_ptr<VerticalWorldSessionAckRouter> ack_router;UserLifecycleState db_state{11,2,0,true};auto save_store=std::make_shared<SessionSaveStore>();auto save_mutex=std::make_shared<std::recursive_mutex>();std::atomic<unsigned> database_leave{0};unsigned other_replies=0;
 using SessionSocial=VerticalSocialSession<VerticalWorldSession,SessionSocialDb>;using SessionSocialJobs=VerticalSocialPacketJobs<VerticalWorldSession,SessionSocialDb>;
 auto social_db=std::make_shared<SessionSocialDb>();auto social_account=std::make_shared<VerticalWorldAccountSocial>(11);save_mutex=social_account->shared_mutex();auto social_replica=std::shared_ptr<VerticalSocialReplica>(social_account,&social_account->replica());social_replica->friends()[0].id=70;social_replica->friends()[0].name="Retained";auto social_requests=std::make_shared<ZoneLeaveRequestState>();std::shared_ptr<VerticalSocialWorldOwner<SessionSocialDb>> social_world;std::shared_ptr<SessionSocial> social;std::weak_ptr<SessionSocial> social_weak;std::shared_ptr<SessionSocialJobs> social_jobs;
 asio::post(strand,[&]{try {
  check(live->database_ack_user() && !live->database_ack_user()->socket_open,"socket closure not reflected in ack owner");
  ack_router=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[&](std::shared_ptr<VerticalWorldSession> retained){
   check(retained==live && sessions.pending_database_work(11)==1,"ack lost live session or active fence");
   check(retained->disconnect_in_zone(world_users,true,*disconnect_services)==VerticalDisconnectResult::finalized && disconnect_services->leaves==1 && disconnect_services->finalized==1,"final disconnect did not leave zone");
   check(!world_users.find(43) && !live_zone.users->find_actor(43) && sessions.size()==1,"finalized membership or fence differs");
   save_teardown->start([&]{try {check(sessions.pending_database_work(11)==0 && other_replies==1 && save_store->exits==2 && database_leave==1 && social_db->calls.size()==1 && social_db->calls[0].find("usp_Create_FriendChar_E")!=std::string::npos && !save_preparation->binding() && !social_weak.expired(),"save delivery or teardown drained early");social_jobs.reset();social.reset();social_world.reset();check(social_weak.expired(),"social owner survived teardown");disconnect_done();disconnect_done={};check(sessions.size()==0,"disconnect completion retained session");zone_finished.set_value();}catch(...){zone_finished.set_exception(std::current_exception());}});
  });
  CharacterSaveLifecyclePorts ports;
  ports.diagnostic=[](std::string_view){throw std::runtime_error("social persistence failed");};
  ports.send=[](eternity::net::Packet){throw std::logic_error("uncollected DB reply");};ports.leave=[&]{++database_leave;};ports.reset=[]{throw std::logic_error("unexpected DB reset");};ports.roster=[](bool){throw std::logic_error("unexpected DB roster");};ports.save_friends=[]{};ports.save_blocks=[]{};
  save_preparation=make_vertical_world_session_social_preparation<SessionSaveStore,SessionSaveStore,SessionSocialDb>(strand,db.get_executor(),sessions,live,[save_store]{return save_store;},
   [&](VerticalWorldSession& owner,std::shared_ptr<SessionSaveStore> store) {
    check(&owner==live.get() && store==save_store,"social preparation lost resource");
    SessionSocial::Services services{[&](VerticalWorldSession& peer)->std::shared_ptr<SessionSocial>{if(&peer!=live.get() || !social_world)return {};return {social_world,&social_world->social()};},[]{return 100U;},[&](eternity::net::Packet packet){if(!social_jobs || !social_jobs->submit(std::move(packet)))throw std::runtime_error("social transport rejected");}};
    auto before_social=sessions.pending_database_work(11);bool wrong_account=false;
    try{(void)VerticalSocialWorldOwner<SessionSocialDb>::create(strand,owner.shared_from_this(),std::make_shared<VerticalSocialReplica>(99),social_db,save_mutex,social_requests,sessions,world_users,services,save_dispatcher,[&](std::exception_ptr error){zone_finished.set_exception(error);},"test.");}catch(const std::invalid_argument&){wrong_account=true;}
    check(wrong_account && sessions.pending_database_work(11)==before_social,"foreign social account accepted");
    VerticalWorldSessionSocial selection(social_account,owner.character()->snapshot());
    social_world=VerticalSocialWorldOwner<SessionSocialDb>::create_for_session(strand,owner.shared_from_this(),selection,social_db,sessions,world_users,std::move(services),save_dispatcher,[&](std::exception_ptr error){zone_finished.set_exception(error);},"test.");
    auto prepared=social_world->prepared();social=prepared.social;social_jobs=prepared.packets;
    check(&selection.catalog()==&social->catalog() && social_jobs->uses_save_mutex(social_account->mutex()),"runtime and social owners differ");
    check(social->catalog().character_id()==43 && social->catalog().find(70) && !social_world->friends_loaded() && !social_world->blocks_loaded(),"social selection state differed");return prepared;},
   [&,ports=std::move(ports)](VerticalWorldSession& owner,std::shared_ptr<SessionSaveStore> store,VerticalPreparedSocialOwners<SessionSocialDb> prepared) mutable {
    check(prepared.social==social && prepared.packets==social_jobs,"social preparation lost owners");
    auto pending_before=sessions.pending_database_work(11);bool wrong_mutex=false;try{(void)SessionBinding::create_social(strand,sessions,owner.shared_from_this(),save_dispatcher,store,std::make_shared<std::recursive_mutex>(),db_state,"fixture",ports,ack_router,
      [](VerticalWorldSession&,eternity::net::Packet){},[](std::exception_ptr){},social,social_jobs);}catch(const std::invalid_argument&){wrong_mutex=true;}
    check(wrong_mutex && sessions.pending_database_work(11)==pending_before,"foreign social mutex accepted");
    auto binding=SessionBinding::create_social(strand,sessions,owner.shared_from_this(),save_dispatcher,std::move(store),save_mutex,db_state,"fixture",std::move(ports),ack_router,
     [&](VerticalWorldSession& session,eternity::net::Packet packet){check(&session==live.get() && packet==eternity::net::Packet({3,3,11,0,0,0}),"unexpected non-ack DB reply");++other_replies;},
     [&](std::exception_ptr error){zone_finished.set_exception(error);},social,social_jobs);
    bool duplicate=false;try{binding->attach_social(sessions,social,social_jobs,[](std::exception_ptr){});}catch(const std::logic_error&){duplicate=true;}check(duplicate,"duplicate social fence accepted");return binding;});
  save_preparation->start([&](std::exception_ptr error){try {if(error)std::rethrow_exception(error);
   save_teardown=SaveTeardown::create(strand,save_preparation,[&](std::exception_ptr failure){zone_finished.set_exception(failure);});
   check(social_jobs->submit(eternity::net::Packet{2,12,11,0,0,0,44,0,0,0,1,2,3,'F',0}),"social mutation rejected");
   social_weak=social;social.reset();check(!social_weak.expired(),"binding did not retain social callbacks");
   disconnect_services->on_save=[&]{auto binding=save_preparation->binding();check(binding && binding->disconnect(vertical_database_disconnect_packet(11)),"save disconnect packet rejected");};
   check(live->disconnect_in_zone(world_users,false,*disconnect_services)==VerticalDisconnectResult::save_requested && disconnect_services->saves==1,"logout save not requested");
   check(sessions.size()==1 && sessions.pending_database_work(11)>=1 && live_zone.users->find_actor(43),"save request released owner");
  }catch(...){zone_finished.set_exception(std::current_exception());}});
 }catch(...){zone_finished.set_exception(std::current_exception());}});zone_finished_future.get();
 save_dispatcher.wait_idle();std::promise<void> save_cleanup;auto save_cleaned=save_cleanup.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && save_store->exits==2,"save session remained registered");std::weak_ptr<VerticalWorldSession> retained=live;live.reset();check(!retained.expired(),"save preparation lost its session");save_teardown.reset();save_preparation.reset();ack_router.reset();check(retained.expired(),"drained save preparation leaked its session");save_cleanup.set_value();}catch(...){save_cleanup.set_exception(std::current_exception());}});save_cleaned.get();save_dispatcher.shutdown();check(!save_dispatcher.failure(),"save dispatcher failed");
 std::promise<void> pass_closed,pass_test;auto pass_closed_future=pass_closed.get_future();auto pass_test_future=pass_test.get_future();WorldSessionTestServices pass_services(pass_closed);
 asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,pass_services,account,channel,roster,
   [](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::logic_error("unexpected selection");});
  live->open();close_during_pass=true;live_admission.update_world(live_world);close_during_pass=false;
  check(sessions.size()==0 && live_world.selection_count()==0,"admission pass did not finalize deferred release");live.reset();pass_test.set_value();
 }catch(...){pass_test.set_exception(std::current_exception());}});pass_test_future.get();pass_closed_future.get();
 std::promise<void> select_entered,select_closed,select_checked;auto select_entered_future=select_entered.get_future();auto select_closed_future=select_closed.get_future();auto select_checked_future=select_checked.get_future();WorldSessionSuccessServices select_services(select_entered,select_closed);
 select_services.disconnect_override=[&](VerticalWorldSession& owner,std::function<void()> done){check(owner.world_user() && world_users.find(43)==owner.world_user(),"selection disconnect lost world user");world_users.leave(*owner.world_user());done();select_closed.set_value();};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,select_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){select_entered.set_exception(std::current_exception());}});select_entered_future.get();
 asio::post(strand,[&]{try {close_during_select=true;live_admission.update_world(live_world);close_during_select=false;
  check(sessions.size()==0 && live_world.waiting_count()==0 && live_world.zone_waiting_count(live_zone)==0 && !world_users.find(43),"selection-pass disconnect left a dangling admission user");live.reset();select_checked.set_value();
 }catch(...){select_checked.set_exception(std::current_exception());}});select_checked_future.get();select_closed_future.get();
 db.join();selection.reset();work.reset();thread.join();
 std::cout<<"world session duplicate and stale completion guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif
