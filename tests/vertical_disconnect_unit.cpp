#include "eternity/game/vertical_disconnect.hpp"
#include "eternity/game/zone_logout_update.hpp"
#include "eternity/game/vertical_database_ack.hpp"
#include "eternity/game/zone_actor_leave.hpp"
#include "eternity/game/zone_send_cell_view.hpp"
#include "eternity/game/zone_leave_exchange.hpp"
#include "eternity/game/zone_leave_interactions.hpp"
#include "eternity/game/zone_shop_shutdown.hpp"
#include "eternity/game/zone_leave_bound.hpp"
#include "eternity/game/zone_leave_requests.hpp"
#include "eternity/game/zone_leave_duel.hpp"
#include "eternity/game/zone_leave_guild_duel.hpp"
#include "eternity/game/guild_leave_duel.hpp"
#include "eternity/game/zone_leave_guild_creation.hpp"
#include "eternity/game/zone_leave_lifecycle.hpp"
#include "eternity/game/vertical_save_core.hpp"
#include "eternity/game/quest_runtime_save.hpp"
#include "eternity/game/user_quick_slots.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include "eternity/game/user_view_cell_transport.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace eternity;using namespace eternity::game;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Scene final:ActorScene {
 std::vector<net::Packet> packets;void send_self(const net::Packet& p) noexcept override {packets.push_back(p);} void send_nearby(const net::Packet&) override {} void send_cell_view(const net::Packet&) override {}
 std::optional<bool> party_kind() const noexcept override{return {};} void send_party(const net::Packet&) noexcept override {} void reset_damage() override {}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};} void release_effect_id(std::uint32_t) noexcept override {}
 std::uint32_t tick_ms() const noexcept override{return 1;} void stop_target_mob(std::uint32_t) override {} void taunt(std::uint16_t,float) override {}
 ZoneUsers* user_zone() override{return nullptr;} UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {}
};
struct Services final:VerticalDisconnectServices {
 std::recursive_mutex user_lock,zone_lock;std::vector<std::string>& events;
 explicit Services(std::vector<std::string>& e):events(e){}
 std::recursive_mutex& user_mutex(WorldUser&) override{return user_lock;}
 std::recursive_mutex& zone_mutex(RuntimeZone&) override{return zone_lock;}
 void dead_disconnect(WorldUser&) override{events.push_back("dead");}
 std::uint32_t tick_ms() override{events.push_back("tick");return 1234;}
 void prepare_save(WorldUser&) override{events.push_back("prepare");}
 void request_save(WorldUser&) override{events.push_back("save");}
 void leave_zone(WorldUser&,RuntimeZone&) override{events.push_back("zone");}
 void finish_final(WorldUser&) override{events.push_back("finish");}
};
struct AdmissionServices final:WorldAdmissionServices {
 RuntimeZone& zone;explicit AdmissionServices(RuntimeZone& z):zone(z){}
 void packets(WorldAdmissionUser&) override{} RuntimeZone* select_zone(WorldAdmissionUser&) override{return &zone;}
 void register_world(WorldAdmissionUser&) override{} bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override{return true;}
 void close(WorldAdmissionUser&,std::uint32_t) override{} void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override{}
 std::optional<std::uint32_t> rank_window(RuntimeZone&) override{return {};} bool lock_guild(WorldAdmissionUser&) override{return false;} void unlock_guild(WorldAdmissionUser&) override{}
};
struct AckServices final:VerticalDatabaseAckServices {
 VerticalDatabaseAckUser& user;bool first,second;unsigned lookups=0;std::vector<std::string> events;
 AckServices(VerticalDatabaseAckUser& u,bool f,bool s):user(u),first(f),second(s){}
 VerticalDatabaseAckUser* find(std::uint32_t account) override{check(account==11,"wrong account");events.push_back("lookup");return (++lookups==1?first:second)?&user:nullptr;}
 void close(VerticalDatabaseAckUser&,std::uint32_t reason) override{check(reason==104,"wrong close reason");events.push_back("close");}
 void leave(VerticalDatabaseAckUser&) override{events.push_back("leave");}
};
struct ViewServices final:UserViewCellServices {
 struct Buffer final:UserViewBuffer {net::Packet packet;explicit Buffer(net::Packet p):packet(std::move(p)){}};
 bool fail=false;unsigned allocations=0;std::vector<std::uint32_t> recipients;std::vector<net::Packet> packets;
 UserViewLiveAppearance appearance(const Actor&) override{return {};}
 std::unique_ptr<UserViewBuffer> allocate(net::Packet p) override{++allocations;if(fail)return {};return std::make_unique<Buffer>(std::move(p));}
 std::shared_ptr<void> retain(Actor& actor) override{return std::shared_ptr<void>(&actor,[](void*){});}
 void enqueue(Actor& actor,UserViewBuffer& buffer) override{packets.push_back(dynamic_cast<Buffer&>(buffer).packet);recipients.push_back(actor.id());}
 void leave_snapshot(CellCoordinates) override{} void enter_snapshot(CellCoordinates) override{}
};
struct LeaveInteractions final:ZoneLeaveInteractionServices {
 Actor& peer;VerticalNpcInteraction& npc;Npc& object;bool found;std::vector<std::string> events;std::uint32_t timer=1;
 LeaveInteractions(Actor& p,VerticalNpcInteraction& n,Npc& o,bool f):peer(p),npc(n),object(o),found(f){}
 Actor* find_in_zone(Actor&,std::uint32_t id) override{check(id==99,"wrong shop owner");events.push_back("find");return found?&peer:nullptr;}
 void shutdown_shop(Actor&,ZoneLeaveShopState& shop) override{check(!npc.npc,"NPC not cleared before shop");events.push_back("shop");}
 std::shared_ptr<void> retain(Actor& p) override{return {&p,[](void*){}};}
 std::uint32_t& exchange_timer(Actor&) override{return timer;}
};
struct ShopServices final:ZoneShopShutdownServices {
 Actor& customer;ZoneLeaveShopState& customer_state;bool in_zone;std::vector<std::string> events;
 ShopServices(Actor& c,ZoneLeaveShopState& state,bool present):customer(c),customer_state(state),in_zone(present){}
 Actor* find_customer(Actor&,std::uint32_t id) override{events.push_back("find"+std::to_string(id));return id==customer.id()?&customer:nullptr;}
 std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
 ZoneLeaveShopState& shop_state(Actor&) override{return customer_state;}
 std::optional<CellCoordinates> cell(Actor&) override{return in_zone?std::optional<CellCoordinates>{{1,2}}:std::nullopt;}
 void send_view(CellCoordinates cell,const net::Packet& p) override{check(cell.x==1 && cell.z==2 && p==net::Packet({0x23,5,42,0,0,0,0}),"shop view packet");events.push_back("view");}
};
struct BoundOwners final:ZoneLeaveBoundOwners {
 Actor& subject;ZoneLeaveShopState own,customer;std::uint32_t a=0,b=0;
 explicit BoundOwners(Actor& actor):subject(actor){}
 std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
 ZoneLeaveShopState& shop(Actor& actor) override{return &actor==&subject?own:customer;}
 std::uint32_t& exchange_timer(Actor& actor) override{return &actor==&subject?a:b;}
};
struct RequestServices final:ZoneLeaveRequestServices {
 Actor& peer;bool found;std::vector<std::uint32_t> lookups;
 RequestServices(Actor& p,bool f):peer(p),found(f){}
 Actor* find_in_zone(Actor&,std::uint32_t id) override{lookups.push_back(id);return found?&peer:nullptr;}
 std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
};
struct LeaveDuelWorld final:DuelWorld {
 Actor& peer;bool available;unsigned finds=0;std::vector<net::Packet> logs;
 LeaveDuelWorld(Actor& p,bool a):peer(p),available(a){}
 Actor* find_duel_user(Actor&,std::uint32_t id) override{check(id==99,"duel lookup id");++finds;return available?&peer:nullptr;}
 UserPosition duel_position(const Actor&) override{return {};}
 std::int32_t drop_random() override{throw std::logic_error("unexpected random");}
 DuelDropItem* allocate_drop_item() override{throw std::logic_error("unexpected drop");}
 bool enter_duel_item(Actor&,DuelDropItem&,bool) override{throw std::logic_error("unexpected item");}
 GameLogSubject duel_log_subject(const Actor& a) override{GameLogSubject s;s.character_id=a.id();return s;}
 std::chrono::sys_seconds local_calendar() override{return std::chrono::sys_seconds{};}
 void send_duel_log(const net::Packet& p) override{logs.push_back(p);}
};
struct GuildZoneServices final:ZoneLeaveGuildDuelServices {
 Actor& subject;Actor& peer;GuildDuelState source,other;VerticalGuildDuelMovement a,b;bool guild_present,found;unsigned locks=0,leaves=0,lookups=0;
 GuildZoneServices(Actor& s,Actor& p,bool g,bool f):subject(s),peer(p),guild_present(g),found(f){}
 std::shared_ptr<void> capture_guild_lock(Actor& actor) override{if(&actor==&subject && !guild_present)return {}; ++locks;return {&actor,[](void*){}};}
 void* current_guild(Actor& actor) override{return &actor==&subject?(guild_present?&source:nullptr):&other;}
 std::int32_t guild_duel_status(void* g) override{return static_cast<GuildDuelState*>(g)->status;}
 void leave_guild_duel(void* g,Actor& a,std::uint8_t mode) override{check(g==&source && &a==&subject && mode==1,"wrong guild leave");++leaves;}
 GuildDuelState& duel_state(void* g) override{return *static_cast<GuildDuelState*>(g);}
 VerticalGuildDuelMovement& movement_state(Actor& a) override{return &a==&subject?this->a:b;}
 Actor* find_in_zone(Actor&,std::uint32_t id) override{check(id==99,"guild peer lookup");++lookups;return found?&peer:nullptr;}
 std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
};
struct CreationServices final:ZoneGuildCreationServices {
 Actor& owner;ZoneGuildCreation* a;ZoneGuildCreation* b;std::vector<net::Packet> packets;std::vector<ZoneGuildCreation*> released;
 CreationServices(Actor& o,ZoneGuildCreation* x,ZoneGuildCreation* y):owner(o),a(x),b(y){}
 ZoneGuildCreation*& current(Actor& actor) override{return &actor==&owner?a:b;}
 void broadcast(ZoneGuildCreation&,const net::Packet& p) override{packets.push_back(p);}
 void release(ZoneGuildCreation* p) override{released.push_back(p);}
};
struct CoreSaveServices final:VerticalSaveCoreServices {
 CharacterReplicaDelivery& delivery;std::vector<net::Packet> packets;
 explicit CoreSaveServices(CharacterReplicaDelivery& d):delivery(d){}
 UserPosition position(Actor&) override{return {1.25F,-2.5F,3.75F};}
 void send_database(const net::Packet& p) override{packets.push_back(p);check(delivery.apply(p)==InventoryReplicaResult::applied,"save update rejected");}
};
struct QuestPoolFixture final:QuestRuntimePoolServices {std::unique_ptr<RestoredQuest> construct() override{return std::make_unique<RestoredQuest>();}};
QuestDefinitions timed_fixture(bool timed) {
 std::ifstream file(std::filesystem::path(__FILE__).parent_path()/"data/quests-oracle.tsv");std::string line,hex;
 while(std::getline(file,line))if(line.starts_with("S\t")){std::istringstream row(line);char tag;row>>tag>>hex;break;}
 if(hex.empty())throw std::runtime_error("Quest definition fixture missing");net::Packet data;for(std::size_t i=0;i<hex.size();i+=2)data.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));
 for(unsigned i=0;i<4;++i){data[48+i]=static_cast<std::uint8_t>(3U>>(8*i));data[52+i]=static_cast<std::uint8_t>((timed?7U:0U)>>(8*i));}return QuestDefinitions::decode(data);
}
int main(){try{
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;
 for(bool dead:{false,true})for(bool final:{false,true})for(unsigned logout:{0U,1U,2U})for(bool has_zone:{false,true}){
  Actor actor(42,{}, {},{},scene,{items,enchant,mobs});actor.attack_state().mode=dead?1:0;
  RuntimeZone zone;WorldAdmissionUser admission;admission.location=4;admission.logout_type=logout;admission.zone=has_zone?&zone:nullptr;
  WorldUser user{actor,admission};std::vector<std::string> events;
  WorldUsers users({[](WorldUser&){},[&](WorldUser&){events.push_back("world");}});users.enter(user);Services services(events);
  auto result=vertical_disconnect_in_zone(users,user,final,services);std::vector<std::string> expected;
  if(dead)expected.push_back("dead");
  if(final){expected.push_back("world");if(has_zone)expected.push_back("zone");expected.push_back("finish");check(result==VerticalDisconnectResult::finalized && users.find(42)==nullptr,"final branch");}
  else if(logout==0){expected.push_back("tick");check(result==VerticalDisconnectResult::deferred && admission.logout_type==2 && admission.logout_tick==11234 && users.find(42)==&user && users.find_zone_move(42)==nullptr,"deferred branch");}
  else {expected.push_back("prepare");expected.push_back("save");check(result==VerticalDisconnectResult::save_requested && users.find(42)==&user,"save branch");}
  check(events==expected,"disconnect callback order");
 }
 for(unsigned type:{0U,1U,2U,3U})for(std::uint32_t deadline:{0U,1U,100U,101U,0xffffffffU})for(std::uint32_t now:{0U,100U,101U,0xffffffffU}){
  Actor actor(42,{}, {},{},scene,{items,enchant,mobs});ZoneUsers zone(2,2,0);zone.attach(actor,0,0,{});
  WorldAdmissionUser admission;admission.logout_type=type;admission.logout_tick=deadline;unsigned calls=0;
  zone_update_logout_deadlines(zone,now,[](Actor& a){return std::shared_ptr<void>(&a,[](void*){});},
   [&](Actor& a)->WorldAdmissionUser&{check(&a==&actor,"wrong logout actor");return admission;},
   [&](Actor& a,WorldAdmissionUser& state){check(&a==&actor && state.logout_tick==0,"deadline not cleared before callback");++calls;check(zone.detach(a.id()),"logout callback detach");});
  const bool due=deadline!=0 && deadline<=now;check(calls==unsigned(due) && admission.logout_tick==(due?0:deadline) && zone.size()==unsigned(!due),"logout deadline mismatch");
  if(!due)check(zone.detach(actor.id()),"logout cleanup");
 }
 for(unsigned type:{0U,1U,2U,3U})for(std::uint32_t deadline:{0U,1U,100U,101U,0xffffffffU})for(std::uint32_t now:{0U,100U,101U,0xffffffffU}){
  RuntimeZone zone;AdmissionServices services(zone);WorldAdmissionUser user;WorldAdmission admission(services);
  admission.track_selection(user);check(admission.wait(user),"wait registration");admission.update_packets();
  user.logout_type=type;user.logout_tick=deadline;unsigned calls=0;
  admission.update_zone_wait_logout_deadlines(zone,now,[&](WorldAdmissionUser& subject){check(&subject==&user && subject.logout_tick==0,"wait deadline not cleared");++calls;admission.untrack(subject);});
  const bool due=type==2 && deadline!=0 && deadline<=now;
  check(calls==unsigned(due) && user.logout_tick==(due?0:deadline) && admission.zone_waiting_count(zone)==unsigned(!due),"zone-wait deadline mismatch");
  if(!due)admission.untrack(user);
 }
 for(unsigned presence=0;presence<3;++presence)for(std::uint32_t counter:{0U,1U,0xffffffffU})for(bool open:{false,true})for(unsigned transfer:{0U,1U,2U}){
  Actor actor(42,{}, {},{},scene,{items,enchant,mobs});WorldAdmissionUser admission;WorldUser user{actor,admission};std::recursive_mutex mutex;
  VerticalDatabaseAckUser ack{user,mutex};ack.acknowledgments=counter;ack.socket_open=open;ack.transfer_state=transfer;AckServices services(ack,presence!=0,presence==2);
  check(vertical_database_disconnect_ack(net::Packet{2,3,11,0,0,0},services),"ack rejected");std::vector<std::string> expected{"lookup"};
  if(presence){expected.push_back("lookup");if(presence==2 && counter==0){if(!open)expected.push_back("leave");else if(transfer!=1)expected.push_back("close");}}
  check(services.events==expected && ack.acknowledgments.load()==(presence==2?counter+1U:counter),"DB acknowledgement branch mismatch");
 }
 for(bool present:{false,true})for(int x:{0,1})for(int z:{0,1}){
  Actor actor(42,{}, {},{},scene,{items,enchant,mobs});RuntimeZone zone;zone.users=std::make_unique<ZoneUsers>(2,2,0);
  WorldAdmissionUser admission;WorldUser user{actor,admission};ZoneEntryState entry;entry.character_id=42;entry.cell={x,z};entry.bound=present;
  if(present){admission.zone=&zone;zone.users->attach(actor,x,z,{});}std::vector<std::string> events;
  ZoneActorLeaveServices ports;ports.user_leave_lifecycle=[&](Actor& subject){check(&subject==&actor && !zone.users->find_actor(42) && subject.status().in_zone && admission.zone==&zone,"zone lifecycle ordering");events.push_back("lifecycle");};
  ports.send_view=[&](CellCoordinates cell,const net::Packet& packet){check(cell.x==x && cell.z==z && packet==net::Packet({2,2,42,0,0,0}) && actor.status().in_zone && admission.zone==&zone,"zone view ordering");events.push_back("view");};
  check(leave_zone_actor(zone,user,entry,ports)==present && events==(present?std::vector<std::string>{"lifecycle","view"}:std::vector<std::string>{}),"zone leave result");
  if(present)check(!admission.zone && !actor.status().in_zone && !entry.bound && entry.cell.x==0 && entry.cell.z==0,"zone leave cleanup");
 }
 {
  Actor subject(42,{}, {},{},scene,{items,enchant,mobs}),near(43,{}, {},{},scene,{items,enchant,mobs}),far(44,{}, {},{},scene,{items,enchant,mobs});
  RuntimeZone zone;zone.users=std::make_unique<ZoneUsers>(7,7,0);zone.users->attach(subject,1,1,{});zone.users->attach(near,2,2,{});zone.users->attach(far,6,6,{});
  WorldAdmissionUser admission;admission.zone=&zone;WorldUser user{subject,admission};ZoneEntryState entry;entry.character_id=42;entry.cell={1,1};entry.bound=true;
  ViewServices view;UserViewCellTransport transport(*zone.users,view);ZoneActorLeaveServices ports;
  ports.user_leave_lifecycle=[](Actor&){};ports.send_view=[&](CellCoordinates cell,const net::Packet& packet){zone_send_cell_view(7,7,cell,packet,transport);};
  check(leave_zone_actor(zone,user,entry,ports) && view.allocations==1 && view.recipients==std::vector<std::uint32_t>{43},"real cell recipient routing");
  view.fail=true;zone_send_cell_view(7,7,{1,1},{2,2,42,0,0,0},transport);check(view.allocations==2 && view.recipients.size()==1,"allocation failure broadcast");
  check(zone.users->detach(43) && zone.users->detach(44),"view cleanup");
 }
 for(int mode:{0,1,2,3,7})for(bool present:{false,true})for(int other:{0,1,2,3}){
  Scene local_scene,peer_scene;Actor actor(0x12345678,{}, {},{},local_scene,{items,enchant,mobs}),peer(99,{}, {},{},peer_scene,{items,enchant,mobs});
  actor.exchange().mode=mode;actor.exchange().peer=present?&peer:nullptr;actor.exchange().money=999;peer.exchange().mode=other;peer.exchange().peer=&actor;peer.exchange().money=888;
  std::uint32_t a=77,b=88;zone_leave_exchange(actor,[](Actor& p){return std::shared_ptr<void>(&p,[](void*){});},[&](Actor& p)->std::uint32_t&{return &p==&actor?a:b;});
  const bool local_reset=mode==1 || mode==2 || (mode!=0 && present),peer_reset=present && ((mode==1 || mode==2)?other==1:mode!=0);
  check(actor.exchange().mode==(local_reset?0:mode) && peer.exchange().mode==(peer_reset?0:other) && a==(mode==2?0U:77U) && b==(mode==1 && peer_reset?0U:88U),"trade cleanup state");
  std::vector<net::Packet> local_packets,peer_packets;
  if(mode==2 && present && other==1)peer_packets.push_back({3,10,0x78,0x56,0x34,0x12,1});
  if(mode!=0 && mode!=1 && mode!=2 && present){local_packets.push_back({4,10,2});peer_packets.push_back({4,10,3});}
  check(local_scene.packets==local_packets && peer_scene.packets==peer_packets,"trade cleanup packets");
 }
 for(bool has_npc:{false,true})for(unsigned partner:{0U,99U})for(bool found:{false,true})for(int mode:{0,3}){
  Scene a_scene,b_scene;Actor actor(42,{}, {},{},a_scene,{items,enchant,mobs}),peer(99,{}, {},{},b_scene,{items,enchant,mobs});Npc object;object.users_using=0;VerticalNpcInteraction npc{has_npc?&object:nullptr};
  ZoneLeaveShopState shop;shop.visited_owner=partner;shop.customer_count=2;shop.customer_ids[0]=42;shop.customer_ids[1]=99;
  actor.exchange().mode=mode;actor.exchange().peer=mode?&peer:nullptr;peer.exchange().mode=3;peer.exchange().peer=&actor;
  LeaveInteractions services(peer,npc,object,found);zone_leave_interactions(actor,npc,shop,services);const bool removed=partner && found;
  check(object.users_using==(has_npc?-1:0) && !npc.npc && shop.visited_owner==(removed?0U:partner) && shop.customer_count==(removed?1:2) && shop.customer_ids[0]==(removed?99U:42U),"leave interaction state");
  check(services.events==(partner?std::vector<std::string>{"find","shop"}:std::vector<std::string>{"shop"}),"leave interaction ordering");
  check(a_scene.packets==(mode?std::vector<net::Packet>{{4,10,2}}:std::vector<net::Packet>{}) && b_scene.packets==(mode?std::vector<net::Packet>{{4,10,3}}:std::vector<net::Packet>{}),"leave interaction trade packets");
 }
 for(int mode:{0,1,2,3,-1})for(bool in_zone:{false,true})for(int count:{0,2}){
  Scene owner_scene,customer_scene;Actor owner(42,{}, {},{},owner_scene,{items,enchant,mobs}),customer(43,{}, {},{},customer_scene,{items,enchant,mobs});
  ZoneLeaveShopState shop,customer_shop;shop.mode=mode;shop.customer_count=count;shop.customer_ids[0]=41;shop.customer_ids[1]=43;shop.offer_bytes.fill(0xa5);customer_shop.visited_owner=42;
  ShopServices services(customer,customer_shop,in_zone);zone_shutdown_shop(owner,shop,services);const bool active=mode==1 || mode==2;
  check(shop.mode==(active?0:mode) && shop.customer_count==(active?0:count) && shop.offer_bytes[0]==(active?0:0xa5),"shop shutdown state");
  check(customer_shop.visited_owner==(active && count==2?0U:42U),"shop customer state");
  check(customer_scene.packets==(active && count==2?std::vector<net::Packet>{{0x0c,0x23}}:std::vector<net::Packet>{}),"shop customer packet");
  check(owner_scene.packets==(active?std::vector<net::Packet>{{2,0x23}}:std::vector<net::Packet>{}),"shop owner packet");
  std::vector<std::string> expected;if(active && count==2)expected={"find41","find43"};if(active && in_zone)expected.push_back("view");check(services.events==expected,"shop shutdown order");
 }
 {
  Scene subject_scene,customer_scene;Actor subject(42,{}, {},{},subject_scene,{items,enchant,mobs}),customer(43,{}, {},{},customer_scene,{items,enchant,mobs});
  ConfiguredMap map;map.definition.size=160;RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.users->attach(subject,1,1,{});zone.users->attach(customer,2,2,{});
  WorldAdmissionUser admission;admission.zone=&zone;WorldUser user{subject,admission};ZoneEntryState entry;entry.character_id=42;entry.cell={1,1};entry.bound=true;
  BoundOwners owners(subject);owners.own.mode=1;owners.own.customer_count=1;owners.own.customer_ids[0]=43;owners.customer.visited_owner=42;
  subject.exchange().mode=3;subject.exchange().peer=&customer;customer.exchange().mode=3;customer.exchange().peer=&subject;
  subject.actions().vehicle_invitation=43;customer.appearance().vehicle_mode=2;customer.actions().vehicle_invitation=42;PartyRequestState party;party.inviter_id=43;ZoneLeaveRequestState requests;
  Npc object;object.users_using=1;VerticalNpcInteraction npc{&object};ViewServices view;UserViewCellTransport transport(*zone.users,view);ZoneLeaveBound bound(zone,user,entry,owners,transport);
  CreationServices creation(subject,nullptr,nullptr);ZoneLeaveTailState tail;LeaveDuelWorld duel(customer,false);GuildZoneServices guild(subject,customer,false,false);
  ZoneActorLeaveServices ports;ports.user_leave_lifecycle=[&](Actor& a){zone_leave_user_lifecycle(a,bound,npc,party,requests,creation,{},tail,duel,guild,[](Actor& p){return std::shared_ptr<void>(&p,[](void*){});},{});};ports.send_view=[&](CellCoordinates c,const net::Packet& p){bound.send_view(c,p);};
  check(leave_zone_actor(zone,user,entry,ports),"bound leave failed");
  check(!npc.npc && object.users_using==0 && owners.own.mode==0 && owners.customer.visited_owner==0 && subject.exchange().mode==0 && customer.exchange().mode==0 && !party.inviter_id && !customer.actions().vehicle_invitation,"bound cleanup state");
  check(subject_scene.packets==std::vector<net::Packet>({{2,0x23},{4,10,2}}) && customer_scene.packets==std::vector<net::Packet>({{0x0c,0x23},{4,10,3},{0x1e,2,2},{3,0x0b,1,42,0,0,0}}),"bound direct packets");
  check(view.recipients==std::vector<std::uint32_t>({43,43}) && view.packets==std::vector<net::Packet>({{0x23,5,42,0,0,0,0},{2,2,42,0,0,0}}),"bound broadcast ordering");
  check(zone.users->detach(43),"bound cleanup");
 }
 for(int mode:{0,1,2,3})for(int other_mode:{0,2})for(bool found:{false,true})for(bool active:{false,true}){
  Scene local_scene,peer_scene;Actor actor(42,{}, {},{},local_scene,{items,enchant,mobs}),peer(99,{}, {},{},peer_scene,{items,enchant,mobs});
  actor.appearance().vehicle_mode=mode;peer.appearance().vehicle_mode=other_mode;actor.actions().vehicle_invitation=active?99:0;actor.actions().vehicle_invitation_until=111;peer.actions().vehicle_invitation=42;peer.actions().vehicle_invitation_until=222;
  PartyRequestState party;party.inviter_id=active?99:0;party.invitation_deadline=777;ZoneLeaveRequestState requests{active?99U:0U,777,active?99U:0U,777};RequestServices services(peer,found);zone_leave_requests(actor,party,requests,services);
  check(actor.actions().vehicle_invitation==(active && mode!=0 && mode!=2?99U:0U) && actor.actions().vehicle_invitation_until==(active && mode==2?0U:111U),"local vehicle cleanup");
  check(peer.actions().vehicle_invitation==(active && found && (mode==2 || (mode==0 && other_mode==2))?0U:42U) && peer.actions().vehicle_invitation_until==(active && found && mode==0 && other_mode==2?0U:222U),"peer vehicle cleanup");
  check(party.inviter_id==0 && party.invitation_deadline==(active?0U:777U) && requests.request_1754==0 && requests.request_5478==0 && requests.deadline_5838==(active?0U:777U) && requests.deadline_583c==(active?0U:777U),"invitation cleanup");
  std::vector<net::Packet> packets;if(active && found){if(mode==0 && other_mode==2)packets.push_back({0x1e,2,2});packets.push_back({3,0x0b,1,42,0,0,0});packets.push_back({6,0x0d,0,99,0,0,0});packets.push_back({3,0x22,0});}
  check(peer_scene.packets==packets && services.lookups.size()==(active?(mode==0 || mode==2?4U:3U):0U),"invitation packet order");
 }
 for(int state:{0,1,2,3,4,5,6})for(bool found:{false,true}){
  Scene a_scene,b_scene;Actor actor(42,{}, {},{},a_scene,{items,enchant,mobs}),peer(99,{}, {},{},b_scene,{items,enchant,mobs});actor.relations().duel_state=state;actor.relations().duel_opponent=99;peer.relations().duel_state=4;
  LeaveDuelWorld world(peer,found);zone_leave_individual_duel(actor,world,[](Actor& a){return std::shared_ptr<void>(&a,[](void*){});},{});
  const bool ended=state>=1 && state<=5 && found;check(world.finds==unsigned(state!=0) && actor.relations().duel_state==(ended?0:state) && peer.relations().duel_state==(ended?0:4),"duel leave state");
  std::vector<net::Packet> own,other;if(ended){if(state==5){own={{8,0x24,2}};other={{8,0x24,3}};}else{const bool wager=state==3 || state==4;const auto id=state==1?99:42;net::Packet packet{static_cast<std::uint8_t>(wager?6:2),0x24,static_cast<std::uint8_t>(wager?0:3),static_cast<std::uint8_t>(id),0,0,0};other.push_back(packet);if(!wager)own.push_back(packet);}}
  check(a_scene.packets==own && b_scene.packets==other && world.logs.size()==(ended && (state==3 || state==4)?2U:0U),"duel leave packets");for(const auto& log:world.logs)check(log.size()==100 && log[0]==3 && log[1]==0x0e && log[99]==0,"duel leave log");
 }
 for(bool by_guild:{false,true})for(unsigned enemy:{0U,123U})for(bool guild:{false,true})for(bool found:{false,true})for(int status:{0,1,2,3,4,5})for(unsigned leader:{42U,77U}){
  Scene a_scene,b_scene;Actor actor(42,{}, {},{},a_scene,{items,enchant,mobs}),peer(99,{}, {},{},b_scene,{items,enchant,mobs});actor.relations().opposing_guild_id=enemy;peer.relations().opposing_guild_id=123;
  GuildZoneServices services(actor,peer,guild,found);services.source.status=status;services.source.stage=99;services.source.leader_id=leader;services.other.status=2;
  if(by_guild)guild_leave_duel(actor,services);else zone_leave_guild_duel(actor,services);const bool present=enemy && guild;const bool negotiation=present && found && (status==1 || status==2) && leader==42;const bool active=present && found && (status==3 || status==4);
  check(services.lookups==unsigned(present) && services.locks==unsigned(present)+unsigned(negotiation) && services.leaves==unsigned(active),"guild leave routing");
  check(actor.relations().opposing_guild_id==(negotiation?0U:enemy) && services.source.status==(negotiation?0:status),"guild leave state");
  std::vector<net::Packet> packets;if(negotiation)packets.push_back({2,0x25,3,static_cast<std::uint8_t>(status==1?99:42),0,0,0});
  check(a_scene.packets==packets && b_scene.packets==packets,"guild leave notifications");
 }
 for(bool present:{false,true})for(unsigned phase:{0U,1U,2U})for(int slot:{0,1,2}){
  Scene a_scene,b_scene;Actor actor(42,{}, {},{},a_scene,{items,enchant,mobs}),peer(43,{}, {},{},b_scene,{items,enchant,mobs});ZoneGuildCreation creation;creation.phase=phase;creation.name="Guild";creation.leader=slot==0?&actor:&peer;creation.participants[0]=slot==1?&actor:&peer;
  CreationServices services(actor,present?&creation:nullptr,&creation);std::array<std::uint8_t,98> scratch;scratch.fill(0xa5);zone_leave_guild_creation(actor,services,scratch);
  std::vector<net::Packet> expected;if(present){if(phase){net::Packet p(98,0xa5);p[0]=1;p[1]=0x0d;p[2]=7;std::copy_n("Guild",6,p.begin()+8);expected.push_back(p);}else expected.push_back({3,0x0d,3});}
  check(services.packets==expected && services.released.size()==unsigned(present),"creation notification");
  if(present)check(!creation.leader && !creation.participants[0] && !services.b && services.a==(slot==2?&creation:nullptr) && services.released[0]==(slot==2?&creation:nullptr),"creation cleanup state");
 }
 for(int mode:{0,1,2,3,-1})for(unsigned pending:{0U,123U}){
  Scene tail_scene;Actor actor(42,{}, {},{},tail_scene,{items,enchant,mobs});actor.appearance().vehicle_mode=mode;actor.actions().vehicle_request=55;ZoneLeaveTailState tail{pending,66};unsigned calls=0;
  zone_leave_tail(actor,tail,[&]{++calls;check(actor.appearance().vehicle_mode==(mode==1?0:mode) && actor.actions().vehicle_request==(mode==1?0U:55U) && tail.pending_147c==0 && tail.associated_1480==(pending?0U:66U),"tail order");});check(calls==1,"duel tail count");
 }
 for(unsigned mask:{0U,1U,0x555U,0x1fffU})for(bool changed:{false,true}){
  Scene save_scene;std::vector<PlayerItem> equipment;PlayerCore core;core.user_id=0x12345678;core.character.id=42;InventoryReplicaPool pool;CharacterReplica replica(core,pool);
  for(unsigned i=0;i<13;++i)if(mask&(1U<<i)){PlayerItem item;item.bag=0;item.slot=i;item.quality=100+i;check(replica.inventory().load(item),"replica equipment setup");item.quality+=unsigned(changed);equipment.push_back(item);}
  Actor actor(42,{}, {0x10001,0x20002,0x30003},{},save_scene,{items,enchant,mobs},equipment);actor.account_id()=core.user_id;for(unsigned i=0;i<13;++i)actor.equipment_database_quality()[i]=100+i;
  ZoneEntryState entry;entry.map=321;entry.direction=654;std::recursive_mutex mutex;CharacterReplicaDelivery delivery(replica,mutex);CoreSaveServices services(delivery);vertical_save_core(actor,entry,services);
  net::Packet resources{6,6,0x78,0x56,0x34,0x12,1,0,2,0,3,0},position{7,6,0x78,0x56,0x34,0x12,0x41,1,0x8e,2,0,0,0xa0,0x3f,0,0,0x20,0xc0,0,0,0x70,0x40};
  check(services.packets.size()==(changed && mask?3U:2U) && services.packets[0]==resources && services.packets[1]==position,"save core packets differ");
  if(changed && mask){net::Packet expected{0x0d,7,0x78,0x56,0x34,0x12,static_cast<std::uint8_t>(std::popcount(mask))};for(unsigned i=0;i<13;++i)if(mask&(1U<<i)){expected.push_back(i);expected.push_back(101+i);expected.push_back(0);}check(services.packets[2]==expected,"save quality packet differs");}
  auto saved=replica.core();check(saved.character.hp==1 && saved.character.mp==2 && saved.character.sp==3 && saved.character.map==321 && saved.direction==654 && saved.x==1.25F && saved.y==-2.5F && saved.z==3.75F,"replica save state differs");
  for(const auto& item:replica.inventory().inventory())check(item.item.quality==100+item.item.slot+unsigned(changed),"replica equipment quality differs");
 }
 for(unsigned count:{0U,1U,10U,254U,255U,256U,500U})for(std::uint32_t tick:{1000U,0xffffffffU}){
  Scene effect_scene;Actor actor(42,{}, {},{},effect_scene,{items,enchant,mobs});std::vector<SkillDefinition> definitions(count);std::vector<PersistedEffect> cached;std::vector<net::Packet> expected;net::Packet batch{4,9,42,0,0,0,0};
  for(unsigned i=0;i<count;++i){auto& d=definitions[i];d.id=i+1;d.level=1;d.effect=(i%3==0?3:i%3==1?4:2);d.offline_countdown=i%2;d.detail=i%11==0?100:1;d.keep_time_seconds=7;const auto end=i%4?0xffffffffU:0U;
   PersistedEffect effect{static_cast<std::uint16_t>(i+1),1,123};cached.push_back(effect);EffectTimer timer;timer.effect=effect;timer.end=end;timer.offline_countdown=d.offline_countdown;actor.effects().append(i+1,timer,d);
   if((d.effect==3 || d.effect==4) && d.detail!=100){const auto now=d.offline_countdown?tick/1000U:tick;const auto diff=end>now?end-now:0U;const auto left=std::min(d.offline_countdown?diff:diff/1000U,7U);batch.push_back(static_cast<std::uint8_t>(d.id));batch.push_back(static_cast<std::uint8_t>(d.id>>8));batch.push_back(1);for(unsigned k=0;k<4;++k)batch.push_back(static_cast<std::uint8_t>(left>>(8*k)));if(++batch[6]==255){expected.push_back(batch);batch.resize(7);batch[6]=0;}}
  }
  if(batch[6])expected.push_back(batch);PlayerCore core;core.user_id=42;InventoryReplicaPool pool;CharacterReplica replica(core,pool);check(replica.skills().load({},cached),"effect cache setup");std::recursive_mutex mutex;CharacterReplicaDelivery delivery(replica,mutex);std::vector<net::Packet> actual;
  bool effect_locked=false;actor.effects().save_database(42,tick,{[&]{check(!effect_locked,"effect lock nested");effect_locked=true;},[&]{check(effect_locked,"effect unlock");effect_locked=false;},[&](const net::Packet& p){check(effect_locked==(p[6]==255),"effect send lock order");actual.push_back(p);check(delivery.apply(p)==InventoryReplicaResult::applied,"effect cache rejected packet");}});check(!effect_locked && actual==expected,"effect persistence packets differ");
  const auto saved=replica.skills().effects();check(saved.size()==count,"effect replica count");for(unsigned i=0;i<count;++i){const auto& d=definitions[i];const auto end=i%4?0xffffffffU:0U;const auto now=d.offline_countdown?tick/1000U:tick;const auto diff=end>now?end-now:0U;const auto left=std::min(d.offline_countdown?diff:diff/1000U,7U);check(saved[i].delay==((d.effect==3 || d.effect==4) && d.detail!=100?left:123U),"effect replica delay");}
 }
 {
  const auto timed=timed_fixture(true),untimed=timed_fixture(false);QuestPoolFixture pool_services;QuestRuntimePool pool(pool_services);
  for(unsigned count:{0U,1U,200U,201U,202U,450U})for(std::uint32_t tick:{1000U,0xffffffffU}){
   QuestRuntime runtime(42,pool);PlayerCore core;core.user_id=42;InventoryReplicaPool item_pool;CharacterReplica replica(core,item_pool);std::vector<net::Packet> expected;net::Packet batch{4,0x0a,42,0,0,0,0};
   for(unsigned i=0;i<count;++i){RestoredQuest q;q.packet.id=i+1;q.packet.counters={static_cast<std::uint8_t>(i),2,3};q.definition=(i%2?timed:untimed).find(1);q.start_tick=i%2?0:0xffffffffU;q.packet.end_tick=i%4?0xffffffffU:0;
    check(runtime.load_active(q),"quest runtime setup");check(replica.quests().load({q.packet.id,0,q.packet.counters}),"quest cache setup");const auto maximum=q.definition->maximum_minutes();const auto delay=maximum?(q.packet.end_tick<=tick?1U:std::min((q.packet.end_tick-tick)/60000U,maximum)):(tick<=q.start_tick?0U:std::min((tick-q.start_tick)/60000U,q.definition->minimum_minutes()));
    batch.push_back(static_cast<std::uint8_t>(q.packet.id));batch.push_back(static_cast<std::uint8_t>(q.packet.id>>8));batch.push_back(static_cast<std::uint8_t>(delay));batch.push_back(static_cast<std::uint8_t>(delay>>8));batch.insert(batch.end(),q.packet.counters.begin(),q.packet.counters.end());if(++batch[6]==201){expected.push_back(batch);batch.resize(7);batch[6]=0;}}
   if(batch[6])expected.push_back(batch);std::recursive_mutex mutex;CharacterReplicaDelivery delivery(replica,mutex);std::vector<net::Packet> actual;
   bool locked=false;quest_runtime_save(runtime,42,tick,{[&]{check(!locked,"nested quest lock");locked=true;},[&]{check(locked,"quest unlock");locked=false;},[&](const net::Packet& p){check(locked==(p[6]==201),"quest send lock order");actual.push_back(p);check(delivery.apply(p)==InventoryReplicaResult::applied,"quest replica rejected");}});check(!locked,"quest lock retained");check(actual==expected && replica.quests().active().size()==count,"quest persistence packets");
  }
 }
 for(unsigned n:{0U,1U,4U,127U})for(unsigned pattern:{0U,1U,2U}){
  std::vector<QuickSlot> seed;net::Packet request{0x0b,1,static_cast<std::uint8_t>(n)};std::vector<QuickSlot> after;unsigned accepted=0;
  for(unsigned i=0;i<n;++i){QuickSlot old{static_cast<std::uint8_t>(i%2?20:5),8,9,0x1234};seed.push_back(old);const auto bag=static_cast<std::uint8_t>(pattern==1 || (pattern==2 && i%2==0)?255:3);QuickSlot next{static_cast<std::uint8_t>(i),4,bag,static_cast<std::uint16_t>(100+i)};request.insert(request.end(),{next.bar,next.slot,next.bag,static_cast<std::uint8_t>(next.number),static_cast<std::uint8_t>(next.number>>8)});if(bag!=255 || old.bar>=10){++accepted;after.push_back(next);}else after.push_back(old);}
  UserQuickSlots slots(seed);PlayerCore core;core.user_id=42;InventoryReplicaPool pool;CharacterReplica replica(core,pool);check(replica.interface().load_quick_slots(seed),"quick cache seed");std::vector<net::Packet> sent;unsigned closed=0;
  check(slots.receive(request,42,[&](const net::Packet& p){sent.push_back(p);check(replica.apply(p)==InventoryReplicaResult::applied,"quick cache rejected");},[&](std::uint32_t){++closed;})==QuickSlotUpdate::accepted,"quick update rejected");
  check(!closed && slots.rows().size()==accepted && sent.size()==unsigned(accepted!=0),"quick update count");net::Packet expected{9,6,42,0,0,0,static_cast<std::uint8_t>(accepted)};
  for(unsigned i=0;i<accepted;++i){const auto& a=after[i];expected.insert(expected.end(),{a.bar,a.slot,a.bag,static_cast<std::uint8_t>(a.number),static_cast<std::uint8_t>(a.number>>8)});}if(accepted)check(sent[0]==expected,"quick snapshot differs");else check(replica.interface().quick_slots().size()==seed.size(),"empty update changed replica");
 }
 std::cout<<"Disconnect, persistence and 12 quick-slot cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
