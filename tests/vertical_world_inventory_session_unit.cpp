#include <limits>
#include "eternity/game/vertical_world_inventory_gameplay.hpp"
#include "eternity/game/item_pool.hpp"
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
#include "eternity/game/vertical_world_inventory_party.hpp"
#include "eternity/game/vertical_world_inventory_actions.hpp"
#include "eternity/game/vertical_world_inventory_cancel.hpp"
#include "eternity/game/vertical_world_inventory_settlement.hpp"
#include "eternity/game/zone_leave_bound.hpp"
#include <bit>
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

struct InventoryTestScene final:ActorScene {
 std::function<void(const net::Packet&)> on_self;
 void send_self(const net::Packet& packet) noexcept override {if(on_self)on_self(packet);} void send_nearby(const net::Packet&) override {} void send_cell_view(const net::Packet&) override {}
 std::optional<bool> party_kind() const noexcept override {return {};} void send_party(const net::Packet&) noexcept override {} void reset_damage() override {}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};} void release_effect_id(std::uint32_t) noexcept override {}
 std::uint32_t tick_ms() const noexcept override {return 1;} void stop_target_mob(std::uint32_t) override {} void taunt(std::uint16_t,float) override {}
 ZoneUsers* user_zone() override {return nullptr;} UserShapeSocial user_shape_social() override {return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {}
};
struct WorldSessionSuccessServices final:VerticalWorldSessionServices {
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;std::weak_ptr<InventoryTestScene> scene;Actor* scene_actor=nullptr;unsigned scene_binds=0,scene_unbinds=0;StatusRules statuses;SkillDefinitions skills;
 QuestDefinitions quests=QuestDefinitions::decode(eternity::net::Packet{0,0,0,0});ActorLoadRules rules{statuses,items,enchant,skills,quests,mobs};
 SocialCatalog social_catalog{11,43};std::vector<eternity::net::Packet> sent;std::promise<void>& entered;std::promise<void>& closed;
 std::function<void(VerticalWorldSession&,std::function<void()>)> disconnect_override;
 std::function<void(VerticalWorldSession&,const eternity::net::Packet&)> receive_override;
 std::function<void(VerticalWorldSession&,std::function<void(std::exception_ptr)>)> prepare_override;
 std::function<void(VerticalWorldSession&,std::function<void()>)> cancel_override;
 WorldSessionSuccessServices(std::promise<void>& a,std::promise<void>& b):entered(a),closed(b){}
 const ActorLoadRules& actor_rules() override{return rules;} ActorLoadOptions actor_options(const PlayerSnapshot&) override {ActorLoadOptions o;o.country=0;return o;}
 VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot& snapshot) override{auto value=std::make_shared<InventoryTestScene>();scene=value;const auto id=snapshot.core.character.id;return {value,[this,id](Actor& actor){check(!scene_actor && actor.id()==id,"incorrect scene binding");scene_actor=&actor;++scene_binds;},[this]{check(scene_actor && !scene_actor->status().in_zone,"scene unbound while attached");scene_actor=nullptr;++scene_unbinds;}};}
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
 void waiting(VerticalWorldSession&) override {entered.set_value();} void receive(VerticalWorldSession& owner,const eternity::net::Packet& packet) override {if(receive_override)receive_override(owner,packet);else throw std::logic_error("unexpected packet");}
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

struct SessionSaveStore {
 std::atomic<unsigned> exits{0};
 UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>&){throw std::logic_error("unexpected periodic save");}
 UserCharacterSaveResult save_exit(CharacterReplica&,bool,std::string_view,const CharacterSaveCallbacks& callbacks,const std::function<void(std::string_view)>&) {
  if(++exits==1)callbacks.queue_exit();else callbacks.exit();return {};
 }
};

struct PickupServices final:ItemPickupWorld,ZoneItemServices {
 std::shared_ptr<GroundItemPool> pool;ZoneItems* zone=nullptr;VerticalWorldSession& owner;unsigned service_db=0,unlocks=0,clock_reads=0;bool capture_money_log=false;std::vector<net::Packet> money_logs;std::uint32_t now=100;std::function<void()> on_pick_party,on_lock,on_tick;std::function<void(const net::Packet&)> on_cell_view;std::mutex party_mutex;bool capture_lock=false,lock_held=false;
 PickupServices(std::shared_ptr<GroundItemPool> value,VerticalWorldSession& session):pool(std::move(value)),owner(session) {}
 std::uint32_t tick_ms() override{++clock_reads;if(on_tick)on_tick();return now;}void send_cell_view(CellCoordinates,const net::Packet& packet) override {if(on_cell_view)on_cell_view(packet);}void release(GroundItem& item) override {pool->release(item);}
 std::uint32_t pick_tick_ms() override{throw std::logic_error("pickup bypassed zone clock");}std::uint32_t pick_party_id(Actor& a) override {check(&a==&owner.world_user()->actor && !owner.party_user().party,"unexpected party owner");if(on_pick_party)on_pick_party();return 0;}
 UserPosition pick_position(const Actor&) override {throw std::logic_error("pickup bypassed actual zone position");}
 bool pick_quest_active(Actor&,std::uint16_t) override {throw std::logic_error("unexpected quest lookup");}
 std::uint32_t lock_inventory_party(Actor& a) override {check(&a==&owner.world_user()->actor && !owner.party_user().party,"unexpected party lock");if(!capture_lock)return 0;party_mutex.lock();lock_held=true;if(on_lock)on_lock();return 77;}
 void unlock_inventory_party(std::uint32_t token) noexcept override {if(token!=77 || !lock_held)std::terminate();lock_held=false;++unlocks;party_mutex.unlock();}
 std::optional<bool> inventory_party_kind(const Actor& a) override {check(&a==&owner.world_user()->actor && !owner.party_user().party,"unexpected party notice");return {};}
 void send_inventory_party(Actor&,const net::Packet&) override {throw std::logic_error("unexpected party send");}
 void send_inventory_database(const net::Packet&) override {++service_db;throw std::logic_error("pickup bypassed session save owner");}
 void send_money_database(const net::Packet&) override {++service_db;throw std::logic_error("money bypassed session save owner");}
 std::string inventory_item_name(const ItemDefinition&) override {throw std::logic_error("unexpected loot log name");}
 std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {throw std::logic_error("unexpected drop name");}
 bool return_inventory_item(Actor&,GroundItem&) override {throw std::logic_error("pickup bypassed actual zone item owner");}
 GameLogSubject money_log_subject(const Actor& actor) override {if(!capture_money_log || &actor!=&owner.world_user()->actor)throw std::logic_error("unexpected money subject");GameLogSubject subject;subject.account_id=owner.account_id();subject.character_id=actor.id();return subject;}
 std::chrono::sys_seconds money_local_calendar() override {if(!capture_money_log)throw std::logic_error("unexpected money clock");return std::chrono::sys_seconds{};}
 void send_money_log(const net::Packet& packet) override {if(!capture_money_log)throw std::logic_error("unexpected money log");money_logs.push_back(packet);}
};
struct PickupActions final:VerticalInventoryActions {
 unsigned cancelled=0,shops=0;bool reject=false,allow_exchange=false;std::function<void()> on_checks;void cancel_action_exchange(Actor& a) override {check(allow_exchange || a.exchange().mode==0,"unexpected active exchange");++cancelled;if(reject)throw std::runtime_error("action cancellation failed");}
 void end_shop(Actor&) override {++shops;}bool pickup_checks() override{if(on_checks)on_checks();return true;}
};
struct RejectExchangeSettlement final:VerticalExchangeSettlement {
 unsigned validations=0,settlements=0;
 void validate_before_ready(Actor&,Actor&) override {++validations;throw std::runtime_error("settlement unavailable");}
 void settle_exchange(Actor&,Actor&) override {++settlements;throw std::runtime_error("unexpected settlement");}
};

struct OpenedSessionResource {std::atomic<unsigned>& destroyed;std::function<void()> on_destroy;explicit OpenedSessionResource(std::atomic<unsigned>& value,std::function<void()> hook={}):destroyed(value),on_destroy(std::move(hook)){}~OpenedSessionResource(){if(on_destroy)on_destroy();++destroyed;}};

struct SessionMoveView final:UserViewActorTransport {
 UserViewLiveAppearance appearance(const Actor&) override {throw std::logic_error("unexpected cell appearance");}
 std::unique_ptr<UserViewBuffer> allocate(eternity::net::Packet) override {throw std::logic_error("unexpected cell allocation");}
 void broadcast(CellCoordinates,UserViewBuffer&) override {throw std::logic_error("unexpected cell broadcast");}
 void leave_snapshot(CellCoordinates) override {throw std::logic_error("unexpected cell leave");}
 void enter_snapshot(CellCoordinates) override {throw std::logic_error("unexpected cell enter");}
};
struct SessionShopOwners final:ZoneLeaveBoundOwners {
 VerticalWorldSession::Registry& sessions;Actor& actor;ZoneLeaveShopState& state;std::function<void()> on_shop;
 SessionShopOwners(VerticalWorldSession::Registry& r,Actor& a,ZoneLeaveShopState& s):sessions(r),actor(a),state(s){}
 std::shared_ptr<void> retain(Actor& a) override {return sessions.retain_actor(a);}
 ZoneLeaveShopState& shop(Actor& a) override {check(&a==&actor,"unexpected shop actor");if(on_shop)on_shop();return state;}
 std::uint32_t& exchange_timer(Actor&) override {throw std::logic_error("unexpected shop timer");}
};
struct SessionShopView final:UserViewActorTransport {
 struct Buffer final:UserViewBuffer {net::Packet packet;explicit Buffer(net::Packet p):packet(std::move(p)) {}};
 std::vector<net::Packet> sent;unsigned cells=0;
 UserViewLiveAppearance appearance(const Actor&) override {throw std::logic_error("unexpected shop appearance");}
 std::unique_ptr<UserViewBuffer> allocate(net::Packet p) override {sent.push_back(p);return std::make_unique<Buffer>(std::move(p));}
 void broadcast(CellCoordinates,UserViewBuffer& b) override {check(dynamic_cast<Buffer*>(&b)!=nullptr,"wrong shop buffer");++cells;}
 void leave_snapshot(CellCoordinates) override {throw std::logic_error("unexpected shop leave");}
 void enter_snapshot(CellCoordinates) override {throw std::logic_error("unexpected shop enter");}
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
 PartyMembershipSendBuffer buffer;bool buffer_active=false;unsigned buffers_freed=0;std::function<void(PartyMembershipUser&,PartyMembershipSendBuffer&)> on_enqueue;
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
 void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override {if(!on_enqueue)throw std::logic_error("unexpected pool enqueue");on_enqueue(user,buffer);}
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
 VerticalSessionRegistry<Owner> other_world;other_world.bind_network_registry(logins,1);
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
 std::promise<void> preparation_failure;auto preparation_failure_future=preparation_failure.get_future();std::shared_ptr<VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>> failed_preparation;
 asio::post(strand,[&]{try {failed_preparation=VerticalWorldSessionPreparation<OpenedSessionResource,OpenedSessionResource>::create(strand,db.get_executor(),sessions,live,
  []()->std::shared_ptr<OpenedSessionResource>{throw std::runtime_error("fixture open failure");},
  [](VerticalWorldSession&,std::shared_ptr<OpenedSessionResource>)->std::shared_ptr<OpenedSessionResource>{throw std::logic_error("failed open bound resource");});
  failed_preparation->start([&](std::exception_ptr error){try {check(bool(error) && sessions.pending_database_work(11)==0 && !failed_preparation->binding(),"preparation failure escaped fence or bound resource");preparation_failure.set_value();}catch(...){preparation_failure.set_exception(std::current_exception());}});
 }catch(...){preparation_failure.set_exception(std::current_exception());}});preparation_failure_future.get();
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
 std::promise<void> entered_zone,closed_zone,zone_entered,zone_finished;auto entered_zone_future=entered_zone.get_future();auto closed_zone_future=closed_zone.get_future();auto zone_entered_future=zone_entered.get_future();auto zone_finished_future=zone_finished.get_future();
 WorldSessionSuccessServices live_services(entered_zone,closed_zone);std::function<void()> disconnect_done;unsigned other_packets=0,owned_packets=0;
 SessionMoveView move_view;SessionMoveServices move_services;std::shared_ptr<VerticalWorldSessionGameplay> gameplay;std::weak_ptr<SessionRetainedMove> retained_move;unsigned retained_calls=0;
 packet_router=std::make_unique<VerticalWorldSessionPacketRouter>(strand,sessions,2,64,[&](VerticalWorldSession& owner,RuntimeZone& zone){
  check(&owner==live.get() && &zone==&live_zone,"gameplay factory lost owner");
  gameplay=VerticalWorldSessionGameplay::create(strand,sessions,live,zone,live->zone_entry(),live->motion(),move_view,move_services,
   [&](VerticalWorldSession& current,const eternity::net::Packet& other){check(&current==live.get() && other==eternity::net::Packet({7,7}),"wrong other gameplay packet");++other_packets;});return gameplay;},
  [&](VerticalWorldSession& owner,const eternity::net::Packet& packet){if(packet!=eternity::net::Packet({5,0x22}))return false;check(&owner==live.get(),"owned packet lost session");++owned_packets;return true;});
 move_services.on_finish=[&]{if(move_services.finishes==1)check(packet_router->try_enqueue(*live,{7,7}),"reentrant packet enqueue failed");};
 live_services.disconnect_override=[&](VerticalWorldSession& session,std::function<void()> done){check(session.disconnect_in_zone(world_users,false,*disconnect_services)==VerticalDisconnectResult::deferred,"world logout did not defer");disconnect_done=std::move(done);};
 asio::post(strand,[&]{try {auto account=world_claim_fixture();account.permission=7;
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,live_world,live_services,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;snapshot.core.x=40;snapshot.core.z=40;snapshot.core.direction=123;
    auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  live->open();live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){entered_zone.set_exception(std::current_exception());}});entered_zone_future.get();
 bool off_strand_timeout=false;try{(void)live->exchange_timeout();}catch(const std::logic_error&){off_strand_timeout=true;}check(off_strand_timeout,"exchange timeout escaped world strand");
 asio::post(strand,[&]{try {check(live->zone_entry().character_id==43 && live->zone_entry().position.x==40 && live->zone_entry().position.z==40 && live->zone_entry().direction==123 && live->zone_entry().permission==7,"selected character zone state not retained");disconnect_services=std::make_unique<LiveZoneDisconnect>(live->zone_entry());WorldAdmissionUser foreign;foreign.user_id=11;bool rejected_foreign=false;try{(void)live_admission.select_zone(foreign);}catch(const std::logic_error&){rejected_foreign=true;}check(rejected_foreign,"foreign admission user accepted");
  packet_router->attach(live);
  live_services.receive_override=[&](VerticalWorldSession& owner,const eternity::net::Packet& packet){check(&owner==live.get(),"wrong socket owner");packet_router->receive(owner,packet);};
  eternity::net::Packet move{1,5,0x81,0x34,0x12};for(float value:{45.F,0.F,40.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)move.push_back(static_cast<std::uint8_t>(bits>>(i*8)));}
  check(!packet_router->try_enqueue(*live,eternity::net::Packet(65,1)),"packet byte bound was ignored");live->receive({1,2});live->receive(move);check(!packet_router->try_enqueue(*live,{8,8}),"packet count bound was ignored");
  bool overflow_closed=false;auto disconnect=live->channel().disconnect;live->channel().disconnect=[&]{overflow_closed=true;};live->receive({8,8});live->channel().disconnect=std::move(disconnect);check(overflow_closed,"packet overflow did not request socket close");
  live_admission.update_world(live_world);check(packet_router->pending(*live)==2 && live->admission().location==3,"world wait did not preserve queued input");live->zone_ready();
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
  {

   using PickupBinding=VerticalWorldSessionSaveBinding<SessionSaveStore>;using Pickup=VerticalWorldInventoryPickup<SessionSaveStore>;
   DbJobDispatcher pickup_dispatcher(false,1);auto pickup_store=std::make_shared<SessionSaveStore>();auto pickup_mutex=std::make_shared<std::recursive_mutex>();
   auto pickup_ack=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[](std::shared_ptr<VerticalWorldSession>){throw std::logic_error("unexpected pickup acknowledgment");});
   CharacterSaveLifecyclePorts pickup_ports;pickup_ports.send=[](net::Packet){throw std::logic_error("unexpected pickup DB reply");};pickup_ports.leave=[]{throw std::logic_error("unexpected pickup leave");};pickup_ports.reset=[]{throw std::logic_error("unexpected pickup reset");};pickup_ports.roster=[](bool){throw std::logic_error("unexpected pickup roster");};pickup_ports.save_friends=[]{throw std::logic_error("unexpected pickup save");};pickup_ports.save_blocks=[]{throw std::logic_error("unexpected pickup save");};
   auto binding=PickupBinding::create(strand,sessions,live,pickup_dispatcher,pickup_store,pickup_mutex,UserLifecycleState{11,2,0,true},"fixture",std::move(pickup_ports),pickup_ack,[](VerticalWorldSession&,net::Packet){throw std::logic_error("unexpected pickup reply");},[](std::exception_ptr){throw std::logic_error("unexpected pickup DB failure");});
   auto pool=std::make_shared<GroundItemPool>();auto services=std::make_shared<PickupServices>(pool,*live);auto actions=std::make_shared<PickupActions>();auto links=std::make_shared<ItemQuestLinks>();auto geometry=live_map.definition.geometry();live_zone.items=std::make_unique<ZoneItems>(geometry,*services);services->zone=live_zone.items.get();auto& actor=live->world_user()->actor;actor.status().skills.max_bags=1;
   ItemDefinition definition;definition.id=5001;definition.stack_count=20;auto* item=pool->allocate();check(item,"pickup pool allocation failed");item->definition=&definition;item->id=777;item->item.uid=99;item->item.type=5;item->item.type_id=1;item->item.count=1;item->position=live->zone_entry().position;check(live_zone.items->enter(*item,1),"ground item enter failed");
   unsigned fallback=0;auto pickup=Pickup::create(strand,sessions,live,live_zone,binding,services,actions,links,pool,[&](VerticalWorldSession& session,const net::Packet& packet){check(&session==live.get() && packet==net::Packet({7,7}),"pickup fallback lost session");++fallback;});
   bool missing_insertion=false;try{pickup->send_inventory_database(inventory_add_packet(11,1,1,*item));}catch(const VerticalInventorySourceMismatch&){missing_insertion=true;}
   check(missing_insertion && live->character()->replica().inventory().inventory().empty(),"pickup inserted a non-owned item");
   GroundItemPool foreign_pool;auto* foreign_item=foreign_pool.allocate();check(foreign_item,"foreign pickup item allocation failed");foreign_item->definition=&definition;foreign_item->item.uid=999;foreign_item->item.type=5;foreign_item->item.type_id=1;foreign_item->item.count=1;
   check(actor.claim_inventory_item(1,1,*foreign_item,foreign_pool),"foreign pickup item claim failed");bool foreign_pool_rejected=false;
   try{pickup->send_inventory_database(inventory_add_packet(11,1,1,*foreign_item));}catch(const VerticalInventorySourceMismatch&){foreign_pool_rejected=true;}
   check(foreign_pool_rejected && live->character()->replica().inventory().inventory().empty() && actor.release_inventory_item(1,1,foreign_item),"pickup accepted a different item pool");
   const auto clock_before=services->clock_reads;check(pickup->pick_tick_ms()==100 && services->clock_reads==clock_before+1,"pickup did not use actual zone clock");
   services->on_tick=[&]{live->admission().zone=nullptr;};bool clock_zone_changed=false;try{(void)pickup->pick_tick_ms();}catch(const std::logic_error&){clock_zone_changed=true;}live->admission().zone=&live_zone;services->on_tick={};
   check(clock_zone_changed,"pickup accepted reentrant zone clock change");
   const auto actual_position=pickup->pick_position(actor);check(actual_position.x==46 && actual_position.z==live->zone_entry().position.z,"pickup did not use actual zone position");
   GroundItem returned;returned.id=776;returned.position=actual_position;returned.item.type=5;returned.item.type_id=1;returned.item.count=1;
   check(pickup->return_inventory_item(actor,returned) && live_zone.items->find(776)==&returned && returned.zone==live_zone.items.get() && returned.expires_at==60100,"pickup did not return item to actual zone");
   check(live_zone.items->leave(returned),"returned pickup item failed cleanup");
   bool foreign_position=false;try{(void)pickup->pick_position(second->actor);}catch(const std::logic_error&){foreign_position=true;}check(foreign_position,"pickup accepted foreign position actor");
   live->admission().zone=nullptr;bool wrong_zone=false;try{(void)pickup->return_inventory_item(actor,returned);}catch(const std::logic_error&){wrong_zone=true;}live->admission().zone=&live_zone;
   check(wrong_zone && !returned.zone && !live_zone.items->find(776),"pickup returned item after zone ownership changed");
   bool missing_quests=false;try{(void)pickup->pick_quest_active(actor,1);}catch(const std::logic_error&){missing_quests=true;}check(missing_quests && !live->quests_lease(),"pickup accepted a missing retained quest session");
   bool foreign_quest_actor=false;try{(void)pickup->pick_quest_active(second->actor,1);}catch(const std::logic_error&){foreign_quest_actor=true;}check(foreign_quest_actor,"pickup accepted a foreign quest actor");
   auto pickup_gameplay=VerticalWorldSessionGameplay::create_retained(strand,sessions,live,live_zone,live->zone_entry(),[](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&){return std::make_shared<SessionIgnoredMove>();},Pickup::bind(pickup));
   VerticalWorldSessionPacketRouter pickup_router(strand,sessions,8,128,[&](VerticalWorldSession& session,RuntimeZone& zone){check(&session==live.get() && &zone==&live_zone,"pickup router lost owner");return pickup_gameplay;});pickup_router.attach(live);
   check(pool->is_allocated(*item) && !foreign_pool.is_allocated(*item),"item pool ownership check differs");
   auto* wrong_pool_item=foreign_pool.allocate();check(wrong_pool_item==foreign_item,"foreign pool did not reuse its actual item");wrong_pool_item->definition=&definition;wrong_pool_item->id=781;wrong_pool_item->item.type=5;wrong_pool_item->item.type_id=1;wrong_pool_item->item.count=1;wrong_pool_item->position=actual_position;
   check(live_zone.items->enter(*wrong_pool_item,1),"foreign pool ground item enter failed");bool foreign_ground_rejected=false;try{pickup->dispatch(*live,{5,2,13,3,0,0});}catch(const std::logic_error&){foreign_ground_rejected=true;}
   check(foreign_ground_rejected && live_zone.items->find(781)==wrong_pool_item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"pickup unlinked a foreign-pool ground item");
   check(live_zone.items->leave(*wrong_pool_item),"foreign ground cleanup failed");foreign_pool.release(*wrong_pool_item);
   auto* free_item=pool->allocate();check(free_item,"free pickup item allocation failed");free_item->definition=&definition;free_item->id=782;free_item->item.type=5;free_item->item.type_id=1;free_item->item.count=1;free_item->position=actual_position;pool->release(*free_item);
   check(!pool->is_allocated(*free_item) && live_zone.items->enter(*free_item,1),"free-item fixture setup failed");bool free_ground_rejected=false;try{pickup->dispatch(*live,{5,2,14,3,0,0});}catch(const std::logic_error&){free_ground_rejected=true;}
   check(free_ground_rejected && live_zone.items->find(782)==free_item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"pickup unlinked an available-pool item");
   check(live_zone.items->leave(*free_item),"free ground cleanup failed");check(pool->allocate()==free_item,"free item was not reusable");pool->release(*free_item);actions->cancelled=0;actions->shops=0;
   auto* full=pool->allocate();check(full,"full-inventory item allocation failed");full->definition=&definition;full->id=775;full->item.uid=98;full->item.type=5;full->item.type_id=1;full->item.count=1;full->position=actual_position;
   check(live_zone.items->enter(*full,1),"full-inventory item enter failed");actor.status().skills.max_bags=0;
   pickup_router.receive(*live,{5,2,7,3,0,0});pickup_router.update(*live);
   check(live_zone.items->find(775)==full && full->zone==live_zone.items.get() && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty() && services->service_db==0,"full-inventory pickup did not return its real item");
   check(live_zone.items->leave(*full),"full-inventory item cleanup failed");pool->release(*full);actor.status().skills.max_bags=1;actions->cancelled=0;actions->shops=0;
   item->user_id=999;item->expires_at=30100;const auto before_protected_clock=services->clock_reads;
   pickup_router.receive(*live,{5,2,9,3,0,0});pickup_router.update(*live);
   check(services->clock_reads==before_protected_clock+1 && live_zone.items->find(777)==item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"protected pickup did not use strict real-clock boundary");
   item->user_id=0;actions->cancelled=0;actions->shops=0;
   services->on_pick_party=[&]{item->item.count=9;};bool changed_count=false;try{pickup->dispatch(*live,{5,2,9,3,0,0});}catch(const std::logic_error&){changed_count=true;}services->on_pick_party={};item->item.count=1;
   check(changed_count && live_zone.items->find(777)==item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"pickup accepted item mutation during authorization");actions->cancelled=0;actions->shops=0;
   services->on_pick_party=[&]{check(live_zone.items->leave(*item),"reentrant selected-item leave failed");};bool item_changed=false;try{pickup->dispatch(*live,{5,2,9,3,0,0});}catch(const std::logic_error&){item_changed=true;}services->on_pick_party={};
   check(item_changed && !item->zone && pool->is_allocated(*item) && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"pickup accepted reentrant selected-item removal");
   check(live_zone.items->enter(*item,1),"reentrant selected item restoration failed");actions->cancelled=0;actions->shops=0;
   services->capture_lock=true;services->on_lock=[&]{item->item.count=8;};bool changed_under_lock=false;try{pickup->dispatch(*live,{5,2,9,3,0,0});}catch(const std::logic_error&){changed_under_lock=true;}services->on_lock={};services->capture_lock=false;item->item.count=1;
   check(changed_under_lock && !services->lock_held && services->unlocks==1 && !item->zone && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"pickup accepted mutation during party lock or leaked its lock");
   check(live_zone.items->enter(*item,1),"lock mutation item restoration failed");services->unlocks=0;actions->cancelled=0;actions->shops=0;
   const net::Packet request{5,2,9,3,0,0};actions->reject=true;bool rejected=false;try{pickup->dispatch(*live,request);}catch(const std::runtime_error&){rejected=true;}check(rejected && live_zone.items->find(777)==item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"failed pickup cancellation mutated item");
   actions->reject=false;services->on_pick_party=[&]{live->admission().zone=nullptr;};bool zone_changed=false;try{pickup->dispatch(*live,request);}catch(const std::logic_error&){zone_changed=true;}live->admission().zone=&live_zone;services->on_pick_party={};check(zone_changed && live_zone.items->find(777)==item && actor.inventory().empty() && live->character()->replica().inventory().inventory().empty(),"reentrant pickup zone change mutated owners");
   services->capture_lock=true;services->on_lock=[&]{live->admission().zone=nullptr;};bool lock_changed=false;try{(void)pickup->lock_inventory_party(actor);}catch(const std::logic_error&){lock_changed=true;}live->admission().zone=&live_zone;services->on_lock={};services->capture_lock=false;check(lock_changed && !services->lock_held && services->unlocks==1,"reentrant party lock was not released");
   pickup_router.receive(*live,request);pickup_router.update(*live);const auto retained=actor.retained_inventory_item(1,0);const auto rows=live->character()->replica().inventory().inventory();check(retained && retained->item==item && rows.size()==1 && rows[0].item.uid==99 && live_zone.items->size()==0 && services->service_db==0 && actions->cancelled==3 && actions->shops==2,"session pickup missed actor, zone or save replica");
   auto* stack=pool->allocate();check(stack,"stack pickup allocation failed");stack->definition=&definition;stack->id=778;stack->item.uid=101;stack->item.type=5;stack->item.type_id=1;stack->item.count=2;stack->position=live->zone_entry().position;check(live_zone.items->enter(*stack,1),"stack item enter failed");pickup_router.receive(*live,{5,2,10,3,0,0});pickup_router.update(*live);const auto merged=live->character()->replica().inventory().inventory();check(actor.retained_inventory_item(1,0)->item==item && item->item.count==3 && merged.size()==1 && merged[0].item.uid==99 && merged[0].item.count==3 && services->service_db==0,"session stack pickup missed actor or save replica");
   pickup_router.receive(*live,{7,7});pickup_router.update(*live);check(fallback==1,"pickup chain swallowed another opcode");bool foreign=false;try{pickup->send_inventory_database(inventory_count_packet(99,1,0,1));}catch(const VerticalInventoryDeliveryError&){foreign=true;}check(foreign && live->character()->replica().inventory().inventory()[0].item.uid==99,"foreign pickup DB packet was accepted");
   bool occupied=false;try{pickup->send_inventory_database(inventory_add_packet(11,1,0,*item));}catch(const VerticalInventoryDestinationOccupied&){occupied=true;}check(occupied && live->character()->replica().inventory().inventory()[0].item.count==3,"occupied pickup replica slot was merged");
   check(binding->apply(vertical_inventory_exchange_db_remove_packet(11,{1,0,3},3))==InventoryReplicaResult::applied,"cannot remove first pickup replica");
   auto mismatched_add=inventory_add_packet(11,1,0,*item);mismatched_add[6]^=1;bool bad_add_uid=false;try{pickup->send_inventory_database(mismatched_add);}catch(const VerticalInventorySourceMismatch&){bad_add_uid=true;}
   mismatched_add=inventory_add_packet(11,1,0,*item);mismatched_add[20]^=1;bool bad_add_type=false;try{pickup->send_inventory_database(mismatched_add);}catch(const VerticalInventorySourceMismatch&){bad_add_type=true;}
   check(bad_add_uid && bad_add_type && live->character()->replica().inventory().inventory().empty(),"pickup accepted a mismatched item row");
   pickup->send_inventory_database(inventory_add_packet(11,1,0,*item));check(live->character()->replica().inventory().inventory().size()==1 && live->character()->replica().inventory().inventory()[0].item.uid==item->item.uid,"valid retained item row was rejected");
   check(binding->apply(vertical_inventory_exchange_db_remove_packet(11,{1,0,3},3))==InventoryReplicaResult::applied,"cannot remove validated pickup replica");GroundItem wrong;wrong.definition=&definition;wrong.item=item->item;wrong.item.uid=123;check(binding->apply(inventory_add_packet(11,1,0,wrong))==InventoryReplicaResult::applied,"cannot seed wrong stack source");bool wrong_uid=false;try{pickup->send_inventory_database(inventory_count_packet(11,1,0,3));}catch(const VerticalInventorySourceMismatch&){wrong_uid=true;}check(wrong_uid && live->character()->replica().inventory().inventory()[0].item.uid==123 && item->item.count==3,"wrong stack UID was updated");
   check(binding->apply(vertical_inventory_exchange_db_remove_packet(11,{1,0,3},3))==InventoryReplicaResult::applied && actor.release_inventory_item(1,0,item),"first pickup cleanup failed");
   auto* conflicted=pool->allocate();check(conflicted,"conflict item allocation failed");conflicted->definition=&definition;conflicted->id=779;conflicted->item.uid=100;conflicted->item.type=5;conflicted->item.type_id=1;conflicted->item.count=1;conflicted->position=live->zone_entry().position;check(live_zone.items->enter(*conflicted,1),"conflict item enter failed");GroundItem stale;stale.definition=&definition;stale.item=conflicted->item;stale.item.uid=123;check(binding->apply(inventory_add_packet(11,1,0,stale))==InventoryReplicaResult::applied,"cannot seed conflicting replica slot");
   bool conflict=false;try{pickup->dispatch(*live,{5,2,11,3,0,0});}catch(const VerticalInventoryDestinationOccupied&){conflict=true;}const auto conflict_actor=actor.retained_inventory_item(1,0);const auto conflict_rows=live->character()->replica().inventory().inventory();check(conflict && conflict_actor && conflict_actor->item==conflicted && conflict_rows.size()==1 && conflict_rows[0].item.uid==123 && live_zone.items->size()==0 && services->service_db==0,"replica conflict was hidden or merged");
   check(binding->apply(vertical_inventory_exchange_db_remove_packet(11,{1,0,1},1))==InventoryReplicaResult::applied && actor.release_inventory_item(1,0,conflicted),"conflicting pickup cleanup failed");
   const auto before_fenced_money=actor.money();auto* fenced_money=pool->allocate();check(fenced_money,"fenced money allocation failed");fenced_money->id=783;fenced_money->item.type=26;fenced_money->item.count=1;fenced_money->money=17;fenced_money->drop_type=0;fenced_money->user_id=0;fenced_money->party_id=0;fenced_money->position=actual_position;
   check(live_zone.items->enter(*fenced_money,1),"fenced money enter failed");bool skipped_pinned=false;
   services->on_cell_view=[&](const net::Packet& packet){if(packet.size()<2 || packet[0]!=2 || packet[1]!=4)return;fenced_money->cell.reset();pool->release(*fenced_money);auto* replacement=pool->allocate();check(replacement && replacement!=fenced_money,"pool reused pinned pickup item");skipped_pinned=true;pool->release(*replacement);};
   bool detached_rejected=false;try{pickup->dispatch(*live,{5,2,15,3,0,0});}catch(const std::logic_error&){detached_rejected=true;}services->on_cell_view={};
   check(detached_rejected && skipped_pinned && !live_zone.items->find(783) && !pool->is_allocated(*fenced_money) && actor.money()==before_fenced_money && live->character()->replica().core().money==before_fenced_money,"released detached money reached actor or replica");
   auto* changed_money=pool->allocate();check(changed_money,"changed money allocation failed");changed_money->id=784;changed_money->item.type=26;changed_money->item.count=1;changed_money->money=17;changed_money->drop_type=0;changed_money->user_id=0;changed_money->party_id=0;changed_money->position=actual_position;
   check(live_zone.items->enter(*changed_money,1),"changed money enter failed");services->on_cell_view=[&](const net::Packet& packet){if(packet.size()>=2 && packet[0]==2 && packet[1]==4)changed_money->money=999;};
   bool changed_amount=false;try{pickup->dispatch(*live,{5,2,16,3,0,0});}catch(const std::logic_error&){changed_amount=true;}services->on_cell_view={};
   check(changed_amount && !changed_money->zone && pool->is_allocated(*changed_money) && actor.money()==before_fenced_money && live->character()->replica().core().money==before_fenced_money,"pickup accepted money mutation during unlink");pool->release(*changed_money);
   const auto initial_money=actor.money();check(initial_money==live->character()->replica().core().money && initial_money<=UINT32_MAX-250,"money fixture initial balance differs");
   auto* gold=pool->allocate();check(gold,"money pickup allocation failed");gold->id=780;gold->item.type=26;gold->item.type_id=1;gold->item.count=1;gold->money=250;gold->drop_type=0;gold->user_id=0;gold->party_id=0;gold->position=actual_position;
   check(live_zone.items->enter(*gold,1),"money pickup ground enter failed");const auto available_before=pool->statistics().available;services->capture_money_log=true;
   pickup_router.receive(*live,{5,2,12,3,0,0});pickup_router.update(*live);
   check(actor.money()==initial_money+250 && live->character()->replica().core().money==actor.money() && !live_zone.items->find(780) && pool->statistics().available==available_before+1 && services->service_db==0 && services->money_logs.size()==1 && services->money_logs[0].size()==82 && services->money_logs[0][0]==1 && services->money_logs[0][1]==8,"money pickup lost actor, replica, pool or log delivery");
   bool bad_money=false;try{pickup->send_money_database(money_database_packet(11,actor.money()+1));}catch(const VerticalInventoryMoneyMismatch&){bad_money=true;}
   bool wrong_money_opcode=false;try{pickup->send_money_database(inventory_count_packet(11,1,0,1));}catch(const VerticalInventoryDeliveryError&){wrong_money_opcode=true;}
   bool cross_channel=false;try{pickup->send_inventory_database(money_database_packet(11,actor.money()+1));}catch(const VerticalInventoryDeliveryError&){cross_channel=true;}
   check(bad_money && wrong_money_opcode && cross_channel && live->character()->replica().core().money==actor.money(),"money replica accepted a mismatched balance or opcode");services->capture_money_log=false;
   pickup->stop();bool stopped=false;try{pickup->dispatch(*live,request);}catch(const std::logic_error&){stopped=true;}check(stopped,"stopped pickup accepted traffic");pickup_router.stop(*live);pickup_gameplay.reset();pickup.reset();bool drained=false;binding->stop_and_drain([&]{drained=true;});check(drained,"pickup save owner did not drain");
   binding.reset();live_zone.items.reset();pickup_dispatcher.shutdown();check(!pickup_dispatcher.failure(),"pickup dispatcher failed");
  }
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
  {
   auto& actor=live->world_user()->actor;ZoneLeaveShopState shop_state;SessionShopOwners shop_owners(sessions,actor,shop_state);SessionShopView shop_view;
   auto resources=std::make_shared<VerticalMoveSessionResources>(live_membership,live->party_user(),shop_state);
   auto bound=std::make_shared<ZoneLeaveBound>(live_zone,*live->world_user(),live->zone_entry(),shop_owners,shop_view);
   auto checks=std::make_shared<std::uint8_t>(1);auto& exchange_timer=live->exchange_timeout();check(exchange_timer==0,"session exchange timeout did not start clear");exchange_timer=77;
   auto cancellation=std::make_shared<VerticalWorldInventoryCancellation>(strand,sessions,live,live_zone);
   VerticalWorldInventoryActions actions(strand,sessions,live,live_zone,resources,bound,checks,VerticalWorldInventoryCancellation::bind(cancellation));
   bool missing_cancel=false;try{VerticalWorldInventoryActions missing(strand,sessions,live,live_zone,resources,bound,checks,{});}catch(const std::invalid_argument&){missing_cancel=true;}
   check(missing_cancel,"inventory actions accepted a missing cancellation owner");
   actor.exchange().mode=2;shop_state.mode=1;shop_state.offer_bytes.fill(7);actions.cancel_action_exchange(actor);actions.end_shop(actor);
   check(actor.exchange().mode==0 && exchange_timer==0 && shop_state.mode==0 && shop_state.customer_count==0 && std::all_of(shop_state.offer_bytes.begin(),shop_state.offer_bytes.end(),[](auto value){return value==0;}) && shop_view.sent==std::vector<net::Packet>({{0x23,5,43,0,0,0,0}}) && shop_view.cells>0,"inventory actions did not use retained shop shutdown");
   check(vertical_inventory_exchange_invite(actor,net::Packet{1,0x0a,43,0,0,0},*live_zone.users,live->exchange_timeout(),actions)==VerticalExchangeRequest::updated && actor.exchange().mode==2 && exchange_timer==actor.tick_ms()+30000U,"exchange invite did not use the retained session timer");
   actions.cancel_action_exchange(actor);check(actor.exchange().mode==0 && exchange_timer==0,"pickup cancellation did not clear the invite timer");
   actor.exchange().mode=1;actor.exchange().peer=&actor;exchange_timer=77;cancellation->cancel(*live,actor);
   check(actor.exchange().mode==0 && exchange_timer==0,"self exchange invitation cancellation differed");
   actor.exchange().mode=3;actor.exchange().peer=&actor;cancellation->cancel(*live,actor);check(actor.exchange().mode==0,"self exchange cancellation differed");
   actor.exchange().mode=3;actor.exchange().peer=&second->actor;bool foreign_peer=false;try{cancellation->cancel(*live,actor);}catch(const std::logic_error&){foreign_peer=true;}
   check(foreign_peer && actor.exchange().mode==3 && actor.exchange().peer==&second->actor,"inventory cancellation accepted a foreign exchange peer");actor.exchange()={};
   {auto departed=std::make_unique<Owner>(79,1001);auto* departed_actor=&departed->actor;departed.reset();
    actor.exchange().mode=3;actor.exchange().peer=departed_actor;live->exchange_timeout()=42;bool departed_rejected=false;
    try{cancellation->cancel(*live,actor);}catch(const std::logic_error&){departed_rejected=true;}
    check(departed_rejected && actor.exchange().mode==3 && actor.exchange().peer==departed_actor && live->exchange_timeout()==42,"cancellation accepted a departed peer or changed state");
    actor.exchange()={};live->exchange_timeout()=0;}
   actor.relations().duel_state=5;actor.relations().duel_type=3;actor.relations().duel_opponent=999;cancellation->cancel(*live,actor);
   check(actor.relations().duel_state==5 && actor.relations().duel_type==3,"missing duel peer changed retained duel state");
   {Owner foreign_duel(80,999);live_zone.users->attach(foreign_duel.actor,0,0,{0,0,0});bool rejected_duel=false;
    try{cancellation->cancel(*live,actor);}catch(const std::logic_error&){rejected_duel=true;}
    check(rejected_duel && actor.relations().duel_state==5 && actor.relations().duel_type==3,"unregistered duel peer changed retained duel state");
    check(live_zone.users->detach(999),"foreign duel peer cleanup failed");}
   actor.relations().duel_opponent=actor.id();actor.status().current_target_type=2;cancellation->cancel(*live,actor);
   check(actor.relations().duel_state==0 && actor.relations().duel_type==-1 && actor.status().current_target_type==0,"self duel cancellation lost actor state");
   bool missing_session=false;try{VerticalWorldInventoryCancellation missing(strand,sessions,{},live_zone);}catch(const std::invalid_argument&){missing_session=true;}
   check(missing_session,"inventory cancellation accepted a missing session owner");
   check(actions.pickup_checks(),"inventory actions lost live pickup flag");*checks=0;check(!actions.pickup_checks(),"inventory actions cached pickup flag");
   ZoneLeaveShopState other_state;SessionShopOwners wrong_owners(sessions,actor,other_state);auto wrong_bound=std::make_shared<ZoneLeaveBound>(live_zone,*live->world_user(),live->zone_entry(),wrong_owners,shop_view);
   VerticalWorldInventoryActions wrong(strand,sessions,live,live_zone,resources,wrong_bound,checks,[](VerticalWorldSession&,Actor&){throw std::logic_error("unexpected cancellation");});
   shop_state.mode=1;bool wrong_shop=false;try{wrong.end_shop(actor);}catch(const std::logic_error&){wrong_shop=true;}check(wrong_shop && shop_state.mode==1,"inventory actions accepted a different shop owner");
   bool foreign_actor=false;try{actions.end_shop(second->actor);}catch(const std::logic_error&){foreign_actor=true;}check(foreign_actor && shop_state.mode==1,"inventory actions accepted a foreign actor");
   PartyMembershipUser foreign_connection;auto foreign_resources=std::make_shared<VerticalMoveSessionResources>(live_membership,foreign_connection,shop_state);
   VerticalWorldInventoryActions foreign_actions(strand,sessions,live,live_zone,foreign_resources,bound,checks,[](VerticalWorldSession&,Actor&){throw std::logic_error("unexpected cancellation");});
   bool foreign_movement=false;try{foreign_actions.end_shop(actor);}catch(const std::logic_error&){foreign_movement=true;}check(foreign_movement && shop_state.mode==1,"inventory actions accepted a foreign movement connection");
   check(foreign_resources->close_queue()==0,"foreign movement fixture did not drain");
   check(resources->close_queue()==0,"shop fixture queue did not drain");bool closed=false;try{actions.end_shop(actor);}catch(const std::logic_error&){closed=true;}check(closed && shop_state.mode==1,"inventory actions accepted closed movement resources");
  }
  auto* departing=allocate_registry_party(live_party_registry.registry(),live_party_registry);check(departing!=nullptr,"party allocation failed");
  departing->id=7;departing->count=1;departing->slots[0]=&live->party_user();live->party_user().party=departing;live_party_registry.registry().entries.emplace(7,departing);party_world.leaving=true;party_world.live_registry=&live_party_registry;
  live_party_registry.lock_party(*departing);live_party_registry.lock_party(*departing);live_party_registry.unlock_party(*departing);live_party_registry.unlock_party(*departing);
  {
   auto service=std::make_shared<PickupServices>(std::make_shared<GroundItemPool>(),*live);
   VerticalWorldInventoryParty inventory_party(strand,sessions,live,party_runtime,service);
   auto& actor=live->world_user()->actor;
   check(inventory_party.pick_party_id(actor)==7 && inventory_party.inventory_party_kind(actor)==std::optional<bool>(false),"pickup did not use retained party");
   auto token=inventory_party.lock_inventory_party(actor);check(token!=0,"pickup failed to capture real party lock");
   live_party_registry.lock_party(*departing);live_party_registry.unlock_party(*departing);
   live->party_user().party=nullptr;inventory_party.unlock_inventory_party(token);
   check(inventory_party.pick_party_id(actor)==0 && !inventory_party.inventory_party_kind(actor) && inventory_party.lock_inventory_party(actor)==0,"pickup retained a departed party");
   live->party_user().party=departing;
   PartyMembershipUser foreign;foreign.id=999;foreign.send_socket=7;departing->slots[1]=&foreign;departing->count=2;
   bool send_failed=false;try{inventory_party.send_inventory_party(actor,{1,2});}catch(const std::logic_error&){send_failed=true;}
   check(send_failed && party_pool.outstanding()==0,"pickup lost foreign member failure or shared buffer");
   departing->slots[1]=nullptr;departing->count=1;
   inventory_party.send_inventory_party(actor,{1,2});check(party_pool.outstanding()==0,"self-only party pickup leaked buffer");
  }
  {
   std::array<PartyMembershipUser,7> users{};std::vector<PartyMembershipUser*> delivered;
   auto& source=live->party_user();source.position={0,0,0};departing->count=8;
   for(std::size_t i=0;i<users.size();++i){users[i].send_socket=7;users[i].position={0,0,0};if(i<6)departing->slots[i+1]=&users[i];else departing->extra_slots[0]=&users[i];}
   users[0].position={99.5F,1000,0};users[1].position={100,0,0};users[2].position={0,0,-100};
   users[3].position={60,0,80};users[4].position={0,0,0};users[4].send_socket=-1;
   users[5].position={std::numeric_limits<float>::quiet_NaN(),0,0};users[6].position={0,500,3};
   party_world.on_enqueue=[&](auto& user,auto& buffer){delivered.push_back(&user);check(buffer.frame==eternity::net::Packet({4,0,1,2}),"pickup party frame changed");release_party_membership_buffer(buffer,party_world);};
   vertical_inventory_party_send_range(*departing,source,{1,2},party_world,[](PartyMembershipUser& user){return user.position;});
   check(delivered==std::vector<PartyMembershipUser*>({&users[0],&users[6]}) && !party_world.buffer_active,"pickup party radius or raid slots differed");
   party_world.on_enqueue={};departing->count=1;for(std::size_t i=1;i<7;++i)departing->slots[i]=nullptr;departing->extra_slots[0]=nullptr;
  }
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
  party_world.live_registry=nullptr;
  {auto service=std::make_shared<PickupServices>(std::make_shared<GroundItemPool>(),*live);auto inventory=std::make_shared<VerticalWorldInventoryParty>(strand,sessions,live,party_runtime,service);
   bool inventory_shutdown_rejected=false;try{party_runtime.shutdown();}catch(const std::logic_error&){inventory_shutdown_rejected=true;}
   check(inventory_shutdown_rejected,"empty party runtime shut down while inventory gameplay was retained");}
  party_runtime.shutdown();bool stopped_party_rejected=false;try{(void)party_runtime.requests();}catch(const std::logic_error&){stopped_party_rejected=true;}check(stopped_party_rejected,"stopped party runtime accepted requests");party_world.leaving=false;
  PartyMembershipUser foreign_party;foreign_party.id=43;bool bad_party=false;try{(void)party_directory.retain(foreign_party);}catch(const std::logic_error&){bad_party=true;}check(bad_party,"party directory accepted foreign member");
  std::vector<eternity::net::Packet> routed_guild;unsigned allocated=0,released=0;
  auto previous_send=live->channel().send;live->channel().send=[&](eternity::net::Packet p){routed_guild.push_back(std::move(p));};
  VerticalWorldGuildTransport guild_transport(strand,sessions,world_users,[&](const eternity::net::Packet&){++allocated;return true;},[&]{++released;});
  auto guild_ports=guild_transport.ports();check(guild_ports.allocate({1,2}),"guild allocator was not used");guild_ports.send(live->guild_user(),{3,4});guild_ports.release();
  check(allocated==1 && released==1 && routed_guild==std::vector<eternity::net::Packet>{{3,4}},"guild transport did not route to retained channel");
  GuildUser foreign_guild;foreign_guild.character_id=43;bool foreign_rejected=false;try{guild_ports.send(foreign_guild,{3,4});}catch(const std::logic_error&){foreign_rejected=true;}
  check(foreign_rejected && routed_guild.size()==1,"guild transport accepted a foreign user");live->channel().send=std::move(previous_send);
  live->party_user().send_socket=7;live->close();bool gameplay_stopped=false;try{(void)gameplay->dispatch(*live,eternity::net::Packet{7,7});}catch(const std::logic_error&){gameplay_stopped=true;}
  check(live->party_user().send_socket==-1 && gameplay_stopped && disconnect_done && sessions.size()==1 && live->admission().logout_tick==10100 && live_zone.users->find_actor(43),"deferred logout allowed party sends, gameplay or released owner");zone_entered.set_value();
 }catch(...){zone_entered.set_exception(std::current_exception());}});zone_entered_future.get();
 DbJobDispatcher save_dispatcher(false,1);using SessionBinding=VerticalWorldSessionSaveBinding<SessionSaveStore>;using SavePreparation=VerticalWorldSessionPreparation<SessionSaveStore,SessionBinding>;using SaveTeardown=VerticalWorldSessionSaveTeardown<SessionSaveStore,SessionSaveStore>;
 std::shared_ptr<SavePreparation> save_preparation;std::shared_ptr<SaveTeardown> save_teardown;
 std::shared_ptr<VerticalWorldSessionAckRouter> ack_router;UserLifecycleState db_state{11,2,0,true};auto save_store=std::make_shared<SessionSaveStore>();auto save_mutex=std::make_shared<std::recursive_mutex>();std::atomic<unsigned> database_leave{0};unsigned other_replies=0;
 asio::post(strand,[&]{try {
  check(live->database_ack_user() && !live->database_ack_user()->socket_open,"socket closure not reflected in ack owner");
  ack_router=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[&](std::shared_ptr<VerticalWorldSession> retained){
   check(retained==live && sessions.pending_database_work(11)==1,"ack lost live session or active fence");
   check(retained->disconnect_in_zone(world_users,true,*disconnect_services)==VerticalDisconnectResult::finalized && disconnect_services->leaves==1 && disconnect_services->finalized==1,"final disconnect did not leave zone");
   check(!world_users.find(43) && !live_zone.users->find_actor(43) && sessions.size()==1,"finalized membership or fence differs");
   save_teardown->start([&]{try {check(sessions.pending_database_work(11)==0 && other_replies==1 && save_store->exits==2 && database_leave==1 && !save_preparation->binding(),"save delivery or teardown drained early");disconnect_done();disconnect_done={};check(sessions.size()==0,"disconnect completion retained session");zone_finished.set_value();}catch(...){zone_finished.set_exception(std::current_exception());}});
  });
  CharacterSaveLifecyclePorts ports;
  ports.send=[](eternity::net::Packet){throw std::logic_error("uncollected DB reply");};ports.leave=[&]{++database_leave;};ports.reset=[]{throw std::logic_error("unexpected DB reset");};ports.roster=[](bool){throw std::logic_error("unexpected DB roster");};ports.save_friends=[]{};ports.save_blocks=[]{};
  save_preparation=SavePreparation::create(strand,db.get_executor(),sessions,live,[save_store]{return save_store;},
   [&,ports=std::move(ports)](VerticalWorldSession& owner,std::shared_ptr<SessionSaveStore> store) mutable {
    return SessionBinding::create(strand,sessions,owner.shared_from_this(),save_dispatcher,std::move(store),save_mutex,db_state,"fixture",std::move(ports),ack_router,
     [&](VerticalWorldSession& session,eternity::net::Packet packet){check(&session==live.get() && packet==eternity::net::Packet({3,3,11,0,0,0}),"unexpected non-ack DB reply");++other_replies;},
     [&](std::exception_ptr error){zone_finished.set_exception(error);});});
  save_preparation->start([&](std::exception_ptr error){try {if(error)std::rethrow_exception(error);
   save_teardown=SaveTeardown::create(strand,save_preparation,[&](std::exception_ptr failure){zone_finished.set_exception(failure);});
   disconnect_services->on_save=[&]{auto binding=save_preparation->binding();check(binding && binding->disconnect(vertical_database_disconnect_packet(11)),"save disconnect packet rejected");};
   check(live->disconnect_in_zone(world_users,false,*disconnect_services)==VerticalDisconnectResult::save_requested && disconnect_services->saves==1,"logout save not requested");
   check(sessions.size()==1 && sessions.pending_database_work(11)==1 && live_zone.users->find_actor(43),"save request released owner");
  }catch(...){zone_finished.set_exception(std::current_exception());}});
 }catch(...){zone_finished.set_exception(std::current_exception());}});zone_finished_future.get();
 save_dispatcher.wait_idle();std::promise<void> save_cleanup;auto save_cleaned=save_cleanup.get_future();asio::post(strand,[&]{try {check(sessions.size()==0 && save_store->exits==2,"save session remained registered");std::weak_ptr<VerticalWorldSession> retained=live;live.reset();check(!retained.expired(),"save preparation lost its session");save_teardown.reset();save_preparation.reset();ack_router.reset();packet_router.reset();gameplay.reset();check(retained_move.expired(),"retained movement owner leaked");check(retained.expired(),"drained save preparation leaked its session");save_cleanup.set_value();}catch(...){save_cleanup.set_exception(std::current_exception());}});save_cleaned.get();save_dispatcher.shutdown();check(!save_dispatcher.failure(),"save dispatcher failed");
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
 TestRuntime* runtime_ptr=nullptr;std::shared_ptr<VerticalWorldPartyRuntime> runtime_party_ptr;auto runtime_items=std::make_shared<GroundItemPool>();std::shared_ptr<PickupServices> runtime_pickup_services;auto runtime_pickup_actions=std::make_shared<PickupActions>();std::shared_ptr<VerticalWorldInventoryActions> runtime_actions;auto runtime_item_links=std::make_shared<ItemQuestLinks>();unsigned runtime_fallback=0,runtime_exchange_fallback=0;std::weak_ptr<VerticalWorldInventoryExchange> runtime_exchange_owner;
 runtime_ports.gameplay=[&](VerticalWorldSession& owner,RuntimeZone& zone)->std::shared_ptr<VerticalWorldSessionGameplay>{check(runtime_ptr && runtime_party_ptr && runtime_pickup_services,"missing runtime pickup owners");return vertical_world_inventory_party_exchange_gameplay(strand,sessions,*runtime_ptr,owner.shared_from_this(),zone,runtime_party_ptr,[](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&){return std::make_shared<SessionIgnoredMove>();},runtime_pickup_services,runtime_actions,runtime_item_links,runtime_items,[&](VerticalWorldSession& session,const net::Packet& packet){check(&session==live.get(),"runtime pickup fallback lost session");if(packet==net::Packet({1,2}))++runtime_fallback;else if(packet==net::Packet({5,0x0a,0}))++runtime_exchange_fallback;else throw std::logic_error("runtime pickup fallback changed packet");},&runtime_exchange_owner);};runtime_ports.nprotect=[](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected runtime nprotect");};runtime_ports.unavailable=[&](VerticalWorldSession& owner){runtime_delegate.unavailable(owner);};runtime_ports.failure=[&](VerticalWorldSession& owner,std::exception_ptr error){runtime_delegate.failure(owner,error);};
 unsigned runtime_disconnects=0;bool guild_release_order=false,duplicate_release_rejected=false,reentrant_release_rejected=false,social_released=false,social_reentrant_rejected=false,social_duplicate_rejected=false,social_rebind_rejected=false;std::shared_ptr<SessionSocialFixture> social_owner;
 runtime_ports.disconnect=[&](VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done){++runtime_disconnects;
  if(owner.world_user() && world_users.find(owner.world_user()->actor.id())==owner.world_user())world_users.leave(*owner.world_user());
  if(owner.guild_user().character_id){WorldLifecycleState state;state.subject.character_id=owner.guild_user().character_id;WorldLifecyclePorts world_ports;
   world_ports.admin_enter=[] {return true;};world_ports.step=[&](WorldLifecycleStep step){if(step==WorldLifecycleStep::release_friends)guild_release_order=runtime_guilds.find(77)->membership.offline.contains(43);};
   world_ports.close=[](std::uint32_t){};world_ports.local_calendar=[] {return std::chrono::sys_seconds{};};world_ports.game_log=[](eternity::net::Packet){};world_ports.manager=[](eternity::net::Packet){};
   world_ports.take_money=[](std::uint32_t)->std::optional<std::uint32_t>{return {};};world_ports.save_money=[](std::uint32_t,std::uint32_t){};
   auto bound=runtime_ptr->bind_social_world_release(owner,social_owner,std::move(world_ports));WorldLifecyclePorts duplicate_ports;duplicate_ports.step=[](WorldLifecycleStep){};try{(void)runtime_ptr->bind_social_world_release(owner,social_owner,std::move(duplicate_ports));}catch(const std::logic_error&){social_rebind_rejected=true;}social_owner->on_release=[&]{try{bound.step(WorldLifecycleStep::release_friends);}catch(const std::logic_error& error){social_reentrant_rejected=std::string(error.what())=="Social release already started";}};on_guild_allocate=[&]{try{bound.step(WorldLifecycleStep::release_guild);}catch(const std::logic_error& error){reentrant_release_rejected=std::string(error.what())=="Guild release already started";}};leave_world(state,bound);on_guild_allocate={};social_owner->on_release={};try{bound.step(WorldLifecycleStep::release_friends);}catch(const std::logic_error&){social_duplicate_rejected=true;}
   try{bound.step(WorldLifecycleStep::release_guild);}catch(const std::logic_error&){duplicate_release_rejected=true;}}
  done();};
 TestRuntime runtime(strand,sessions,2,64,std::move(runtime_ports));runtime_ptr=&runtime;
 std::promise<void> runtime_empty;auto runtime_empty_future=runtime_empty.get_future();asio::post(strand,[&]{try {auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,runtime,account,channel,roster,[](std::uint32_t,std::uint32_t)->std::optional<VerticalPersistenceSelection>{throw std::logic_error("unexpected runtime selection");});
  live->open();live->close();check(sessions.size()==0 && world.selection_count()==0 && runtime_disconnects==1,"runtime failed preselection disconnect");live.reset();runtime_empty.set_value();
 }catch(...){runtime_empty.set_exception(std::current_exception());}});runtime_empty_future.get();
 asio::post(strand,[&]{try {runtime_ack=std::make_shared<VerticalWorldSessionAckRouter>(strand,sessions,[](std::shared_ptr<VerticalWorldSession>){throw std::logic_error("unexpected runtime acknowledgment");});auto account=world_claim_fixture();
  live=VerticalWorldSession::create(strand,db.get_executor(),sessions,world,runtime,account,channel,roster,
   [pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;auto replica=std::make_unique<CharacterReplica>(snapshot.core,*pool);replica->retain_resources(pool);return VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};});
  runtime_social=runtime.social_accounts().acquire(*live);runtime_social->replica().friends()[0].id=71;live->open();runtime.update_packets(*live);live->receive(eternity::net::Packet{4,1,43,0,0,0});
 }catch(...){runtime_entered.set_exception(std::current_exception());}});runtime_entered_future.get();
 asio::post(strand,[&]{try {check(live->entry_state() && live->entry_state()->ready && world.waiting_count()==1 && runtime.save_binding(*live),"concrete runtime did not prepare entry");check(!runtime_delegate.scene.expired() && runtime_delegate.scene_actor==&live->world_user()->actor && runtime_delegate.scene_binds==1,"session failed to bind its actor scene");
  const auto actual_experience=runtime.experience(*live);check(actual_experience.previous==60 && actual_experience.next==0,"runtime did not use the selected character experience curve");
  auto data=std::find_if(runtime_delegate.sent.begin(),runtime_delegate.sent.end(),[](const auto& packet){return packet.size()>=22 && packet[0]==5 && packet[1]==1;});
  check(data!=runtime_delegate.sent.end() && (*data)[14]==60 && (*data)[15]==0 && (*data)[18]==0,"entry packet did not use runtime experience bounds");
  check(data->size()==64 && std::string(data->begin()+58,data->end()-1)=="Guild","entry data did not use guild initialized during entry");
  check(live->entry_state()->guild_present && live->guild_user().guild==&runtime_guilds.find(77)->membership && runtime_guilds.find(77)->membership.online.contains(43) && !guild_packets.empty(),"runtime did not initialize actual guild membership");
  check(!runtime.social_session(*live).friends_loaded() && runtime.social(*live).friends().size()==1 && runtime.social_session(*live).account()==runtime_social,"runtime lost retained social state on missing SQL catalog");
  check(runtime.social_accounts().acquire(*live)==runtime_social && !runtime.social_accounts().retire(11),"active social account was replaced or retired");
  auto request_lease=live->zone_requests_lease();check(request_lease.get()==&live->zone_requests(),"social request lease did not use session state");request_lease.reset();
  auto& selected_social=runtime.social_session(*live);auto social_catalog=selected_social.take_catalog();social_owner=std::make_shared<SessionSocialFixture>(*live,std::move(social_catalog),social_released);selected_social.bind_catalog(social_owner);
  auto foreign_social=std::make_shared<SessionSocialFixture>(*live,social_owner->catalog(),social_released);WorldLifecyclePorts mismatched_ports;mismatched_ports.step=[](WorldLifecycleStep){};bool mismatch_rejected=false;try{(void)runtime.bind_social_world_release(*live,foreign_social,std::move(mismatched_ports));}catch(const std::invalid_argument&){mismatch_rejected=true;}check(mismatch_rejected,"social release accepted a separate catalog");
  live->receive({1,2});runtime.update_packets(*live);check(live->admission().location==2 && sessions.size()==1,"runtime processed world-wait traffic early");
  world_users.enter(*live->world_user());bool still_indexed=false;try{runtime.assert_world_detached(*live);}catch(const std::logic_error&){still_indexed=true;}check(still_indexed,"world index guard accepted retained user");
  auto resolve_zone=vertical_world_actor_zone(live);check(!resolve_zone(),"detached session exposed a zone");live->admission().zone=&live_zone;
  live_zone.users->attach(live->world_user()->actor,0,0,{0,0,0});check(resolve_zone()==live_zone.users.get(),"scene did not resolve retained session zone");
  {
   auto geometry=live_map.definition.geometry();runtime_pickup_services=std::make_shared<PickupServices>(runtime_items,*live);live_zone.items=std::make_unique<ZoneItems>(geometry,*runtime_pickup_services);runtime_pickup_services->zone=live_zone.items.get();auto& actor=live->world_user()->actor;actor.status().skills.max_bags=1;live->admission().location=4;live->zone_entry().bound=true;
   ItemDefinition definition;definition.id=5001;definition.stack_count=1;auto* item=runtime_items->allocate();check(item,"runtime pickup allocation failed");item->definition=&definition;item->id=888;item->item.uid=202;item->item.type=5;item->item.type_id=1;item->item.count=1;item->position=live->zone_entry().position;check(live_zone.items->enter(*item,1),"runtime pickup ground enter failed");
   bool movement_failure=false;try{(void)vertical_world_inventory_gameplay(strand,sessions,runtime,live,live_zone,[](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&)->std::shared_ptr<SessionIgnoredMove>{throw std::runtime_error("movement unavailable");},runtime_pickup_services,runtime_pickup_actions,runtime_item_links,runtime_items,[](VerticalWorldSession&,const net::Packet&){throw std::logic_error("unexpected failed factory dispatch");});}catch(const std::runtime_error&){movement_failure=true;}check(movement_failure && runtime_pickup_services.use_count()==1 && live_zone.items->find(888)==item && actor.inventory().empty(),"failed gameplay construction retained pickup owners or mutated state");
   VerticalWorldPartyMembership::Services party_services;
   party_services.tick=[]()->std::uint32_t{throw std::logic_error("unexpected runtime party tick");};
   party_services.lock_effects=[](auto&){throw std::logic_error("unexpected runtime effect lock");};party_services.unlock_effects=[](auto&){};
   party_services.cleanup_guild_creation=[](auto&){throw std::logic_error("unexpected runtime guild cleanup");};
   party_services.send_scene=[](auto&,const auto&){throw std::logic_error("unexpected runtime party scene");};
   party_services.fill_log=[](auto&,auto){throw std::logic_error("unexpected runtime party log");};party_services.send_log=[](const auto&){throw std::logic_error("unexpected runtime party log send");};
   party_services.close_socket=[](auto&,std::int32_t){throw std::logic_error("unexpected runtime party close");};unsigned runtime_party_failures=0;
   party_services.send_failure=[&](auto&,std::exception_ptr){++runtime_party_failures;};
   auto retained_parties=std::make_shared<VerticalWorldPartyRuntime>(strand,sessions,world_users,std::move(party_services),[](const PartyRequestUser&,std::int32_t,std::int32_t){throw std::logic_error("unexpected runtime party request close");},[](const PartyRequestUser&,eternity::net::Packet&){throw std::logic_error("unexpected runtime party request fallback");});
   runtime_party_ptr=retained_parties;auto& party_registry=retained_parties->registry();auto* runtime_party=allocate_registry_party(party_registry.registry(),party_registry);check(runtime_party,"runtime party allocation failed");
   runtime_party->id=71;runtime_party->count=1;runtime_party->slots[0]=&live->party_user();live->party_user().party=runtime_party;party_registry.registry().entries.emplace(71,runtime_party);
   ZoneLeaveShopState runtime_shop;SessionShopOwners runtime_shop_owners(sessions,actor,runtime_shop);SessionShopView runtime_shop_view;
   auto runtime_move_resources=std::make_shared<VerticalMoveSessionResources>(retained_parties->membership(),live->party_user(),runtime_shop);
   auto runtime_shop_services=std::make_shared<ZoneLeaveBound>(live_zone,*live->world_user(),live->zone_entry(),runtime_shop_owners,runtime_shop_view);auto runtime_checks=std::make_shared<std::uint8_t>(1);
   auto& runtime_exchange_timer=live->exchange_timeout();runtime_exchange_timer=0;runtime_pickup_actions->allow_exchange=true;auto runtime_cancel=std::make_shared<VerticalWorldInventoryCancellation>(strand,sessions,live,live_zone);
   runtime_actions=std::make_shared<VerticalWorldInventoryActions>(strand,sessions,live,live_zone,runtime_move_resources,runtime_shop_services,runtime_checks,
    [&](VerticalWorldSession& session,Actor& subject){check(&session==live.get(),"runtime cancellation lost session");runtime_pickup_actions->cancel_action_exchange(subject);runtime_cancel->cancel(session,subject);if(runtime_pickup_actions->on_checks)runtime_pickup_actions->on_checks();});
   runtime.update_packets(*live);check(runtime_fallback==1,"runtime gameplay swallowed queued unrelated packet");
   live->receive({5,0x0a,0});runtime.update_packets(*live);check(runtime_exchange_fallback==1 && actor.exchange().mode==0,"exchange settlement packet did not reach next owner");
   auto retained_expiry=runtime_exchange_owner.lock();check(bool(retained_expiry),"gameplay did not expose its retained exchange owner");
   bool short_invite=false;try{retained_expiry->dispatch(*live,{1,0x0a});}catch(const std::invalid_argument&){short_invite=true;}
   check(short_invite && actor.exchange().mode==0 && runtime_exchange_timer==0,"short exchange invitation changed state");
   runtime_shop_owners.on_shop=[&]{live->admission().zone=nullptr;};bool lost_zone=false;
   try{retained_expiry->dispatch(*live,{1,0x0a,43,0,0,0});}catch(const std::logic_error&){lost_zone=true;}live->admission().zone=&live_zone;runtime_shop_owners.on_shop={};
   check(lost_zone && actor.exchange().mode==0 && runtime_exchange_timer==0,"exchange accepted a reentrant zone change");
   auto retained_expiry=runtime_exchange_owner.lock();check(bool(retained_expiry),"gameplay did not expose its retained exchange owner");
   {struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override{return 1;}std::uint32_t packed_local_calendar() override{return 1;}};
    auto clock=std::make_shared<Clock>();auto ids=std::make_shared<ItemIds>(7,*clock);auto scratch=std::make_shared<VerticalWorldInventorySettlementScratch>();
    scratch->db.resize(57);scratch->outgoing_item.resize(181);scratch->receiver.resize(36);scratch->incoming_item.resize(179);scratch->outgoing_money.resize(126);scratch->incoming_money.resize(111);
    using Settlement=VerticalWorldInventorySettlement<SessionSaveStore,SessionSaveStore>;
    bool missing_address=false;try{Settlement missing(strand,sessions,runtime,live,live_zone,runtime_items,ids,clock,runtime_pickup_services,scratch,{});}catch(const std::invalid_argument&){missing_address=true;}
    bool missing_ip=false;try{(void)vertical_world_inventory_connection_ipv4(*live);}catch(const std::logic_error&){missing_ip=true;}
    live->channel().peer_ipv4=std::array<std::uint8_t,4>{1,2,3,4};check(vertical_world_inventory_connection_ipv4(*live)==0x04030201U,"settlement connection IPv4 byte order changed");live->channel().peer_ipv4.reset();
    unsigned address_calls=0;Settlement actual(strand,sessions,runtime,live,live_zone,runtime_items,ids,clock,runtime_pickup_services,scratch,[&](VerticalWorldSession&)->std::uint32_t{++address_calls;throw std::logic_error("address unavailable");});
    bool self_rejected=false;try{actual.validate_before_ready(actor,actor);}catch(const std::logic_error&){self_rejected=true;}
    check(missing_address && missing_ip && self_rejected && address_calls==0 && actor.exchange().mode==0,"retained settlement accepted missing address or identical sessions");
    auto composed=vertical_world_inventory_party_settlement_gameplay(strand,sessions,runtime,live,live_zone,runtime_party_ptr,
        [](VerticalWorldSession&,Actor&,RuntimeZone&,WorldAdmissionUser&,ZoneEntryState&){return std::make_shared<SessionIgnoredMove>();},
        runtime_pickup_services,runtime_actions,runtime_item_links,runtime_items,ids,clock,runtime_pickup_services,scratch,
        [](VerticalWorldSession&,const net::Packet&){throw std::logic_error("unexpected settlement fallback");});
    actor.exchange().mode=3;actor.exchange().peer=&actor;actor.exchange().ready=1;bool composed_rejected=false;
    try{(void)composed->dispatch(*live,{5,0x0a,0});}catch(const std::logic_error&){composed_rejected=true;}
    check(composed_rejected && actor.exchange().mode==3 && actor.exchange().ready==1,"composed gameplay hid invalid settlement participants");
    actor.exchange()={};}
   {auto reject=std::make_shared<RejectExchangeSettlement>();auto ready_owner=VerticalWorldInventoryExchange::create(strand,sessions,live,live_zone,runtime_actions,
      [](VerticalWorldSession&,const net::Packet&){throw std::logic_error("ready fell through");},reject);
    actor.exchange().mode=3;actor.exchange().peer=&actor;actor.exchange().ready=1;bool failed=false;
    try{ready_owner->dispatch(*live,{5,0x0a,0});}catch(const std::runtime_error&){failed=true;}
    check(failed && reject->validations==1 && reject->settlements==0 && actor.exchange().mode==3 && actor.exchange().ready==1,"ready did not propagate settlement validation failure");
    actor.exchange()={};}
   {Owner foreign(77,999);live_zone.users->attach(foreign.actor,0,0,{0,0,0});bool foreign_peer=false;
    try{retained_expiry->dispatch(*live,{1,0x0a,0xe7,3,0,0});}catch(const std::logic_error&){foreign_peer=true;}check(foreign_peer && actor.exchange().mode==0 && runtime_exchange_timer==0,"exchange accepted an unretained peer");
    actor.exchange().mode=1;actor.exchange().peer=&foreign.actor;runtime_exchange_timer=1;bool foreign_expiry=false;
    try{(void)retained_expiry->expire(*live,1);}catch(const std::logic_error&){foreign_expiry=true;}
    check(foreign_expiry && actor.exchange().mode==1 && actor.exchange().peer==&foreign.actor && runtime_exchange_timer==1,"expiry accepted an unretained peer or changed state");
    actor.exchange().mode=2;bool foreign_response=false;
    try{retained_expiry->dispatch(*live,{2,0x0a,0});}catch(const std::logic_error&){foreign_response=true;}
    check(foreign_response && actor.exchange().mode==2 && actor.exchange().peer==&foreign.actor && runtime_exchange_timer==1,"response accepted an unretained peer or changed state");
    actor.exchange().mode=3;bool foreign_control=false;
    try{retained_expiry->dispatch(*live,{5,0x0a,2});}catch(const std::logic_error&){foreign_control=true;}
    check(foreign_control && actor.exchange().mode==3 && actor.exchange().peer==&foreign.actor && runtime_exchange_timer==1,"control accepted an unretained peer or changed state");
    actor.exchange().money=7;bool foreign_money=false;
    try{retained_expiry->dispatch(*live,{8,0x0a,0,0,0,0});}catch(const std::logic_error&){foreign_money=true;}
    check(foreign_money && actor.exchange().money==7 && actor.exchange().peer==&foreign.actor,"money offer accepted an unretained peer or changed state");
    actor.exchange()={};runtime_exchange_timer=0;
    check(live_zone.users->detach(999),"foreign exchange peer cleanup failed");}
   {auto departed=std::make_unique<Owner>(78,1000);auto* departed_actor=&departed->actor;departed.reset();
    actor.exchange().mode=2;actor.exchange().peer=departed_actor;runtime_exchange_timer=1;
    bool departed_response=false;try{retained_expiry->dispatch(*live,{2,0x0a,0});}catch(const std::logic_error&){departed_response=true;}
    bool departed_expiry=false;try{(void)retained_expiry->expire(*live,1);}catch(const std::logic_error&){departed_expiry=true;}
    check(departed_response && departed_expiry && actor.exchange().mode==2 && actor.exchange().peer==departed_actor && runtime_exchange_timer==1,"departed peer was accepted or changed state");
    actor.exchange()={};runtime_exchange_timer=0;}
   live->receive({1,0x0a,43,0,0,0});runtime.update_packets(*live);check(actor.exchange().mode==2 && runtime_exchange_timer==actor.tick_ms()+30000U,"retained exchange invitation did not update session deadline");
   actor.exchange().mode=3;actor.exchange().money=7;actor.exchange().items[0]={1,2,1};
   live->receive({8,0x0a,0,0,0,0});runtime.update_packets(*live);check(actor.exchange().money==0,"retained money offer was not applied");
   live->receive({7,0x0a,0});runtime.update_packets(*live);check(actor.exchange().items[0].bag==0,"retained item offer removal failed");
   live->receive({5,0x0a,2});runtime.update_packets(*live);check(actor.exchange().mode==0,"retained exchange abort did not reset state");
   live->receive({1,0x0a,43,0,0,0});runtime.update_packets(*live);check(actor.exchange().mode==2,"retained exchange reinvitation after abort failed");
   live->receive({2,0x0a,0});runtime.update_packets(*live);check(actor.exchange().mode==0,"retained self response did not follow original peer-mode reset");
   live->receive({1,0x0a,43,0,0,0});runtime.update_packets(*live);check(actor.exchange().mode==2,"retained exchange reinvitation after response failed");
   check(!retained_expiry->expire(*live,runtime_exchange_timer-1) && actor.exchange().mode==2,"exchange expired before its retained deadline");
   check(retained_expiry->expire(*live,runtime_exchange_timer) && actor.exchange().mode==0 && runtime_exchange_timer==0,"exchange expiry did not clear retained self invitation");
   live->receive({1,0x0a,43,0,0,0});runtime.update_packets(*live);check(actor.exchange().mode==2 && runtime_exchange_timer==actor.tick_ms()+30000U,"exchange could not invite after expiry");
   runtime_shop.mode=1;
   std::weak_ptr<VerticalWorldPartyRuntime> retained_party_weak=retained_parties;runtime_party_ptr.reset();retained_parties.reset();
   check(!retained_party_weak.expired(),"retained gameplay did not keep party runtime alive");retained_parties=retained_party_weak.lock();runtime_party_ptr=retained_parties;
   bool gameplay_shutdown_rejected=false;try{retained_parties->shutdown();}catch(const std::logic_error&){gameplay_shutdown_rejected=true;}
   check(gameplay_shutdown_rejected,"party runtime shut down while pickup gameplay was retained");const net::Packet request{5,2,0x78,3,0,0};
   retained_expiry.reset();runtime_pickup_actions->reject=false;bool invalidated_during_pickup=false;runtime_pickup_actions->on_checks=[&]{runtime.invalidate_gameplay(*live);invalidated_during_pickup=runtime_pickup_services.use_count()>1;};live->receive(request);runtime.update_packets(*live);runtime_pickup_actions->on_checks={};const auto held=actor.retained_inventory_item(1,0);const auto rows=live->character()->replica().inventory().inventory();check(invalidated_during_pickup && runtime_exchange_owner.expired() && held && held->item==item && rows.size()==1 && rows[0].item.uid==202 && live_zone.items->size()==0 && runtime_pickup_services->service_db==0 && actor.exchange().mode==0 && runtime_exchange_timer==0 && runtime_shop.mode==0 && runtime_shop_view.cells>0 && runtime_shop_view.sent==std::vector<net::Packet>({{0x23,5,43,0,0,0,0}}),"runtime packet, retained shop or reentrant gameplay invalidation lost pickup owners");
   actor.exchange().mode=3;actor.exchange().peer=&actor;const auto saved_item=item->item;const auto saved_craft=item->craft_storage;
   live->receive({6,0x0a,255,255,1,0});runtime.update_packets(*live);check(actor.exchange().items[0].bag==0,"invalid item offer changed exchange state");
   auto retained_offer=runtime_exchange_owner.lock();check(bool(retained_offer),"item offer lost its exchange owner");
   runtime_items->release(*item);live->receive({6,0x0a,1,0,0,0});runtime.update_packets(*live);
   check(actor.exchange().items[0].bag==0 && !runtime_items->is_allocated(*item),"invalid zero-count offer changed the freed item or exchange state");
   bool free_offer=false;
   try{retained_offer->dispatch(*live,{6,0x0a,1,0,1,0});}catch(const std::logic_error&){free_offer=true;}
   check(free_offer && actor.exchange().items[0].bag==0 && !runtime_items->is_allocated(*item),"item offer accepted a freed pool allocation");
   check(runtime_items->allocate()==item,"item offer fixture could not restore pool allocation");item->item=saved_item;item->craft_storage=saved_craft;
   {auto scene=runtime_delegate.scene.lock();check(bool(scene),"item offer lost scene");bool mutated=false;
    scene->on_self=[&](const net::Packet& packet){if(packet.size()>=2 && packet[0]==6 && packet[1]==0x0a) {item->craft_storage[20]^=1;mutated=true;}};
    bool changed_offer=false;try{retained_offer->dispatch(*live,{6,0x0a,1,0,1,0});}catch(const std::logic_error&){changed_offer=true;}
    scene->on_self={};check(mutated && changed_offer && actor.exchange().items[0].bag==1,"item offer failed to detect callback mutation after partial effects");
    item->craft_storage=saved_craft;actor.exchange().items[0]={};}
   live->receive({6,0x0a,1,0,1,0});runtime.update_packets(*live);
   check(actor.exchange().items[0].bag==1 && actor.exchange().items[0].slot==0 && actor.exchange().items[0].count==1 && runtime_items->is_allocated(*item),"retained item offer did not use the picked-up pool item");actor.exchange()={};retained_offer.reset();
   auto binding=runtime.save_binding(*live);check(binding && binding->apply(vertical_inventory_exchange_db_remove_packet(11,{1,0,1},1))==InventoryReplicaResult::applied && actor.release_inventory_item(1,0,item),"runtime pickup cleanup failed");binding.reset();runtime.invalidate_gameplay(*live);check(runtime_pickup_services.use_count()==1 && runtime_party_failures==0,"runtime invalidation retained pickup services or party failure");live->party_user().party=nullptr;runtime_party->count=0;runtime_party->slots[0]=nullptr;party_registry.registry().entries.erase(71);runtime_actions.reset();check(runtime_move_resources->close_queue()==0,"runtime inventory movement queue did not drain");runtime_move_resources.reset();retained_parties->shutdown();runtime_party_ptr.reset();retained_parties.reset();check(retained_party_weak.expired(),"drained gameplay retained party runtime");live_zone.items.reset();runtime_pickup_services.reset();live->zone_entry().bound=false;live->admission().location=2;
  }
  bool zone_indexed=false;try{runtime.assert_zone_detached(*live);}catch(const std::logic_error&){zone_indexed=true;}
  check(zone_indexed && live_zone.users->detach(43) && !resolve_zone(),"zone guard accepted an attached actor");live->admission().zone=nullptr;live->zone_entry().bound=true;bool entry_bound=false;try{runtime.assert_zone_detached(*live);}catch(const std::logic_error&){entry_bound=true;}live->zone_entry().bound=false;
  check(entry_bound,"zone guard accepted unfinished zone entry state");runtime.assert_zone_detached(*live);
  PartyNormalMembership party_fixture;live->party_user().party=&party_fixture;bool party_attached=false;try{runtime.assert_party_detached(*live);}catch(const std::logic_error&){party_attached=true;}live->party_user().party=nullptr;check(party_attached,"party guard accepted retained membership");runtime.assert_party_detached(*live);
  live->close();check(sessions.size()==1,"runtime released before save binding teardown");runtime_checked.set_value();
 }catch(const std::exception& e){std::cerr<<"runtime fixture: "<<e.what()<<std::endl;runtime_checked.set_exception(std::current_exception());}});runtime_checked_future.get();
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
