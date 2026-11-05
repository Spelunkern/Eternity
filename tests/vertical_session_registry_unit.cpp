#include "eternity/game/vertical_world_session_zone_services.hpp"
#include "eternity/game/vertical_world_session_sql_preparation_provider.hpp"
#include "eternity/game/vertical_world_session_owner.hpp"
#include <barrier>
#include <array>
#include <atomic>
#include "world_claim_fixture.hpp"
#define main unused_gate_main
#include "vertical_protocol_gate_unit.cpp"
#undef main
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_owned_selection.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_world_session_admission.hpp"
#include "eternity/game/vertical_world_session_packets.hpp"
#include "eternity/game/vertical_world_session_gameplay.hpp"
#include "eternity/game/vertical_world_session_packet_router.hpp"
#include "eternity/game/vertical_world_session_runtime.hpp"
#include "eternity/game/vertical_world_social_accounts.hpp"
#include "eternity/game/vertical_world_guild_transport.hpp"
#include "eternity/game/vertical_world_session_sql_selection.hpp"
#include "eternity/game/vertical_world_actor_scene.hpp"
#include "eternity/game/vertical_world_party_directory.hpp"
#include "eternity/game/vertical_world_party_requests.hpp"
#include "eternity/game/vertical_world_party_registry.hpp"
#include "eternity/game/vertical_world_party_membership.hpp"
#include "eternity/game/vertical_world_send_buffer_pool.hpp"
#include "eternity/game/vertical_world_party_runtime.hpp"
#include <bit>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "eternity/game/vertical_world_session_ack.hpp"
#include "eternity/game/vertical_world_session_save_binding.hpp"
#include "eternity/game/vertical_world_session_sql_save.hpp"
#include "eternity/game/vertical_world_session_resource_open.hpp"
#include "eternity/game/vertical_world_session_preparation.hpp"
#include "eternity/game/vertical_world_session_save_teardown.hpp"
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
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;std::weak_ptr<Scene> scene;Actor* scene_actor=nullptr;unsigned scene_binds=0,scene_unbinds=0;StatusRules statuses;SkillDefinitions skills;
 QuestDefinitions quests=QuestDefinitions::decode(eternity::net::Packet{0,0,0,0});ActorLoadRules rules{statuses,items,enchant,skills,quests,mobs};
 SocialCatalog social_catalog{11,43};std::vector<eternity::net::Packet> sent;std::promise<void>& entered;std::promise<void>& closed;
 std::function<void(VerticalWorldSession&,std::function<void()>)> disconnect_override;
 std::function<void(VerticalWorldSession&,const eternity::net::Packet&)> receive_override;
 std::function<void(VerticalWorldSession&,std::function<void(std::exception_ptr)>)> prepare_override;
 std::function<void(VerticalWorldSession&,std::function<void()>)> cancel_override;
 std::function<void()> on_options,on_rules,on_scene,on_bind,on_configure,on_entry_state,on_protocol,on_social,on_experience,on_send,on_manager,on_lifecycle,on_lock; std::function<void(std::exception_ptr)> failure_override; unsigned unlocks=0;
 WorldSessionSuccessServices(std::promise<void>& a,std::promise<void>& b,std::uint32_t account=11,std::uint32_t character=43):social_catalog(account,character),entered(a),closed(b){}
 const ActorLoadRules& actor_rules() override{if(on_rules)on_rules();return rules;} ActorLoadOptions actor_options(const PlayerSnapshot&) override {if(on_options)on_options();ActorLoadOptions o;o.country=0;return o;}
 VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot& snapshot) override{if(on_scene)on_scene();auto value=std::make_shared<Scene>();scene=value;const auto id=snapshot.core.character.id;return {value,[this,id](Actor& actor){check(!scene_actor && actor.id()==id,"incorrect scene binding");scene_actor=&actor;++scene_binds;if(on_bind)on_bind();},[this]{check(scene_actor && !scene_actor->status().in_zone,"scene unbound while attached");scene_actor=nullptr;++scene_unbinds;}};}
 void configure_admission(VerticalWorldSession&,WorldAdmissionUser& user,const PlayerSnapshot& p) override {check(user.map==p.core.character.map,"admission map missing");if(on_configure)on_configure();}
 void prepare(VerticalWorldSession& session,std::function<void(std::exception_ptr)> done) override {if(prepare_override)prepare_override(session,std::move(done));else done({});}
 void cancel_preparation(VerticalWorldSession& session,std::function<void()> done) override {if(cancel_override)cancel_override(session,std::move(done));else done();}
 CharacterEntryState entry_state(VerticalWorldSession& session) override {if(on_entry_state)on_entry_state();CharacterEntryState state;state.user_id=session.account_id();state.character_id=session.character()->snapshot().core.character.id;state.level=session.character()->snapshot().core.character.level;return state;}
 const SocialCatalog& social(VerticalWorldSession&) override{if(on_social)on_social();return social_catalog;} ExperienceBounds experience(VerticalWorldSession&) override{if(on_experience)on_experience();return {0,100};}
 std::string guild_name(VerticalWorldSession&) override{return {};}
 VerticalProtocolServices protocol(VerticalWorldSession&) override {if(on_protocol)on_protocol();VerticalProtocolServices p;
  p.delivery.send_self=[this](eternity::net::Packet packet){sent.push_back(std::move(packet));if(on_send)on_send();};p.delivery.send_peer=[](std::uint32_t,eternity::net::Packet){};
  p.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};p.delivery.tick=[]{return 1U;};p.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
  p.lifecycle=[this](CharacterEntryStep,Actor&){if(on_lifecycle)on_lifecycle();};p.send_manager=[this](eternity::net::Packet){if(on_manager)on_manager();};p.world_day=[]{return 0U;};p.system_time=[]{return 0U;};p.lock_guild=[this]{if(on_lock){on_lock();return true;}return false;};p.unlock_guild=[this]{++unlocks;};return p;}
 void waiting(VerticalWorldSession&) override {entered.set_value();} void receive(VerticalWorldSession& owner,const eternity::net::Packet& packet) override {if(receive_override)receive_override(owner,packet);else throw std::logic_error("unexpected packet");}
 void receive_nprotect(VerticalWorldSession&,const eternity::net::Packet&) override {throw std::logic_error("unexpected nprotect");}
 void unavailable(VerticalWorldSession&) override {entered.set_exception(std::make_exception_ptr(std::runtime_error("selection unavailable")));}
 void failure(VerticalWorldSession&,std::exception_ptr error) override {if(failure_override)failure_override(error);else entered.set_exception(error);}
 void disconnect(VerticalWorldSession& session,std::optional<std::uint32_t>,std::function<void()> done) override {if(disconnect_override){disconnect_override(session,std::move(done));return;}done();closed.set_value();}
};

struct ZoneSaveSkillFixture final:UserSkillRuntimeServices {
 SkillDefinition effect;UserSkillDefinition info{effect,"fixture"};
 ZoneSaveSkillFixture(){info.reset_seconds=60;effect.id=11;effect.level=1;effect.effect=3;effect.keep_time_seconds=60;}
 const UserSkillDefinition* definition(std::uint16_t id,std::uint8_t level) override{return id==11 && level==1?&info:nullptr;}
 UserSkillHandle allocate() override{return std::make_shared<UserSkillRuntimeSkill>();}
 void destroy(UserSkillHandle) noexcept override{} void strand(UserSkillHandle) noexcept override{}
 void invalid_definition(std::uint16_t,std::uint8_t) override{throw std::logic_error("unexpected skill definition");}
 void disconnect(std::uint32_t,std::uint32_t) override{throw std::logic_error("unexpected skill disconnect");}
};
struct ZoneSaveQuestPoolFixture final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override{return std::make_unique<RestoredQuest>();}};
struct ZoneSaveSkills {ZoneSaveSkillFixture services;UserSkillRuntime runtime;explicit ZoneSaveSkills(std::shared_ptr<Actor> actor):runtime(std::move(actor),services){}};
QuestDefinitions zone_save_quest_definitions(){
 std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"data/quests-oracle.tsv");std::string line,hex;
 while(std::getline(file,line))if(line.starts_with("S\t")){std::istringstream row(line);char tag;row>>tag>>hex;break;}
 check(!hex.empty(),"quest fixture missing");eternity::net::Packet data;for(std::size_t i=0;i<hex.size();i+=2)data.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));
 for(unsigned i=0;i<4;++i){data[48+i]=static_cast<std::uint8_t>(3U>>(8*i));data[52+i]=static_cast<std::uint8_t>(7U>>(8*i));}return QuestDefinitions::decode(data);
}
struct ZoneSaveQuests {QuestDefinitions definitions=zone_save_quest_definitions();ZoneSaveQuestPoolFixture services;QuestRuntimePool pool{services};QuestRuntime runtime{11,pool};};
struct ZoneSaveWorldFixture final:VerticalSaveCoreSkillServices {
 std::recursive_mutex mutex;unsigned bypass=0;std::function<void()> on_position;
 UserPosition position(Actor&) override{if(on_position)on_position();return {40,0,40};}std::uint32_t tick_ms() override{return 100;}
 void send_database(const eternity::net::Packet&) override{++bypass;throw std::logic_error("zone save bypassed binding");}
 void lock_effects() override{mutex.lock();}void unlock_effects() override{mutex.unlock();}
 void lock_quests() override{mutex.lock();}void unlock_quests() override{mutex.unlock();}
};

struct LiveZoneDisconnect final:VerticalDisconnectServices {
 ZoneEntryState& position;std::recursive_mutex user_lock,zone_lock;unsigned saves=0,leaves=0,finalized=0;std::function<void()> on_prepare,on_save;std::function<void(WorldUser&,RuntimeZone&)> on_leave;std::function<void(WorldUser&)> on_final;
 explicit LiveZoneDisconnect(ZoneEntryState& p):position(p){}
 std::recursive_mutex& user_mutex(WorldUser&) override{return user_lock;}std::recursive_mutex& zone_mutex(RuntimeZone&) override{return zone_lock;}
 void dead_disconnect(WorldUser&) override{throw std::logic_error("unexpected dead disconnect");}std::uint32_t tick_ms() override{return 100;}
 void prepare_save(WorldUser&) override {if(on_prepare)on_prepare();}void request_save(WorldUser&) override {++saves;if(on_save)on_save();}
 void leave_zone(WorldUser& user,RuntimeZone& zone) override {if(on_leave)on_leave(user,zone);else {ZoneActorLeaveServices ports;ports.user_leave_lifecycle=[](Actor&){};ports.send_view=[](CellCoordinates,const eternity::net::Packet&){};check(leave_zone_actor(zone,user,position,ports),"zone leave failed");}++leaves;}
 void finish_final(WorldUser& user) override {if(on_final)on_final(user);++finalized;}
};

struct SessionSaveStore {
 std::atomic<unsigned> exits{0};
 UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>&){throw std::logic_error("unexpected periodic save");}
 UserCharacterSaveResult save_exit(CharacterReplica&,bool,std::string_view,const CharacterSaveCallbacks& callbacks,const std::function<void(std::string_view)>&) {
  if(++exits==1)callbacks.queue_exit();else callbacks.exit();return {};
 }
};

struct ForeignPreparationBinding {
 std::atomic<unsigned>& destroyed;std::atomic<bool>& destroyed_on_world;VerticalWorldSession::Strand strand;std::function<void()> on_destroy;
 ForeignPreparationBinding(std::atomic<unsigned>& count,std::atomic<bool>& on_world,VerticalWorldSession::Strand executor,std::function<void()> hook={}):destroyed(count),destroyed_on_world(on_world),strand(std::move(executor)),on_destroy(std::move(hook)){}
 ~ForeignPreparationBinding(){destroyed_on_world=strand.running_in_this_thread();if(on_destroy)on_destroy();++destroyed;}
 bool matches(const VerticalWorldSession::Strand&,const VerticalWorldSession::Registry&,const VerticalWorldSession&) const noexcept{return false;}
};

struct OpenedSessionResource {std::atomic<unsigned>& destroyed;std::function<void()> on_destroy;explicit OpenedSessionResource(std::atomic<unsigned>& value,std::function<void()> hook={}):destroyed(value),on_destroy(std::move(hook)){}~OpenedSessionResource(){if(on_destroy)on_destroy();++destroyed;}};

struct SessionMoveView final:UserViewActorTransport {
 UserViewLiveAppearance appearance(const Actor&) override {throw std::logic_error("unexpected cell appearance");}
 std::unique_ptr<UserViewBuffer> allocate(eternity::net::Packet) override {throw std::logic_error("unexpected cell allocation");}
 void broadcast(CellCoordinates,UserViewBuffer&) override {throw std::logic_error("unexpected cell broadcast");}
 void leave_snapshot(CellCoordinates) override {throw std::logic_error("unexpected cell leave");}
 void enter_snapshot(CellCoordinates) override {throw std::logic_error("unexpected cell enter");}
};
struct SessionMoveServices final:VerticalMovePacketServices {
 std::array<float,5> speeds{3,4,5,6,7};unsigned stands=0,finishes=0;eternity::net::Packet sent;std::function<void()> on_finish;
 void status_stand(Actor&) override{++stands;}std::uint16_t direction(const Actor&) override{return 0;}
 void send_view_action(Actor&,const eternity::net::Packet&) override{throw std::logic_error("unexpected movement correction");}
 std::uint32_t tick_ms() override{return 200;}
 bool send_priority_view(Actor&,const eternity::net::Packet& packet,std::uint8_t priority) override{check(priority==7,"movement priority differed");sent=packet;return false;}
 void finish_move(Actor&,bool wide) override{check(!wide,"unexpected movement wide pass");++finishes;if(on_finish)on_finish();}
 const std::array<float,5>& movement_speed_table() override{return speeds;}
 void close(Actor&,std::uint32_t) override{throw std::logic_error("unexpected movement close");}
};

struct SessionIgnoredMove {std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t>){return {};}};
struct SessionPartyFixture final:PartyMembershipSendWorld,PartyRegistryPorts {
 std::vector<eternity::net::Packet> sent;
 PartyMembershipSendBuffer buffer;bool buffer_active=false;unsigned buffers_freed=0;
 bool leaving=false;unsigned unregistered=0,scenes=0,logs=0;
 VerticalWorldPartyRegistry* live_registry=nullptr;
 std::uint32_t tick() override {throw std::logic_error("unexpected party tick");}
 void lock(PartyNormalMembership& party) override {if(!leaving)throw std::logic_error("unexpected party lock");if(live_registry)live_registry->lock_party(party);}void unlock(PartyNormalMembership& party) noexcept override {if(live_registry)live_registry->unlock_party(party);}
 void lock_effects(PartyMembershipUser&) override {throw std::logic_error("unexpected effect lock");}void unlock_effects(PartyMembershipUser&) noexcept override {}
 void send_party(PartyNormalMembership&,const eternity::net::Packet& p) override {if(!leaving)throw std::logic_error("unexpected party send");sent.push_back(p);}
 void send_user(PartyMembershipUser&,const eternity::net::Packet& packet) override {sent.push_back(packet);}
 void send_scene(PartyMembershipUser&,const eternity::net::Packet&) override {if(!leaving)throw std::logic_error("unexpected party scene");++scenes;}
 void unregister_party(PartyNormalMembership& party) override {if(!leaving)throw std::logic_error("unexpected party unregister");if(live_registry)live_registry->unregister(party);++unregistered;}
 void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("unexpected guild cleanup");}
 void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {if(!leaving)throw std::logic_error("unexpected party log");}
 void send_log(const eternity::net::Packet&) override {if(!leaving)throw std::logic_error("unexpected party log send");++logs;}
 void initialize_lock(PartyRegistryLock,PartyRegistryParty*,std::uint32_t) override {throw std::logic_error("unexpected registry init");}
 void destroy_lock(PartyRegistryLock,PartyRegistryParty*) override {throw std::logic_error("unexpected registry destroy");}
 void lock_registry(PartyRegistryLock,PartyRegistryParty*) override {if(!leaving)throw std::logic_error("unexpected registry lock");}
 void unlock_registry(PartyRegistryLock,PartyRegistryParty*) noexcept override {}
 PartyRegistryParty* allocate_object() override {throw std::logic_error("unexpected party allocation");}
 void deallocate_object(PartyRegistryParty&) override {throw std::logic_error("unexpected party deallocation");}
 PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {if(buffer_active)throw std::logic_error("party buffer already active");buffer_active=true;return &buffer;}
 void free_send_buffer(PartyMembershipSendBuffer& value) override {if(&value!=&buffer || !buffer_active)throw std::logic_error("foreign party buffer");buffer_active=false;++buffers_freed;}
 void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer&) override {throw std::logic_error("unexpected pool enqueue");}
};
struct SessionSocialFixture {
 VerticalWorldSession& session;SocialCatalog value;bool& released;std::function<void()> on_release;
 SessionSocialFixture(VerticalWorldSession& s,SocialCatalog catalog,bool& result):session(s),value(std::move(catalog)),released(result){}
 bool owns_session(const VerticalWorldSession& owner) const {return &session==&owner;}
 const SocialCatalog& catalog() const {return value;}
 WorldLifecyclePorts bind_world_release(WorldLifecyclePorts ports) {
  auto next=std::move(ports.step);ports.step=[this,next=std::move(next)](WorldLifecycleStep step){if(step==WorldLifecycleStep::release_friends){if(on_release)on_release();released=true;}next(step);};return ports;
 }
};
struct SessionRetainedMove {
 VerticalMoveSession movement;unsigned& calls;
 SessionRetainedMove(Actor& actor,RuntimeZone& zone,WorldAdmissionUser& user,ZoneEntryState& entry,VerticalMotionState& motion,
  UserViewActorTransport& view,VerticalMovePacketServices& services,unsigned& value):movement(actor,zone,user,entry,motion,view,services),calls(value){}
 std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet){++calls;return movement.receive(packet);}
};

static void verify_single_adoption() {
 using namespace eternity;constexpr std::size_t count=8;
 auto issuer=std::make_shared<session::Registry>();session::LoginSession login{77,301,1,"native",{}, {}};login.token.fill(7);
 issuer->add(login);check(issuer->select(77,301,0,1),"adoption handoff failed");auto claim=issuer->claim_world(77,login.token,0,401,2);
 check(bool(claim.session),"adoption claim failed");auto token=claim.session->world_reservation;
 std::array<std::unique_ptr<VerticalSessionRegistry<Owner>>,count> registries;std::array<std::shared_ptr<Owner>,count> owners;std::array<bool,count> won{};
 for(std::size_t i=0;i<count;++i){registries[i]=std::make_unique<VerticalSessionRegistry<Owner>>();registries[i]->bind_network_registry(issuer,0);owners[i]=std::make_shared<Owner>(77,static_cast<std::uint32_t>(500+i));}
 check(registries[0]->owner_identity() && registries[0]->owner_identity()!=registries[1]->owner_identity(),"bound registries shared owner identity");
 bool bad_owner=false;try{registries[0]->reserve(77,{},token);}catch(const std::invalid_argument&){bad_owner=true;}
 check(bad_owner && issuer->world_adoptions().empty(),"failed local registration consumed the claim");
 std::barrier ready(static_cast<std::ptrdiff_t>(count+1));std::atomic_uint wins{0},unexpected{0};std::vector<std::jthread> threads;
 for(std::size_t i=0;i<count;++i)threads.emplace_back([&,i]{ready.arrive_and_wait();try{registries[i]->reserve(77,owners[i],token);won[i]=true;++wins;}catch(const std::invalid_argument&){}catch(...){++unexpected;}});
 ready.arrive_and_wait();threads.clear();check(wins==1 && unexpected==0,"one token was adopted by multiple world owners");
 std::size_t winner=0;while(!won[winner])++winner;auto active=issuer->world_adoptions();check(active.size()==1 && active[0]->reservation()==token,"issuer lost adoption capability");
 auto replacement=login;replacement.session_id=302;issuer->add(replacement);check(issuer->select(77,302,0,2),"replacement handoff failed");
 check(registries[winner]->begin_database_work(77,*owners[winner]) && !registries[winner]->release(77,*owners[winner]),"adopted owner lost DB fence");
 check(issuer->claim_world(77,login.token,0,402,3).duplicate,"replacement bypassed adoption");
 check(registries[winner]->finish_database_work(77,*owners[winner]) && registries[winner]->release(77,*owners[winner]),"adopted owner failed to release");
 check(!issuer->release_world_adoption(active[0]),"stale adoption released twice");
 bool reused=false;try{registries[(winner+1)%count]->reserve(77,owners[(winner+1)%count],token);}catch(const std::invalid_argument&){reused=true;}
 check(reused && registries[(winner+1)%count]->size()==0,"spent claim was adopted again");
 check(issuer->claim_world(77,login.token,0,402,3).duplicate,"retained claim snapshot stopped fencing");
 active.clear();token.reset();claim.session.reset();auto next=issuer->claim_world(77,login.token,0,402,3);check(bool(next.session),"drained claim did not admit replacement");
 next.session.reset();check(issuer->release_world(77,302,402),"replacement release failed");

 auto abandoned_issuer=std::make_shared<session::Registry>();abandoned_issuer->add(login);check(abandoned_issuer->select(77,301,0,1),"abandoned setup failed");
 auto abandoned_claim=abandoned_issuer->claim_world(77,login.token,0,401,2);check(bool(abandoned_claim.session),"abandoned claim failed");
 {auto registry=std::make_unique<VerticalSessionRegistry<Owner>>();registry->bind_network_registry(abandoned_issuer,0);registry->reserve(77,std::make_shared<Owner>(77,600),abandoned_claim.session->world_reservation);}
 abandoned_claim.session.reset();abandoned_issuer->add(replacement);check(abandoned_issuer->select(77,302,0,2),"abandoned replacement failed");
 check(abandoned_issuer->claim_world(77,login.token,0,402,3).duplicate,"abandoned owner lost its fence");
 auto retained=abandoned_issuer->world_adoptions();check(retained.size()==1 && retained[0]->reservation()->account()==77,"abandoned adoption unavailable for recovery");
 check(abandoned_issuer->release_world_adoption(retained[0]),"abandoned adoption recovery failed");retained.clear();
 auto recovered=abandoned_issuer->claim_world(77,login.token,0,402,3);check(bool(recovered.session),"recovered adoption stayed fenced");
 recovered.session.reset();check(abandoned_issuer->release_world(77,302,402),"recovered owner release failed");
}

#ifndef ETERNITY_SESSION_REGISTRY_FIXTURE_ONLY
int main(){try {
 verify_single_adoption();
 bool missing_path=false;try{(void)VerticalWorldSessionSqlSave::open({},1,{});}catch(const std::invalid_argument&){missing_path=true;}check(missing_path,"empty recovery path accepted");
 {auto social=std::make_shared<VerticalWorldAccountSocial>(11);std::weak_ptr<VerticalWorldAccountSocial> weak_social=social;
  auto opener=vertical_world_session_sql_save_opener({},1,"fixture-recovery","PS_GameData",social->shared_mutex());social.reset();
  check(!weak_social.expired(),"SQL opener dropped retained social mutex");opener={};check(weak_social.expired(),"SQL opener leaked retained social mutex");}
 VerticalSessionRegistry<Owner> registry;auto first=std::make_shared<Owner>(11,42),second=std::make_shared<Owner>(11,43),same_character=std::make_shared<Owner>(12,42);
 std::weak_ptr<Owner> weak=first;registry.retain(first,first->actor);check(registry.find_account(11)==first && registry.find_character(42)==first && registry.retain_actor(first->actor)==first,"registered owner missing");
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
 // A real network claim remains fenced after login replacement and socket close,
 // until the retained world owner has drained its tracked database work.
 auto logins=std::make_shared<eternity::session::Registry>();eternity::session::LoginSession login{11,201,1,"owner",{}, {}};
 logins->add(login);check(logins->select(11,201,0,1),"claim setup failed");
 auto claim=logins->claim_world(11,login.token,0,301,2);check(claim.session && claim.session->world_reservation,"claim missing reservation");
 VerticalSessionRegistry<Owner> network_owners;
 bool unbound=false;try{network_owners.reserve(11,second,claim.session->world_reservation);}catch(const std::invalid_argument&){unbound=true;}
 check(unbound,"unbound registry accepted network claim");network_owners.bind_network_registry(logins,0);
 bool bypass=false,rebind=false;try{network_owners.reserve(11,second);}catch(const std::logic_error&){bypass=true;}
 try{network_owners.bind_network_registry(logins,0);}catch(const std::logic_error&){rebind=true;}
 check(bypass && rebind,"network binding allowed bypass or rebinding");
 VerticalSessionRegistry<Owner> invalid_realm;bool invalid=false;
 try{invalid_realm.bind_network_registry(logins,254);}catch(const std::logic_error&){invalid=true;}
 check(invalid,"invalid realm binding was accepted");
 eternity::session::LoginSession other_login{12,211,1,"other",{}, {}};
 logins->add(other_login);check(logins->select(12,211,1,1),"other realm setup failed");
 auto other_claim=logins->claim_world(12,other_login.token,1,311,2);check(bool(other_claim.session),"other realm claim failed");
 bool wrong_realm=false;try{network_owners.reserve(12,same_character,other_claim.session->world_reservation);}catch(const std::invalid_argument&){wrong_realm=true;}
 check(wrong_realm && network_owners.size()==0 && logins->world_adoptions().empty(),"wrong realm registered or consumed its claim");
 VerticalSessionRegistry<Owner> other_world;check(!other_world.owner_identity(),"unbound registry has an owner identity");other_world.bind_network_registry(logins,1);
 other_world.reserve(12,same_character,other_claim.session->world_reservation);check(other_world.release(12,*same_character),"correct realm could not adopt claim");
 other_claim.session.reset();check(logins->release_world(12,211,311),"other realm release failed");
 auto foreign=std::make_shared<eternity::session::Registry>();foreign->add(login);check(foreign->select(11,201,0,1),"foreign setup failed");
 auto foreign_claim=foreign->claim_world(11,login.token,0,301,2);check(bool(foreign_claim.session),"foreign claim missing");
 bool rejected_foreign=false;try{network_owners.reserve(11,second,foreign_claim.session->world_reservation);}catch(const std::invalid_argument&){rejected_foreign=true;}
 check(rejected_foreign && network_owners.size()==0,"foreign registry claim entered world");
 network_owners.reserve(11,second,claim.session->world_reservation);
 auto replacement=login;replacement.session_id=202;logins->add(replacement);check(logins->select(11,202,0,2),"replacement selection failed");
 claim.session.reset();check(network_owners.begin_database_work(11,*second),"retained owner DB work failed");
 check(!network_owners.release(11,*second),"pending DB work released reservation");
 auto denied=logins->claim_world(11,login.token,0,302,3);check(denied.duplicate && !denied.session,"replacement bypassed retained owner");
 check(network_owners.finish_database_work(11,*second) && network_owners.release(11,*second),"retained owner did not drain");
 auto admitted=logins->claim_world(11,login.token,0,302,3);check(admitted.session && !admitted.duplicate,"drained reservation blocked replacement");
 admitted.session.reset();check(logins->release_world(11,202,302),"replacement world release failed");

 asio::io_context io;auto work=asio::make_work_guard(io);auto strand=asio::make_strand(io);asio::thread_pool db(1);std::jthread thread([&]{io.run();});
 struct StopIoOnExit {asio::io_context& io;~StopIoOnExit(){io.stop();}} stop_io_on_exit{io};
 CharacterRoster roster;roster.country=0;CharacterSummary item;item.id=43;roster.slots[0]=item;
 std::promise<void> started,release_worker,cancelled,drained;auto started_future=started.get_future();auto worker_gate=release_worker.get_future();auto cancelled_future=cancelled.get_future();auto drained_future=drained.get_future();
 std::shared_ptr<VerticalOwnedSelection<Owner>> selection;
 asio::post(strand,[&]{registry.reserve(11,second);selection=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
  [&](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{started.set_value();worker_gate.wait();return {};},
  [](Owner&,VerticalPersistenceSelection){throw std::runtime_error("cancelled selection delivered");},[](Owner&){throw std::runtime_error("cancelled selection unavailable");},[](Owner&,std::uint32_t){throw std::runtime_error("selection closed");},[](Owner&,std::exception_ptr){throw std::runtime_error("selection failed");});
  (void)selection->receive(eternity::net::Packet{4,1,43,0,0,0});});started_future.wait();
 asio::post(strand,[&]{check(!registry.release(11,*second),"cancel released active SQL selection");bool duplicate=false;try{registry.reserve(11,same_character);}catch(const std::logic_error&){duplicate=true;}check(duplicate,"reconnect overtook active selection");selection->cancel_and_drain([&]{check(registry.release(11,*second),"selection fence did not drain");registry.reserve(11,same_character);check(registry.release(11,*same_character),"replacement account not released");drained.set_value();});cancelled.set_value();});
 cancelled_future.wait();release_worker.set_value();drained_future.wait();
 std::promise<void> queued_selection_blocked,queued_selection_release,queued_selection_cancelled,queued_selection_drained;auto queued_selection_blocked_future=queued_selection_blocked.get_future();auto queued_selection_release_future=queued_selection_release.get_future();auto queued_selection_cancelled_future=queued_selection_cancelled.get_future();auto queued_selection_drained_future=queued_selection_drained.get_future();
 std::atomic<bool> queued_selection_loaded=false;std::shared_ptr<VerticalOwnedSelection<Owner>> queued_selection;
 asio::post(db.get_executor(),[&]{queued_selection_blocked.set_value();queued_selection_release_future.wait();});queued_selection_blocked_future.get();
 asio::post(strand,[&]{try {registry.reserve(11,second);queued_selection=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
  [&](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{queued_selection_loaded=true;return {};},[](Owner&,VerticalPersistenceSelection){throw std::logic_error("queued selection delivered");},[](Owner&){throw std::logic_error("queued selection unavailable");},[](Owner&,std::uint32_t){throw std::logic_error("queued selection closed");},[](Owner&,std::exception_ptr){throw std::logic_error("queued selection failed");});
  (void)queued_selection->receive(eternity::net::Packet{4,1,43,0,0,0});queued_selection->cancel_and_drain([&]{try{check(!queued_selection_loaded && registry.pending_database_work(11)==0 && registry.release(11,*second),"queued selection drain differed");queued_selection.reset();queued_selection_drained.set_value();}catch(...){queued_selection_drained.set_exception(std::current_exception());}});
  check(queued_selection->pending() && registry.pending_database_work(11)==1 && !registry.release(11,*second),"queued selection lost its cancellation fence");queued_selection_cancelled.set_value();
 }catch(...){queued_selection_cancelled.set_exception(std::current_exception());}});queued_selection_cancelled_future.get();queued_selection_release.set_value();queued_selection_drained_future.get();
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
 // The worker must not publish a snapshot or replica for another account or
 // character, even if a loader returns a nonempty result.
 for(unsigned variant=0;variant<5;++variant) {
  auto pool=std::make_shared<InventoryReplicaPool>();std::promise<void> rejected_selection;auto rejected_future=rejected_selection.get_future();std::shared_ptr<VerticalOwnedSelection<Owner>> rejected;
  asio::post(strand,[&,pool,variant]{try {registry.reserve(11,second);rejected=std::make_shared<VerticalOwnedSelection<Owner>>(strand,db.get_executor(),registry,second,CharacterSelectionState{11,0,true},roster,
   [pool,variant](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;
    if(variant==0)++snapshot.core.user_id;if(variant==1)++snapshot.core.character.id;auto core=snapshot.core;if(variant==2)++core.user_id;if(variant==3)++core.character.id;
    std::unique_ptr<CharacterReplica> replica;if(variant!=4)replica=std::make_unique<CharacterReplica>(core,*pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};},
   [](Owner&,VerticalPersistenceSelection){throw std::logic_error("foreign selection published");},[](Owner&){throw std::logic_error("foreign selection reported unavailable");},[](Owner&,std::uint32_t){throw std::logic_error("foreign selection closed");},
   [&,variant](Owner& owner,std::exception_ptr error){try{check(&owner==second.get() && bool(error) && registry.pending_database_work(11)==1,"foreign selection lost failure fence");
    bool identity_error=false;try{std::rethrow_exception(error);}catch(const std::logic_error& e){identity_error=std::string(e.what())==((variant==2 || variant==3)?"Selection loader returned foreign replica":"Selection loader returned foreign snapshot");}check(identity_error,"foreign selection reached a different callback");
    rejected->cancel_and_drain([&]{try{check(registry.pending_database_work(11)==0 && registry.release(11,*second),"foreign selection did not drain");rejected.reset();rejected_selection.set_value();}catch(...){rejected_selection.set_exception(std::current_exception());}});
   }catch(...){rejected_selection.set_exception(std::current_exception());}});
   (void)rejected->receive(eternity::net::Packet{4,1,43,0,0,0});
  }catch(...){rejected_selection.set_exception(std::current_exception());}});rejected_future.get();
 }
 RuntimeZone runtime_zone;ZoneEntryState position;ZoneActorEntryServices entry;Services admission_services(runtime_zone,second->actor,position,entry);WorldAdmission world(admission_services);
 VerticalWorldSession::Registry sessions;sessions.bind_network_registry(world_claim_registry(),0);std::promise<void> world_started,world_release,world_closed,world_cancelled;auto world_started_future=world_started.get_future();auto world_release_future=world_release.get_future();auto world_closed_future=world_closed.get_future();auto world_cancelled_future=world_cancelled.get_future();
 WorldSessionTestServices session_services(world_closed);std::shared_ptr<VerticalWorldSession> live;
 std::weak_ptr<void> io_lease;
 eternity::net::WorldChannel channel;channel.retain_io=[&]{auto lease=std::make_shared<int>(1);io_lease=lease;return lease;};channel.pause_input=[](auto callback){callback(true);};channel.resume_input=[]{};channel.set_admission_dispatch=[](bool){};channel.disconnect=[]{throw std::logic_error("unexpected disconnect");};channel.disconnect_with_reason=[](std::uint32_t){throw std::logic_error("unexpected close");};
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
 std::promise<void> foreign_binding_done,foreign_binding_hidden;auto foreign_binding_future=foreign_binding_done.get_future();auto foreign_binding_hidden_future=foreign_binding_hidden.get_future();std::atomic<unsigned> foreign_binding_destroyed{0};std::atomic<bool> foreign_binding_on_world{true};
 std::shared_ptr<VerticalWorldSessionPreparation<int,ForeignPreparationBinding>> foreign_binding_preparation;
 asio::post(strand,[&]{try {foreign_binding_preparation=VerticalWorldSessionPreparation<int,ForeignPreparationBinding>::create(strand,db.get_executor(),sessions,live,
  []{return std::make_shared<int>(1);},[&](VerticalWorldSession&,std::shared_ptr<int>){asio::post(strand,[&]{try{check(!foreign_binding_preparation->binding(),"foreign binding was visible before disposal");foreign_binding_hidden.set_value();}catch(...){foreign_binding_hidden.set_exception(std::current_exception());}});return std::make_shared<ForeignPreparationBinding>(foreign_binding_destroyed,foreign_binding_on_world,strand);});
  foreign_binding_preparation->start([&](std::exception_ptr error){try{check(bool(error) && !foreign_binding_preparation->binding() && foreign_binding_destroyed==1 && !foreign_binding_on_world && sessions.pending_database_work(11)==0,"foreign binding was published or not disposed on worker");foreign_binding_done.set_value();}catch(...){foreign_binding_done.set_exception(std::current_exception());}});
 }catch(...){foreign_binding_done.set_exception(std::current_exception());}});foreign_binding_hidden_future.get();foreign_binding_future.get();foreign_binding_preparation.reset();
 std::promise<void> failure_disposal_started,failure_disposal_release,failure_cancel_checked,failure_cancel_done;auto failure_disposal_started_future=failure_disposal_started.get_future();auto failure_disposal_release_future=failure_disposal_release.get_future();auto failure_cancel_checked_future=failure_cancel_checked.get_future();auto failure_cancel_done_future=failure_cancel_done.get_future();
 std::shared_ptr<VerticalWorldSessionPreparation<int,ForeignPreparationBinding>> cancelling_failure;std::atomic<unsigned> failure_destroyed{0};std::atomic<bool> failure_on_world{true};bool failure_completion_called=false,failure_cancel_called=false;
 asio::post(strand,[&]{try {cancelling_failure=VerticalWorldSessionPreparation<int,ForeignPreparationBinding>::create(strand,db.get_executor(),sessions,live,
  []{return std::make_shared<int>(1);},[&](VerticalWorldSession&,std::shared_ptr<int>){return std::make_shared<ForeignPreparationBinding>(failure_destroyed,failure_on_world,strand,[&]{failure_disposal_started.set_value();failure_disposal_release_future.wait();});});
  cancelling_failure->start([&](std::exception_ptr){failure_completion_called=true;});
 }catch(...){failure_disposal_started.set_exception(std::current_exception());}});failure_disposal_started_future.get();
 asio::post(strand,[&]{try {cancelling_failure->cancel([&]{try{failure_cancel_called=true;check(!failure_completion_called && failure_destroyed==1 && !failure_on_world && sessions.pending_database_work(11)==0,"failure cancellation drained too early");failure_cancel_done.set_value();}catch(...){failure_cancel_done.set_exception(std::current_exception());}},[&](std::exception_ptr error){failure_cancel_done.set_exception(error);});
  bool duplicate_rejected=false;try{cancelling_failure->cancel([]{},[](std::exception_ptr){});}catch(const std::logic_error&){duplicate_rejected=true;}
  check(duplicate_rejected && !failure_cancel_called && !failure_completion_called && !cancelling_failure->binding() && sessions.pending_database_work(11)==1,"failure cancellation lost disposal fence");failure_cancel_checked.set_value();
 }catch(...){failure_cancel_checked.set_exception(std::current_exception());}});failure_cancel_checked_future.get();failure_disposal_release.set_value();failure_cancel_done_future.get();cancelling_failure.reset();
 std::promise<void> preparation_failure;auto preparation_failure_future=preparation_failure.get_future();std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> failed_preparation;
 asio::post(strand,[&]{try {failed_preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,live,
  []()->std::shared_ptr<OpenedSessionResource>{throw std::runtime_error("fixture open failure");},
  [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>)->std::shared_ptr<OpenedSessionResource>{throw std::logic_error("failed open bound resource");});
  failed_preparation->start([&](std::exception_ptr error){try {check(bool(error) && sessions.pending_database_work(11)==0 && !failed_preparation->binding(),"preparation failure escaped fence or bound resource");preparation_failure.set_value();}catch(...){preparation_failure.set_exception(std::current_exception());}});
 }catch(...){preparation_failure.set_exception(std::current_exception());}});preparation_failure_future.get();
 std::promise<void> queued_open_blocked,queued_open_release,queued_open_cancelled,queued_open_drained;auto queued_open_blocked_future=queued_open_blocked.get_future();auto queued_open_release_future=queued_open_release.get_future();auto queued_open_cancelled_future=queued_open_cancelled.get_future();auto queued_open_drained_future=queued_open_drained.get_future();
 std::atomic<bool> queued_open_started=false;std::shared_ptr<VerticalWorldSessionResourceOpen<OpenedSessionResource>> queued_open;
 asio::post(db.get_executor(),[&]{queued_open_blocked.set_value();queued_open_release_future.wait();});queued_open_blocked_future.get();
 asio::post(strand,[&]{try {queued_open=VerticalWorldSessionResourceOpen<OpenedSessionResource>::create(strand,db.get_executor(),sessions,live,
  [&]()->std::shared_ptr<OpenedSessionResource>{queued_open_started=true;throw std::logic_error("cancelled queued opener ran");},[](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>){throw std::logic_error("queued open delivered");},[](VerticalWorldSession&,std::exception_ptr){throw std::logic_error("queued open failed");});
  queued_open->start();queued_open->cancel_and_drain([&]{try{check(!queued_open_started && !queued_open->pending() && sessions.pending_database_work(11)==0,"queued resource drain differed");queued_open.reset();queued_open_drained.set_value();}catch(...){queued_open_drained.set_exception(std::current_exception());}});
  check(queued_open->pending() && sessions.pending_database_work(11)==1 && !sessions.release(11,*live),"queued open lost its cancellation fence");queued_open_cancelled.set_value();
 }catch(...){queued_open_cancelled.set_exception(std::current_exception());}});queued_open_cancelled_future.get();queued_open_release.set_value();queued_open_drained_future.get();
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
 std::promise<void> success_checked;auto success_checked_future=success_checked.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && world.zone_waiting_count(runtime_zone)==0,"successful session did not drain");resource_open.reset();failed_preparation.reset();check(!preparation->binding() && prepared_destroyed==1,"prepared resource not released");preparation.reset();live.reset();success_checked.set_value();}catch(...){success_checked.set_exception(std::current_exception());}});success_checked_future.get();
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
 std::promise<void> bound_entered,bound_closed,bound_open_disposal,bound_open_release,bound_cancelled,bound_binding_disposal,bound_binding_release,bound_check,bound_cleanup;
 auto bound_closed_future=bound_closed.get_future();auto bound_open_disposal_future=bound_open_disposal.get_future();auto bound_open_release_future=bound_open_release.get_future();auto bound_cancelled_future=bound_cancelled.get_future();auto bound_binding_disposal_future=bound_binding_disposal.get_future();auto bound_binding_release_future=bound_binding_release.get_future();auto bound_check_future=bound_check.get_future();auto bound_cleanup_future=bound_cleanup.get_future();
 std::atomic<unsigned> bound_resource_destroyed{0},bound_binding_destroyed{0};std::atomic<bool> bound_open_on_world{true},bound_binding_on_world{true},bound_created{false};
 std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> bound_preparation;WorldSessionSuccessServices bound_services(bound_entered,bound_closed);
 bound_services.prepare_override=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done){
  bound_preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,owner.shared_from_this(),
   [&]{return std::make_shared<OpenedSessionResource>(bound_resource_destroyed,[&]{bound_open_on_world=strand.running_in_this_thread();bound_open_disposal.set_value();bound_open_release_future.wait();});},
   [&](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>){bound_created=true;return std::make_shared<OpenedSessionResource>(bound_binding_destroyed,[&]{bound_binding_on_world=strand.running_in_this_thread();bound_binding_disposal.set_value();bound_binding_release_future.wait();});});bound_preparation->start(std::move(done));};
 bound_services.cancel_override=[&](VerticalWorldSession&,std::function<void()> done){bound_preparation->cancel(std::move(done),[&](std::exception_ptr error){bound_closed.set_exception(error);});};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,bound_services,account,channel,roster,
  [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){bound_open_disposal.set_exception(std::current_exception());}});bound_open_disposal_future.get();
 asio::post(strand,[&]{try {check(bound_created && !bound_preparation->binding() && sessions.pending_database_work(11)==1 && bound_services.sent.empty(),"binding was exposed before opener drain");live->close();check(sessions.size()==1 && world.selection_count()==1,"bound cancellation released session early");bound_cancelled.set_value();}catch(...){bound_cancelled.set_exception(std::current_exception());}});bound_cancelled_future.get();bound_open_release.set_value();bound_binding_disposal_future.get();
 asio::post(strand,[&]{try {check(bound_resource_destroyed==1 && bound_binding_destroyed==0 && !bound_open_on_world && !bound_binding_on_world && sessions.pending_database_work(11)==1 && sessions.size()==1 && world.selection_count()==1,"binding disposal did not retain account on worker");bound_check.set_value();}catch(...){bound_check.set_exception(std::current_exception());}});bound_check_future.get();bound_binding_release.set_value();bound_closed_future.get();
 asio::post(strand,[&]{try {check(bound_binding_destroyed==1 && sessions.size()==0 && world.selection_count()==0 && bound_services.sent.empty(),"bound cancellation delivered entry or leaked session");bound_preparation.reset();live.reset();bound_cleanup.set_value();}catch(...){bound_cleanup.set_exception(std::current_exception());}});bound_cleanup_future.get();
 // Socket close while rejected-binding disposal is already running must
 // join that disposal before the session can leave the registry.
 std::promise<void> race_entry,race_closed,race_disposal_started,race_disposal_release,race_close_checked,race_cleanup;
 auto race_closed_future=race_closed.get_future();auto race_started_future=race_disposal_started.get_future();auto race_release_future=race_disposal_release.get_future();auto race_checked_future=race_close_checked.get_future();auto race_cleanup_future=race_cleanup.get_future();
 std::atomic<unsigned> race_binding_destroyed{0};std::atomic<bool> race_binding_on_world{true};std::shared_ptr<VerticalWorldSessionPreparation<int,ForeignPreparationBinding>> race_preparation;
 WorldSessionSuccessServices race_services(race_entry,race_closed);
 race_services.prepare_override=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done){
  race_preparation=VerticalWorldSessionPreparation<int,ForeignPreparationBinding>::create(strand,db.get_executor(),sessions,owner.shared_from_this(),
   []{return std::make_shared<int>(1);},[&](VerticalWorldSession&,std::shared_ptr<int>){return std::make_shared<ForeignPreparationBinding>(race_binding_destroyed,race_binding_on_world,strand,[&]{race_disposal_started.set_value();race_release_future.wait();});});race_preparation->start(std::move(done));};
 race_services.cancel_override=[&](VerticalWorldSession&,std::function<void()> done){race_preparation->cancel(std::move(done),[&](std::exception_ptr error){race_closed.set_exception(error);});};
 asio::post(strand,[&]{try{auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,race_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){race_disposal_started.set_exception(std::current_exception());}});race_started_future.get();
 asio::post(strand,[&]{try{live->close();check(sessions.find_account(11)==live && sessions.pending_database_work(11)==1 && world.selection_count()==1 && race_binding_destroyed==0 && race_services.sent.empty(),"session escaped failed binding disposal");race_close_checked.set_value();}catch(...){race_close_checked.set_exception(std::current_exception());}});race_checked_future.get();race_disposal_release.set_value();race_closed_future.get();
 asio::post(strand,[&]{try{check(sessions.size()==0 && world.selection_count()==0 && world.waiting_count()==0 && race_binding_destroyed==1 && !race_binding_on_world && race_services.sent.empty() && race_services.scene_unbinds==1,"session failed to drain rejected binding on close");race_preparation.reset();live.reset();race_cleanup.set_value();}catch(...){race_cleanup.set_exception(std::current_exception());}});race_cleanup_future.get();
 // A live callback may close while the selection delivery still owns its DB
 // fence. Later selection services must not start after that close.
 for(unsigned stage=0;stage<5;++stage){
  std::promise<void> close_entry,close_done,close_checked;auto close_future=close_done.get_future();auto checked_future=close_checked.get_future();WorldSessionSuccessServices close_services(close_entry,close_done);bool preparation_started=false;
  auto close_inside=[&]{check(sessions.pending_database_work(11)==1,"reentrant close lost selection fence");live->close();};
  if(stage==0)close_services.on_options=close_inside;else if(stage==1)close_services.on_scene=close_inside;else if(stage==2)close_services.on_rules=close_inside;else if(stage==3)close_services.on_bind=close_inside;else close_services.on_configure=close_inside;
  close_services.prepare_override=[&](VerticalWorldSession&,std::function<void(std::exception_ptr)>){preparation_started=true;throw std::logic_error("preparation started after close");};
  asio::post(strand,[&]{try{auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,close_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
  }catch(...){close_done.set_exception(std::current_exception());}});close_future.get();
  asio::post(strand,[&]{try{const unsigned bound=stage>=3?1U:0U;check(!preparation_started && sessions.size()==0 && world.selection_count()==0 && close_services.sent.empty() && close_services.scene_binds==bound && close_services.scene_unbinds==bound,"reentrant close continued selection or lost scene cleanup");live.reset();close_checked.set_value();}catch(...){close_checked.set_exception(std::current_exception());}});checked_future.get();
 }
 // Preparation can complete inline while a selection delivery is still
 // fenced. A close from a pre-delivery provider must stop before entry sends.
 for(unsigned stage=0;stage<4;++stage){
  std::promise<void> close_entry,close_done,close_checked;auto close_future=close_done.get_future();auto checked_future=close_checked.get_future();WorldSessionSuccessServices close_services(close_entry,close_done);
  auto close_inside=[&]{check(sessions.pending_database_work(11)==1,"entry close lost selection fence");live->close();};
  if(stage==0)close_services.on_entry_state=close_inside;else if(stage==1)close_services.on_protocol=close_inside;else if(stage==2)close_services.on_social=close_inside;else close_services.on_experience=close_inside;
  asio::post(strand,[&]{try{auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,close_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
  }catch(...){close_done.set_exception(std::current_exception());}});close_future.get();
  asio::post(strand,[&]{try{check(sessions.size()==0 && world.selection_count()==0 && world.waiting_count()==0 && close_services.sent.empty() && close_services.scene_binds==1 && close_services.scene_unbinds==1,"pre-delivery close sent entry or lost cleanup");live.reset();close_checked.set_value();}catch(...){close_checked.set_exception(std::current_exception());}});checked_future.get();
 }
 // Failure during a selection or pre-delivery provider must stop later
 // services while the real socket close and selection fence drain normally.
 for(unsigned stage=0;stage<9;++stage){
  std::promise<void> entry,closed,failed,checked;auto failure_future=failed.get_future();auto close_future=closed.get_future();auto checked_future=checked.get_future();WorldSessionSuccessServices services(entry,closed);unsigned failures=0,disconnects=0,preparations=0;
  services.failure_override=[&](std::exception_ptr error){check(bool(error),"missing provider failure");++failures;failed.set_value();};
  auto fail_inside=[&]{live->report_admission_failure(std::make_exception_ptr(std::runtime_error("provider fixture")));};
  if(stage==0)services.on_options=fail_inside;else if(stage==1)services.on_scene=fail_inside;else if(stage==2)services.on_rules=fail_inside;else if(stage==3)services.on_bind=fail_inside;else if(stage==4)services.on_configure=fail_inside;else if(stage==5)services.on_entry_state=fail_inside;else if(stage==6)services.on_protocol=fail_inside;else if(stage==7)services.on_social=fail_inside;else services.on_experience=fail_inside;
  services.prepare_override=[&](VerticalWorldSession&,std::function<void(std::exception_ptr)> done){++preparations;done({});};
  asio::post(strand,[&]{try{auto account=world_claim_fixture();auto failed_channel=channel;failed_channel.disconnect=[&]{++disconnects;};live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,services,account,failed_channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive({4,1,43,0,0,0});
  }catch(...){failed.set_exception(std::current_exception());}});failure_future.get();
  asio::post(strand,[&]{try{check(live->failed() && failures==1 && disconnects==1 && preparations==(stage>=5?1U:0U) && services.sent.empty() && world.waiting_count()==0,"provider failure continued entry");live->close();check(sessions.size()==0 && world.selection_count()==0 && services.scene_unbinds==services.scene_binds,"provider failure lost cleanup");live.reset();checked.set_value();}catch(...){checked.set_exception(std::current_exception());}});checked_future.get();close_future.get();
 }
 // Failure reporting may synchronously close while selection still owns its
 // database fence. Do not issue another socket disconnect after that close.
 {std::promise<void> entry,closed,checked;auto close_future=closed.get_future();auto checked_future=checked.get_future();WorldSessionSuccessServices services(entry,closed);unsigned failures=0,disconnects=0;
  services.on_options=[&]{live->report_admission_failure(std::make_exception_ptr(std::runtime_error("reentrant failure")));};
  services.failure_override=[&](std::exception_ptr error){check(bool(error) && live->failed() && sessions.pending_database_work(11)==1,"failure lost selection fence");++failures;live->close();check(sessions.find_account(11)==live,"failure close released fenced owner");};
  asio::post(strand,[&]{try{auto account=world_claim_fixture();auto failed_channel=channel;failed_channel.disconnect=[&]{++disconnects;};live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,services,account,failed_channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive({4,1,43,0,0,0});}catch(...){closed.set_exception(std::current_exception());}});close_future.get();
  asio::post(strand,[&]{try{check(failures==1 && disconnects==0 && sessions.size()==0 && world.selection_count()==0 && services.sent.empty(),"failure callback repeated disconnect or lost drain");live.reset();checked.set_value();}catch(...){checked.set_exception(std::current_exception());}});checked_future.get();
 }
 // A completed asynchronous preparation after failure is consumed without
 // constructing entry state or starting delivery.
 {std::promise<void> entry,closed,started,checked;auto started_future=started.get_future();auto close_future=closed.get_future();auto checked_future=checked.get_future();WorldSessionSuccessServices services(entry,closed);std::function<void(std::exception_ptr)> completion;unsigned entry_calls=0,failures=0,disconnects=0;
  services.prepare_override=[&](VerticalWorldSession&,std::function<void(std::exception_ptr)> done){completion=std::move(done);started.set_value();};services.on_entry_state=[&]{++entry_calls;};services.failure_override=[&](std::exception_ptr){++failures;};
  asio::post(strand,[&]{try{auto account=world_claim_fixture();auto failed_channel=channel;failed_channel.disconnect=[&]{++disconnects;};live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,services,account,failed_channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive({4,1,43,0,0,0});}catch(...){started.set_exception(std::current_exception());}});started_future.get();
  asio::post(strand,[&]{try{live->report_admission_failure(std::make_exception_ptr(std::runtime_error("pending fixture")));completion({});check(live->failed() && failures==1 && disconnects==1 && entry_calls==0 && services.sent.empty(),"failed preparation delivered entry");live->close();check(sessions.size()==0 && services.scene_unbinds==1,"failed preparation did not drain");live.reset();checked.set_value();}catch(...){checked.set_exception(std::current_exception());}});checked_future.get();close_future.get();
 }
 // Deferred preparation has no selection fence when entry callbacks run.
 // Closing inside delivery must retain the actor until the pass unwinds.
 for(unsigned stage=0;stage<4;++stage){
  std::promise<void> close_entry,close_done,close_checked;auto close_future=close_done.get_future();auto checked_future=close_checked.get_future();WorldSessionSuccessServices close_services(close_entry,close_done);unsigned late=0;
  auto close_inside=[&]{check(sessions.pending_database_work(11)==0,"delivery unexpectedly retained selection fence");live->close();check(sessions.find_account(11)==live && close_services.scene_actor,"delivery released session during callback");};
  close_services.prepare_override=[&](VerticalWorldSession&,std::function<void(std::exception_ptr)> done){asio::post(strand,[done=std::move(done)]{done({});});};
  close_services.on_manager=[&]{if(live->closing())++late;else if(stage==0)close_inside();};
  close_services.on_send=[&]{if(live->closing())++late;else if(stage==1)close_inside();};
  close_services.on_lifecycle=[&]{if(live->closing())++late;else if(stage==2)close_inside();};
  close_services.on_lock=[&]{if(live->closing())++late;else if(stage==3)close_inside();};
  asio::post(strand,[&]{try{auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,close_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
  }catch(...){close_done.set_exception(std::current_exception());}});close_future.get();
  asio::post(strand,[&]{try{check(sessions.size()==0 && world.selection_count()==0 && world.waiting_count()==0 && late==0 && close_services.unlocks==(stage==3?1U:0U) && close_services.scene_binds==1 && close_services.scene_unbinds==1,"delivery close continued or lost cleanup");live.reset();close_checked.set_value();}catch(...){close_checked.set_exception(std::current_exception());}});checked_future.get();
 }
 // A direct FIFO update has no admission scheduler pass. Reentrant close in
 // gameplay must retain the scene until dispatch returns, including on throw.
 for(unsigned failure=0;failure<2;++failure){
  std::promise<void> queue_entry,queue_close,queue_checked;auto entry_future=queue_entry.get_future();auto close_future=queue_close.get_future();auto checked_future=queue_checked.get_future();WorldSessionSuccessServices queue_services(queue_entry,queue_close);
  asio::post(strand,[&]{try{auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,queue_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
  }catch(...){queue_entry.set_exception(std::current_exception());}});entry_future.get();
  asio::post(strand,[&]{try{live->admission().location=4;unsigned calls=0;auto queue=VerticalWorldSessionPackets::create(strand,live,2,16,[&](VerticalWorldSession& owner,const eternity::net::Packet&){++calls;owner.close();check(sessions.find_account(11).get()==&owner && queue_services.scene_actor,"packet dispatch released its actor");if(failure)throw std::runtime_error("dispatch fixture");});
   check(queue->enqueue({7,7}) && queue->enqueue({8,8}),"packet fixture enqueue failed");bool threw=false;try{queue->update();}catch(const std::runtime_error&){threw=true;}check(threw==bool(failure) && calls==1 && queue->pending()==1 && sessions.size()==0 && queue_services.scene_unbinds==1,"packet dispatch fence or failure propagation differed");live.reset();queue_checked.set_value();
  }catch(...){queue_checked.set_exception(std::current_exception());}});checked_future.get();close_future.get();
 }
 ConfiguredMap live_map;live_map.settings.id=20;live_map.definition.size=81;RuntimeZone live_zone;live_zone.map=&live_map;live_zone.users=std::make_unique<ZoneUsers>(live_map.definition.geometry(),0);live_zone.environment=ZoneEnvironment::prepare(live_map);
 WorldUsers world_users({[](WorldUser&){},[](WorldUser&){}});
 ZoneActorEntryServices live_entry;live_entry.send_view=[](CellCoordinates,eternity::net::Packet){};live_entry.cell_info=[](CellCoordinates){};live_entry.obelisks=[]{};live_entry.nprotect=[]{};
 live_entry.diagnostic=[]{throw std::logic_error("unexpected zone diagnostic");};live_entry.close=[](std::uint32_t){throw std::logic_error("unexpected zone close");};
 std::unique_ptr<VerticalWorldSessionPacketRouter> packet_router;unsigned admission_packets=0;bool close_during_pass=false,close_during_select=false,process_admission_queue=false;VerticalWorldSessionAdmissionPorts admission_ports;
 admission_ports.packets=[&](VerticalWorldSession& owner){check(&owner==live.get(),"wrong packet owner");++admission_packets;if(process_admission_queue){check(bool(packet_router),"missing packet router");packet_router->update(owner);}if(close_during_pass){owner.close();check(sessions.find_account(11).get()==&owner,"session released during admission callback");}};
 admission_ports.select_zone=[&](VerticalWorldSession& owner){check(&owner==live.get(),"wrong zone owner");if(close_during_select){owner.close();check(sessions.find_account(11).get()==&owner,"selection callback released session during pass");}return &live_zone;};
 admission_ports.entry_services=[&](VerticalWorldSession& owner,RuntimeZone& zone){check(&owner==live.get() && &zone==&live_zone,"wrong entry services");return live_entry;};
 admission_ports.diagnostic=[](VerticalWorldSession&,WorldAdmissionError){throw std::logic_error("unexpected admission diagnostic");};
 admission_ports.rank_window=[](RuntimeZone&)->std::optional<std::uint32_t>{throw std::logic_error("unexpected rank");};
 admission_ports.lock_guild=[](VerticalWorldSession&)->bool{throw std::logic_error("unexpected guild");};admission_ports.unlock_guild=[](VerticalWorldSession&){throw std::logic_error("unexpected guild");};
 VerticalWorldSessionAdmissionServices live_admission(strand,sessions,world_users,std::move(admission_ports));WorldAdmission live_world(live_admission);std::unique_ptr<LiveZoneDisconnect> disconnect_services;
 std::shared_ptr<VerticalWorldSessionZoneServices<SessionSaveStore>> concrete_zone_services;unsigned concrete_zone_views=0,concrete_finalizations=0,zone_service_lookups=0;bool provider_reentry_rejected=false,premature_ack_rejected=false,request_ack_rejected=false;VerticalWorldZoneDisconnect* zone_disconnect_ptr=nullptr;
 VerticalWorldZoneDisconnect zone_disconnect(strand,sessions,world_users,[&](VerticalWorldSession& owner)->VerticalDisconnectServices&{++zone_service_lookups;try{zone_disconnect_ptr->resume(owner);}catch(const std::logic_error&){provider_reentry_rejected=true;}return *disconnect_services;});zone_disconnect_ptr=&zone_disconnect;
 std::promise<void> entered_zone,closed_zone,zone_entered,zone_finished;auto entered_zone_future=entered_zone.get_future();auto closed_zone_future=closed_zone.get_future();auto zone_entered_future=zone_entered.get_future();auto zone_finished_future=zone_finished.get_future();
 WorldSessionSuccessServices live_services(entered_zone,closed_zone);std::function<void()> disconnect_done;unsigned other_packets=0,owned_packets=0;
 SessionMoveView move_view;SessionMoveServices move_services;std::shared_ptr<VerticalWorldSessionGameplay> gameplay;std::weak_ptr<SessionRetainedMove> retained_move;unsigned retained_calls=0;
 packet_router=std::make_unique<VerticalWorldSessionPacketRouter>(strand,sessions,2,64,[&](VerticalWorldSession& owner,RuntimeZone& zone){
  check(&owner==live.get() && &zone==&live_zone,"gameplay factory lost owner");
  gameplay=VerticalWorldSessionGameplay::create(strand,sessions,live,zone,live->zone_entry(),live->motion(),move_view,move_services,
   [&](VerticalWorldSession& current,const eternity::net::Packet& other){check(&current==live.get() && other==eternity::net::Packet({7,7}),"wrong other gameplay packet");++other_packets;});return gameplay;},
  [&](VerticalWorldSession& owner,const eternity::net::Packet& packet){if(packet!=eternity::net::Packet({5,0x22}))return false;check(&owner==live.get(),"owned packet lost session");++owned_packets;return true;});
 move_services.on_finish=[&]{if(move_services.finishes==1)check(packet_router->try_enqueue(*live,{7,7}),"reentrant packet enqueue failed");};
 live_services.disconnect_override=[&](VerticalWorldSession& session,std::function<void()> done){zone_disconnect.begin(session,[&,done=std::move(done)]() mutable {disconnect_done=std::move(done);});};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();account.permission=7;
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,live_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;snapshot.core.x=40;snapshot.core.z=40;snapshot.core.direction=123;
    auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){entered_zone.set_exception(std::current_exception());}});entered_zone_future.get();
 asio::post(strand,[&]{try {check(live->zone_entry().character_id==43 && live->zone_entry().position.x==40 && live->zone_entry().position.z==40 && live->zone_entry().direction==123 && live->zone_entry().permission==7,"selected character zone state not retained");disconnect_services=std::make_unique<LiveZoneDisconnect>(live->zone_entry());WorldAdmissionUser foreign;foreign.user_id=11;bool rejected_foreign=false;try{(void)live_admission.select_zone(foreign);}catch(const std::logic_error&){rejected_foreign=true;}check(rejected_foreign,"foreign admission user accepted");
  packet_router->attach(live);
  live_services.receive_override=[&](VerticalWorldSession& owner,const eternity::net::Packet& packet){check(&owner==live.get(),"wrong socket owner");packet_router->receive(owner,packet);};
  eternity::net::Packet move{1,5,0x81,0x34,0x12};for(float value:{45.F,0.F,40.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)move.push_back(static_cast<std::uint8_t>(bits>>(i*8)));}
  check(!packet_router->try_enqueue(*live,eternity::net::Packet(65,1)),"packet byte bound was ignored");live->receive({1,2});live->receive(move);check(!packet_router->try_enqueue(*live,{8,8}),"packet count bound was ignored");
  bool overflow_closed=false;auto disconnect=live->channel().disconnect;live->channel().disconnect=[&]{overflow_closed=true;};live->receive({8,8});live->channel().disconnect=std::move(disconnect);check(overflow_closed,"packet overflow did not request socket close");
  resumed=false;admission_dispatch=false;live_admission.update_world_and_release(live_world);check(packet_router->pending(*live)==2 && live->admission().location==3 && resumed && admission_dispatch,"world wait did not publish and release queued input");
  process_admission_queue=true;live_admission.update_zone_packets(live_world,live_zone);process_admission_queue=false;
  check(admission_packets==1 && move_services.finishes==1 && live->zone_entry().position.x==45 && packet_router->pending(*live)==1,"zone packet snapshot or movement differed");packet_router->update(*live);check(other_packets==1 && move_services.stands==1 && move_services.sent.size()==21 && packet_router->pending(*live)==0,"queued gameplay did not dispatch");
  gameplay=VerticalWorldSessionGameplay::create_retained(strand,sessions,live,live_zone,live->zone_entry(),
   [&](VerticalWorldSession& owner,Actor& actor,RuntimeZone& zone,WorldAdmissionUser& user,ZoneEntryState& state){
    check(&owner==live.get() && &actor==&live->world_user()->actor && &zone==&live_zone && &user==&live->admission() && &state==&live->zone_entry(),"retained movement factory received foreign owners");
    auto retained=std::make_shared<SessionRetainedMove>(actor,zone,user,state,owner.motion(),move_view,move_services,retained_calls);retained_move=retained;return retained;},
   [&](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected retained other packet");});
  auto second_move=move;const auto xbits=std::bit_cast<std::uint32_t>(46.F);for(unsigned i=0;i<4;++i)second_move[5+i]=static_cast<std::uint8_t>(xbits>>(i*8));
  const auto retained_result=gameplay->dispatch(*live,second_move);check(retained_result && *retained_result==VerticalMoveResult::moved && retained_calls==1 && !retained_move.expired() && live->zone_entry().position.x==46 && move_services.finishes==2,"retained movement integration failed");
  auto ignored_gameplay=VerticalWorldSessionGameplay::create_retained(strand,sessions,live,live_zone,live->zone_entry(),
   [](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&){return std::make_shared<SessionIgnoredMove>();},
   [](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected ignored other packet");});
  bool ignored_rejected=false;try{(void)ignored_gameplay->dispatch(*live,move);}catch(const std::logic_error&){ignored_rejected=true;}check(ignored_rejected,"ignored 0501 was treated as handled");ignored_gameplay.reset();
  live->receive({5,0x22});packet_router->update(*live);check(owned_packets==1 && other_packets==1,"owned packet did not bypass gameplay dispatcher");
  packet_router->stop(*live);bool detached=false;try{(void)packet_router->try_enqueue(*live,{9,9});}catch(const std::logic_error&){detached=true;}check(detached,"stopped packet router accepted traffic");
  check(live->admission().location==4 && live_zone.users->find_actor(43)==&live->world_user()->actor && world_users.find(43)==live->world_user(),"world and zone indexes lost actual user");
  VerticalWorldPartyDirectory party_directory(strand,sessions,world_users);auto party_view=party_directory.retain(live->party_user());
  check(party_view.lifetime.get()==live.get() && party_view.member.in_zone && party_view.member.position[0]==46 && party_view.state.scene_state==4 && party_view.state.zone==reinterpret_cast<std::uintptr_t>(&live_zone),"party view did not use retained live zone state");
  auto party_by_id=party_directory.find_zone(party_view,43);auto party_by_name=party_directory.find_name(live->world_user()->actor.appearance().identity.name);
  check(party_by_id && party_by_name && &party_by_id->member==&live->party_user() && party_by_name->lifetime.get()==live.get() && !party_directory.find_zone(party_view,999),"party directory lookup differed");
  auto raid_by_name=party_directory.find_active_name(live->world_user()->actor.appearance().identity.name);check(raid_by_name && raid_by_name->lifetime.get()==live.get(),"raid active-name lookup lost session");
  SessionPartyFixture party_world;PartyRegistry party_registry;VerticalWorldPartyRequests party_requests(party_directory,party_world,party_registry,party_world,
   [](const PartyRequestUser&,std::int32_t,std::int32_t){throw std::logic_error("unexpected party close");},[](const PartyRequestUser&,eternity::net::Packet&){throw std::logic_error("unexpected raid fallback");});
  eternity::net::Packet self_invite{2,11,43,0,0,0};party_requests.receive(*live,self_invite);
  check(party_world.sent==std::vector<eternity::net::Packet>{{3,11,4,43,0,0,0}},"retained party request did not use real self-invite rejection");
  unsigned remaining_packets=0;auto party_other=party_requests.bind_gameplay([&](VerticalWorldSession& owner,const eternity::net::Packet& packet){check(&owner==live.get() && packet==eternity::net::Packet({7,7}),"party gameplay changed remaining packet");++remaining_packets;});
  auto party_gameplay=VerticalWorldSessionGameplay::create_retained(strand,sessions,live,live_zone,live->zone_entry(),
   [](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&){return std::make_shared<SessionIgnoredMove>();},party_other);
  (void)party_gameplay->dispatch(*live,self_invite);(void)party_gameplay->dispatch(*live,{7,7});
  check(party_world.sent.size()==2 && party_world.sent.back()==eternity::net::Packet({3,11,4,43,0,0,0}) && remaining_packets==1 && self_invite==eternity::net::Packet({2,11,43,0,0,0}),"party gameplay routing failed");
  for(const auto [opcode,required]:std::array<std::pair<std::uint8_t,std::size_t>,18>{{{3,3},{0x10,3},{0x15,3},{2,6},{6,6},{7,6},{0xf,6},{0x13,6},{0x14,6},{0x17,6},{0x1a,6},{0xe,8},{0x12,10},{9,23},{0x11,23},{0x18,23},{0x16,2},{0x0c,2}}}) {
   if(required==2)continue;eternity::net::Packet short_packet(required-1,0);short_packet[0]=opcode;short_packet[1]=0x0b;bool rejected=false;
   try{party_other(*live,short_packet);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"truncated party packet reached dispatcher");
  }
  VerticalWorldPartyMembership::Services membership_services;
  membership_services.tick=[&]{return party_world.tick();};membership_services.lock_effects=[&](auto& user){party_world.lock_effects(user);};membership_services.unlock_effects=[&](auto& user){party_world.unlock_effects(user);};
  std::function<void()> during_party_scene;membership_services.cleanup_guild_creation=[&](auto& user){party_world.cleanup_guild_creation(user);};membership_services.send_scene=[&](auto& user,const auto& packet){party_world.send_scene(user,packet);if(during_party_scene)during_party_scene();};
  unsigned party_failures=0;membership_services.fill_log=[&](auto& user,auto packet){party_world.fill_log(user,packet);};membership_services.send_log=[&](const auto& packet){party_world.send_log(packet);};membership_services.close_socket=[](auto&,std::int32_t){throw std::logic_error("unexpected close");};membership_services.send_failure=[&](auto& user,std::exception_ptr error){check(&user==&live->party_user() && bool(error),"party failure lost owner");++party_failures;};
  VerticalWorldPartyRuntime party_runtime(strand,sessions,world_users,std::move(membership_services),
   [](const PartyRequestUser&,std::int32_t,std::int32_t){throw std::logic_error("unexpected party close");},[](const PartyRequestUser&,eternity::net::Packet&){throw std::logic_error("unexpected raid fallback");});
  auto& live_party_registry=party_runtime.registry();auto& party_pool=party_runtime.buffers();auto& live_membership=party_runtime.membership();auto& live_party_requests=party_runtime.requests();
  auto* departing=allocate_registry_party(live_party_registry.registry(),live_party_registry);check(departing!=nullptr,"party allocation failed");
  departing->id=7;departing->count=1;departing->slots[0]=&live->party_user();live->party_user().party=departing;live_party_registry.registry().entries.emplace(7,departing);party_world.leaving=true;party_world.live_registry=&live_party_registry;
  live_party_registry.lock_party(*departing);live_party_registry.lock_party(*departing);live_party_registry.unlock_party(*departing);live_party_registry.unlock_party(*departing);
  auto* retained_buffer=allocate_party_membership_send_buffer(std::array<std::uint8_t,2>{8,9},live_membership);check(retained_buffer && party_pool.outstanding()==1,"party buffer allocation failed");
  bool buffer_shutdown_rejected=false;try{party_runtime.shutdown();}catch(const std::logic_error&){buffer_shutdown_rejected=true;}check(buffer_shutdown_rejected,"party runtime accepted an outstanding buffer");
  retained_buffer->references.fetch_add(1);release_party_membership_buffer(*retained_buffer,live_membership);check(party_pool.outstanding()==1,"party buffer freed before last reference");
  release_party_membership_buffer(*retained_buffer,live_membership);check(party_pool.outstanding()==0,"party buffer retained after last reference");
  std::vector<eternity::net::Packet> party_channel_packets;auto old_party_send=live->channel().send;live->channel().send=[&](eternity::net::Packet packet){party_channel_packets.push_back(std::move(packet));};live->party_user().send_socket=7;
  live_membership.send_user(live->party_user(),{1,2});check(party_channel_packets==std::vector<eternity::net::Packet>{{1,2}} && party_pool.outstanding()==0,"direct party send lost retained channel");
  live_party_requests.receive(*live,self_invite);check(party_channel_packets==std::vector<eternity::net::Packet>({{1,2},{3,11,4,43,0,0,0}}) && party_pool.outstanding()==0,"party runtime request lost its retained channel");
  live->channel().send=[](eternity::net::Packet){throw std::runtime_error("party channel failed");};live_membership.send_user(live->party_user(),{3,4});
  check(party_failures==1 && party_pool.outstanding()==0,"party send failure lost its buffer or fault");
  live->channel().send=[&](eternity::net::Packet packet){party_channel_packets.push_back(std::move(packet));};
  WorldLifecyclePorts party_ports;unsigned forwarded=0;party_ports.step=[&](WorldLifecycleStep){++forwarded;};auto bound_party=live_party_requests.bind_world_release(*live,std::move(party_ports));
  check(live_party_requests.lifecycle_party(*live)==std::optional<bool>(false),"normal party lifecycle snapshot failed");
  bool active_shutdown_rejected=false;try{party_runtime.shutdown();}catch(const std::logic_error&){active_shutdown_rejected=true;}check(active_shutdown_rejected,"party runtime shutdown accepted live user");
  bool wrong_kind=false;try{bound_party.step(WorldLifecycleStep::leave_raid);}catch(const std::logic_error&){wrong_kind=true;}
  check(wrong_kind && live->party_user().party==departing,"party lifecycle accepted wrong kind");
  bool reentrant_shutdown_rejected=false;during_party_scene=[&]{try{party_runtime.shutdown();}catch(const std::logic_error&){reentrant_shutdown_rejected=true;}};
  bound_party.step(WorldLifecycleStep::leave_party);bound_party.step(WorldLifecycleStep::release_friends);
  bool repeated=false;try{bound_party.step(WorldLifecycleStep::leave_party);}catch(const std::logic_error&){repeated=true;}
  check(reentrant_shutdown_rejected && repeated && !live->party_user().party && departing->count==0 && live_party_registry.registry().entries.empty() && party_world.scenes==1 && party_world.logs==1 && forwarded==1 && party_channel_packets==std::vector<eternity::net::Packet>({{1,2},{3,11,4,43,0,0,0},{5,11,43,0,0,0}}) && party_pool.outstanding()==0,"retained normal party release failed");
  live->party_user().send_socket=-1;live->channel().send=std::move(old_party_send);
  party_world.live_registry=nullptr;party_runtime.shutdown();bool stopped_party_rejected=false;try{(void)party_runtime.requests();}catch(const std::logic_error&){stopped_party_rejected=true;}check(stopped_party_rejected,"stopped party runtime accepted requests");party_world.leaving=false;
  PartyMembershipUser foreign_party;foreign_party.id=43;bool bad_party=false;try{(void)party_directory.retain(foreign_party);}catch(const std::logic_error&){bad_party=true;}check(bad_party,"party directory accepted foreign member");
  std::vector<eternity::net::Packet> routed_guild;unsigned allocated=0,released=0;
  auto previous_send=live->channel().send;live->channel().send=[&](eternity::net::Packet p){routed_guild.push_back(std::move(p));};
  VerticalWorldGuildTransport guild_transport(strand,sessions,world_users,[&](const eternity::net::Packet&){++allocated;return true;},[&]{++released;});
  auto guild_ports=guild_transport.ports();check(guild_ports.allocate({1,2}),"guild allocator was not used");guild_ports.send(live->guild_user(),{3,4});guild_ports.release();
  check(allocated==1 && released==1 && routed_guild==std::vector<eternity::net::Packet>{{3,4}},"guild transport did not route to retained channel");
  GuildUser foreign_guild;foreign_guild.character_id=43;bool foreign_rejected=false;try{guild_ports.send(foreign_guild,{3,4});}catch(const std::logic_error&){foreign_rejected=true;}
  check(foreign_rejected && routed_guild.size()==1,"guild transport accepted a foreign user");live->channel().send=std::move(previous_send);
  live->party_user().send_socket=7;live->close();bool gameplay_stopped=false;try{(void)gameplay->dispatch(*live,eternity::net::Packet{7,7});}catch(const std::logic_error&){gameplay_stopped=true;}
  check(live->party_user().send_socket==-1 && gameplay_stopped && zone_disconnect.pending()==1 && !disconnect_done && sessions.size()==1 && live->admission().logout_tick==10100 && live_zone.users->find_actor(43),"deferred logout allowed party sends, gameplay or released owner");zone_entered.set_value();
 }catch(...){zone_entered.set_exception(std::current_exception());}});zone_entered_future.get();
 DbJobDispatcher save_dispatcher(false,1);using SessionBinding=VerticalWorldSessionSaveBinding<SessionSaveStore>;using SavePreparation=VerticalWorldSessionPreparation<SessionSaveStore,SessionBinding>;using SaveTeardown=VerticalWorldSessionSaveTeardown<SessionSaveStore,SessionSaveStore>;
 std::shared_ptr<SavePreparation> save_preparation;std::shared_ptr<SaveTeardown> save_teardown;
 std::shared_ptr<VerticalWorldSessionAckRouter> ack_router;UserLifecycleState db_state{11,2,0,true};auto save_store=std::make_shared<SessionSaveStore>();auto save_mutex=std::make_shared<std::recursive_mutex>();std::atomic<unsigned> database_leave{0};unsigned other_replies=0;
 asio::post(strand,[&]{try {
  check(live->database_ack_user() && !live->database_ack_user()->socket_open,"socket closure not reflected in ack owner");
  ack_router=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[&](std::shared_ptr<VerticalWorldSession> retained){
   check(retained==live && sessions.pending_database_work(11)==1,"ack lost live session or active fence");
   zone_disconnect.acknowledge(*retained);check(disconnect_done && concrete_zone_views==1 && concrete_finalizations==1 && zone_service_lookups==1 && provider_reentry_rejected && premature_ack_rejected && request_ack_rejected && zone_disconnect.pending()==0,"final disconnect did not leave zone");
   check(!world_users.find(43) && !live_zone.users->find_actor(43) && sessions.size()==1,"finalized membership or fence differs");
   auto stopped_binding=save_preparation->binding();check(bool(stopped_binding),"missing save binding before teardown");
   save_teardown->start([&]{try {check(sessions.pending_database_work(11)==0 && other_replies==1 && save_store->exits==2 && database_leave==1 && !save_preparation->binding(),"save delivery or teardown drained early");disconnect_done();disconnect_done={};check(sessions.size()==0,"disconnect completion retained session");zone_finished.set_value();}catch(...){zone_finished.set_exception(std::current_exception());}});
   const eternity::net::Packet late_update{6,6,11,0,0,0,1,0,1,0,1,0};
   check(!stopped_binding->submit(9) && !stopped_binding->disconnect(vertical_database_disconnect_packet(11)) && stopped_binding->apply(late_update)==InventoryReplicaResult::rejected && stopped_binding->apply_zone_save(late_update)==InventoryReplicaResult::rejected,"stopped save binding accepted a late producer");stopped_binding.reset();
  });
  CharacterSaveLifecyclePorts ports;
  ports.send=[](eternity::net::Packet){throw std::logic_error("uncollected DB reply");};ports.leave=[&]{++database_leave;};ports.reset=[]{throw std::logic_error("unexpected DB reset");};ports.roster=[](bool){throw std::logic_error("unexpected DB roster");};ports.save_friends=[]{};ports.save_blocks=[]{};
  VerticalWorldSession::Registry foreign_save_registry;auto foreign_router=std::make_shared<VerticalWorldSessionAckRouter>(strand,foreign_save_registry,[](auto){throw std::logic_error("unexpected foreign ack");});bool wrong_save_router=false;
  try{(void)SessionBinding::create(strand,sessions,live,save_dispatcher,save_store,save_mutex,db_state,"fixture",ports,foreign_router,[](VerticalWorldSession&,eternity::net::Packet){},[](std::exception_ptr){});}catch(const std::invalid_argument&){wrong_save_router=true;}
  check(wrong_save_router && sessions.pending_database_work(11)==0,"save binding accepted foreign ACK registry");
  save_preparation=SavePreparation::create(strand,db.get_executor(),sessions,live,[save_store]{return save_store;},
   [&,ports=std::move(ports)](VerticalWorldSession& owner,std::shared_ptr<SessionSaveStore> store) mutable {
    return SessionBinding::create(strand,sessions,owner.shared_from_this(),save_dispatcher,std::move(store),save_mutex,db_state,"fixture",std::move(ports),ack_router,
     [&](VerticalWorldSession& session,eternity::net::Packet packet){check(&session==live.get() && packet==eternity::net::Packet({3,3,11,0,0,0}),"unexpected non-ack DB reply");++other_replies;},
     [&](std::exception_ptr error){zone_finished.set_exception(error);});});
  save_preparation->start([&](std::exception_ptr error){try {if(error)std::rethrow_exception(error);
   save_teardown=SaveTeardown::create(strand,save_preparation,[&](std::exception_ptr failure){zone_finished.set_exception(failure);});
   using ConcreteZone=VerticalWorldSessionZoneServices<SessionSaveStore>;ConcreteZone::Ports zone_ports;
   zone_ports.user_mutex=[&](WorldUser&)->std::recursive_mutex&{return disconnect_services->user_lock;};zone_ports.zone_mutex=[&](RuntimeZone&)->std::recursive_mutex&{return disconnect_services->zone_lock;};
   zone_ports.dead_disconnect=[](WorldUser&){throw std::logic_error("unexpected dead user");};zone_ports.tick_ms=[]{return 100U;};
   auto actor=std::shared_ptr<Actor>(live,&live->world_user()->actor);auto skills=std::make_shared<ZoneSaveSkills>(actor);auto quests=std::make_shared<ZoneSaveQuests>();auto save_world=std::make_shared<ZoneSaveWorldFixture>();
   check(skills->runtime.load(eternity::net::Packet{4,5,11,0,0,0,1,11,0,1,0,10,0},0)==UserSkillLoadResult::loaded,"zone skill fixture failed");
   const std::array<PlayerSkill,1> learned{{PlayerSkill{11,3,1,0,0}}};const std::array<PersistedEffect,1> effects{{PersistedEffect{11,1,3}}};check(live->character()->replica().skills().load(learned,effects),"zone skill replica fixture failed");
   EffectTimer effect;effect.effect={11,1,3};effect.end=30100;live->world_user()->actor.effects().append(1,effect,skills->services.effect);
   RestoredQuest quest;quest.definition=quests->definitions.find(1);quest.packet.id=1;quest.packet.end_tick=121234;quest.packet.counters={3,4,5};
   check(quest.definition && quests->runtime.load_active(quest),"zone quest fixture failed");check(live->character()->replica().quests().load({1,9,{0,0,0}}),"zone quest replica fixture failed");
   zone_ports.skills=[skills](VerticalWorldSession&)->UserSkillRuntime&{return skills->runtime;};
   zone_ports.save_quests=[quests](VerticalWorldSession& session,std::uint32_t tick,VerticalSaveCoreSkillServices& save){check(tick==100,"quest save lost captured tick");quest_runtime_save(quests->runtime,session.account_id(),tick,{[&]{save.lock_quests();},[&]{save.unlock_quests();},[&](const eternity::net::Packet& packet){save.send_database(packet);}});};
   zone_ports.save=[save_world](VerticalWorldSession&)->VerticalSaveCoreSkillServices&{return *save_world;};zone_ports.binding=[&](VerticalWorldSession&){auto binding=save_preparation->binding();check(binding && binding->matches(*live),"save binding lost its owner");return binding;};
   zone_ports.leave=[&](VerticalWorldSession& owner,RuntimeZone& zone)->ZoneActorLeaveServices{check(&owner==live.get() && &zone==&live_zone,"zone leave lost owner");ZoneActorLeaveServices leave;leave.user_leave_lifecycle=[](Actor&){};leave.send_view=[&](CellCoordinates,const eternity::net::Packet&){++concrete_zone_views;};return leave;};zone_ports.finish_final=[&](VerticalWorldSession& owner,WorldUser& user){check(&owner==live.get() && &user==live->world_user(),"finalization lost owner");++concrete_finalizations;};
   concrete_zone_services=std::make_shared<ConcreteZone>(strand,sessions,live,std::move(zone_ports));
   const auto binding=save_preparation->binding();const eternity::net::Packet foreign_quest{4,10,11,0,0,0,1,99,0,1,0,8,8,8};
   check(binding && binding->apply_zone_save(foreign_quest)==InventoryReplicaResult::rejected && live->character()->replica().quests().active()[0].counters==std::array<std::uint8_t,3>{0,0,0},"zone save accepted a missing quest replica");
   const eternity::net::Packet foreign_skill{4,8,11,0,0,0,1,4,1,0},foreign_effect{4,9,11,0,0,0,1,22,0,1,1,0,0,0};
   check(binding->apply_zone_save(foreign_skill)==InventoryReplicaResult::rejected && binding->apply_zone_save(foreign_effect)==InventoryReplicaResult::rejected,"zone save accepted a missing skill or effect replica");
   live->world_user()->actor.replace_resources_raw({123,45,67});skills->runtime.slot(0)->skill_id=12;bool foreign_skill_rejected=false;
   try{concrete_zone_services->prepare_save(*live->world_user());}catch(const std::logic_error& error){foreign_skill_rejected=std::string(error.what())=="Zone save skill owner differs from replica";}
   check(foreign_skill_rejected && live->character()->replica().core().character.hp!=123,"mismatched skill identity allowed partial zone save");skills->runtime.slot(0)->skill_id=11;
   const eternity::net::Packet changed_skill{1,8,11,0,0,0,0,0,11,0,2};
   save_world->on_position=[binding,changed_skill]{check(binding->apply(changed_skill)==InventoryReplicaResult::applied,"skill mutation fixture failed");};bool changed_during_save_rejected=false;
   try{concrete_zone_services->prepare_save(*live->world_user());}catch(const std::runtime_error& error){changed_during_save_rejected=std::string(error.what())=="Zone save replica update rejected";}
   save_world->on_position={};check(changed_during_save_rejected && live->character()->replica().skills().learned()[0].level==2,"zone save accepted a changed skill at application");skills->runtime.slot(0)->level=2;
   disconnect_services->on_prepare=[&]{try{zone_disconnect.acknowledge(*live);}catch(const std::logic_error& error){premature_ack_rejected=std::string(error.what())=="Zone disconnect acknowledgment before save request acceptance";}concrete_zone_services->prepare_save(*live->world_user());};
   disconnect_services->on_save=[&]{try{zone_disconnect.acknowledge(*live);}catch(const std::logic_error& error){request_ack_rejected=std::string(error.what())=="Zone disconnect acknowledgment before save request acceptance";}concrete_zone_services->request_save(*live->world_user());};
   disconnect_services->on_leave=[&](WorldUser& user,RuntimeZone& zone){concrete_zone_services->leave_zone(user,zone);};
   disconnect_services->on_final=[&](WorldUser& user){concrete_zone_services->finish_final(user);};
   zone_disconnect.resume(*live);
   auto saved_skills=live->character()->replica().skills().learned();auto saved_effects=live->character()->replica().skills().effects();auto saved_quests=live->character()->replica().quests().active();
   check(save_world->bypass==0 && live->character()->replica().core().character.hp==123,"zone save bypassed retained binding");
   check(saved_skills.size()==1 && saved_skills[0].id==11 && saved_skills[0].delay==9 && saved_effects.size()==1 && saved_effects[0].id==11 && saved_effects[0].delay==30 && saved_quests.size()==1 && saved_quests[0].id==1 && saved_quests[0].delay==2 && saved_quests[0].counters==std::array<std::uint8_t,3>{3,4,5},"populated zone save did not update retained skill and quest replicas");
   check(sessions.size()==1 && sessions.pending_database_work(11)==1 && live_zone.users->find_actor(43),"save request released owner");
  }catch(...){zone_finished.set_exception(std::current_exception());}});
 }catch(...){zone_finished.set_exception(std::current_exception());}});zone_finished_future.get();
 save_dispatcher.wait_idle();std::promise<void> save_cleanup;auto save_cleaned=save_cleanup.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && save_store->exits==2,"save session remained registered");std::weak_ptr<VerticalWorldSession> retained=live;live.reset();check(!retained.expired(),"save preparation lost its session");save_teardown.reset();save_preparation.reset();ack_router.reset();packet_router.reset();gameplay.reset();concrete_zone_services.reset();check(retained_move.expired(),"retained movement owner leaked");check(retained.expired(),"drained save preparation leaked its session");save_cleanup.set_value();}catch(...){save_cleanup.set_exception(std::current_exception());}});save_cleaned.get();save_dispatcher.shutdown();check(!save_dispatcher.failure(),"save dispatcher failed");
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
 asio::post(strand,[&]{try {resumed=false;admission_dispatch=false;close_during_select=true;live_admission.update_world_and_release(live_world);close_during_select=false;
  check(!resumed && !admission_dispatch && sessions.size()==0 && live_world.waiting_count()==0 && live_world.zone_waiting_count(live_zone)==0 && !world_users.find(43),"selection-pass disconnect left a dangling admission user");live.reset();select_checked.set_value();
 }catch(...){select_checked.set_exception(std::current_exception());}});select_checked_future.get();select_closed_future.get();
 // A scheduler callback can publish the zone from within entry delivery.
 // Input stays paused until the rest of delivery and waiting complete.
 std::promise<void> early_entered,early_closed,early_checked;auto early_future=early_entered.get_future();auto early_close_future=early_closed.get_future();auto early_checked_future=early_checked.get_future();WorldSessionSuccessServices early_services(early_entered,early_closed);bool early_update=false;
 early_services.on_lifecycle=[&]{if(!early_update && live->admission().location==2){early_update=true;live_admission.update_world_and_release(live_world);check(live->admission().location==3 && !resumed && !admission_dispatch,"reentrant publication released input during entry");}};
 early_services.disconnect_override=[&](VerticalWorldSession& owner,std::function<void()> done){check(world_users.find(43)==owner.world_user(),"reentrant publication lost world user");world_users.leave(*owner.world_user());done();early_closed.set_value();};
 asio::post(strand,[&]{try{resumed=false;admission_dispatch=false;auto account=world_claim_fixture();live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,early_services,account,channel,roster,
  [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){early_entered.set_exception(std::current_exception());}});early_future.get();
 asio::post(strand,[&]{try{check(early_update && resumed && admission_dispatch && live->admission().location==3,"deferred zone publication did not release input after entry");live->close();check(sessions.size()==0 && live_world.zone_waiting_count(live_zone)==0,"deferred publication did not drain");live.reset();early_checked.set_value();}catch(...){early_checked.set_exception(std::current_exception());}});early_checked_future.get();early_close_future.get();
 // A failed socket publication must report the real error and request close.
 std::promise<void> bad_entered,bad_closed,bad_checked;auto bad_future=bad_entered.get_future();auto bad_close_future=bad_closed.get_future();auto bad_checked_future=bad_checked.get_future();WorldSessionSuccessServices bad_services(bad_entered,bad_closed);unsigned publication_failures=0,publication_closes=0;
 bad_services.failure_override=[&](std::exception_ptr error){check(bool(error) && live->failed(),"missing handoff failure latch");live->receive({7,7});live->receive_nprotect({2,0xa3});++publication_failures;};
 bad_services.disconnect_override=[&](VerticalWorldSession& owner,std::function<void()> done){check(world_users.find(43)==owner.world_user(),"handoff failure lost world user");world_users.leave(*owner.world_user());done();bad_closed.set_value();};
 asio::post(strand,[&]{try{auto account=world_claim_fixture();auto failed_channel=channel;failed_channel.set_admission_dispatch=[](bool){throw std::runtime_error("handoff fixture");};failed_channel.disconnect=[&]{++publication_closes;};
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,bad_services,account,failed_channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){bad_entered.set_exception(std::current_exception());}});bad_future.get();
 asio::post(strand,[&]{try{bool threw=false;try{live_admission.update_world_and_release(live_world);}catch(const std::runtime_error& e){threw=std::string(e.what())=="handoff fixture";}check(threw && publication_failures==1 && publication_closes==1 && live->failed() && live->admission().location==3,"publication error was lost or not closed");auto rejected_queue=VerticalWorldSessionPackets::create(strand,live,1,8,[](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("failed session dispatched");});check(!rejected_queue->enqueue({7,7}) && live->receive_zone_wait({1,2})==VerticalEntryRequest::unhandled,"failed session accepted input");live->report_admission_failure(std::make_exception_ptr(std::runtime_error("duplicate")));check(publication_failures==1 && publication_closes==1,"failure reported twice");live->close();check(sessions.size()==0 && live_world.zone_waiting_count(live_zone)==0,"failed handoff did not drain");live.reset();bad_checked.set_value();}catch(...){bad_checked.set_exception(std::current_exception());}});bad_checked_future.get();bad_close_future.get();
 // Both sessions are published in one pass: a failed first socket must not
 // prevent the second from enabling dispatch and resuming input.
 std::promise<void> batch_entered_a,batch_entered_b,batch_closed_a,batch_closed_b,batch_checked;auto batch_a=batch_entered_a.get_future();auto batch_b=batch_entered_b.get_future();auto batch_close_a=batch_closed_a.get_future();auto batch_close_b=batch_closed_b.get_future();auto batch_check=batch_checked.get_future();
 WorldSessionSuccessServices batch_services_a(batch_entered_a,batch_closed_a,11,43),batch_services_b(batch_entered_b,batch_closed_b,12,44);std::shared_ptr<VerticalWorldSession> batch_session_a,batch_session_b;unsigned batch_failure=0,batch_close_request=0,batch_dispatch=0,batch_resume=0;
 WorldUsers batch_users({[](WorldUser&){},[](WorldUser&){}});VerticalWorldSessionAdmissionPorts batch_ports;
 batch_ports.packets=[](VerticalWorldSession&){};batch_ports.select_zone=[&](VerticalWorldSession&){return &live_zone;};batch_ports.entry_services=[&](VerticalWorldSession&,RuntimeZone&){return live_entry;};batch_ports.diagnostic=[](VerticalWorldSession&,WorldAdmissionError){throw std::logic_error("unexpected batch diagnostic");};batch_ports.rank_window=[](RuntimeZone&)->std::optional<std::uint32_t>{throw std::logic_error("unexpected batch rank");};batch_ports.lock_guild=[](VerticalWorldSession&)->bool{throw std::logic_error("unexpected batch lock");};batch_ports.unlock_guild=[](VerticalWorldSession&){throw std::logic_error("unexpected batch unlock");};
 VerticalWorldSessionAdmissionServices batch_admission(strand,sessions,batch_users,std::move(batch_ports));WorldAdmission batch_world(batch_admission);
 batch_services_a.failure_override=[&](std::exception_ptr e){check(bool(e),"missing batch failure");++batch_failure;};
 batch_services_a.disconnect_override=[&](VerticalWorldSession& owner,std::function<void()> done){batch_users.leave(*owner.world_user());done();batch_closed_a.set_value();};batch_services_b.disconnect_override=[&](VerticalWorldSession& owner,std::function<void()> done){batch_users.leave(*owner.world_user());done();batch_closed_b.set_value();};
 auto batch_load=[pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.name=character==43?"BatchA":"BatchB";snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};};
 asio::post(strand,[&]{try{auto account=world_claim_fixture();auto first=channel;first.set_admission_dispatch=[](bool){throw std::runtime_error("batch handoff");};first.disconnect=[&]{++batch_close_request;};batch_session_a=VerticalWorldSession::create(strand,db.get_executor(),sessions,batch_world,batch_services_a,account,first,roster,batch_load);batch_session_a->open();batch_session_a->receive({4,1,43,0,0,0});}catch(...){batch_entered_a.set_exception(std::current_exception());}});batch_a.get();
 asio::post(strand,[&]{try{auto account=world_claim_fixture(12);auto second=channel;second.set_admission_dispatch=[&](bool enabled){check(enabled,"batch dispatch disabled");++batch_dispatch;};second.resume_input=[&]{++batch_resume;};auto second_roster=roster;second_roster.slots[0]->id=44;batch_session_b=VerticalWorldSession::create(strand,db.get_executor(),sessions,batch_world,batch_services_b,account,second,second_roster,batch_load);batch_session_b->open();batch_session_b->receive({4,1,44,0,0,0});}catch(...){batch_entered_b.set_exception(std::current_exception());}});batch_b.get();
 asio::post(strand,[&]{try{bool threw=false;try{batch_admission.update_world_and_release(batch_world);}catch(const std::runtime_error& e){threw=std::string(e.what())=="batch handoff";}check(threw && batch_failure==1 && batch_close_request==1 && batch_dispatch==1 && batch_resume==1 && batch_world.zone_waiting_count(live_zone)==2,"failed handoff blocked another session");batch_session_a->close();batch_session_b->close();check(sessions.size()==0 && batch_world.zone_waiting_count(live_zone)==0,"batch sessions did not drain");batch_session_a.reset();batch_session_b.reset();batch_checked.set_value();}catch(...){batch_checked.set_exception(std::current_exception());}});batch_check.get();batch_close_a.get();batch_close_b.get();
 using TestRuntime=VerticalWorldSessionRuntime<SessionSaveStore,SessionSaveStore>;using RuntimePreparation=typename TestRuntime::Preparation;using RuntimeBinding=typename TestRuntime::Binding;
 DbJobDispatcher runtime_dispatcher(false,1);auto runtime_store=std::make_shared<SessionSaveStore>();auto runtime_mutex=std::make_shared<std::recursive_mutex>();
 std::promise<void> runtime_entered,runtime_closed,runtime_checked;auto runtime_entered_future=runtime_entered.get_future();auto runtime_closed_future=runtime_closed.get_future();auto runtime_checked_future=runtime_checked.get_future();
 WorldSessionSuccessServices runtime_delegate(runtime_entered,runtime_closed);std::shared_ptr<VerticalWorldSessionAckRouter> runtime_ack;std::atomic<unsigned> runtime_db_leave{0};
 eternity::storage::ResultSet exp_rows;exp_rows.columns={"Level","Grow","Exp"};for(unsigned grow=1;grow<=4;++grow)for(unsigned level=1;level<=60;++level)exp_rows.rows.push_back({std::to_string(level),std::to_string(grow),std::to_string(grow)});const auto experience_table=Experience::load(exp_rows);
 std::function<void()> on_guild_allocate;GuildCatalogPorts guild_ports;guild_ports.allocate_guild=[] {return true;};guild_ports.allocate_member=[] {return true;};guild_ports.transport.allocate=[&](const eternity::net::Packet&){if(on_guild_allocate)on_guild_allocate();return true;};guild_ports.transport.release=[]{};std::vector<eternity::net::Packet> guild_packets;guild_ports.transport.send=[&](GuildUser&,eternity::net::Packet packet){guild_packets.push_back(std::move(packet));};GuildCatalog runtime_guilds(std::move(guild_ports));
 eternity::net::Packet guild_load(3+135,0);guild_load[0]=2;guild_load[1]=11;guild_load[2]=1;guild_load[3]=77;std::copy_n("Guild",5,guild_load.begin()+7);check(runtime_guilds.apply(guild_load)==EntryCatalogUpdate::applied,"guild fixture failed to load");
 eternity::net::Packet member_load(7+29,0);member_load[0]=3;member_load[1]=11;member_load[2]=77;member_load[6]=1;member_load[7]=43;member_load[14]=3;std::copy_n("Member",6,member_load.begin()+15);check(runtime_guilds.apply(member_load)==EntryCatalogUpdate::applied,"guild member fixture failed to load");
 TestRuntime::Ports runtime_ports;
 runtime_ports.actor_rules=[&]()->const ActorLoadRules&{return runtime_delegate.actor_rules();};runtime_ports.actor_options=[&](const PlayerSnapshot& snapshot){return runtime_delegate.actor_options(snapshot);};runtime_ports.actor_scene=[&](VerticalWorldSession& owner,const PlayerSnapshot& snapshot){check(&owner==live.get() && sessions.find_account(owner.account_id()).get()==&owner && snapshot.core.character.id==43 && owner.party_user().id==43 && owner.party_user().name==snapshot.core.character.name && owner.party_requests().country==0 && owner.party_user().send_socket==-1,"actor scene lost session or party identity");return runtime_delegate.actor_scene(owner,snapshot);};
 runtime_ports.configure_admission=[&](VerticalWorldSession& owner,WorldAdmissionUser& user,const PlayerSnapshot& snapshot){runtime_delegate.configure_admission(owner,user,snapshot);};
 runtime_ports.preparation=[&](std::shared_ptr<VerticalWorldSession> owner){
  return RuntimePreparation::create(strand,db.get_executor(),sessions,std::move(owner),[runtime_store]{return runtime_store;},
   [&](VerticalWorldSession& session,std::shared_ptr<SessionSaveStore> store){CharacterSaveLifecyclePorts ports;
    ports.send=[](eternity::net::Packet){throw std::logic_error("unexpected runtime DB send");};ports.leave=[&]{++runtime_db_leave;};ports.reset=[]{throw std::logic_error("unexpected runtime reset");};ports.roster=[](bool){throw std::logic_error("unexpected runtime roster");};ports.save_friends=[]{};ports.save_blocks=[]{};
    return RuntimeBinding::create(strand,sessions,session.shared_from_this(),runtime_dispatcher,std::move(store),runtime_mutex,UserLifecycleState{11,2,0,true},"fixture",std::move(ports),runtime_ack,
     [](VerticalWorldSession&,eternity::net::Packet){throw std::logic_error("unexpected runtime DB reply");},[](std::exception_ptr){throw std::logic_error("unexpected runtime DB failure");});});};
 std::shared_ptr<VerticalWorldAccountSocial> runtime_social;
 runtime_ports.entry_state=[&](VerticalWorldSession& owner){return runtime_delegate.entry_state(owner);};runtime_ports.experience=&experience_table;runtime_ports.guilds=&runtime_guilds;runtime_ports.world_users=&world_users;runtime_ports.protocol=[&](VerticalWorldSession& owner){return runtime_delegate.protocol(owner);};runtime_ports.waiting=[&](VerticalWorldSession& owner){runtime_delegate.waiting(owner);};
 runtime_ports.gameplay=[](VerticalWorldSession&,RuntimeZone&)->std::shared_ptr<VerticalWorldSessionGameplay>{throw std::logic_error("unexpected runtime gameplay");};runtime_ports.nprotect=[](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected runtime nprotect");};runtime_ports.unavailable=[&](VerticalWorldSession& owner){runtime_delegate.unavailable(owner);};runtime_ports.failure=[&](VerticalWorldSession& owner,std::exception_ptr error){runtime_delegate.failure(owner,error);};
 unsigned runtime_disconnects=0;bool guild_release_order=false,duplicate_release_rejected=false,reentrant_release_rejected=false,social_released=false,social_reentrant_rejected=false,social_duplicate_rejected=false,social_rebind_rejected=false;std::shared_ptr<SessionSocialFixture> social_owner;TestRuntime* runtime_ptr=nullptr;
 runtime_ports.disconnect=[&](VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done){++runtime_disconnects;
  if(owner.world_user() && world_users.find(owner.world_user()->actor.id())==owner.world_user())world_users.leave(*owner.world_user());
  if(owner.guild_user().character_id){WorldLifecycleState state;state.subject.character_id=owner.guild_user().character_id;WorldLifecyclePorts world_ports;
   world_ports.admin_enter=[] {return true;};world_ports.step=[&](WorldLifecycleStep step){if(step==WorldLifecycleStep::release_friends)guild_release_order=runtime_guilds.find(77)->membership.offline.contains(43);};
   world_ports.close=[](std::uint32_t){};world_ports.local_calendar=[] {return std::chrono::sys_seconds{};};world_ports.game_log=[](eternity::net::Packet){};world_ports.manager=[](eternity::net::Packet){};
   world_ports.take_money=[](std::uint32_t)->std::optional<std::uint32_t>{return {};};world_ports.save_money=[](std::uint32_t,std::uint32_t){};
   auto bound=runtime_ptr->bind_social_world_release(owner,social_owner,std::move(world_ports));WorldLifecyclePorts duplicate_ports;duplicate_ports.step=[](WorldLifecycleStep){};try{(void)runtime_ptr->bind_social_world_release(owner,social_owner,std::move(duplicate_ports));}catch(const std::logic_error&){social_rebind_rejected=true;}social_owner->on_release=[&]{try{bound.step(WorldLifecycleStep::release_friends);}catch(const std::logic_error& error){social_reentrant_rejected=std::string(error.what())=="Social release already started";}};on_guild_allocate=[&]{try{bound.step(WorldLifecycleStep::release_guild);}catch(const std::logic_error& error){reentrant_release_rejected=std::string(error.what())=="Guild release already started";}};leave_world(state,bound);on_guild_allocate={};social_owner->on_release={};try{bound.step(WorldLifecycleStep::release_friends);}catch(const std::logic_error&){social_duplicate_rejected=true;}
   try{bound.step(WorldLifecycleStep::release_guild);}catch(const std::logic_error&){duplicate_release_rejected=true;}}
  done();};
 using ComposedOwner=VerticalWorldSessionOwner<SessionSaveStore,SessionSaveStore>;std::shared_ptr<ComposedOwner> composed_owner;std::unique_ptr<eternity::net::WorldApplication> composed_application,composed_roster;std::shared_ptr<VerticalWorldSessionAckRouter> composed_ack;eternity::net::WorldApplicationFactory composed_factory;
 std::promise<void> owner_started,owner_release,owner_cancelled,owner_closed,owner_checked;auto owner_started_future=owner_started.get_future();auto owner_release_future=owner_release.get_future();auto owner_cancelled_future=owner_cancelled.get_future();auto owner_closed_future=owner_closed.get_future();auto owner_checked_future=owner_checked.get_future();
 auto owner_ports=runtime_ports;owner_ports.disconnect=[&](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done){done();owner_closed.set_value();};
 asio::post(strand,[&]{try {
  VerticalWorldSessionAdmissionPorts admission;admission.select_zone=[](VerticalWorldSession&)->RuntimeZone*{throw std::logic_error("unexpected composed zone selection");};
  admission.entry_services=[](VerticalWorldSession&,RuntimeZone&)->ZoneActorEntryServices{throw std::logic_error("unexpected composed entry");};admission.diagnostic=[](VerticalWorldSession&,WorldAdmissionError){throw std::logic_error("unexpected composed diagnostic");};
  admission.rank_window=[](RuntimeZone&)->std::optional<std::uint32_t>{throw std::logic_error("unexpected composed rank");};admission.lock_guild=[](VerticalWorldSession&)->bool{throw std::logic_error("unexpected composed guild lock");};admission.unlock_guild=[](VerticalWorldSession&){throw std::logic_error("unexpected composed guild unlock");};
  composed_owner=std::make_shared<ComposedOwner>(strand,db.get_executor(),world_users,2,64,std::move(owner_ports),std::move(admission),world_claim_registry(),0);
  auto root_bindings=ComposedOwner::root_bindings(composed_owner);
  check(root_bindings.sessions.get()==&composed_owner->registry() && root_bindings.admission.get()==&composed_owner->world() && !root_bindings.sessions.owner_before(root_bindings.admission) && !root_bindings.admission.owner_before(root_bindings.sessions),"root bindings differ from owner");root_bindings.update_admission();
  bool root_blocked=false;try{composed_owner->shutdown();}catch(const std::logic_error&){root_blocked=true;}check(root_blocked,"owner shutdown ignored retained root");root_bindings={};
  bool missing_zone_rejected=false;try{(void)composed_owner->make_zone_ack_router();}catch(const std::logic_error&){missing_zone_rejected=true;}check(missing_zone_rejected,"owner accepted a zone ACK router before installation");
  bool missing_cleanup_rejected=false;try{composed_owner->install_zone_disconnect([](VerticalWorldSession&)->VerticalDisconnectServices&{throw std::logic_error("unexpected zone service");},{});}catch(const std::invalid_argument&){missing_cleanup_rejected=true;}check(missing_cleanup_rejected,"owner accepted missing zone cleanup");
  ComposedOwner::ZoneServices::Ports installed_zone;
  installed_zone.user_mutex=[](WorldUser&)->std::recursive_mutex&{throw std::logic_error("unexpected user lock");};installed_zone.zone_mutex=[](RuntimeZone&)->std::recursive_mutex&{throw std::logic_error("unexpected zone lock");};
  installed_zone.dead_disconnect=[](WorldUser&){throw std::logic_error("unexpected dead disconnect");};installed_zone.tick_ms=[]{return 0U;};
  installed_zone.skills=[](VerticalWorldSession&)->UserSkillRuntime&{throw std::logic_error("unexpected skills");};installed_zone.quests=[](VerticalWorldSession&)->QuestRuntime&{throw std::logic_error("unexpected quests");};
  installed_zone.save=[](VerticalWorldSession&)->VerticalSaveCoreSkillServices&{throw std::logic_error("unexpected save");};installed_zone.leave=[](VerticalWorldSession&,RuntimeZone&)->ZoneActorLeaveServices{throw std::logic_error("unexpected leave");};installed_zone.finish_final=[](VerticalWorldSession&,WorldUser&){throw std::logic_error("unexpected final");};
  auto zone_completion=[](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()>){throw std::logic_error("unexpected zone completion");};
  auto duplicate_quests=installed_zone;duplicate_quests.save_quests=[](VerticalWorldSession&,std::uint32_t,VerticalSaveCoreSkillServices&){};bool duplicate_quests_rejected=false;try{composed_owner->install_session_zone_services(std::move(duplicate_quests),zone_completion);}catch(const std::invalid_argument&){duplicate_quests_rejected=true;}check(duplicate_quests_rejected,"owner accepted conflicting quest save owners");
  auto foreign_binding=installed_zone;foreign_binding.binding=[](VerticalWorldSession&)->std::shared_ptr<ComposedOwner::ZoneServices::Binding>{return {};};bool foreign_binding_rejected=false;try{composed_owner->install_session_zone_services(std::move(foreign_binding),zone_completion);}catch(const std::invalid_argument&){foreign_binding_rejected=true;}check(foreign_binding_rejected,"owner accepted external zone binding");
  installed_zone.quests={};composed_owner->install_retained_quest_zone_services(std::move(installed_zone),zone_completion);
  auto zone_ack=composed_owner->make_zone_ack_router();check(zone_ack->matches(strand,composed_owner->registry()),"owner zone ACK router lost registry or strand");zone_ack.reset();
  composed_owner->update_zone_user_logout(live_zone,100,[](VerticalWorldSession&){throw std::logic_error("unexpected zone logout");});
  bool empty_ack_rejected=false;try{(void)composed_owner->make_ack_router({});}catch(const std::invalid_argument&){empty_ack_rejected=true;}check(empty_ack_rejected,"owner accepted an ACK router without a leave service");
  composed_ack=composed_owner->make_ack_router([](auto){throw std::logic_error("unexpected composed ACK");});check(composed_ack->matches(strand,composed_owner->registry()),"owner ACK router lost registry or strand");
  composed_factory=composed_owner->factory([&](const eternity::session::LoginSession&){return roster;},[](const eternity::session::LoginSession&,CharacterRoster&,const eternity::net::Packet&)->CharacterReply{throw std::logic_error("unexpected roster command");},
   [&](const eternity::session::LoginSession&,const CharacterRoster&){return VerticalAsyncSelection::Load([&](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{owner_started.set_value();owner_release_future.wait();return {};});});
  auto account=world_claim_fixture();account.username="owner-fixture";auto owner_channel=channel;owner_channel.send=[](eternity::net::Packet){};
  composed_roster=composed_factory(account,owner_channel);composed_roster->open();composed_application=composed_factory(account,owner_channel);composed_application->open();composed_application->receive({4,1,43,0,0,0});
 }catch(...){owner_started.set_exception(std::current_exception());}});owner_started_future.get();
 asio::post(strand,[&]{try {
  auto selected=composed_owner->registry().find_account(11);check(selected && selected->username()=="owner-fixture","session did not retain authenticated account name");
  bool matching_state=true;unsigned router_variant=0;VerticalWorldSession::Registry foreign_ack_registry;VerticalWorldSession::Strand foreign_ack_strand=asio::make_strand(io);auto sql_mutex=std::make_shared<std::recursive_mutex>();VerticalWorldSessionSqlPreparationServices sql_services;
  sql_services.state=[&](VerticalWorldSession& owner){check(&owner==selected.get(),"SQL provider lost session");return UserLifecycleState{matching_state?11U:12U,2,0,true};};
  sql_services.recovery_path=[](VerticalWorldSession&){return std::filesystem::path(".local/session-owner-recovery");};
  sql_services.lifecycle=[](VerticalWorldSession&){CharacterSaveLifecyclePorts ports;ports.send=[](eternity::net::Packet){throw std::logic_error("unexpected save send");};ports.leave=[]{throw std::logic_error("unexpected save leave");};ports.reset=[]{throw std::logic_error("unexpected save reset");};ports.roster=[](bool){throw std::logic_error("unexpected save roster");};ports.save_friends=[]{throw std::logic_error("unexpected friends save");};ports.save_blocks=[]{throw std::logic_error("unexpected blocks save");};return ports;};
  sql_services.cache_mutex=[&](VerticalWorldSession&){return sql_mutex;};sql_services.ack_router=[&](VerticalWorldSession::Registry& registry){
   if(router_variant==0){check(&registry==&composed_owner->registry(),"SQL provider requested foreign owner registry");return composed_owner->make_ack_router([](auto){throw std::logic_error("unexpected SQL ack");});}
   return std::make_shared<VerticalWorldSessionAckRouter>(router_variant==2?foreign_ack_strand:VerticalWorldSession::Strand(strand),router_variant==1?foreign_ack_registry:registry,[](auto){throw std::logic_error("unexpected SQL ack");});};
  sql_services.other=[](VerticalWorldSession&,eternity::net::Packet){throw std::logic_error("unexpected SQL reply");};sql_services.failure=[](std::exception_ptr){throw std::logic_error("unexpected SQL failure");};
  auto sql_provider=vertical_world_session_sql_preparation_provider(runtime_dispatcher,{},1,"PS_GameData",std::move(sql_services));
  auto sql_preparation=sql_provider(strand,db.get_executor(),composed_owner->registry(),selected);check(sql_preparation && sql_preparation->matches(strand,composed_owner->registry(),*selected) && !sql_preparation->matches(strand,foreign_ack_registry,*selected) && !sql_preparation->matches(foreign_ack_strand,composed_owner->registry(),*selected),"retained SQL preparation identity differed");sql_preparation.reset();
  matching_state=false;bool wrong_account=false;try{(void)sql_provider(strand,db.get_executor(),composed_owner->registry(),selected);}catch(const std::invalid_argument&){wrong_account=true;}check(wrong_account,"SQL provider accepted foreign account state");matching_state=true;
  for(router_variant=1;router_variant<=2;++router_variant){bool rejected=false;try{(void)sql_provider(strand,db.get_executor(),composed_owner->registry(),selected);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"SQL provider accepted a foreign acknowledgment router");}selected.reset();
  composed_owner->stop_accepting();composed_owner->stop_accepting();composed_owner->update_world();
  auto drain_ack=composed_owner->make_ack_router([](auto){throw std::logic_error("unexpected draining ACK");});check(drain_ack->matches(strand,composed_owner->registry()),"stopping owner could not bind draining ACK router");drain_ack.reset();
  eternity::session::LoginSession account{};account.user_id=11;auto owner_channel=channel;owner_channel.send=[](eternity::net::Packet){};bool new_roster_rejected=false,late_selection_rejected=false;
  try{(void)composed_factory(account,owner_channel);}catch(const std::logic_error&){new_roster_rejected=true;}try{composed_roster->receive({4,1,43,0,0,0});}catch(const std::logic_error&){late_selection_rejected=true;}
  check(new_roster_rejected && late_selection_rejected,"stopping owner accepted a new roster or selection");bool blocked=false;try{composed_owner->shutdown();}catch(const std::logic_error&){blocked=true;}
  check(blocked && composed_owner->registry().pending_database_work(11)==1 && composed_owner->world().selection_count()==1,"composed owner did not fence active selection");
  composed_application->close();composed_application.reset();check(composed_owner->registry().size()==1,"composed owner released canceled SQL early");owner_cancelled.set_value();
 }catch(...){owner_cancelled.set_exception(std::current_exception());}});owner_cancelled_future.get();owner_release.set_value();owner_closed_future.get();
 asio::post(strand,[&]{try {check(composed_owner->registry().size()==0 && composed_owner->world().selection_count()==0,"composed owner did not drain selection");
  eternity::session::LoginSession account{};account.user_id=11;auto owner_channel=channel;owner_channel.send=[](eternity::net::Packet){};
  bool roster_blocked=false;try{composed_owner->shutdown();}catch(const std::logic_error&){roster_blocked=true;}check(roster_blocked,"composed owner ignored live roster application");composed_roster->close();composed_roster.reset();
  auto ack_alias=composed_ack;composed_ack.reset();bool ack_blocked=false;try{composed_owner->shutdown();}catch(const std::logic_error&){ack_blocked=true;}check(ack_blocked,"composed owner ignored retained ACK router alias");ack_alias.reset();composed_owner->shutdown();
  bool stopped=false;try{(void)composed_factory(account,owner_channel);}catch(const std::logic_error&){stopped=true;}check(stopped,"stopped owner accepted a new roster");
  auto stale_factory=std::move(composed_factory);composed_owner.reset();bool stale_rejected=false;try{(void)stale_factory(account,owner_channel);}catch(const std::logic_error&){stale_rejected=true;}check(stale_rejected,"factory outlived owner without a fence");owner_checked.set_value();
 }catch(...){owner_checked.set_exception(std::current_exception());}});owner_checked_future.get();
 // Close from the real preparation provider while selection delivery still
 // owns its account fence; no new resource-open job may be posted afterwards.
 std::promise<void> provider_unused_entry,provider_unused_close,provider_close_done,provider_close_checked;auto provider_close_future=provider_close_done.get_future();auto provider_checked_future=provider_close_checked.get_future();
 WorldSessionSuccessServices provider_delegate(provider_unused_entry,provider_unused_close);std::unique_ptr<ComposedOwner> provider_owner;std::unique_ptr<eternity::net::WorldApplication> provider_app;eternity::net::WorldApplicationFactory provider_factory;
 std::atomic<bool> unexpected_open=false;bool provider_closed_inside=false;std::weak_ptr<RuntimePreparation> discarded_preparation;std::shared_ptr<VerticalWorldAccountSocial> provider_social;
 auto provider_ports=runtime_ports;provider_ports.preparation={};provider_ports.actor_rules=[&]()->const ActorLoadRules&{return provider_delegate.actor_rules();};provider_ports.actor_options=[&](const PlayerSnapshot& value){return provider_delegate.actor_options(value);};
 provider_ports.actor_scene=[&](VerticalWorldSession& owner,const PlayerSnapshot& value){return provider_delegate.actor_scene(owner,value);};provider_ports.configure_admission=[&](VerticalWorldSession& owner,WorldAdmissionUser& user,const PlayerSnapshot& value){provider_delegate.configure_admission(owner,user,value);};
 provider_ports.failure=[&](VerticalWorldSession&,std::exception_ptr error){provider_close_done.set_exception(error);};provider_ports.disconnect=[&](VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done){check(owner.closing() && provider_owner->registry().pending_database_work(11)==0,"provider close bypassed selection fence");done();provider_close_done.set_value();};
 asio::post(strand,[&]{try {
  VerticalWorldSessionAdmissionPorts admission;admission.select_zone=[](VerticalWorldSession&)->RuntimeZone*{throw std::logic_error("unexpected provider zone");};admission.entry_services=[](VerticalWorldSession&,RuntimeZone&)->ZoneActorEntryServices{throw std::logic_error("unexpected provider entry");};
  admission.diagnostic=[](VerticalWorldSession&,WorldAdmissionError){throw std::logic_error("unexpected provider diagnostic");};admission.rank_window=[](RuntimeZone&)->std::optional<std::uint32_t>{throw std::logic_error("unexpected provider rank");};admission.lock_guild=[](VerticalWorldSession&)->bool{throw std::logic_error("unexpected provider guild");};admission.unlock_guild=[](VerticalWorldSession&){throw std::logic_error("unexpected provider guild unlock");};
  ComposedOwner::RuntimePreparationProvider provider=[&](VerticalWorldSession::Strand world_strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,TestRuntime& session_runtime,std::shared_ptr<VerticalWorldSession> session){
   provider_social=session_runtime.social_session(*session).account();check(&registry==&provider_owner->registry() && &session_runtime==&provider_owner->runtime() && bool(provider_social->shared_mutex()),"provider did not receive owner runtime and account mutex");auto value=RuntimePreparation::create(world_strand,database,registry,session,[&]{unexpected_open=true;return std::make_shared<SessionSaveStore>();},[](VerticalWorldSession&,std::shared_ptr<SessionSaveStore>)->std::shared_ptr<RuntimeBinding>{throw std::logic_error("closed session bound save resource");});discarded_preparation=value;
   session->close();provider_closed_inside=session->closing() && registry.pending_database_work(11)==1;return value;};
  provider_owner=std::make_unique<ComposedOwner>(strand,db.get_executor(),world_users,2,64,std::move(provider_ports),std::move(admission),world_claim_registry(),0,ComposedOwner::PreparationProvider{},std::move(provider));
  provider_factory=provider_owner->factory([&](const eternity::session::LoginSession&){return roster;},[](const eternity::session::LoginSession&,CharacterRoster&,const eternity::net::Packet&)->CharacterReply{throw std::logic_error("unexpected provider roster command");},
   [pool](const eternity::session::LoginSession&,const CharacterRoster&){return VerticalAsyncSelection::Load([pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});});
  auto account=world_claim_fixture();auto provider_channel=channel;provider_channel.send=[](eternity::net::Packet){};provider_app=provider_factory(account,provider_channel);provider_app->open();provider_app->receive({4,1,43,0,0,0});
 }catch(...){provider_close_done.set_exception(std::current_exception());}});provider_close_future.get();
 asio::post(strand,[&]{try {check(provider_closed_inside && !unexpected_open && discarded_preparation.expired() && provider_owner->registry().size()==0 && provider_owner->world().selection_count()==0,"reentrant provider close started or retained preparation");provider_app->close();provider_app.reset();
  bool account_blocked=false;try{provider_owner->shutdown();}catch(const std::logic_error&){account_blocked=true;}check(account_blocked && !provider_owner->runtime().social_accounts().retire_all(),"owner discarded retained social account");
  provider_social.reset();check(provider_owner->runtime().social_accounts().retire_all(),"drained social accounts did not retire");provider_owner->shutdown();provider_factory={};provider_owner.reset();provider_close_checked.set_value();}catch(...){provider_close_checked.set_exception(std::current_exception());}});provider_checked_future.get();
 for(bool stats:{false,true})for(bool skills:{false,true}){PlayerCore core;core.reset_stats=stats;core.reset_skills=skills;const auto flags=vertical_world_session_entry_flags(core,77);check(flags.auto_character_id==77 && flags.flag_584c==0 && flags.flag_584d==static_cast<std::uint8_t>(skills),"original reset flag normalization differed");}
 {eternity::net::Packet manager(99,0);manager[0]=1;manager[1]=8;manager[3]=0x78;manager[4]=0x56;manager[7]=11;manager[90]=0x78;manager[91]=0x56;manager[92]=0x34;manager[93]=0x12;
  check(VerticalWorldSessionAutoCharacter::from_manager(manager,11,0x5678).value()==0x12345678,"manager auto-character lost byte order");
  for(unsigned invalid=0;invalid<4;++invalid){auto packet=manager;if(invalid==0)packet.pop_back();else if(invalid==1)packet[2]=1;else if(invalid==2)packet[7]=12;else packet[0]=2;bool rejected=false;try{(void)VerticalWorldSessionAutoCharacter::from_manager(packet,11,0x5678);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid manager connection accepted");}
 }
 auto contextless_ports=runtime_ports;contextless_ports.disconnect=[](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done){done();};
 auto foreign_runtime_ports=runtime_ports;runtime_ports.entry_state={};unsigned projected_entries=0;runtime_ports.entry_auto_character_id=[&](VerticalWorldSession& owner){check(owner.character() && owner.world_user(),"entry projection lost session");++projected_entries;return owner.retained_auto_character_id();};TestRuntime runtime(strand,sessions,2,64,std::move(runtime_ports));runtime_ptr=&runtime;
 std::promise<void> runtime_empty;auto runtime_empty_future=runtime_empty.get_future();asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,runtime,account,channel,roster,[](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::logic_error("unexpected runtime selection");});
  TestRuntime contextless(strand,sessions,2,64,std::move(contextless_ports));bool shutdown_blocked=false,duplicate_blocked=false;
  contextless.disconnect(*live,{},[&]{try{contextless.assert_shutdown_ready();}catch(const std::logic_error& e){shutdown_blocked=std::string(e.what())=="Session runtime still has disconnects";}try{contextless.disconnect(*live,{},[]{});}catch(const std::logic_error& e){duplicate_blocked=std::string(e.what())=="Session disconnect already started";}});
  check(shutdown_blocked && duplicate_blocked,"contextless disconnect fence failed");contextless.assert_shutdown_ready();
  live->open();live->close();check(sessions.size()==0 && world.selection_count()==0 && runtime_disconnects==1,"runtime failed preselection disconnect");live.reset();runtime_empty.set_value();
 }catch(...){runtime_empty.set_exception(std::current_exception());}});runtime_empty_future.get();
 asio::post(strand,[&]{try {runtime_ack=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[](std::shared_ptr<VerticalWorldSession>){throw std::logic_error("unexpected runtime acknowledgment");});auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,runtime,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.reset_stats=true;snapshot.core.reset_skills=true;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  eternity::net::Packet manager_connect(99,0);manager_connect[0]=1;manager_connect[1]=8;manager_connect[3]=0x34;manager_connect[4]=0x12;manager_connect[7]=11;
  bool foreign=false;try{live->install_manager_connect(manager_connect,0x1235);}catch(const std::invalid_argument&){foreign=true;}check(foreign,"foreign manager connection accepted");live->install_manager_connect(manager_connect,0x1234);bool duplicate=false;try{live->install_manager_connect(manager_connect,0x1234);}catch(const std::logic_error&){duplicate=true;}check(duplicate && live->retained_auto_character_id()==0,"manager connection ownership differed");
  runtime_social=runtime.social_accounts().acquire(*live);runtime_social->replica().friends()[0].id=71;live->open();runtime.update_packets(*live);live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){runtime_entered.set_exception(std::current_exception());}});runtime_entered_future.get();
 std::promise<void> binding_probe_ready;auto binding_probe_future=binding_probe_ready.get_future();std::shared_ptr<TestRuntime::Binding> binding_probe;
 asio::post(strand,[&]{try{binding_probe=runtime.save_binding(*live);check(bool(binding_probe),"missing runtime binding for strand probe");binding_probe_ready.set_value();}catch(...){binding_probe_ready.set_exception(std::current_exception());}});binding_probe_future.get();
 unsigned off_strand_rejected=0;const eternity::net::Packet off_strand_packet{6,6,11,0,0,0,1,0,1,0,1,0};
 try{(void)binding_probe->apply(off_strand_packet);}catch(const std::logic_error&){++off_strand_rejected;}
 try{(void)binding_probe->apply_zone_save(off_strand_packet);}catch(const std::logic_error&){++off_strand_rejected;}
 try{(void)binding_probe->disconnect(vertical_database_disconnect_packet(11));}catch(const std::logic_error&){++off_strand_rejected;}
 try{(void)binding_probe->submit(9);}catch(const std::logic_error&){++off_strand_rejected;}
 try{binding_probe->stop_and_drain([]{});}catch(const std::logic_error&){++off_strand_rejected;}
 check(off_strand_rejected==5,"save binding accepted off-strand operation");binding_probe.reset();
 asio::post(strand,[&]{try {check(live->entry_state() && live->entry_state()->ready && world.waiting_count()==1 && runtime.save_binding(*live),"concrete runtime did not prepare entry");check(!runtime_delegate.scene.expired() && runtime_delegate.scene_actor==&live->world_user()->actor && runtime_delegate.scene_binds==1,"session failed to bind its actor scene");
  auto reentrant_ports_base=foreign_runtime_ports;VerticalWorldSession::Registry foreign_preparation_registry;std::weak_ptr<RuntimePreparation> foreign_preparation;bool foreign_disconnected=false;
  foreign_runtime_ports.preparation=[&](std::shared_ptr<VerticalWorldSession> owner){foreign_preparation_registry.reserve(owner->account_id(),owner);
   auto value=RuntimePreparation::create(strand,db.get_executor(),foreign_preparation_registry,std::move(owner),[]{throw std::logic_error("foreign opener started");return std::shared_ptr<SessionSaveStore>{};},[](VerticalWorldSession&,std::shared_ptr<SessionSaveStore>)->std::shared_ptr<RuntimeBinding>{throw std::logic_error("foreign binding started");});foreign_preparation=value;return value;};
  foreign_runtime_ports.disconnect=[](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done){done();};TestRuntime foreign_runtime(strand,sessions,2,64,std::move(foreign_runtime_ports));
  bool foreign_rejected=false;try{foreign_runtime.prepare(*live,[](std::exception_ptr){throw std::logic_error("foreign preparation completed");});}catch(const std::logic_error&){foreign_rejected=true;}
  check(foreign_rejected && foreign_preparation.expired() && sessions.pending_database_work(11)==0,"foreign preparation remained in session context");
  auto* saved_guild=live->guild_user().guild;live->guild_user().guild=nullptr;foreign_runtime.disconnect(*live,{},[&]{foreign_disconnected=true;});live->guild_user().guild=saved_guild;
  check(foreign_disconnected && foreign_preparation_registry.release(11,*live),"foreign preparation blocked session cleanup");
  for(bool fail_provider:{false,true}) {
   auto reentrant_ports=reentrant_ports_base;TestRuntime* reentrant_runtime_ptr=nullptr;bool cancelled=false,opener_started=false,completion_called=false,disconnected=false;std::weak_ptr<RuntimePreparation> pending_preparation;
   reentrant_ports.preparation=[&](std::shared_ptr<VerticalWorldSession> owner){
    auto value=RuntimePreparation::create(strand,db.get_executor(),sessions,owner,[&]{opener_started=true;return std::make_shared<SessionSaveStore>();},[](VerticalWorldSession&,std::shared_ptr<SessionSaveStore>)->std::shared_ptr<RuntimeBinding>{throw std::logic_error("cancelled binder ran");});pending_preparation=value;
    reentrant_runtime_ptr->cancel_preparation(*owner,[&]{cancelled=true;});check(!cancelled,"construction cancellation completed before provider returned");
    if(fail_provider)throw std::runtime_error("provider failed during cancellation");return value;};
   reentrant_ports.disconnect=[](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done){done();};TestRuntime reentrant_runtime(strand,sessions,2,64,std::move(reentrant_ports));reentrant_runtime_ptr=&reentrant_runtime;
   bool rejected=false;try{reentrant_runtime.prepare(*live,[&](std::exception_ptr){completion_called=true;});}catch(const std::runtime_error&){rejected=true;}
   check(rejected==fail_provider && cancelled && !opener_started && !completion_called && pending_preparation.expired() && sessions.pending_database_work(11)==0,"construction cancellation started or retained work");
   saved_guild=live->guild_user().guild;live->guild_user().guild=nullptr;reentrant_runtime.disconnect(*live,{},[&]{disconnected=true;});live->guild_user().guild=saved_guild;check(disconnected,"construction cancellation blocked disconnect cleanup");
  }
  for(bool cancel_inside:{false,true}) {
   auto started_ports=reentrant_ports_base;TestRuntime* started_runtime_ptr=nullptr;bool provider_completion=false,cancel_completion=false;unsigned failure_reports=0;std::weak_ptr<RuntimePreparation> started_preparation;
   started_ports.failure=[&](VerticalWorldSession&,std::exception_ptr error){check(bool(error),"started preparation failure lost error");++failure_reports;};
   started_ports.preparation=[&](std::shared_ptr<VerticalWorldSession> owner){
    auto value=RuntimePreparation::create(strand,asio::any_io_executor{},sessions,owner,[]{throw std::logic_error("rejected opener ran");return std::shared_ptr<SessionSaveStore>{};},[](VerticalWorldSession&,std::shared_ptr<SessionSaveStore>)->std::shared_ptr<RuntimeBinding>{throw std::logic_error("rejected binder ran");});
    started_preparation=value;value->start([&](std::exception_ptr error){check(bool(error),"invalid executor reported success");provider_completion=true;});
    if(cancel_inside)started_runtime_ptr->cancel_preparation(*owner,[&]{cancel_completion=true;});return value;};
   started_ports.disconnect=[](VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done){done();};
   {TestRuntime started_runtime(strand,sessions,2,64,std::move(started_ports));started_runtime_ptr=&started_runtime;
    bool rejected=false;try{started_runtime.prepare(*live,[](std::exception_ptr){throw std::logic_error("started preparation published");});}catch(const std::logic_error&){rejected=true;}
    if(!cancel_inside)started_runtime.cancel_preparation(*live,[&]{cancel_completion=true;});
    bool disconnect_rejected=false;try{started_runtime.disconnect(*live,{},[]{throw std::logic_error("invalid preparation disconnected");});}catch(const std::logic_error&){disconnect_rejected=true;}
    check(rejected!=cancel_inside && provider_completion && !cancel_completion && failure_reports==1 && disconnect_rejected && !started_preparation.expired() && sessions.pending_database_work(11)==0,"started preparation was treated as drained");
   }
   check(started_preparation.expired(),"test runtime retained rejected preparation after destruction");
  }
  const auto actual_experience=runtime.experience(*live);check(actual_experience.previous==60 && actual_experience.next==0,"runtime did not use the selected character experience curve");
  auto data=std::find_if(runtime_delegate.sent.begin(),runtime_delegate.sent.end(),[](const auto& packet){return packet.size()>=22 && packet[0]==5 && packet[1]==1;});
  check(data!=runtime_delegate.sent.end() && (*data)[14]==60 && (*data)[15]==0 && (*data)[18]==0,"entry packet did not use runtime experience bounds");
  check(data->size()==64 && std::string(data->begin()+58,data->end()-1)=="Guild","entry data did not use guild initialized during entry");
  check(projected_entries==1 && live->entry_state()->user_id==11 && live->entry_state()->character_id==43 && live->entry_state()->flag_584c==0 && live->entry_state()->flag_584d==1 && std::find(runtime_delegate.sent.begin(),runtime_delegate.sent.end(),eternity::net::Packet{0x1b,2,0,0})!=runtime_delegate.sent.end(),"runtime entry projection lost live session fields");
  check(live->entry_state()->guild_present && live->guild_user().guild==&runtime_guilds.find(77)->membership && runtime_guilds.find(77)->membership.online.contains(43) && !guild_packets.empty(),"runtime did not initialize actual guild membership");
  // Exercise the actual FIFO/runtime guild route while the retained Actor is
  // admitted to a zone; restore the fixture's admission state afterward.
  auto previous_location=live->admission().location;auto* previous_zone=live->admission().zone;auto prior_send=live->channel().send;
  std::vector<eternity::net::Packet> guild_responses;live->channel().send=[&](eternity::net::Packet p){guild_responses.push_back(std::move(p));};
  live->admission().location=4;live->admission().zone=&live_zone;live->receive({0x23,0x0d});runtime.update_packets(*live);
  check(guild_responses==std::vector<eternity::net::Packet>{{0x23,0x0d,0,0,0,0}},"runtime guild etin request was not routed to the retained owner");
  live->receive({7,13,77,0,0,0});runtime.update_packets(*live);
  check(guild_responses.size()==2 && guild_responses.back()==eternity::net::Packet({7,13,0}),"runtime guild application rejection did not use retained session");
  auto* prior_guild=live->guild_user().guild;live->guild_user().guild=nullptr;live->receive({9,13});runtime.update_packets(*live);
  check(guild_responses.size()==2,"guild leave without membership emitted a response");live->guild_user().guild=prior_guild;
  live->admission().location=previous_location;live->admission().zone=previous_zone;live->channel().send=std::move(prior_send);
  check(!runtime.social_session(*live).friends_loaded() && runtime.social(*live).friends().size()==1 && runtime.social_session(*live).account()==runtime_social,"runtime lost retained social state on missing SQL catalog");
  check(runtime.social_accounts().acquire(*live)==runtime_social && !runtime.social_accounts().retire(11),"active social account was replaced or retired");
  auto request_lease=live->zone_requests_lease();check(request_lease.get()==&live->zone_requests(),"social request lease did not use session state");request_lease.reset();
  auto& selected_social=runtime.social_session(*live);auto social_catalog=selected_social.take_catalog();social_owner=std::make_shared<SessionSocialFixture>(*live,std::move(social_catalog),social_released);selected_social.bind_catalog(social_owner);
  auto foreign_social=std::make_shared<SessionSocialFixture>(*live,social_owner->catalog(),social_released);WorldLifecyclePorts mismatched_ports;mismatched_ports.step=[](WorldLifecycleStep){};bool mismatch_rejected=false;try{(void)runtime.bind_social_world_release(*live,foreign_social,std::move(mismatched_ports));}catch(const std::invalid_argument&){mismatch_rejected=true;}check(mismatch_rejected,"social release accepted a separate catalog");
  live->receive({1,2});runtime.update_packets(*live);check(live->admission().location==2 && sessions.size()==1,"runtime processed world-wait traffic early");
  world_users.enter(*live->world_user());bool still_indexed=false;try{runtime.assert_world_detached(*live);}catch(const std::logic_error&){still_indexed=true;}check(still_indexed,"world index guard accepted retained user");
  auto resolve_zone=vertical_world_actor_zone(live);check(!resolve_zone(),"detached session exposed a zone");live->admission().zone=&live_zone;
  live_zone.users->attach(live->world_user()->actor,0,0,{0,0,0});check(resolve_zone()==live_zone.users.get(),"scene did not resolve retained session zone");bool zone_indexed=false;try{runtime.assert_zone_detached(*live);}catch(const std::logic_error&){zone_indexed=true;}
  check(zone_indexed && live_zone.users->detach(43) && !resolve_zone(),"zone guard accepted an attached actor");live->admission().zone=nullptr;live->zone_entry().bound=true;bool entry_bound=false;try{runtime.assert_zone_detached(*live);}catch(const std::logic_error&){entry_bound=true;}live->zone_entry().bound=false;
  check(entry_bound,"zone guard accepted unfinished zone entry state");runtime.assert_zone_detached(*live);
  PartyNormalMembership party_fixture;live->party_user().party=&party_fixture;bool party_attached=false;try{runtime.assert_party_detached(*live);}catch(const std::logic_error&){party_attached=true;}live->party_user().party=nullptr;check(party_attached,"party guard accepted retained membership");runtime.assert_party_detached(*live);
  live->close();check(sessions.size()==1,"runtime released before save binding teardown");runtime_checked.set_value();
 }catch(...){runtime_checked.set_exception(std::current_exception());}});runtime_checked_future.get();
 // The real runtime's worker-side teardown completes asynchronously; a posted
 // poll cannot substitute for its registry-release completion.
 std::promise<void> runtime_drained;auto runtime_drained_future=runtime_drained.get_future();
 auto poll=std::make_shared<std::function<void()>>();std::weak_ptr<std::function<void()>> weak_poll=poll;
 *poll=[&,weak_poll]{if(sessions.size()==0){runtime_drained.set_value();return;}asio::post(strand,[weak_poll]{if(auto next=weak_poll.lock())(*next)();});};
 asio::post(strand,[poll]{(*poll)();});runtime_drained_future.get();poll.reset();
 runtime_dispatcher.wait_idle();std::promise<void> runtime_cleanup;auto runtime_cleanup_future=runtime_cleanup.get_future();asio::post(strand,[&]{try{std::weak_ptr<VerticalWorldSession> weak=live;social_owner.reset();live.reset();runtime_ack.reset();check(weak.expired() && world.waiting_count()==0 && runtime_disconnects==2 && !world_users.find(43),"concrete runtime retained closed session or world index");check(runtime_delegate.scene.expired() && !runtime_delegate.scene_actor && runtime_delegate.scene_unbinds==1,"actor scene was not unbound before session destruction");check(social_released && social_reentrant_rejected && social_duplicate_rejected && social_rebind_rejected && guild_release_order && duplicate_release_rejected && reentrant_release_rejected && runtime_guilds.find(77)->membership.offline.contains(43) && !runtime_guilds.find(77)->membership.online.contains(43),"guild world release order or duplicate guard differed");auto replacement=world_claim_fixture();auto next=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,runtime,replacement,channel,roster,[](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::logic_error("unexpected replacement selection");});check(runtime.social_accounts().acquire(*next)==runtime_social && runtime_social->replica().friends()[0].id==71,"social account did not survive session replacement");next->guild_user().guild=&runtime_guilds.find(77)->membership;bool omitted_rejected=false,omitted_completed=false;try{runtime.disconnect(*next,{},[&]{omitted_completed=true;});}catch(const std::logic_error&){omitted_rejected=true;}check(omitted_rejected && !omitted_completed && sessions.find_account(11)==next,"disconnect accepted omitted guild release");next->guild_user().guild=nullptr;next->open();next->close();next.reset();auto lease=runtime_social->shared_mutex();runtime_social.reset();check(!runtime.social_accounts().retire(11),"social account retired while mutex lease remained");lease.reset();check(runtime.social_accounts().retire(11) && runtime.social_accounts().size()==0 && !runtime.social_accounts().retire(11),"social account retirement differed");runtime_cleanup.set_value();}catch(...){runtime_cleanup.set_exception(std::current_exception());}});runtime_cleanup_future.get();runtime_dispatcher.shutdown();check(!runtime_dispatcher.failure(),"runtime DB dispatcher failed");
 std::promise<void> sql_factory_closed,sql_factory_checked;auto sql_factory_closed_future=sql_factory_closed.get_future();auto sql_factory_checked_future=sql_factory_checked.get_future();WorldSessionTestServices sql_factory_services(sql_factory_closed);
 auto sql_pool=std::make_shared<InventoryReplicaPool>();std::weak_ptr<InventoryReplicaPool> weak_sql_pool=sql_pool;
 auto sql_select=vertical_world_session_sql_select(strand,db.get_executor(),sessions,world,sql_factory_services,{},1,"PS_GameData",sql_pool);
 bool invalid_sql_factory=false;try{(void)vertical_world_session_sql_select(strand,db.get_executor(),sessions,world,sql_factory_services,{},1,"",sql_pool);}catch(const std::invalid_argument&){invalid_sql_factory=true;}check(invalid_sql_factory,"SQL selection accepted missing catalog");
 sql_pool.reset();asio::post(strand,[&]{try{auto account=world_claim_fixture();account.username="fixture";
  auto app=sql_select(account,channel,roster);app->open();check(sessions.size()==1 && world.selection_count()==1 && !weak_sql_pool.expired(),"SQL selection did not retain its owners");
  app->close();app.reset();check(sessions.size()==0 && world.selection_count()==0,"SQL selection did not release preselection session");sql_factory_checked.set_value();
 }catch(...){sql_factory_checked.set_exception(std::current_exception());}});sql_factory_checked_future.get();sql_factory_closed_future.get();sql_select={};check(weak_sql_pool.expired(),"SQL selection factory leaked its pool");
 db.join();selection.reset();work.reset();thread.join();
 std::cout<<"world session duplicate and stale completion guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif
