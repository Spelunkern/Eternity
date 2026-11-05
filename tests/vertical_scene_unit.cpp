#include "eternity/game/vertical_scene.hpp"
#include "eternity/game/vertical_world_actor_scene.hpp"
#include "eternity/game/character_replica.hpp"
#include "eternity/game/vertical_session_character.hpp"
#include "eternity/game/user_view_packets.hpp"
#include "eternity/game/user_cell_snapshot.hpp"
#include "eternity/game/user_cell_snapshot_channel.hpp"
#include "eternity/game/cell_snapshot.hpp"
#include "eternity/game/cell_entry_view.hpp"
#include "eternity/game/zone_actor_leave.hpp"
#include "eternity/game/zone_send_cell_view.hpp"
#include "eternity/game/user_view_snapshot_transport.hpp"
#include "eternity/game/user_view_zone_delivery.hpp"
#include "eternity/game/vertical_move_user.hpp"
#include "eternity/game/vertical_move_status_stand.hpp"
#include "eternity/game/vertical_move_shop_services.hpp"
#include "eternity/game/vertical_move_shop_shared.hpp"
#include "eternity/game/vertical_move_bound_operations.hpp"
#include "eternity/game/vertical_move_retained_session.hpp"
#include "eternity/game/vertical_move_retained_world.hpp"
#include "eternity/game/vertical_move_composed_world.hpp"
#include "eternity/game/vertical_move_gameplay_binding.hpp"
#include "eternity/game/vertical_move_gameplay_factory.hpp"
#include "eternity/game/vertical_move_directory_gameplay.hpp"
#include "eternity/game/vertical_move_runtime_composition.hpp"
#include "eternity/game/vertical_move_shared_world.hpp"
#include "eternity/game/vertical_move_continuation.hpp"
#include "eternity/game/vertical_move_interactions.hpp"
#include "eternity/game/vertical_move_retained_interactions.hpp"
#include "eternity/game/vertical_move_retained_pvp.hpp"
#include "eternity/game/vertical_move_duel_world.hpp"
#include "eternity/game/vertical_move_pvp.hpp"
#include "eternity/game/vertical_move_guild_duel.hpp"
#include "eternity/game/guild_duel_leave.hpp"
#include "eternity/game/guild_duel_reset_members.hpp"
#include "eternity/game/guild_duel_membership_world.hpp"
#include "eternity/game/guild_duel_catalog_movement.hpp"
#include "eternity/game/zone_leave_guild_catalog.hpp"
#include "eternity/game/vertical_move_packet.hpp"
#include "eternity/game/vertical_move_priority_services.hpp"
#include "eternity/game/vertical_move_priority_retained.hpp"
#include "eternity/game/vertical_move_priority_channel.hpp"
#include "eternity/game/vertical_move_channel_send_world.hpp"
#include "eternity/game/user_priority_view.hpp"
#include "eternity/game/user_priority_zone_view.hpp"
#include "eternity/game/user_priority_cell.hpp"
#include "eternity/game/user_priority_area.hpp"
#include "eternity/game/user_priority_shared_cell.hpp"
#include "eternity/game/user_priority_queue.hpp"
#include "eternity/game/user_view_actor_publication.hpp"
#include "eternity/game/user_view_cell_transport.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"
#include "eternity/game/user_view_shared_cells.hpp"
#include "eternity/game/user_view_world_channel.hpp"
#include "eternity/game/entry_packets.hpp"
#include "eternity/game/maps.hpp"
#include "login_socket.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
struct EffectDatabase final:ActorEffectDatabase {
    std::vector<net::Packet> packets;
    void send_effect_database(const net::Packet& packet) noexcept override {packets.push_back(packet);}
};
struct EffectDiagnostics final:ActorEffectDatabaseDiagnostics {
    unsigned rejected=0,failed=0;
    void rejected_effect_database(const net::Packet&,InventoryReplicaResult) noexcept override {++rejected;}
    void failed_effect_database(const net::Packet&) noexcept override {++failed;}
};
struct PriorityCellServices:UserPriorityAreaServices {
    struct Buffer final:UserViewBuffer {};
    std::unique_ptr<UserViewBuffer> allocate(const net::Packet&) override{return std::make_unique<Buffer>();}
    std::vector<std::pair<std::uint32_t,float>> queued;std::vector<std::uint32_t> immediate_ids;
    std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}UserPosition origin() override{return {100,0,100};}
    void immediate(Actor& a,UserViewBuffer&) override{immediate_ids.push_back(a.id());}
    void priority(Actor& a,UserViewBuffer&,std::uint8_t priority,float distance) override{check(priority==7,"Wrong priority");queued.emplace_back(a.id(),distance);}
};
struct PriorityViewServices final:UserPriorityViewServices {
    bool present=true;std::vector<int> events;
    bool has_zone(const Actor&) override{return present;}
    void send_near(Actor& a,const net::Packet&,float radius,std::uint32_t source,std::uint32_t extra,std::uint8_t priority) override{check(radius==80 && source==a.id() && extra==0 && priority==7,"Near priority arguments differ");events.push_back(1);}
    void send_ring(Actor& a,const net::Packet&,float inner,float outer,std::uint32_t source,std::uint32_t extra,std::uint8_t priority) override{check(inner==80 && outer==120 && source==a.id() && extra==0 && priority==7,"Wide priority arguments differ");events.push_back(2);}
};
struct StandShopServices final:ZoneShopShutdownServices {
    Actor* find_customer(Actor&,std::uint32_t) override {throw std::logic_error("Unexpected shop customer");}
    std::shared_ptr<void> retain(Actor&) override {throw std::logic_error("Unexpected shop retention");}
    ZoneLeaveShopState& shop_state(Actor&) override {throw std::logic_error("Unexpected customer shop");}
    std::optional<CellCoordinates> cell(Actor&) override {return {};}
    void send_view(CellCoordinates,const net::Packet&) override {throw std::logic_error("Unexpected shop broadcast");}
};
template<class Session> struct MovementGameplayFixture {
    using Other=std::function<void()>;
    std::shared_ptr<VerticalMoveGameplayBinding<Session>> movement;
    template<class Factory> static std::shared_ptr<MovementGameplayFixture> create_retained(int,VerticalSessionRegistry<Session>&,
        const std::shared_ptr<Session>& owner,RuntimeZone& zone,ZoneEntryState& entry,Factory factory,Other other) {
        if(!other)throw std::invalid_argument("Missing other dispatcher");
        auto result=std::make_shared<MovementGameplayFixture>();result->movement=factory(*owner,*owner->actor,zone,owner->admission(),entry);return result;
    }
};
struct MovePacketServices:VerticalMovePacketServices {
    std::vector<std::string> events;net::Packet broadcast;std::function<void()> on_stand;
    void status_stand(Actor&) override{events.push_back("stand");if(on_stand)on_stand();}std::uint16_t direction(const Actor&) override{return 0x1234;}
    void send_view_action(Actor&,const net::Packet&) override{events.push_back("correct");}
    std::uint32_t tick_ms() override{events.push_back("tick");return 100;}
    bool send_priority_view(Actor&,const net::Packet& p,std::uint8_t priority) override{check(priority==7,"Wrong packet priority");events.push_back("broadcast");broadcast=p;return true;}
    void finish_move(Actor&,bool sent) override{check(sent,"Lost broadcast result");events.push_back("finish");}
    std::array<float,5> speeds{3,4,5,6,7};
    const std::array<float,5>& movement_speed_table() override{events.push_back("speed");return speeds;}
    void close(Actor&,std::uint32_t) override{events.push_back("close");}
};
// Demonstrates that the composed world owner need not invent priority or
// interaction callbacks: those are implemented by the retained adapters.
struct MoveWorldOperations final:VerticalMoveWorldOperations {
    MovePacketServices& delegate;explicit MoveWorldOperations(MovePacketServices& value):delegate(value){}
    void status_stand(Actor& a) override{delegate.status_stand(a);}
    std::uint16_t direction(const Actor& a) override{return delegate.direction(a);}
    void send_view_action(Actor& a,const net::Packet& p) override{delegate.send_view_action(a,p);}
    std::uint32_t tick_ms() override{return delegate.tick_ms();}
    const std::array<float,5>& movement_speed_table() override{return delegate.movement_speed_table();}
    void close(Actor& a,std::uint32_t reason) override{delegate.close(a,reason);}
};
struct BoundMoveServices final:VerticalMovePriorityServices {
    BoundMoveServices(UserPriorityViewState& state,UserPriorityZoneView& view):VerticalMovePriorityServices(state,view){}
    void status_stand(Actor&) override{}std::uint16_t direction(const Actor&) override{return 0;}
    void send_view_action(Actor&,const net::Packet&) override{}std::uint32_t tick_ms() override{return 1;}
    void finish_move(Actor&,bool) override{}const std::array<float,5>& movement_speed_table() override{static const std::array<float,5> values{3,4,5,6,7};return values;}
    void close(Actor&,std::uint32_t) override{}
};
struct GuildBroadcastFixture final:GuildDuelBroadcastServices {
    struct Buffer final:UserViewBuffer {std::vector<std::string>& events;explicit Buffer(std::vector<std::string>& e):events(e){}~Buffer(){events.push_back("release");}};
    std::array<Actor*,3> actors;std::array<VerticalGuildDuelMovement,3> states;bool fail=false;std::vector<std::string> events;
    GuildBroadcastFixture(Actor& a,Actor& b,Actor& c):actors{&a,&b,&c}{}
    Actor& actor(GuildUser& u) override{return *actors.at(u.character_id-42);}
    std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
    VerticalGuildDuelMovement& movement_state(Actor& a) override{return states.at(a.id()-42);}
    std::unique_ptr<UserViewBuffer> allocate(const net::Packet&) override{events.push_back("alloc");return fail?nullptr:std::make_unique<Buffer>(events);}
    void enqueue(GuildUser&,Actor& a,UserViewBuffer&) override{events.push_back("send"+std::to_string(a.id()));}
};
struct CatalogGuildMovement final:GuildDuelCatalogMovement {
    GuildUser& user;VerticalGuildDuelMovement& state;
    CatalogGuildMovement(GuildCatalog& c,GuildDuelCatalogWorld& w,GuildUser& u,VerticalGuildDuelMovement& s):GuildDuelCatalogMovement(c,w),user(u),state(s){}
    GuildUser& guild_user(Actor&) override{return user;}
    VerticalGuildDuelMovement& movement_state(Actor&) override{return state;}
};
struct PairCatalogMovement final:GuildDuelCatalogMovement {
 GuildUser& a;GuildUser& b;VerticalGuildDuelMovement& sa;VerticalGuildDuelMovement& sb;
 PairCatalogMovement(GuildCatalog& c,GuildDuelCatalogWorld& w,GuildUser& x,GuildUser& y,VerticalGuildDuelMovement& p,VerticalGuildDuelMovement& q):GuildDuelCatalogMovement(c,w),a(x),b(y),sa(p),sb(q){}
 GuildUser& guild_user(Actor& actor) override{return actor.id()==a.character_id?a:b;}
 VerticalGuildDuelMovement& movement_state(Actor& actor) override{return actor.id()==a.character_id?sa:sb;}
};
struct GuildResetFixture final:GuildDuelResetServices {
    Actor& first;Actor& second;VerticalGuildDuelMovement a,b;
    GuildResetFixture(Actor& x,Actor& y):first(x),second(y){}
    Actor& actor(GuildUser& u) override{return u.character_id==first.id()?first:second;}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
    VerticalGuildDuelMovement& movement_state(Actor& actor) override{return &actor==&first?a:b;}
};
struct GuildLeaveFixture final:GuildDuelLeaveWorld {
    GuildDuelState& rival;bool found=false;std::vector<std::string> events;
    explicit GuildLeaveFixture(GuildDuelState& state):rival(state){}
    GuildDuelState* find_guild(std::uint32_t) override{events.push_back("find");return found?&rival:nullptr;}
    std::shared_ptr<void> lock(GuildDuelState&) override{events.push_back("lock");return std::shared_ptr<void>(this,[this](void*){events.push_back("unlock");});}
    void send_all(GuildDuelState& state,const net::Packet& packet) override{check(packet.size()==7 && packet[0]==6 && packet[1]==0x25,"Bad guild leave packet");events.push_back(&state==&rival?"other-all":"own-all");}
    void send_members(GuildDuelState& state,const net::Packet& packet) override{check(packet.size()==3 && packet[0]==5 && packet[1]==0x25 && packet[2]==(&state==&rival?1:2),"Bad guild end packet");events.push_back(&state==&rival?"other-members":"own-members");}
    void reset_all_members(GuildDuelState& state) override{events.push_back(&state==&rival?"other-reset":"own-reset");}
};
struct MoveGuildServices final:VerticalGuildDuelMovementServices {
    bool present=false,disappear=false;int status=0;std::vector<std::string> events;
    std::shared_ptr<void> capture_guild_lock(Actor&) override {
        if(!present)return {};events.push_back("lock");if(disappear){present=false;events.push_back("unlock");return {};}
        return std::shared_ptr<void>(this,[this](void*){events.push_back("unlock");});
    }
    void* current_guild(Actor&) override{return present?this:nullptr;}
    std::int32_t guild_duel_status(void*) override{return status;}
    void leave_guild_duel(void*,Actor&,std::uint8_t mode) override{check(mode==1,"Wrong guild leave mode");events.push_back("leave");}
};
struct RetainedDuelWorld final:DuelWorld {
    ZoneUsers& users;unsigned lookups=0,positions=0;explicit RetainedDuelWorld(ZoneUsers& value):users(value){}
    Actor* find_duel_user(Actor&,std::uint32_t id) override{++lookups;return users.find_actor(id);}
    UserPosition duel_position(const Actor& actor) override{++positions;auto pos=users.position(actor.id());if(!pos)throw std::logic_error("Missing duel position");return *pos;}
    std::int32_t drop_random() override{throw std::logic_error("Unexpected random");}
    DuelDropItem* allocate_drop_item() override{throw std::logic_error("Unexpected item");}
    bool enter_duel_item(Actor&,DuelDropItem&,bool) override{throw std::logic_error("Unexpected drop");}
    GameLogSubject duel_log_subject(const Actor&) override{throw std::logic_error("Unexpected duel log");}
    std::chrono::sys_seconds local_calendar() override{throw std::logic_error("Unexpected calendar");}
    void send_duel_log(const net::Packet&) override{throw std::logic_error("Unexpected log send");}
};
struct MovePvpServices final:VerticalMovePvpServices {
    Actor& actor;Actor& peer;bool found=false;float distance=0;std::vector<std::string> events;
    MovePvpServices(Actor& a,Actor& p):actor(a),peer(p){}
    UserPosition position(const Actor&) override{return {distance,0,0};}
    Actor* find_opponent(Actor&,std::uint32_t) override{events.push_back("find");return found?&peer:nullptr;}
    std::shared_ptr<void> retain(Actor& a) override{return {&a,[](void*){}};}
    void end_duel(Actor& a) override{events.push_back(&a==&actor?"end-self":"end-peer");}
    void leave_active_duel(Actor&) override{events.push_back("active");}
    void check_guild_duel(Actor&) override{events.push_back("guild");}
};
struct MoveInteractionServices final:VerticalMoveInteractionServices {
    Actor& subject;UserPosition other;unsigned guild=0,pvp=0;
    explicit MoveInteractionServices(Actor& a):subject(a){}
    UserPosition position(const Actor& actor) override{return &actor==&subject?UserPosition{}:other;}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
    void refresh_guild_mark(Actor&) override{++guild;}void check_pvp(Actor&) override{++pvp;}
};
struct MoveContinuationServices final:VerticalMoveContinuationServices {
    unsigned finished=0;net::Packet packet;
    std::uint32_t tick_ms() override{return 0x12345678;}
    bool send_priority_view(Actor&,const net::Packet& p,std::uint8_t priority) override{check(priority==7,"Wrong movement priority");packet=p;return true;}
    void finish_move(Actor&,bool sent) override{check(sent,"Movement send result lost");++finished;}
};
struct SnapshotCorpseServices final:ZoneCorpseServices {
    Corpse* allocate() override{return nullptr;}void release(Corpse&) override{}
    void diagnostic(CorpseIssue,std::uint32_t) override{}void send_cell_view(CellCoordinates,const net::Packet&) override{}
};
struct SnapshotMobServices final:ZoneMobServices {
    std::uint32_t tick_ms() override{return 0;}void send_cell_view(CellCoordinates,const net::Packet&) override{}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>) override{return {};}
    void lua_init(MobRuntime&) override{}
};
struct SnapshotNpcServices final:ZoneNpcServices {
    Npc* allocate() override{return nullptr;}std::uint32_t next_id() override{return 0;}
    std::uint32_t tick_ms() const noexcept override{return 0;}void invalid_guard_position(const MapNpc&) override{}
    void send_cell_view(CellCoordinates,const net::Packet&) override{}
    std::unique_ptr<NpcBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t>) override{return {};}
};
struct SnapshotItemServices final:ZoneItemServices {
    std::uint32_t tick_ms() override{return 0;}void send_cell_view(CellCoordinates,const net::Packet&) override{}
    void release(GroundItem&) override{}
};
struct SnapshotViewer final:UserCellSnapshotServices {
    ZoneUsers& zone;std::vector<net::Packet> packets;
    explicit SnapshotViewer(ZoneUsers& z):zone(z){}
    std::shared_ptr<void> retain(Actor&) override {return std::make_shared<int>(1);}
    UserViewLiveAppearance appearance(const Actor& actor) override {return capture_user_view_appearance(zone,actor,10,0x1234,-7);}
    std::array<std::uint8_t,4> speed_stack_bytes() override {return {0x55,0x55,0x55,0x55};}
    void send_self(const net::Packet& p) override {packets.push_back(p);}
};
struct ViewBuffer final:UserViewBuffer {explicit ViewBuffer(net::Packet p):packet(std::move(p)){}net::Packet packet;};
struct ViewTransport final:UserViewActorTransport {
    Actor& actor;UserViewLiveAppearance live{11,0,{20,0,20},0};std::vector<net::Packet> allocated;unsigned leaves=0,enters=0;
    explicit ViewTransport(Actor& a):actor(a){}
    UserViewLiveAppearance appearance(const Actor&) override{return live;}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet p) override {allocated.push_back(p);return std::make_unique<ViewBuffer>(std::move(p));}
    void broadcast(CellCoordinates,UserViewBuffer&) override {}
    void leave_snapshot(CellCoordinates) override {++leaves;live={10,0x1234,{12.5F,-3.F,99.F},-7};actor.status().skills.equipment.attack_speed=3;actor.status().skills.equipment.move_speed=2;}
    void enter_snapshot(CellCoordinates) override {++enters;actor.status().shape=7;}
};
struct LiveZoneViewTransport final:UserViewActorTransport {
    ZoneUsers& zone;Actor& actor;std::vector<net::Packet> allocated;
    LiveZoneViewTransport(ZoneUsers& z,Actor& a):zone(z),actor(a){}
    UserViewLiveAppearance appearance(const Actor& subject) override{return capture_user_view_appearance(zone,subject,11,0x1234,-7);}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet p) override{allocated.push_back(p);return std::make_unique<ViewBuffer>(std::move(p));}
    void broadcast(CellCoordinates,UserViewBuffer&) override{}
    void leave_snapshot(CellCoordinates) override{check(zone.move_at_position(actor.id(),{65,8,20}),"Live position change failed");}
    void enter_snapshot(CellCoordinates) override{}
};
struct CellServices final:UserViewCellServices {
    std::vector<std::pair<std::uint32_t,net::Packet>> delivered;unsigned leaves=0,enters=0;
    std::function<std::shared_ptr<void>(Actor&)> retain_actor;std::function<void(Actor&)> on_enqueue;
    UserViewLiveAppearance appearance(const Actor&) override{return {10,0,{140,0,140},0};}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet packet) override{return std::make_unique<ViewBuffer>(std::move(packet));}
    std::shared_ptr<void> retain(Actor& actor) override{return retain_actor?retain_actor(actor):std::shared_ptr<void>(&actor,[](void*){});} // Fixture retains actors unless overridden.
    void enqueue(Actor& actor,UserViewBuffer& buffer) override {delivered.emplace_back(actor.id(),static_cast<ViewBuffer&>(buffer).packet);if(on_enqueue)on_enqueue(actor);}
    void leave_snapshot(CellCoordinates) override{++leaves;}
    void enter_snapshot(CellCoordinates) override{++enters;}
};
struct QueuedViewBuffer final:UserViewBuffer {
    std::shared_ptr<net::Packet> packet;
    explicit QueuedViewBuffer(net::Packet p):packet(std::make_shared<net::Packet>(std::move(p))){}
};
struct DeferredCellServices final:UserViewCellServices {
    std::vector<std::pair<std::uint32_t,std::shared_ptr<net::Packet>>> queued;
    std::vector<std::weak_ptr<net::Packet>> allocated;
    UserViewLiveAppearance appearance(const Actor&) override{return {11,0,{140,0,140},0};}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet p) override{auto buffer=std::make_unique<QueuedViewBuffer>(std::move(p));allocated.push_back(buffer->packet);return buffer;}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
    void enqueue(Actor& actor,UserViewBuffer& buffer) override{queued.emplace_back(actor.id(),static_cast<QueuedViewBuffer&>(buffer).packet);}
    void leave_snapshot(CellCoordinates) override{}
    void enter_snapshot(CellCoordinates) override{}
};
struct PartyWorld:PartyMembershipSendWorld {
    PartyMembershipSendBuffer buffer;std::vector<std::uint32_t> sent;std::vector<net::Packet> frames;bool active=false,deny_allocation=false,fail_allocation=false,fail_enqueue=false,fail_free=false,defer=false,allow_unlocked=false;unsigned locks=0,frees=0;std::function<void()> on_enqueue;
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {check(!active,"Duplicate buffer allocation");if(fail_allocation) throw std::bad_alloc();if(deny_allocation) return nullptr;active=true;return &buffer;}
    void free_send_buffer(PartyMembershipSendBuffer& b) override {check(&b==&buffer && b.references==0,"Wrong buffer release");active=false;++frees;if(fail_free) throw std::runtime_error("Free failure");}
    void enqueue_send(PartyMembershipUser& u,PartyMembershipSendBuffer& b) override {check((locks || defer || allow_unlocked) && &b==&buffer,"Missing party lock/buffer");if(fail_enqueue) throw std::runtime_error("Queue failure");sent.push_back(u.id);frames.push_back(b.frame);if(on_enqueue) {auto callback=std::move(on_enqueue);callback();}if(!defer)release_party_membership_buffer(b,*this);}
    void lock(PartyNormalMembership&) override {++locks;}
    void unlock(PartyNormalMembership&) noexcept override {--locks;}
    std::uint32_t tick() override {throw std::logic_error("Unexpected tick");}
    void lock_effects(PartyMembershipUser&) override {throw std::logic_error("Unexpected effect lock");}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
    void send_user(PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("Unexpected user send");}
    void send_scene(PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("Unexpected scene send");}
    void unregister_party(PartyNormalMembership&) override {throw std::logic_error("Unexpected unregister");}
    void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("Unexpected cleanup");}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {throw std::logic_error("Unexpected log");}
    void send_log(const net::Packet&) override {throw std::logic_error("Unexpected log send");}
};
struct MultiWorld:PartyWorld {
    std::array<PartyMembershipSendBuffer,4> slots;std::array<bool,4> used{};
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override{for(std::size_t i=0;i<slots.size();++i)if(!used[i]){used[i]=true;return &slots[i];}return nullptr;}
    void free_send_buffer(PartyMembershipSendBuffer& buffer) override{for(std::size_t i=0;i<slots.size();++i)if(&slots[i]==&buffer){check(used[i] && buffer.references==0,"Invalid pool release");used[i]=false;++frees;return;}throw std::logic_error("Foreign pool buffer");}
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override{sent.push_back(user.id);frames.push_back(buffer.frame);release_party_membership_buffer(buffer,*this);}
};
struct SharedPriorityCell final:UserPrioritySharedCellServices {
    PartyMembershipUser source,recipient;UserPriorityQueue& queue;
    SharedPriorityCell(UserViewSharedBuffers& buffers,UserPriorityQueue& q):UserPrioritySharedCellServices(buffers),queue(q){source.id=42;recipient.id=43;source.send_socket=recipient.send_socket=1;}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
    UserPosition origin() override{return {0,0,0};}
    PartyMembershipUser& connection(Actor& actor) override{return actor.id()==42?source:recipient;}
    UserPriorityQueue& connection_queue(Actor&) override{return queue;}
};
struct ThreePriorityCells final:UserPrioritySharedCellServices {
    ZoneUsers& zone;Actor& source;PartyMembershipUser users[3];UserPriorityQueue &near,&far;
    ThreePriorityCells(UserViewSharedBuffers& b,ZoneUsers& z,Actor& a,UserPriorityQueue& n,UserPriorityQueue& f):UserPrioritySharedCellServices(b),zone(z),source(a),near(n),far(f){for(unsigned i=0;i<3;++i){users[i].id=42+i;users[i].send_socket=1;}}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};}
    UserPosition origin() override{return *zone.position(source.id());}
    PartyMembershipUser& connection(Actor& actor) override{return users[actor.id()-42];}
    UserPriorityQueue& connection_queue(Actor& actor) override{return actor.id()==43?near:far;}
};
struct MultiChannelWorld final:MultiWorld {
    net::WorldChannel channel;explicit MultiChannelWorld(net::WorldChannel c):channel(std::move(c)){}
    void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer& buffer) override {enqueue_user_view_world_channel(buffer,*this,channel);}
};
struct ChannelWorld final:PartyWorld {
    net::WorldChannel channel;explicit ChannelWorld(net::WorldChannel c):channel(std::move(c)){}
    void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer& buffer) override {enqueue_user_view_world_channel(buffer,*this,channel);}
};
struct SharedCellServices final:UserViewSharedCellServices {
    PartyMembershipUser old_user,new_user;unsigned leaves=0,enters=0;
    explicit SharedCellServices(UserViewSharedBuffers& buffers):UserViewSharedCellServices(buffers){old_user.id=43;new_user.id=44;old_user.send_socket=new_user.send_socket=1;}
    UserViewLiveAppearance appearance(const Actor&) override{return {10,0,{140,0,140},0};}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};} // Fixture owns actors.
    PartyMembershipUser& connection(Actor& actor) override{return actor.id()==43?old_user:new_user;}
    void leave_snapshot(CellCoordinates) override{++leaves;}
    void enter_snapshot(CellCoordinates) override{++enters;}
};
struct SelfCellServices final:UserViewSharedCellServices {
    PartyMembershipUser& user;
    SelfCellServices(UserViewSharedBuffers& buffers,PartyMembershipUser& current):UserViewSharedCellServices(buffers),user(current){}
    UserViewLiveAppearance appearance(const Actor&) override{return {10,0,{20,0,20},0};}
    std::shared_ptr<void> retain(Actor& actor) override{return {&actor,[](void*){}};} // Fixture owns actor.
    PartyMembershipUser& connection(Actor&) override{return user;}
    void leave_snapshot(CellCoordinates) override{}
    void enter_snapshot(CellCoordinates) override{}
};
struct Services final:VerticalSceneServices {
    PartyRegistryParty* group=nullptr;std::vector<std::string> calls;std::uint32_t freed=0;unsigned failures=0;VerticalSceneFailure failure=VerticalSceneFailure::self_send;bool has_exception=false;
    void transport_failure(VerticalSceneFailure kind,std::exception_ptr error) noexcept override {++failures;failure=kind;has_exception=bool(error);}
    void nearby(ZoneUsers& z,Actor& a,const net::Packet&) override {check(z.find_actor(a.id())==&a,"Wrong nearby owner");calls.push_back("near");}
    void cell_view(ZoneUsers& z,Actor& a,const net::Packet&) override {check(z.find_actor(a.id())==&a,"Wrong cell owner");calls.push_back("cell");}
    PartyRegistryParty* party(PartyMembershipUser&) noexcept override {return group;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return 19;}
    void release_effect_id(std::uint32_t id) noexcept override {freed=id;}
    std::uint32_t tick_ms() const noexcept override {return 101;}
    void reset_damage(Actor&) override {calls.push_back("reset");}
    void stop_target_mob(Actor&,std::uint32_t id) override {check(id==44,"Wrong target");calls.push_back("stop");}
    void taunt(Actor&,std::uint16_t amount,float range) override {check(amount==5 && range==60,"Wrong taunt");calls.push_back("taunt");}
    UserShapeSocial user_shape_social(Actor&) override {return {2,"guild"};}
    void finish_periodic_damage(Actor&,EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed) override {check(elapsed==7,"Wrong damage time");calls.push_back("damage");}
};

struct SceneApplication final:net::WorldApplication {
    MapGeometry geometry{160};ZoneUsers zone{geometry,0};PartyMembershipUser user;PartyWorld party_world;Services services;EffectDatabase effect_database;
    ChannelWorld channel_world;MultiChannelWorld view_world;ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;VerticalScene scene;Actor actor;
    explicit SceneApplication(net::WorldChannel channel):channel_world(channel),view_world(channel),scene(std::move(channel),zone,user,party_world,services,effect_database),actor(42,{}, {},{},scene,{items,enchant,mobs}) {user.id=42;user.send_socket=1;}
    void open() override {scene.bind(actor);check(zone.attach_at_position(actor,{20,0,20}),"Network scene attachment failed");scene.send_self({1,1,42});
        UserViewSharedBuffers buffers(channel_world);auto shared=buffers.allocate(user_view_leave_packet(42));check(bool(shared),"Network view allocation failed");buffers.enqueue(user,*shared);shared.reset();check(!channel_world.active && channel_world.frees==1,"Network view buffer not released");
        UserViewSharedBuffers cell_buffers(view_world);SelfCellServices cell_services(cell_buffers,user);UserViewCellTransport transport(zone,cell_services);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(5,5,{3,0},{2,0},publication);check(view_world.frees==3,"Network cell buffers not released");
        Actor leaving(99,{}, {},{},scene,{items,enchant,mobs});zone.attach(leaving,0,0,{20,0,20});
        RuntimeZone runtime;WorldAdmissionUser admission;WorldUser world_user{leaving,admission};ZoneEntryState leave_state;
        runtime.users=std::make_unique<ZoneUsers>(geometry,0);
        check(zone.detach(99),"Fixture transfer detach failed");runtime.users->attach(leaving,0,0,{20,0,20});
        // Both users must share the canonical zone; use the scene's zone as the
        // runtime owner temporarily and restore it before application close.
        check(zone.detach(42),"Recipient transfer detach failed");runtime.users->attach(actor,0,0,{20,0,20});
        admission.zone=&runtime;leave_state.character_id=99;leave_state.cell={0,0};leave_state.bound=true;
        UserViewCellTransport leave_transport(*runtime.users,cell_services);ZoneActorLeaveServices leave_services;
        leave_services.user_leave_lifecycle=[](Actor&){};
        leave_services.send_view=[&](CellCoordinates cell,const net::Packet& packet){zone_send_cell_view(geometry.cell_count(),geometry.cell_count(),cell,packet,leave_transport);};
        check(leave_zone_actor(runtime,world_user,leave_state,leave_services),"Network zone leave failed");
        check(runtime.users->detach(42),"Recipient restore detach failed");zone.attach(actor,0,0,{20,0,20});
        auto queued=buffers.allocate(net::Packet{0x77,0x05,42});UserPriorityQueue queue(channel_world);
        check(buffers.enqueue_priority(user,*queued,queue,7,35),"TCP priority enqueue failed");queued.reset();auto batch=queue.take_batch();check(bool(batch),"TCP priority batch missing");
        deliver_priority_batch(*batch,channel_world,channel_world.channel);check(!channel_world.active,"TCP priority buffer retained after channel copy");}
    void receive(const net::Packet&) override {throw std::logic_error("Unexpected network request");}
    void close() override {check(zone.detach(actor.id()),"Network scene detach failed");scene.unbind();}
};
void network_channel() {
    asio::io_context io;auto registry=std::make_shared<session::Registry>();session::LoginSession account{};account.user_id=42;account.session_id=7;account.username="scene";account.token.fill(0x23);
    for(unsigned i=0;i<account.game_key.size();++i) account.game_key[i]=static_cast<std::uint8_t>(i+1);
    registry->add(account);const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(42,7,0,now),"Cannot reserve test world");
    net::WorldServerOptions options;options.port=0;net::WorldServer server(io,options,registry,[](const session::LoginSession&,net::WorldChannel channel) {return std::make_unique<SceneApplication>(std::move(channel));});server.start();
    struct Running {asio::io_context& io;net::WorldServer& server;std::jthread thread;Running(asio::io_context& i,net::WorldServer& s):io(i),server(s),thread([&]{io.run();}){}~Running(){server.stop();if(thread.joinable()) thread.join();}} running(io,server);
    net::SessionSecrets secrets{};std::copy_n(account.game_key.begin(),16,secrets.key.begin());std::copy_n(account.game_key.begin()+16,16,secrets.iv.begin());net::LegacyCipher receive(secrets);
    PeerSocket peer(server.port());Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));
    auto greeting=peer.packet();receive.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"World greeting failed");
    auto message=peer.packet();receive.crypt(message);check(message==Packet({1,1,42}),"Actual WorldChannel scene delivery failed");
    auto shared=peer.packet();receive.crypt(shared);check(shared==user_view_leave_packet(42),"Shared view buffer did not reach actual TCP channel");
    auto appeared=peer.packet();receive.crypt(appeared);check(appeared.size()==29 && appeared[0]==1 && appeared[1]==2,"Cell appearance did not reach actual TCP channel");
    auto speed=peer.packet();receive.crypt(speed);check(speed.size()==8 && speed[0]==0x1c && speed[1]==5,"Cell speed did not follow appearance over TCP");
    auto left=peer.packet();receive.crypt(left);check(left==Packet({2,2,99,0,0,0}),"Zone leave did not reach nearby actor over TCP");
    auto queued=peer.packet();receive.crypt(queued);check(queued==Packet({0x77,0x05,42}),"Priority queue payload did not reach TCP channel");
}
}
int main() {try {
    std::vector<net::Packet> outgoing;bool fail_self=false;net::WorldChannel channel;channel.send=[&](net::Packet p){if(fail_self) throw std::bad_alloc();outgoing.push_back(std::move(p));};
    const MapGeometry geometry(160);ZoneUsers zone(geometry,0);PartyMembershipUser user;user.id=42;PartyWorld party_world;Services services;
    EffectDatabase effect_database;VerticalScene scene(channel,zone,user,party_world,services,effect_database);const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;
    Actor actor(42,{}, {},{},scene,{definitions,enchant,mobs});actor.account_id()=314;scene.bind(actor);
    check(user_view_leave_packet(42)==net::Packet({2,2,42,0,0,0}) && !user_view_shape_packet(actor),"Normal user view packets differ");
    actor.appearance().visible=false;check(user_view_shape_packet(actor)==net::Packet({0x1d,5,42,0,0,0,13}),"Invisible shape packet differs");
    actor.appearance().visible=true;actor.appearance().vehicle_mode=2;actor.appearance().vehicle_shape=255;actor.appearance().vehicle_addition=1;
    check(user_view_shape_packet(actor)==net::Packet({0x1d,5,42,0,0,0,0}),"Nonzero DWORD vehicle shape lost after byte truncation");
    actor.appearance().vehicle_mode=3;actor.appearance().vehicle_partner=0x12345678;
    check(user_view_appearance_packet(actor,10,0x1234,{12.5F,-3.F,99.F},-7)==net::Packet({1,2,42,0,0,0,2,0x34,0x12,0,0,0x48,0x41,0,0,0x40,0xc0,0,0,0xc6,0x42,0xf9,0xff,0xff,0xff,0x78,0x56,0x34,0x12}),"Original moved-user appearance packet differs");
    const auto ordinary=user_view_appearance_packet(actor,11,0x1234,{12.5F,-3.F,99.F},-7);actor.appearance().vehicle_mode=0;
    const auto no_partner=user_view_appearance_packet(actor,11,0x1234,{12.5F,-3.F,99.F},-7);
    check(ordinary[6]==0 && no_partner[6]==0 && no_partner[25]==0 && no_partner[26]==0 && no_partner[27]==0 && no_partner[28]==0,"Permission or vehicle gate differs");
    actor.appearance().vehicle_mode=0;actor.appearance().vehicle_shape=0;actor.appearance().vehicle_addition=0;
    {
        ZoneUsers zone(3,3,0);zone.attach(actor,0,0,{12.5F,-3.F,99.F});SnapshotViewer snapshot(zone);
        actor.appearance().visible=false;user_cell_snapshot_enter(zone,{0,0},snapshot);
        check(snapshot.packets.size()==3 && snapshot.packets[0]==user_view_appearance_packet(actor,10,0x1234,{12.5F,-3.F,99.F},-7) &&
              snapshot.packets[1]==user_view_shape_packet(actor) && snapshot.packets[2][0]==0x1c && snapshot.packets[2][2]==0x55 && snapshot.packets[2][5]==0x55,"User cell entry snapshot differs");
        snapshot.packets.clear();user_cell_snapshot_leave(zone,{0,0},snapshot);
        check(snapshot.packets==std::vector<net::Packet>{user_view_leave_packet(actor.id())},"User cell leave snapshot differs");
        zone.detach(actor.id());actor.appearance().visible=true;
    }
    {
        MapGeometry geometry(81);Corpse corpse;MobRuntime mob;Npc npc;GroundItem item;
        corpse.id=100;mob.id=101;npc.id=102;item.id=103;
        corpse.position=mob.position=npc.position=item.position={1,2,3};
        SnapshotCorpseServices corpse_services;SnapshotMobServices mob_services;SnapshotNpcServices npc_services;SnapshotItemServices item_services;
        WorldCorpses world_corpses;ZoneUsers users(geometry,0);ZoneCorpses corpses(geometry,world_corpses,corpse_services);
        ZoneMobs mobs(geometry,mob_services);ZoneNpcs npcs(geometry,npc_services);ZoneItems items(geometry,item_services);
        users.attach(actor,0,0,{1,2,3});actor.appearance().visible=false;
        check(corpses.enter(corpse) && mobs.enter(mob) && npcs.enter(npc) && items.enter(item,1),"Cell snapshot entity setup failed");
        ConfiguredMap environment_map;environment_map.definition.size=81;environment_map.definition.doors={{{1,0,1},1,1}};auto environment=ZoneEnvironment::prepare(environment_map);environment.doors()[0].open=true;
        SnapshotViewer viewer(users);CellSnapshot snapshot(users,corpses,mobs,npcs,items,environment,viewer);snapshot.enter({0,0});
        std::vector<std::uint16_t> opcodes;for(const auto& packet:viewer.packets)opcodes.push_back(packet[0]|(packet[1]<<8));
        check(opcodes==std::vector<std::uint16_t>{0x201,0x51d,0x51c,0x405,0x601,0xe01,0x401,0x403},"Cell entry entity order differs");
        viewer.packets.clear();snapshot.leave({0,0});opcodes.clear();for(const auto& packet:viewer.packets)opcodes.push_back(packet[0]|(packet[1]<<8));
        check(opcodes==std::vector<std::uint16_t>{0x202,0x406,0x602,0xe02,0x402},"Cell leave entity order differs");
        viewer.packets.clear();auto entry=bind_entry_cell_snapshots({},geometry.cell_count(),geometry.cell_count(),snapshot);entry.cell_info({0,0});
        check(viewer.packets.size()==8 && viewer.packets.back()==net::Packet({3,4,0,1}) && viewer.packets[3]==corpse_appearance(corpse) && viewer.packets[5]==npc_appearance(npc),"Entry view did not dispatch complete snapshots");
        users.detach(actor.id());actor.appearance().visible=true;
    }
    {
        MapGeometry geometry(280);Corpse corpse;Npc npc;corpse.id=100;npc.id=102;
        corpse.position={1,0,81};npc.position={201,0,81};
        SnapshotCorpseServices corpse_services;SnapshotMobServices mob_services;SnapshotNpcServices npc_services;SnapshotItemServices item_services;
        WorldCorpses world_corpses;ConfiguredMap movement_map;movement_map.settings.id=8;movement_map.definition.size=280;RuntimeZone runtime;runtime.map=&movement_map;runtime.users=std::make_unique<ZoneUsers>(geometry,0);auto& users=*runtime.users;
        runtime.corpses=std::make_unique<ZoneCorpses>(geometry,world_corpses,corpse_services);
        runtime.mobs=std::make_unique<ZoneMobs>(geometry,mob_services);runtime.npcs=std::make_unique<ZoneNpcs>(geometry,npc_services);runtime.items=std::make_unique<ZoneItems>(geometry,item_services);
        users.attach(actor,2,2,{100,0,81});check(runtime.corpses->enter(corpse) && runtime.npcs->enter(npc),"Movement snapshot setup failed");
        std::vector<net::Packet> sent;net::WorldChannel channel;channel.send=[&](net::Packet packet){sent.push_back(std::move(packet));};
        UserCellSnapshotChannel viewer(channel,[](Actor& a){return std::shared_ptr<void>(&a,[](void*){});},
            [&](const Actor& a){return capture_user_view_appearance(users,a,10,0x1234,-7);},[]{return std::array<std::uint8_t,4>{};});
        ConfiguredMap environment_map;environment_map.definition.size=280;runtime.environment=ZoneEnvironment::prepare(environment_map);
        CellServices recipients;
        { RuntimeZone incomplete;bool rejected=false;try { UserViewZoneDelivery bad(incomplete,recipients,viewer); }
          catch(const std::invalid_argument&) {rejected=true;} check(rejected,"Incomplete zone accepted for view delivery"); }
        UserViewZoneDelivery delivery(runtime,recipients,viewer);auto& transport=delivery.transport();
        ZoneEntryState admission;admission.character_id=actor.id();admission.map=8;admission.bound=true;admission.cell={2,2};admission.position={100,0,81};
        bool closed=false;auto close=[&](auto){closed=true;};
        check(!vertical_move_user(actor,runtime,admission,3,{140,0,81},transport,close) && users.position(actor.id())->x==100,"Wrong-location move changed state");
        check(!vertical_move_user(actor,runtime,admission,4,{-1,0,81},transport,close) && users.position(actor.id())->x==100,"Invalid move changed state");
        check(vertical_move_user(actor,runtime,admission,4,{110,0,81},transport,close) && sent.empty() && admission.cell.x==2,"Same-cell move published visibility");
        check(vertical_move_user(actor,runtime,admission,4,{140,0,81},transport,close),"Integrated user move failed");
        check(!closed && admission.cell.x==3 && admission.position.x==140 && users.cell(actor.id())->x==3,"User move state differs");
        check(sent.size()==2 && sent[0]==corpse_removed(corpse.id) && sent[1]==npc_appearance(npc),"Movement did not deliver live cell snapshots in order");
        users.detach(actor.id());
    }
    {
        for(int z=0;z<5;++z)for(int x=0;x<5;++x) {
            std::vector<CellCoordinates> actual;visit_cell_entry_view(5,5,{x,z},[&](auto cell){actual.push_back(cell);});
            std::vector<CellCoordinates> expected;for(int zz=std::max(0,z-2);zz<=std::min(4,z+2);++zz)
                for(int xx=std::max(0,x-2);xx<=std::min(4,x+2);++xx)expected.push_back({xx,zz});
            check(actual.size()==expected.size(),"Entry view size differs");
            for(std::size_t i=0;i<actual.size();++i)check(actual[i].x==expected[i].x && actual[i].z==expected[i].z,"Entry view order differs");
        }
    }
    {
        ConfiguredMap map;map.settings.id=7;map.definition.size=160;const auto map_geometry=map.definition.geometry();
        Corpse corpse;Npc npc;corpse.id=100;npc.id=102;corpse.position={1,0,1};npc.position={81,0,1};
        SnapshotCorpseServices corpse_services;SnapshotMobServices mob_services;SnapshotNpcServices npc_services;SnapshotItemServices item_services;
        WorldCorpses world_corpses;RuntimeZone runtime;runtime.map=&map;runtime.users=std::make_unique<ZoneUsers>(map_geometry,0);
        runtime.corpses=std::make_unique<ZoneCorpses>(map_geometry,world_corpses,corpse_services);runtime.mobs=std::make_unique<ZoneMobs>(map_geometry,mob_services);
        runtime.npcs=std::make_unique<ZoneNpcs>(map_geometry,npc_services);runtime.items=std::make_unique<ZoneItems>(map_geometry,item_services);
        runtime.environment=ZoneEnvironment::prepare(map);check(runtime.corpses->enter(corpse) && runtime.npcs->enter(npc),"Entry integration setup failed");
        std::vector<net::Packet> delivered;net::WorldChannel entry_channel;entry_channel.send=[&](net::Packet p){delivered.push_back(std::move(p));};
        PartyMembershipUser entrant_user;entrant_user.id=777;PartyWorld entry_pool;Services entry_scene_services;EffectDatabase entry_database;
        VerticalScene entry_scene(entry_channel,*runtime.users,entrant_user,entry_pool,entry_scene_services,entry_database);
        Actor entrant(777,{}, {},{},entry_scene,{definitions,enchant,mobs});entry_scene.bind(entrant);
        UserCellSnapshotChannel snapshot_channel(entry_channel,[](Actor& a){return std::shared_ptr<void>(&a,[](void*){});},
            [&](const Actor& a){return capture_user_view_appearance(*runtime.users,a,255,0,0);},[]{return std::array<std::uint8_t,4>{};});
        CellSnapshot snapshot(*runtime.users,*runtime.corpses,*runtime.mobs,*runtime.npcs,*runtime.items,*runtime.environment,snapshot_channel);
        bool view_sent=false,obelisks=false,nprotect=false;ZoneActorEntryServices entry_services;
        entry_services.send_view=[&](CellCoordinates,const net::Packet&){view_sent=true;};entry_services.obelisks=[&]{obelisks=true;};
        entry_services.nprotect=[&]{nprotect=true;};entry_services.diagnostic=[]{};entry_services.close=[](std::uint32_t){throw std::runtime_error("Unexpected entry close");};
        auto bound=bind_entry_cell_snapshots(entry_services,map_geometry.cell_count(),map_geometry.cell_count(),snapshot);
        ZoneEntryState admission;admission.character_id=entrant.id();admission.position={1,0,1};
        check(enter_zone_actor(runtime,entrant,admission,bound) && admission.bound && view_sent && obelisks && nprotect,"Actor zone admission failed");
        check(std::find(delivered.begin(),delivered.end(),corpse_appearance(corpse))!=delivered.end() &&
              std::find(delivered.begin(),delivered.end(),npc_appearance(npc))!=delivered.end(),"Actor admission omitted visible entities");
        check(runtime.users->detach(entrant.id()),"Actor zone detachment failed");entry_scene.unbind();
    }
    {
        ZoneEntryState entry;entry.bound=true;entry.character_id=actor.id();
        const std::array<std::uint8_t,12> xyz{0,0,0x80,0x3f,0,0,0x20,0xc0,1,0,0xc0,0x7f};
        for(auto motion:{0,1,0x7f,0x80,0xff})for(auto attack:{0,1,128})for(auto vehicle:{0,1,2,3}) {
            actor.attack_state().mode=0;actor.attack_state().motion=static_cast<std::uint8_t>(attack);actor.appearance().vehicle_mode=vehicle;actor.actions().vehicle_request=0xabcdef01;
            net::Packet packet{1,5,static_cast<std::uint8_t>(motion),0x5a,0xa5};packet.insert(packet.end(),xyz.begin(),xyz.end());
            VerticalMotionState state;MoveContinuationServices port;vertical_move_continuation(actor,entry,state,packet,port);
            net::Packet expected{1,5,42,0,0,0,static_cast<std::uint8_t>(motion|(attack?0x80:0)),0x5a,0xa5};expected.insert(expected.end(),xyz.begin(),xyz.end());
            check(port.packet==expected && state.last_tick==0x12345678 && state.motion==expected[6] && entry.direction==0xa55a && port.finished==1,"Original movement continuation differs");
            check(actor.appearance().vehicle_mode==(vehicle==1?0:vehicle) && actor.actions().vehicle_request==(vehicle==1?0U:0xabcdef01U),"Original movement vehicle state differs");
        }
        actor.attack_state().motion=0;actor.appearance().vehicle_mode=0;actor.actions().vehicle_request=0;
    }
    {
        ConfiguredMap map;map.settings.id=9;map.definition.size=160;RuntimeZone runtime;runtime.map=&map;
        runtime.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);runtime.users->attach(actor,0,0,{20,0,20});
        ZoneEntryState admission;admission.bound=true;admission.character_id=actor.id();admission.map=9;admission.cell={0,0};admission.position={20,0,20};
        auto& check_state=actor.movement_check();check_state={};check_state.previous=check_state.back={20,0,20};check_state.check_time=5000;
        ViewTransport view(actor);MovePacketServices port;VerticalMotionState motion;
        auto make_packet=[](float x){net::Packet p{1,5,0,0x34,0x12};for(float value:{x,0.F,20.F}){auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}return p;};
        auto distant=make_packet(60);check(vertical_move_packet(actor,runtime,admission,4,motion,distant,view,port)==VerticalMoveResult::distance_corrected && port.events==std::vector<std::string>{"stand","correct"},"Distance gate sequence differs");
        port.events.clear();auto packet=make_packet(45);check(vertical_move_packet(actor,runtime,admission,4,motion,packet,view,port)==VerticalMoveResult::moved,"Composed movement failed");
        check(port.events==std::vector<std::string>{"stand","speed","tick","tick","broadcast","finish"} && runtime.users->position(actor.id())->x==45 && admission.direction==0x1234 && port.broadcast.size()==21,"Composed movement order differs");
        check(runtime.users->detach(actor.id()),"Movement fixture detach failed");outgoing.clear();
    }
    {
        struct Session:std::enable_shared_from_this<Session> {
            std::shared_ptr<Actor> actor;ZoneEntryState entry;net::WorldChannel wire;std::vector<net::Packet> sent;PartyMembershipUser connection;std::unique_ptr<UserPriorityQueue> queue;ZoneLeaveShopState shop;
            WorldUser* world=nullptr;WorldAdmissionUser* admission_ptr=nullptr;VerticalDatabaseAckUser* ack=nullptr;VerticalMotionState* motion_ptr=nullptr;bool stopped=false;
            WorldUser* world_user(){return world;}PartyMembershipUser& party_user(){return connection;}WorldAdmissionUser& admission(){return *admission_ptr;}ZoneEntryState& zone_entry(){return entry;}
            VerticalDatabaseAckUser* database_ack_user(){return ack;}VerticalMotionState& motion(){return *motion_ptr;}net::WorldChannel& channel(){return wire;}bool closing() const{return stopped;}
        };
        struct RoutingWorld final:MultiWorld {
            std::function<void(PartyMembershipUser&,const net::Packet&)> route;
            void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override {
                sent.push_back(user.id);frames.push_back(buffer.frame);route(user,net::Packet(buffer.frame.begin()+2,buffer.frame.end()));release_party_membership_buffer(buffer,*this);
            }
        } pool;UserViewSharedBuffers buffers(pool);
        ConfiguredMap map;map.settings.id=18;map.definition.size=280;const auto geometry=map.definition.geometry();
        SnapshotCorpseServices cs;SnapshotMobServices ms;SnapshotNpcServices ns;SnapshotItemServices is;WorldCorpses corpses;
        RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(geometry,0);
        zone.corpses=std::make_unique<ZoneCorpses>(geometry,corpses,cs);zone.mobs=std::make_unique<ZoneMobs>(geometry,ms);
        zone.npcs=std::make_unique<ZoneNpcs>(geometry,ns);zone.items=std::make_unique<ZoneItems>(geometry,is);zone.environment=ZoneEnvironment::prepare(map);
        VerticalSessionRegistry<Session> registry;std::array<std::shared_ptr<Session>,3> sessions;
        for(unsigned i=0;i<3;++i) {
            auto owner=std::make_shared<Session>();owner->actor=std::make_shared<Actor>(100+i,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});
            owner->actor->account_id()=200+i;owner->connection.id=100+i;owner->connection.send_socket=1;owner->queue=std::make_unique<UserPriorityQueue>(pool);owner->entry.bound=true;owner->entry.character_id=100+i;owner->entry.map=18;
            owner->wire.send=[weak=std::weak_ptr<Session>(owner)](net::Packet packet){auto pin=weak.lock();check(bool(pin),"Channel lost session");pin->sent.push_back(std::move(packet));};
            registry.retain(owner,*owner->actor);sessions[i]=std::move(owner);
        }
        pool.route=[&](PartyMembershipUser& user,const net::Packet& packet){auto& session=*sessions.at(user.id-100);check(&session.connection==&user,"Wrong routed connection");session.wire.send(packet);};
        auto& source=*sessions[0];source.entry.cell={2,2};source.entry.position={110,0,81};
        zone.users->attach(*source.actor,2,2,source.entry.position);zone.users->attach(*sessions[1]->actor,0,2,{1,0,81});zone.users->attach(*sessions[2]->actor,5,2,{201,0,81});
        auto& movement_check=source.actor->movement_check();movement_check={};movement_check.previous=movement_check.back=source.entry.position;movement_check.check_time=5000;
        WorldAdmissionUser user;user.user_id=200;user.zone=&zone;user.location=4;VerticalMotionState motion;MovePacketServices services;
        UserPriorityViewState cadence;VerticalNpcInteraction npc;VerticalDuelMovement duel;MovePvpServices pvp(*source.actor,*sessions[1]->actor);MoveWorldOperations operations(services);
        {
            WorldUser world_user{*source.actor,user};std::recursive_mutex mutex;VerticalDatabaseAckUser connection{world_user,mutex};
            connection.socket_open=true;user.logout_type=2;user.logout_tick=55;source.actor->presentation_motion(1);source.actor->attack_state().recovery=0;
            RetainedDuelWorld duel_backend(*zone.users);VerticalGuildDuelMovement guild_duel;MoveGuildServices guild_services;
            std::array<float,5> speed_table{3,4,5,6,7};source.wire.disconnect_with_reason=[](std::uint32_t){throw std::logic_error("Unexpected close");};
            source.world=&world_user;source.admission_ptr=&user;source.ack=&connection;source.motion_ptr=&motion;
            auto owner_lease=std::make_shared<int>(1);std::weak_ptr<int> weak_lease=owner_lease;
            using Composition=VerticalMoveRuntimeComposition<MovementGameplayFixture<Session>,int,Session>;
            struct Ports {std::function<std::shared_ptr<MovementGameplayFixture<Session>>(Session&,RuntimeZone&)> gameplay;
                std::function<void(Session&,WorldAdmissionUser&,int)> configure_admission;
                std::function<void(Session&,std::optional<std::uint32_t>,std::function<void()>)> disconnect;} ports;
            std::function<void()> disconnect_complete;bool disconnected=false;
            ports.disconnect=[&](Session& session,std::optional<std::uint32_t>,std::function<void()> done){check(&session==&source,"Wrong disconnect owner");disconnect_complete=std::move(done);};
            bool close_on_configure=false;unsigned configure_calls=0;
            ports.configure_admission=[&](Session& session,WorldAdmissionUser& admission,int value){check(&session==&source && &admission==&user && value==17,"Movement configuration lost its real owner");++configure_calls;if(close_on_configure)session.stopped=true;};
            bool close_on_acquire=false,reenter_acquire=false,reentry_rejected=false;
            std::shared_ptr<Composition> composition;
            composition=Composition::create(0,registry,[&](Session& session)->Composition::Resources {
                if(close_on_acquire)session.stopped=true;
                if(reenter_acquire) {
                    try{composition->resources(session);}catch(const std::logic_error&){reentry_rejected=true;}
                }
                return {owner_lease,pool,session.connection,session.shop};},
                [&](Session&,RuntimeZone&,Composition::Directory& directory)->VerticalMoveDirectoryServices<Session> {
                    return {owner_lease,duel_backend,guild_services,[](Actor&){},[]{return std::array<std::uint8_t,100>{};},buffers,
                        [](Session& session)->PartyMembershipUser&{return session.connection;},[&directory,&source](Session& session)->UserPriorityQueue&{
                            auto retained=directory.find(*session.actor);return retained?retained->queue():*session.queue;},
                        [](Session& session)->ZoneLeaveShopState&{return session.shop;},speed_table,[]{return 100U;},[]{return std::array<std::uint8_t,4>{};}};
                },[]{});
            composition->install(ports);
            close_on_acquire=true;bool acquire_rejected=false;try{composition->resources(source);}catch(const std::logic_error&){acquire_rejected=true;}
            check(acquire_rejected && composition->directory().size()==0,"Closing during resource acquisition attached stale state");source.stopped=false;close_on_acquire=false;
            close_on_configure=true;ports.configure_admission(source,user,17);
            check(source.stopped && composition->directory().size()==0,"Closed admission attached movement resources");source.stopped=false;close_on_configure=false;
            source.connection.send_socket=-1;bool missing_socket=false;try{ports.configure_admission(source,user,17);}catch(const std::logic_error& error){missing_socket=std::string(error.what())=="Movement admission lacks a live connection projection";}
            check(missing_socket && composition->directory().size()==0,"Movement admission accepted missing connection projection");source.connection.send_socket=1;
            reenter_acquire=true;ports.configure_admission(source,user,17);auto resources=composition->resources(source);reenter_acquire=false;
            check(reentry_rejected && composition->directory().size()==1 && composition->resources(source)==resources,
                "Reentrant movement acquisition replaced or duplicated resources");
            check(configure_calls==3,"Movement admission skipped underlying configuration");
            auto foreign=std::make_shared<Session>();foreign->world=&world_user;
            bool foreign_resource=false,foreign_drain=false;
            try{composition->resources(*foreign);}catch(const std::logic_error&){foreign_resource=true;}
            try{composition->drain(*foreign);}catch(const std::logic_error&){foreign_drain=true;}
            check(foreign_resource && foreign_drain && composition->directory().size()==1,"Foreign session accessed registered movement resources");
            source.stopped=true;bool stopped_resource=false;try{composition->resources(source);}catch(const std::logic_error&){stopped_resource=true;}
            check(stopped_resource,"Closing session acquired movement resources");source.stopped=false;
            WorldAdmissionUser observer_admission;observer_admission.user_id=202;observer_admission.zone=&zone;observer_admission.location=4;
            WorldUser observer_world{*sessions[2]->actor,observer_admission};sessions[2]->world=&observer_world;
            auto observer_resources=composition->resources(*sessions[2]);
            auto gameplay=ports.gameplay(source,zone);check(composition->directory().find(*source.actor)==resources,"Gameplay replaced admitted resources");
            auto bound=gameplay->movement;owner_lease.reset();
            check(!weak_lease.expired(),"Movement owner did not retain dependencies");
            net::Packet far{1,5,0,0,0};for(float value:{180.F,0.F,81.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)far.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
            outgoing.clear();check(bound->receive(far)==VerticalMoveResult::distance_corrected && user.logout_type==0 && user.logout_tick==0 && source.actor->presentation_motion()==0 && source.actor->attack_state().recovery==1,"Bound movement stand/correction differs");
            check(pool.sent==std::vector<std::uint32_t>{100} && !pool.frames.empty() && pool.frames.back()[2]==0x0b && pool.frames.back()[3]==2 && !outgoing.empty() && outgoing.back()[0]==2 && outgoing.back()[1]==5,"Bound movement correction delivery differs");
            pool.sent.clear();pool.frames.clear();source.sent.clear();outgoing.clear();
            source.shop.mode=1;source.shop.customer_count=1;source.shop.customer_ids[0]=102;sessions[2]->shop.visited_owner=100;source.actor->presentation_motion(1);
            net::Packet step{1,5,0,0x34,0x12};for(float value:{140.F,0.F,81.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)step.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
            check(bound->receive(step)==VerticalMoveResult::moved && source.entry.cell.x==3 && source.shop.mode==0 && sessions[2]->shop.visited_owner==0,"Complete retained movement failed to close shop and move");
            check(pool.sent==std::vector<std::uint32_t>{101,100,101,102,102,100} && observer_resources->queue().size()==1 && outgoing==std::vector<net::Packet>{{0x0c,0x23},{2,0x23}},"Complete retained movement lost shop or visibility delivery");
            sessions[2]->connection.send_socket=-1;bool closed_drain=false;try{composition->drain(*sessions[2]);}catch(const std::logic_error&){closed_drain=true;}
            check(closed_drain && observer_resources->queue().size()==1,"Closed movement drain consumed pending queue");sessions[2]->connection.send_socket=1;
            bool nested_drain=false;auto observer_send=sessions[2]->wire.send;
            sessions[2]->wire.send=[&](net::Packet packet){try{composition->drain(*sessions[2]);}catch(const std::logic_error& error){nested_drain=std::string(error.what())=="Movement priority drain is already active";}observer_send(std::move(packet));};
            check(composition->drain(*sessions[2])==1 && observer_resources->queue().size()==0 && !sessions[2]->sent.empty() && sessions[2]->sent.back()[0]==1 && sessions[2]->sent.back()[1]==5,"Passive observer did not receive queued movement on its channel");
            check(nested_drain,"Nested priority drain was accepted");sessions[2]->wire.send=std::move(observer_send);
            zone.users->move(source.actor->id(),2,2,{110,0,81});source.entry.cell={2,2};source.entry.position={110,0,81};
            movement_check={};movement_check.previous=movement_check.back=source.entry.position;movement_check.check_time=5000;motion={};resources->cadence()={};
            for(auto& session:sessions)session->sent.clear();pool.sent.clear();pool.frames.clear();outgoing.clear();
            source.ack=nullptr;bool changed_rejected=false;try{bound->receive(step);}catch(const std::logic_error&){changed_rejected=true;}
            check(changed_rejected,"Movement accepted a replaced acknowledgment owner");source.ack=&connection;
            source.stopped=true;bool stopped_rejected=false;try{bound->receive(step);}catch(const std::logic_error&){stopped_rejected=true;}
            check(stopped_rejected,"Closing movement session accepted a packet");source.stopped=false;bound.reset();gameplay.reset();
            source.world=nullptr;bool missing_before=false;try{ports.disconnect(source,{},[&]{disconnected=true;});}catch(const std::logic_error&){missing_before=true;}
            check(missing_before && !disconnect_complete && !disconnected && composition->directory().size()==2,"Movement disconnect skipped an attached actor");source.world=&world_user;
            ports.disconnect(source,{},[&]{disconnected=true;});check(bool(disconnect_complete) && !disconnected,"Movement disconnect completed early");
            source.world=nullptr;bool missing_owner=false;try{disconnect_complete();}catch(const std::logic_error&){missing_owner=true;}
            check(missing_owner && !disconnected && composition->directory().size()==2,"Movement disconnect lost its actor and completed");source.world=&world_user;
            bool open_rejected=false;try{disconnect_complete();}catch(const std::logic_error&){open_rejected=true;}check(open_rejected && !disconnected,"Movement disconnect accepted open connection");
            auto in_flight=buffers.allocate(net::Packet{1,5});check(buffers.enqueue_priority(source.connection,*in_flight,resources->queue(),7,0),"Disconnect drain setup failed");in_flight.reset();
            bool active_rejected=false;auto source_send=source.wire.send;
            source.wire.send=[&](net::Packet packet){source.connection.send_socket=-1;try{disconnect_complete();}catch(const std::logic_error& error){active_rejected=std::string(error.what())=="Movement disconnect completed during priority drain";}source_send(std::move(packet));};
            check(composition->drain(source)==1 && active_rejected && !disconnected && composition->directory().size()==2,"Disconnect escaped active priority drain");source.wire.send=std::move(source_send);
            bool flush_rejected=false;composition->with_priority_flush(source,[&]{try{disconnect_complete();}catch(const std::logic_error& error){flush_rejected=std::string(error.what())=="Movement disconnect completed during priority drain";}});
            check(flush_rejected && !disconnected && composition->directory().size()==2,"Disconnect escaped world priority flush");
            auto visit1=composition->retain_send_visit(source,source.shared_from_this());auto visit2=composition->retain_send_visit(source,source.shared_from_this());
            auto rejects_visit=[&]{try{disconnect_complete();}catch(const std::logic_error& error){return std::string(error.what())=="Movement disconnect completed during world send visit";}return false;};
            check(rejects_visit() && !disconnected,"Disconnect escaped world send visit");visit1.reset();
            check(rejects_visit() && !disconnected,"Disconnect escaped nested world send visit");visit2.reset();
            source.connection.send_socket=-1;disconnect_complete();check(disconnected && !composition->directory().find(*source.actor),"Movement disconnect did not close directory");
            unsigned closed_accesses=0;auto rejects=[&](auto access){try{access();}catch(const std::logic_error&){++closed_accesses;}};
            rejects([&]{(void)resources->pool();});rejects([&]{(void)resources->queue();});rejects([&]{(void)resources->connection();});
            rejects([&]{(void)resources->shop();});rejects([&]{(void)resources->cadence();});rejects([&]{(void)resources->npc();});
            rejects([&]{(void)resources->duel();});rejects([&]{(void)resources->guild_duel();});rejects([&]{(void)resources->close_queue();});
            check(closed_accesses==9,"Retired movement resource handle exposed stale owners");
            disconnect_complete={};resources.reset();sessions[2]->connection.send_socket=-1;check(composition->directory().close(*sessions[2]->actor)==0,"Observer queue remained pending");observer_resources.reset();sessions[2]->connection.send_socket=1;sessions[2]->world=nullptr;
            ports.gameplay={};ports.configure_admission={};ports.disconnect={};composition.reset();check(weak_lease.expired(),"Movement dependencies outlived binding");source.connection.send_socket=1;source.world=nullptr;source.admission_ptr=nullptr;source.ack=nullptr;source.motion_ptr=nullptr;
        }
        {
            WorldAdmissionUser first_user,second_user;first_user.user_id=201;first_user.location=4;first_user.zone=&zone;
            second_user.user_id=202;second_user.location=4;second_user.zone=&zone;
            WorldUser source_world{*source.actor,user},first_world{*sessions[1]->actor,first_user},second_world{*sessions[2]->actor,second_user};
            WorldUsers index({[](WorldUser&){},[](WorldUser&){}});index.enter(source_world);index.enter(first_world);index.enter(second_world);
            source.world=&source_world;source.admission_ptr=&user;source.motion_ptr=&motion;sessions[1]->world=&first_world;sessions[2]->world=&second_world;
            std::recursive_mutex ack_mutex;VerticalDatabaseAckUser ack{source_world,ack_mutex};ack.socket_open=true;source.ack=&ack;
            RetainedDuelWorld duel_backend(*zone.users);MoveGuildServices guild_services;std::array<float,5> speeds{3,4,5,6,7};
            auto owner_lease=std::make_shared<int>(1);
            auto shared=VerticalMoveSharedWorld<Session>::create(owner_lease,registry,index,pool,
                [](Session& session)->ZoneLeaveShopState&{return session.shop;},duel_backend,guild_services,
                [](Actor&){throw std::logic_error("Unexpected guild refresh");},[]{return std::array<std::uint8_t,100>{};},speeds,
                []{return 100U;},[]{return std::array<std::uint8_t,4>{};});
            using Composition=VerticalMoveRuntimeComposition<MovementGameplayFixture<Session>,int,Session>;
            auto composition=shared->composition<MovementGameplayFixture<Session>>(0,[]{});std::weak_ptr<Composition> weak_composition=composition;
            bool duplicate=false;try{(void)shared->composition<MovementGameplayFixture<Session>>(0,[]{});}catch(const std::logic_error&){duplicate=true;}
            check(duplicate,"Shared movement world accepted multiple compositions");
            struct Ports {std::function<std::shared_ptr<MovementGameplayFixture<Session>>(Session&,RuntimeZone&)> gameplay;
                std::function<void(Session&,std::optional<std::uint32_t>,std::function<void()>)> disconnect;} ports;
            ports.disconnect=[](Session&,std::optional<std::uint32_t>,std::function<void()>){throw std::logic_error("Unexpected disconnect");};
            composition->install(ports);auto gameplay=ports.gameplay(source,zone);
            auto source_state=composition->resources(source);check(composition->directory().size()==1,"Shared movement attached passive users early");
            net::Packet step{1,5,0,0x34,0x12};for(float value:{140.F,0.F,81.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)step.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
            movement_check={};movement_check.previous=movement_check.back=source.entry.position;movement_check.check_time=5000;
            sessions[2]->connection.send_socket=-1;sessions[2]->stopped=true;
            check(gameplay->movement->receive(step)==VerticalMoveResult::moved && composition->directory().size()==1,"Closed passive recipient was not skipped");
            zone.users->move(source.actor->id(),2,2,{110,0,81});source.entry.cell={2,2};source.entry.position={110,0,81};motion={};source_state->cadence()={};
            movement_check={};movement_check.previous=movement_check.back=source.entry.position;movement_check.check_time=5000;
            sessions[2]->connection.send_socket=1;sessions[2]->stopped=false;sessions[2]->sent.clear();
            check(gameplay->movement->receive(step)==VerticalMoveResult::moved,"Shared movement rejected open recipient");
            auto observer_state=composition->directory().find(*sessions[2]->actor);
            check(observer_state && observer_state->queue().size()==1 && composition->directory().size()==2,"Passive recipient queue was not retained on first use");
            check(composition->drain(*sessions[2])==1 && observer_state->queue().size()==0 && !sessions[2]->sent.empty() && sessions[2]->sent.back()[0]==1 && sessions[2]->sent.back()[1]==5,"Passive recipient did not receive movement");
            gameplay.reset();source.connection.send_socket=-1;sessions[2]->connection.send_socket=-1;
            check(composition->directory().close(*source.actor)==0 && composition->directory().close(*sessions[2]->actor)==0,"Shared movement cleanup left queued buffers");
            source_state.reset();observer_state.reset();ports.gameplay={};ports.disconnect={};composition.reset();
            check(weak_composition.expired(),"Passive recipient queue created a composition cycle");
            duplicate=false;try{(void)shared->composition<MovementGameplayFixture<Session>>(0,[]{});}catch(const std::logic_error&){duplicate=true;}
            check(duplicate,"Shared movement world replaced its retired composition");shared.reset();
            source.connection.send_socket=1;sessions[2]->connection.send_socket=1;
            index.leave(source_world);index.leave(first_world);index.leave(second_world);source.world=nullptr;sessions[1]->world=nullptr;sessions[2]->world=nullptr;source.admission_ptr=nullptr;source.motion_ptr=nullptr;source.ack=nullptr;
            zone.users->move(source.actor->id(),2,2,{110,0,81});source.entry.cell={2,2};source.entry.position={110,0,81};motion={};
            movement_check={};movement_check.previous=movement_check.back=source.entry.position;movement_check.check_time=5000;
            for(auto& session:sessions)session->sent.clear();pool.sent.clear();pool.frames.clear();outgoing.clear();
        }
        VerticalMoveRetainedWorld<Session> movement(registry,source,*source.actor,zone,user,source.entry,motion,cadence,npc,duel,pvp,[](Actor&){},buffers,
            [](Session& session)->PartyMembershipUser&{return session.connection;},[](Session& session)->UserPriorityQueue&{return *session.queue;},operations,
            [](Session& session)->ZoneEntryState&{return session.entry;},[](Session& session)->net::WorldChannel&{return session.wire;},[]{return std::array<std::uint8_t,4>{};});
        net::Packet packet{1,5,0,0x34,0x12};for(float value:{140.F,0.F,81.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
        services.on_stand=[]{throw std::runtime_error("stand failed");};bool failed=false;
        try{movement.receive(packet);}catch(const std::runtime_error&){failed=true;}check(failed,"Movement callback failure was swallowed");
        bool nested_rejected=false;services.on_stand=[&]{try{movement.receive(packet);}catch(const std::logic_error&){nested_rejected=true;}};
        check(movement.receive(packet)==VerticalMoveResult::moved && nested_rejected && source.entry.cell.x==3,"Retained movement reentry or crossing differs");
        services.on_stand={};
        check(sessions[1]->sent==std::vector<net::Packet>{user_view_leave_packet(100)},"Old retained channel missed leave");
        check(sessions[2]->sent.size()==2 && sessions[2]->sent[0][0]==1 && sessions[2]->sent[0][1]==2 && sessions[2]->sent[1][0]==0x1c && sessions[2]->sent[1][1]==5,"New retained channel missed appearance or speed");
        check(!source.sent.empty() && source.sent.front()==user_view_leave_packet(101),"Moving session missed cell snapshot");
        check(pool.sent==std::vector<std::uint32_t>{101,102,102,100} && sessions[2]->queue->size()==1 && pvp.events==std::vector<std::string>{"guild"},"Movement lost shared visibility, priority or interaction delivery");
        VerticalMovePriorityChannel<Session> drain(registry,pool,[](Session& session)->UserPriorityQueue&{return *session.queue;},[](Session& session)->net::WorldChannel&{return session.wire;});
        check(drain.drain(*sessions[2]->actor)==1 && drain.drain(*sessions[2]->actor)==0,"Movement priority queue did not drain");
        check(sessions[2]->sent.size()==3 && sessions[2]->sent.back()[0]==1 && sessions[2]->sent.back()[1]==5,"Movement priority channel missed 0501");
        {
            pool.sent.clear();pool.frames.clear();
            VerticalMoveShopShared<Session> shop_services(*source.actor,zone,registry,buffers,
                [](Session& session)->PartyMembershipUser&{return session.connection;},[](Session& session)->ZoneLeaveShopState&{return session.shop;});
            source.shop.mode=1;source.shop.customer_count=1;source.shop.customer_ids[0]=sessions[2]->actor->id();sessions[2]->shop.visited_owner=source.actor->id();
            WorldUser shop_user{*source.actor,user};std::recursive_mutex shop_mutex;VerticalDatabaseAckUser shop_connection{shop_user,shop_mutex};
            source.actor->presentation_motion(1);outgoing.clear();vertical_move_status_stand(*source.actor,shop_connection,source.shop,shop_services);
            check(source.actor->presentation_motion()==0 && source.shop.mode==0 && source.shop.customer_count==0 && sessions[2]->shop.visited_owner==0 && outgoing==std::vector<net::Packet>{{0x0c,0x23},{2,0x23}},"Retained shop shutdown did not notify both actors");
            const net::Packet closed{0x23,5,100,0,0,0,0};
            net::Packet framed{9,0};framed.insert(framed.end(),closed.begin(),closed.end());
            check(pool.sent==std::vector<std::uint32_t>{100,102} && pool.frames==std::vector<net::Packet>{framed,framed} && std::none_of(pool.used.begin(),pool.used.end(),[](bool active){return active;}),"Retained shop view reached wrong cells or leaked buffers");outgoing.clear();
        }
        for(auto& owner:sessions){check(zone.users->detach(owner->actor->id()),"Retained fixture detach failed");check(registry.release(owner->actor->account_id(),*owner),"Retained fixture release failed");}
        bool rejected=false;try{movement.receive(packet);}catch(const std::logic_error&){rejected=true;}check(rejected,"Released movement session accepted packet");
    }
    {
        const auto previous_account=actor.account_id();const auto previous_recovery=actor.attack_state().recovery;
        WorldAdmissionUser admission;actor.account_id()=201;admission.user_id=201;WorldUser user{actor,admission};std::recursive_mutex mutex;
        VerticalDatabaseAckUser connection{user,mutex};StandShopServices shop_services;ZoneLeaveShopState shop;
        for(unsigned motion:{0U,1U,2U})for(unsigned logout:{0U,2U})for(bool connected:{false,true})for(unsigned transfer:{0U,1U}) {
            actor.presentation_motion(motion);actor.attack_state().recovery=-3;admission.logout_type=logout;admission.logout_tick=55;
            connection.socket_open=connected;connection.transfer_state=transfer;shop.mode=0;
            vertical_move_status_stand(actor,connection,shop,shop_services);
            const bool clear=motion==1 && logout && connected && transfer!=1;
            check(actor.presentation_motion()==(motion==1?0:motion) && actor.attack_state().recovery==(motion==1?1:-3) &&
                admission.logout_type==(clear?0U:logout) && admission.logout_tick==(clear?0U:55U),"Original StatusStand state differs");
        }
        actor.presentation_motion(1);actor.attack_state().recovery=7;shop.mode=1;shop.customer_count=0;outgoing.clear();
        vertical_move_status_stand(actor,connection,shop,shop_services);
        check(shop.mode==0 && actor.attack_state().recovery==7 && outgoing==std::vector<net::Packet>{{2,0x23}},"StatusStand did not close active shop");
        actor.presentation_motion(0);actor.account_id()=previous_account;actor.attack_state().recovery=previous_recovery;outgoing.clear();
    }
    {
        const std::array<float,5> table{3,4,5,6,7};
        for(auto vehicle:{0,1,2,3,4,-1})for(auto speed:{INT32_MIN,-1,0,1,2,3,4,5,INT32_MAX}) {
            actor.appearance().vehicle_mode=vehicle;actor.status().skills.equipment.move_speed=speed;
            const float expected=vehicle==2?8.F:vehicle==3?10007.F:table[static_cast<std::size_t>(std::clamp(speed,0,4))];
            check(vertical_movement_selected_speed(actor,table)==expected,"Original movement speed selection differs");
        }
        actor.appearance().vehicle_mode=0;actor.status().skills.equipment.move_speed=0;
    }
    {
        for(int mode:{0,1,2,3,4})for(unsigned previous:{0U,99U})for(int target:{0,1,2})for(bool self:{false,true}) {
            auto& attack=actor.attack_state();attack.mode=mode;attack.previous_tick=previous;attack.next_tick=77;attack.resume_tick=88;
            actor.status().current_target_type=target;actor.status().current_target=self?actor.id():43;actor.actions().previous_target_type=2;actor.actions().previous_target=99;
            unsigned calls=0;vertical_status_move(actor,[&]{++calls;return 0x12345678U;});
            const bool restored=mode==3 && target==1 && self;
            const int expected_type=mode==3 && !previous?0:restored?2:target;
            const unsigned expected_target=restored?99:self?actor.id():43;
            check(attack.mode==(mode==3?(previous?2:0):mode) && attack.next_tick==(mode==3?(previous?1U:0U):77U) &&
                  attack.resume_tick==(mode==3 && previous?0x12345678U:88U) && actor.status().current_target_type==expected_type && actor.status().current_target==expected_target && calls==unsigned(mode==3 && previous),"Original StatusMove differs");
        }
        actor.attack_state().mode=0;actor.status().current_target_type=0;
    }
    {
        for(bool present:{false,true})for(std::uint32_t count:{0U,1U,2U,3U,4U,0x7fffffffU,0xffffffffU}) {
            UserPriorityViewState state{count};PriorityViewServices services;services.present=present;
            const auto next=count+1U;const bool wide=present && std::bit_cast<std::int32_t>(next)>=4;
            check(user_priority_view(actor,state,{1,5},0,7,services)==wide && state.counter==(present?(wide?0U:next):count),"Original priority view cadence differs");
            check(services.events==(present?(wide?std::vector<int>{1,2}:std::vector<int>{1}):std::vector<int>{}),"Original priority view delivery differs");
        }
    }
    {
        Actor special(43,{}, {},{},scene,{definitions,enchant,mobs});Actor edge(44,{}, {},{},scene,{definitions,enchant,mobs});Actor far(45,{}, {},{},scene,{definitions,enchant,mobs});
        ZoneUsers recipients(1,1,0);recipients.attach(actor,0,0,{300,0,100});recipients.attach(special,0,0,{300,0,100});recipients.attach(edge,0,0,{180,0,100});recipients.attach(far,0,0,{180.1F,0,100});
        ViewBuffer buffer({1,5});PriorityCellServices services;user_priority_cell(recipients,{0,0},buffer,80,42,43,7,services);
        check(services.immediate_ids==std::vector<std::uint32_t>{42,43} && services.queued==std::vector<std::pair<std::uint32_t,float>>{{44,80}},"Original priority cell recipient selection differs");
        services.immediate_ids.clear();services.queued.clear();user_priority_cell_ring(recipients,{0,0},buffer,80,120,42,43,7,services);
        check(services.immediate_ids==std::vector<std::uint32_t>{42,43} && services.queued.size()==2 && services.queued[0].first==44 && services.queued[0].second==80 && services.queued[1].first==45,"Original priority ring boundaries differ");
        recipients.detach(42);recipients.detach(43);recipients.detach(44);recipients.detach(45);
    }
    {
        MultiWorld pool;UserViewSharedBuffers buffers(pool);PartyMembershipUser user;user.send_socket=1;
        auto a=buffers.allocate(net::Packet{1}),b=buffers.allocate(net::Packet{2}),c=buffers.allocate(net::Packet{3});
        UserPriorityQueue queue(pool);check(buffers.enqueue_priority(user,*a,queue,7,15.9F) && buffers.enqueue_priority(user,*b,queue,6,79.9F) &&
            buffers.enqueue_priority(user,*c,queue,7,3.9F) && buffers.enqueue_priority(user,*a,queue,7,3.1F),"Priority queue insertion failed");
        check(queue.size()==4 && queue.bytes()==12,"Priority queue accounting differs");
        for(auto expected:{2,3,1,1}){auto* next=queue.take_first();check(next && next->frame[2]==expected,"Priority queue order/stability differs");release_party_membership_buffer(*next,pool);}
        check(queue.bytes()==0 && !queue.take_first(),"Priority queue did not drain");user.send_socket=-1;
        check(!buffers.enqueue_priority(user,*a,queue,7,3) && queue.size()==0,"Disconnected user queued priority packet");
        user.send_socket=1;auto large=buffers.allocate(net::Packet(2046,1));check(buffers.enqueue_priority(user,*large,queue,7,3) && queue.bytes()==2048,"Priority queue limit setup failed");
        check(!buffers.enqueue_priority(user,*a,queue,7,3) && queue.size()==1,"Priority queue byte limit ignored");
        auto* next=queue.take_first();check(next && queue.bytes()==0,"Priority queue limit drain failed");release_party_membership_buffer(*next,pool);
        for(int i=0;i<130;++i)check(buffers.enqueue_priority(user,*a,queue,7,10),"Priority batch setup failed");
        auto first=queue.take_batch();check(first && first->size()==128 && queue.size()==2 && queue.bytes()==6,"Priority first batch differs");
        std::vector<net::Packet> delivered;net::WorldChannel channel;channel.send=[&](net::Packet p){delivered.push_back(std::move(p));};
        deliver_priority_batch(*first,pool,channel);check(first->empty() && delivered.size()==128 && delivered.front()==net::Packet{1},"Priority batch channel delivery differs");
        auto second=queue.take_batch();check(second && second->size()==2 && queue.size()==0 && queue.bytes()==0,"Priority second batch differs");
        deliver_priority_batch(*second,pool,channel);check(delivered.size()==130 && !queue.take_batch(),"Priority final batch failed");
    }
    {
        struct RetainedPriority {std::shared_ptr<Actor> actor;PartyMembershipUser connection;std::unique_ptr<UserPriorityQueue> queue;net::WorldChannel channel;std::vector<net::Packet> received;};
        MultiWorld pool;UserViewSharedBuffers buffers(pool);VerticalSessionRegistry<RetainedPriority> registry;ZoneUsers zone(5,5,0);
        std::array<std::shared_ptr<RetainedPriority>,2> owners;
        for(unsigned i=0;i<2;++i) {
            auto owner=std::make_shared<RetainedPriority>();owner->actor=std::make_shared<Actor>(300+i,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});
            owner->actor->account_id()=400+i;owner->connection.id=300+i;owner->connection.send_socket=1;owner->queue=std::make_unique<UserPriorityQueue>(pool);
            registry.retain(owner,*owner->actor);zone.attach(*owner->actor,1,1,{40.F+30.F*i,0,40});owners[i]=std::move(owner);
        }
        {
            WorldUsers world({[](WorldUser&){},[](WorldUser&){}});WorldAdmissionUser a0,a1;a0.user_id=400;a1.user_id=401;
            WorldUser w0{*owners[0]->actor,a0},w1{*owners[1]->actor,a1};world.enter(w0);world.enter(w1);
            for(auto& owner:owners)owner->channel.send=[weak=std::weak_ptr<RetainedPriority>(owner)](net::Packet p){auto pin=weak.lock();check(bool(pin),"Channel lost session");pin->received.push_back(std::move(p));};
            VerticalMoveChannelSendWorld<RetainedPriority> send_world(pool,world,registry,
                [](RetainedPriority& owner)->PartyMembershipUser&{return owner.connection;},[](RetainedPriority& owner)->net::WorldChannel&{return owner.channel;});
            UserViewSharedBuffers bound_buffers(send_world);auto shared=bound_buffers.allocate(net::Packet{0x23,5,1});check(bool(shared),"Channel buffer allocation failed");
            bound_buffers.enqueue(owners[0]->connection,*shared);bound_buffers.enqueue(owners[1]->connection,*shared);shared.reset();
            check(owners[0]->received==std::vector<net::Packet>{{0x23,5,1}} && owners[1]->received==owners[0]->received && std::none_of(pool.used.begin(),pool.used.end(),[](bool value){return value;}),"Channel send did not deliver or release shared buffer");
            auto foreign=owners[1]->connection;auto bad=bound_buffers.allocate(net::Packet{1,2});bool rejected=false;
            try{bound_buffers.enqueue(foreign,*bad);}catch(const std::logic_error&){rejected=true;}check(rejected,"Foreign connection projection accepted");bad.reset();
            check(std::none_of(pool.used.begin(),pool.used.end(),[](bool value){return value;}),"Failed channel send leaked buffer");world.leave(w0);world.leave(w1);
        }
        VerticalMovePriorityRetained<RetainedPriority> retained(registry,*owners[0]->actor,zone,5,5,buffers,
            [](RetainedPriority& owner)->PartyMembershipUser&{return owner.connection;},[](RetainedPriority& owner)->UserPriorityQueue&{return *owner.queue;});
        VerticalDuelMovement duel;MovePvpServices pvp(*owners[0]->actor,*owners[1]->actor);unsigned guild_refreshes=0;
        VerticalMoveRetainedInteractions<RetainedPriority> interactions(registry,zone,duel,pvp,[&](Actor& actor){check(&actor==owners[0]->actor.get(),"Wrong guild refresh actor");++guild_refreshes;});
        Npc npc;npc.position={55,0,40};npc.users_using=2;VerticalNpcInteraction active{&npc};
        vertical_move_interactions(*owners[0]->actor,active,true,interactions);
        check(!active.npc && npc.users_using==1 && guild_refreshes==1 && pvp.events==std::vector<std::string>{"guild"},"Retained movement interactions differ");
        WorldAdmissionUser admission;admission.user_id=owners[0]->actor->account_id();WorldUser world{*owners[0]->actor,admission};std::recursive_mutex mutex;
        VerticalDatabaseAckUser connection{world,mutex};ZoneLeaveShopState shop;StandShopServices shop_services;
        ZoneEntryState entry;entry.bound=true;entry.character_id=owners[0]->actor->id();entry.direction=0x1234;
        std::array<float,5> speeds{3,4,5,6,7};std::uint32_t close_reason=0;net::WorldChannel close_channel;
        close_channel.disconnect_with_reason=[&](std::uint32_t value){close_reason=value;};
        VerticalMoveBoundOperations operations(*owners[0]->actor,entry,connection,shop,shop_services,retained.view(),speeds,[]{return 123U;},close_channel);
        owners[0]->actor->presentation_motion(1);owners[0]->actor->attack_state().recovery=0;operations.status_stand(*owners[0]->actor);
        check(owners[0]->actor->presentation_motion()==0 && owners[0]->actor->attack_state().recovery==1 && operations.direction(*owners[0]->actor)==0x1234 && operations.tick_ms()==123,"Bound movement operations lost actor state");
        operations.send_view_action(*owners[0]->actor,{0x0b,2});
        check(pool.sent==std::vector<std::uint32_t>{300} && owners[1]->queue->size()==1,"Correction did not use retained priority view");
        auto correction=owners[1]->queue->take_batch();check(bool(correction),"Correction queue missing");
        for(auto* buffer:*correction)release_party_membership_buffer(*buffer,pool);pool.sent.clear();operations.close(*owners[0]->actor,9);
        check(close_reason==9,"Movement close did not reach channel");
        {
            RetainedDuelWorld duel_backend(zone);VerticalMoveDuelWorld<RetainedPriority> duel_world(zone,registry,duel_backend);
            VerticalGuildDuelMovement guild_state;MoveGuildServices guild;
            VerticalMoveRetainedPvp<RetainedPriority> retained_pvp(registry,duel_world,guild_state,guild,[]{return std::array<std::uint8_t,100>{};});
            auto& a=*owners[0]->actor;auto& b=*owners[1]->actor;a.relations().duel_state=5;a.relations().duel_opponent=b.id();b.relations().duel_state=5;
            VerticalDuelMovement duel_anchor{{0,0,0}};outgoing.clear();vertical_move_pvp(a,duel_anchor,retained_pvp);
            check(a.relations().duel_state==0 && b.relations().duel_state==0 && outgoing==std::vector<net::Packet>{{8,0x24,3},{8,0x24,2}} && duel_backend.lookups==0 && duel_backend.positions==0,"Retained PvP did not use canonical zone actors");
            check(!duel_world.find_duel_user(a,999),"Missing duel user lookup differs");
            outgoing.clear();
        }
        UserPriorityViewState cadence{};const net::Packet packet{1,5,42};
        check(!user_priority_view(*owners[0]->actor,cadence,packet,0,7,retained.view()),"Unexpected priority wide pass");
        check(pool.sent==std::vector<std::uint32_t>{300} && owners[1]->queue->size()==1,"Retained priority delivery lost immediate or queued recipient");
        auto batch=owners[1]->queue->take_batch();std::vector<net::Packet> received;net::WorldChannel channel;channel.send=[&](net::Packet p){received.push_back(std::move(p));};
        check(bool(batch),"Retained priority batch missing");deliver_priority_batch(*batch,pool,channel);check(received==std::vector<net::Packet>{packet},"Retained priority channel delivery differs");
        {
            auto retained_packet=buffers.allocate(net::Packet{0x31,0x77});check(bool(retained_packet),"Priority reset allocation failed");
            for(unsigned i=0;i<130;++i)check(buffers.enqueue_priority(owners[1]->connection,*retained_packet,*owners[1]->queue,7,10),"Priority reset queue setup failed");
            auto in_flight=owners[1]->queue->take_batch();check(in_flight && in_flight->size()==128 && owners[1]->queue->size()==2,"Priority reset extraction differs");
            VerticalMovePriorityChannel<RetainedPriority> resetter(registry,pool,[](RetainedPriority& owner)->UserPriorityQueue&{return *owner.queue;},[](RetainedPriority& owner)->net::WorldChannel&{return owner.channel;});
            check(resetter.reset_pending(*owners[1]->actor)==2 && resetter.reset_pending(*owners[1]->actor)==0 && owners[1]->queue->bytes()==0,"Priority reset did not release pending entries");
            check(in_flight->front()->references.load()==129,"Priority reset released transferred or owner references");
            deliver_priority_batch(*in_flight,pool,channel);check(in_flight->empty(),"Transferred priority batch did not drain");retained_packet.reset();
            check(std::none_of(pool.used.begin(),pool.used.end(),[](bool active){return active;}),"Priority reset leaked shared buffer");
        }
        for(auto& owner:owners){check(zone.detach(owner->actor->id()),"Priority fixture detach failed");check(registry.release(owner->actor->account_id(),*owner),"Priority fixture release failed");}
    }
    {
        MultiWorld pool;UserViewSharedBuffers buffers(pool);UserPriorityQueue queue(pool);SharedPriorityCell services(buffers,queue);
        Actor other(43,{}, {},{},scene,{definitions,enchant,mobs});ZoneUsers recipients(1,1,0);recipients.attach(actor,0,0,{0,0,0});recipients.attach(other,0,0,{30,0,0});
        auto shared=buffers.allocate(net::Packet{1,5,42});check(bool(shared),"Shared priority packet allocation failed");
        user_priority_cell(recipients,{0,0},*shared,80,42,0,7,services);
        check(pool.sent==std::vector<std::uint32_t>{42} && queue.size()==1 && queue.bytes()==5,"Priority cell delivery did not retain shared packet");
        shared.reset();check(pool.used[0],"Priority buffer released before queue drain");
        auto batch=queue.take_batch();check(batch && batch->size()==1,"Priority recipient batch missing");std::vector<net::Packet> received;
        net::WorldChannel channel;channel.send=[&](net::Packet packet){received.push_back(std::move(packet));};deliver_priority_batch(*batch,pool,channel);
        check(received==std::vector<net::Packet>{{1,5,42}} && !pool.used[0],"Priority recipient batch lost packet or ownership");
        user_priority_area(recipients,1,1,{2,5,42},0,80,false,42,0,7,services);
        auto area_batch=queue.take_batch();check(area_batch && area_batch->size()==1,"Priority area did not enqueue recipient");
        deliver_priority_batch(*area_batch,pool,channel);check(received.back()==net::Packet({2,5,42}) && !pool.used[0],"Priority area queue delivery differs");
        recipients.detach(42);recipients.detach(43);
    }
    {
        Actor edge(43,{}, {},{},scene,{definitions,enchant,mobs});Actor outer(44,{}, {},{},scene,{definitions,enchant,mobs});Actor beyond(45,{}, {},{},scene,{definitions,enchant,mobs});
        ZoneUsers zone(7,7,0);zone.attach(actor,2,2,{100,0,100});zone.attach(edge,4,2,{180,0,100});zone.attach(outer,5,2,{220,0,100});zone.attach(beyond,5,2,{220.1F,0,100});
        PriorityCellServices services;user_priority_area(zone,7,7,{1,5},0,80,false,42,0,7,services);
        check(services.immediate_ids==std::vector<std::uint32_t>{42} && services.queued==std::vector<std::pair<std::uint32_t,float>>{{43,80}},"Priority near area differs");
        services.immediate_ids.clear();services.queued.clear();user_priority_area(zone,7,7,{1,5},80,120,true,42,0,7,services);
        check(services.immediate_ids==std::vector<std::uint32_t>{42} && services.queued==std::vector<std::pair<std::uint32_t,float>>{{43,80},{44,120}},"Priority ring area differs");
        zone.detach(42);zone.detach(43);zone.detach(44);zone.detach(45);
    }
    {
        MultiWorld pool;UserViewSharedBuffers buffers(pool);UserPriorityQueue near(pool),far(pool);
        Actor edge(43,{}, {},{},scene,{definitions,enchant,mobs});Actor outer(44,{}, {},{},scene,{definitions,enchant,mobs});ZoneUsers zone(7,7,0);
        zone.attach(actor,2,2,{100,0,100});zone.attach(edge,4,2,{180,0,100});zone.attach(outer,5,2,{220,0,100});
        ThreePriorityCells services(buffers,zone,actor,near,far);UserPriorityZoneView view(zone,7,7,services);UserPriorityViewState counter;BoundMoveServices bound(counter,view);
        for(int i=0;i<4;++i) {
            check(bound.send_priority_view(actor,{1,5,42},7)==(i==3),"Priority cadence wide flag differs");
            check(near.size()==static_cast<std::size_t>(i==3?2:1) && far.size()==static_cast<std::size_t>(i==3?1:0),"Priority recipient cadence differs");
            while(auto* entry=near.take_first())release_party_membership_buffer(*entry,pool);
            while(auto* entry=far.take_first())release_party_membership_buffer(*entry,pool);
        }
        check(counter.counter==0 && pool.sent==std::vector<std::uint32_t>{42,42,42,42,42},"Priority immediate cadence differs");
        zone.detach(42);zone.detach(43);zone.detach(44);
    }
    {
        Actor peer(43,{}, {},{},scene,{definitions,enchant,mobs});Npc npc;MoveInteractionServices interaction_services(actor);
        for(float distance:{0.F,9.9F,10.F,10.1F,std::numeric_limits<float>::quiet_NaN()})for(bool present:{false,true})for(bool trading:{false,true}) {
            outgoing.clear();npc.position={distance,2,0};npc.users_using=2;VerticalNpcInteraction interaction{present?&npc:nullptr};
            actor.exchange()={};peer.exchange()={};if(trading){actor.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().mode=3;peer.exchange().peer=&actor;}
            interaction_services.other={distance,2,0};interaction_services.guild=interaction_services.pvp=0;
            vertical_move_interactions(actor,interaction,true,interaction_services);const bool apart=distance>10;
            check(interaction_services.guild==1 && interaction_services.pvp==1 && npc.users_using==(present && apart?1:2) && (interaction.npc==nullptr)==(!present || apart),"NPC interaction cancellation differs");
            check(outgoing==(trading && apart?std::vector<net::Packet>{{4,10,2},{4,10,3}}:std::vector<net::Packet>{}) && actor.exchange().mode==(trading && !apart?3:0) && peer.exchange().mode==(trading && !apart?3:0),"Trade interaction cancellation differs");
        }
        actor.exchange()={};peer.exchange()={};outgoing.clear();
    }
    {
        Actor peer(43,{}, {},{},scene,{definitions,enchant,mobs});MovePvpServices services(actor,peer);VerticalDuelMovement duel;
        for(int state:{0,1,2,5,6})for(float distance:{0.F,39.9F,40.F,40.1F,std::numeric_limits<float>::quiet_NaN()})for(bool found:{false,true}) {
            actor.relations().duel_state=state;actor.relations().duel_opponent=43;services.found=found;services.distance=distance;services.events.clear();outgoing.clear();
            vertical_move_pvp(actor,duel,services);const bool exit=state==5 && distance>40;
            const auto expected=exit?(found?std::vector<std::string>{"find","end-peer","end-self","guild"}:std::vector<std::string>{"find","end-self","guild"}):std::vector<std::string>{"guild"};
            const auto packets=exit?(found?std::vector<net::Packet>{{8,0x24,3},{8,0x24,2}}:std::vector<net::Packet>{{8,0x24,2}}):std::vector<net::Packet>{};
            check(services.events==expected && outgoing==packets,"Original movement PvP preparation differs");
        }
        actor.relations().duel_state=0;actor.relations().duel_opponent=0;outgoing.clear();
    }
    {
        for(unsigned enemy:{0U,123U})for(int mode:{-1,0})for(float distance:{0.F,40.F,40.1F,std::numeric_limits<float>::quiet_NaN()})
        for(int status:{0,3,4,5})for(int variant:{0,1,2}) {
            actor.relations().opposing_guild_id=enemy;VerticalGuildDuelMovement state{{0,0,0},mode};MoveGuildServices services;
            services.present=variant!=0;services.disappear=variant==2;services.status=status;
            vertical_move_guild_duel(actor,{distance,0,0},state,services);
            std::vector<std::string> expected;
            if(enemy && mode==-1 && distance>40 && variant) {expected.push_back("lock");if(variant==1 && (status==3 || status==4))expected.push_back("leave");expected.push_back("unlock");}
            check(services.events==expected,"Original guild movement lock/leave differs");
        }
        actor.relations().opposing_guild_id=0;
    }
    {
        for(unsigned count:{0U,1U,2U,3U})for(bool leader:{false,true})for(unsigned mode:{1U,2U})for(bool found:{false,true})for(unsigned enemy:{0U,99U}) {
            GuildDuelState guild{3,11,enemy,leader?42U:43U,count,nullptr},other{4,22,33,44,5,nullptr};GuildLeaveFixture world(other);world.found=found;
            VerticalGuildDuelMovement member{{},8,9};actor.relations().opposing_guild_id=123;actor.relations().guild_duel_ready=9;actor.status().current_target_type=4;services.calls.clear();
            guild_duel_leave(guild,actor,member,mode,world);const bool actual_other=found && enemy;const bool terminal=static_cast<std::int32_t>(count-1)<=0 || (leader && mode==1);
            std::vector<std::string> expected;if(enemy)expected.push_back("find");if(actual_other){expected.insert(expected.end(),{"lock","other-all","unlock"});}expected.push_back("own-all");
            if(terminal){if(actual_other)expected.insert(expected.end(),{"lock","other-members","other-reset","unlock"});expected.insert(expected.end(),{"own-members","own-reset"});}
            check(world.events==expected && guild.participants==count-1 && guild.status==(terminal?0:3),"Original guild duel leave order differs");
            if(!terminal)check(actor.relations().opposing_guild_id==0 && actor.status().current_target_type==0 && member.state==-1 && actor.relations().guild_duel_ready==0 && services.calls==std::vector<std::string>{"reset"},"Guild individual cleanup differs");
        }
        actor.relations().opposing_guild_id=0;actor.status().current_target_type=0;services.calls.clear();
    }
    {
        Actor other(43,{}, {},{},scene,{definitions,enchant,mobs});GuildMemberPorts ports;
        ports.allocate=[](const net::Packet&){return true;};ports.release=[]{};ports.send=[](GuildUser&,net::Packet){};
        GuildMembership guild(1,ports);GuildUser one,two;one.character_id=42;two.character_id=43;
        auto a=std::make_shared<GuildMember>(),b=std::make_shared<GuildMember>();a->user=&one;b->user=&two;
        guild.online.emplace(10,a);guild.online.emplace(20,nullptr);guild.online.emplace(30,b);
        GuildResetFixture fixture(actor,other);fixture.a.state=fixture.b.state=8;fixture.a.ready_flag=fixture.b.ready_flag=1;
        actor.relations().opposing_guild_id=other.relations().opposing_guild_id=123;actor.relations().guild_duel_ready=other.relations().guild_duel_ready=9;
        actor.status().current_target_type=other.status().current_target_type=4;services.calls.clear();guild_duel_reset_members(guild,fixture);
        check(actor.relations().opposing_guild_id==0 && actor.relations().guild_duel_ready==0 && actor.status().current_target_type==0 && fixture.a.state==-1 && fixture.a.ready_flag==0 &&
              other.relations().opposing_guild_id==123 && fixture.b.state==8 && services.calls==std::vector<std::string>{"reset"},"Guild reset did not stop at null record");
        actor.relations().opposing_guild_id=actor.relations().guild_duel_ready=0;actor.status().current_target_type=0;services.calls.clear();
    }
    {
        Actor second(43,{}, {},{},scene,{definitions,enchant,mobs}),third(44,{}, {},{},scene,{definitions,enchant,mobs});
        GuildMemberPorts ports;ports.allocate=[](const net::Packet&){return true;};ports.release=[]{};ports.send=[](GuildUser&,net::Packet){};
        for(int kind:{0,1})for(int status:{0,3})for(bool fail:{false,true})for(bool null_record:{false,true}) {
            GuildMembership membership(1,ports);GuildUser users[3];std::array<GuildMembership::Record,3> rows;
            for(unsigned i=0;i<3;++i){users[i].character_id=42+i;rows[i]=std::make_shared<GuildMember>();rows[i]->user=&users[i];membership.online.emplace(10+i,i==1 && null_record?nullptr:rows[i]);}
            GuildDuelState state;state.status=status;GuildBroadcastFixture fixture(actor,second,third);fixture.fail=fail;
            actor.relations().opposing_guild_id=1;second.relations().opposing_guild_id=0;third.relations().opposing_guild_id=1;
            fixture.states[0].ready_flag=0;fixture.states[1].ready_flag=fixture.states[2].ready_flag=1;
            guild_duel_broadcast(state,membership,{5,0x25,1},kind==0?GuildDuelAudience::ready:GuildDuelAudience::participating,fixture);
            std::vector<std::string> expected;if(status){expected.push_back("alloc");if(!fail){for(int i=0;i<3 && (!null_record || i<1);++i)if(kind==0?i!=0:i!=1)expected.push_back("send"+std::to_string(42+i));expected.push_back("release");}}
            check(fixture.events==expected,"Original guild duel broadcast filtering differs");
        }
        actor.relations().opposing_guild_id=0;
    }
    {
        Actor second(43,{}, {},{},scene,{definitions,enchant,mobs}),third(44,{}, {},{},scene,{definitions,enchant,mobs});
        GuildCatalogPorts ports;ports.transport.allocate=[](const net::Packet&){return true;};ports.transport.release=[]{};ports.transport.send=[](GuildUser&,net::Packet){};
        ports.allocate_guild=[](){return true;};ports.allocate_member=[](){return true;};GuildCatalog catalog(ports);
        net::Packet init(138,0);init[0]=2;init[1]=0x0b;init[2]=1;init[3]=7;init[7]='G';
        check(catalog.apply(init)==EntryCatalogUpdate::applied,"Guild catalog setup failed");auto* guild=catalog.find(7);check(guild,"Missing loaded guild");
        GuildUser user;user.character_id=42;user.guild=&guild->membership;auto record=std::make_shared<GuildMember>();record->id=42;record->user=&user;guild->membership.online.emplace(42,record);
        GuildBroadcastFixture broadcast(actor,second,third);GuildDuelCatalogWorld world(catalog,broadcast);auto& member=broadcast.states[0];member.state=-1;member.ready_flag=1;
        guild->duel.status=3;guild->duel.participants=1;guild->duel.leader_id=42;actor.relations().opposing_guild_id=123;actor.relations().guild_duel_ready=1;services.calls.clear();
        CatalogGuildMovement movement(catalog,world,user,member);vertical_move_guild_duel(actor,{41,0,0},member,movement);
        check(guild->duel.status==0 && guild->duel.participants==0 && actor.relations().opposing_guild_id==0 && actor.relations().guild_duel_ready==0 && member.ready_flag==0 && services.calls==std::vector<std::string>{"reset"},"Catalog-owned guild movement cleanup failed");
        check(broadcast.events==std::vector<std::string>{"alloc","send42","release","alloc","send42","release"},"Catalog-owned guild duel broadcast differs");
        user.guild=nullptr;services.calls.clear();
    }
    {
        Actor second(43,{}, {},{},scene,{definitions,enchant,mobs}),third(44,{}, {},{},scene,{definitions,enchant,mobs});
        GuildCatalogPorts ports;ports.transport.allocate=[](const net::Packet&){return true;};ports.transport.release=[]{};ports.transport.send=[](GuildUser&,net::Packet){};ports.allocate_guild=[](){return true;};ports.allocate_member=[](){return true;};GuildCatalog catalog(ports);
        net::Packet init(138,0);init[0]=2;init[1]=0x0b;init[2]=1;init[3]=7;init[7]='A';check(catalog.apply(init)==EntryCatalogUpdate::applied,"first guild load");init[3]=8;init[7]='B';check(catalog.apply(init)==EntryCatalogUpdate::applied,"second guild load");
        auto* first=catalog.find(7);auto* other=catalog.find(8);GuildUser a,b;a.character_id=42;b.character_id=43;a.guild=&first->membership;b.guild=&other->membership;
        auto row=std::make_shared<GuildMember>();row->id=42;row->user=&a;first->membership.online.emplace(42,row);
        GuildBroadcastFixture broadcast(actor,second,third);GuildDuelCatalogWorld world(catalog,broadcast);PairCatalogMovement movement(catalog,world,a,b,broadcast.states[0],broadcast.states[1]);
        ZoneUsers zone_owner(5,5,0);zone_owner.attach(second,1,1,{});ZoneLeaveGuildCatalog binding(movement,zone_owner,[](Actor& x){return std::shared_ptr<void>(&x,[](void*){});});
        first->duel.status=1;first->duel.stage=43;first->duel.opponent_id=8;first->duel.leader_id=42;other->duel.status=2;actor.relations().opposing_guild_id=8;second.relations().opposing_guild_id=7;outgoing.clear();
        zone_leave_guild_duel(actor,binding);check(first->duel.status==0 && other->duel.status==0 && actor.relations().opposing_guild_id==0 && second.relations().opposing_guild_id==0 && outgoing==std::vector<net::Packet>({{2,0x25,3,43,0,0,0},{2,0x25,3,43,0,0,0}}),"catalog negotiation leave failed");
        first->duel.status=3;first->duel.stage=43;first->duel.opponent_id=0;first->duel.leader_id=42;first->duel.participants=1;actor.relations().opposing_guild_id=8;broadcast.states[0].ready_flag=1;outgoing.clear();broadcast.events.clear();
        zone_leave_guild_duel(actor,binding);check(first->duel.status==0 && first->duel.participants==0 && actor.relations().opposing_guild_id==0 && broadcast.states[0].ready_flag==0 && broadcast.events==std::vector<std::string>({"alloc","send42","release","alloc","send42","release"}),"catalog active leave failed");
        check(zone_owner.detach(43),"guild zone cleanup");a.guild=b.guild=nullptr;outgoing.clear();services.calls.clear();
    }
    {
        PartyWorld pool;pool.defer=true;UserViewSharedBuffers buffers(pool);PartyMembershipUser a,b,offline;a.id=43;b.id=44;offline.id=45;a.send_socket=b.send_socket=1;
        auto shared=buffers.allocate(user_view_leave_packet(42));check(shared && pool.buffer.frame==net::Packet({8,0,2,2,42,0,0,0}),"Shared view frame differs");
        buffers.enqueue(a,*shared);buffers.enqueue(offline,*shared);buffers.enqueue(b,*shared);
        check(pool.buffer.references==3 && pool.sent==std::vector<std::uint32_t>{43,44},"Shared view queue reference or disconnected gate differs");
        shared.reset();check(pool.buffer.references==2 && pool.active,"Owner released pending view buffer");
        release_party_membership_buffer(pool.buffer,pool);check(pool.buffer.references==1 && pool.active,"First completion freed a shared buffer");
        release_party_membership_buffer(pool.buffer,pool);check(!pool.active && pool.frees==1,"Final view completion did not free buffer");
        auto failed=buffers.allocate(user_view_leave_packet(42));pool.fail_enqueue=true;bool threw=false;
        try{buffers.enqueue(a,*failed);}catch(const std::runtime_error&){threw=true;}
        check(threw && pool.buffer.references==1,"Failed enqueue retained a buffer reference");pool.fail_enqueue=false;failed.reset();check(!pool.active && pool.frees==2,"Failed enqueue leaked the buffer");
    }
    {
        ViewTransport transport(actor);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(10,10,{3,3},{4,3},publication);
        check(transport.leaves==5 && transport.enters==5 && transport.allocated.size()==4,"Actor publication lost cell passes");
        check(transport.allocated[0]==user_view_leave_packet(42) && transport.allocated[1]==user_view_appearance_packet(actor,10,0x1234,{12.5F,-3.F,99.F},-7),"Actor publication lost live appearance after leave");
        check(transport.allocated[2]==net::Packet({0x1c,5,42,0,0,0,3,2}) && transport.allocated[3]==net::Packet({0x1d,5,42,0,0,0,7}),"Actor publication lost late speed or shape state");
        actor.status().shape=0;actor.status().skills.equipment.attack_speed=0;actor.status().skills.equipment.move_speed=0;
    }
    {
        Actor moving(46,{}, {},{},scene,{definitions,enchant,mobs});ZoneUsers live_zone(MapGeometry(160),0);
        check(live_zone.attach_at_position(moving,{20,3,20}),"Live actor attachment failed");
        LiveZoneViewTransport transport(live_zone,moving);UserViewActorPublication publication(moving,transport);
        publish_user_view_difference(5,5,{2,0},{3,0},publication);
        check(transport.allocated.size()==3 && transport.allocated[1]==user_view_appearance_packet(moving,11,0x1234,{65,8,20},-7),"Appearance did not sample actual changed zone position");
        ZoneUsers foreign(MapGeometry(160),0);bool rejected=false;try{(void)capture_user_view_appearance(foreign,moving,11,0,0);}catch(const std::logic_error&){rejected=true;}
        check(rejected,"Foreign zone supplied a live appearance");
    }
    {
        Actor old_view(43,{}, {},{},scene,{definitions,enchant,mobs});Actor new_view(44,{}, {},{},scene,{definitions,enchant,mobs});Actor overlap(45,{}, {},{},scene,{definitions,enchant,mobs});
        ZoneUsers grid(10,10,0);grid.attach(old_view,1,1,{0,0,0});grid.attach(new_view,6,1,{0,0,0});grid.attach(overlap,3,3,{0,0,0});
        CellServices services;UserViewCellTransport transport(grid,services);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(10,10,{3,3},{4,3},publication);
        check(services.leaves==5 && services.enters==5 && services.delivered.size()==3,"Real cell traversal lost recipients or snapshots");
        check(services.delivered[0].first==43 && services.delivered[0].second==user_view_leave_packet(42),"Old cell missed leave");
        check(services.delivered[1].first==44 && services.delivered[1].second[0]==1 && services.delivered[1].second[1]==2 && services.delivered[2].first==44 && services.delivered[2].second[0]==0x1c && services.delivered[2].second[1]==5,"New cell missed appearance/speed");
    }
    {
        Actor old_view(43,{}, {},{},scene,{definitions,enchant,mobs});Actor new_view(44,{}, {},{},scene,{definitions,enchant,mobs});
        ZoneUsers grid(10,10,0);grid.attach(old_view,1,1,{0,0,0});grid.attach(new_view,6,1,{0,0,0});
        MultiWorld pool;UserViewSharedBuffers buffers(pool);SharedCellServices services(buffers);
        UserViewCellTransport transport(grid,services);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(10,10,{3,3},{4,3},publication);
        check(pool.sent==std::vector<std::uint32_t>{43,44,44} && pool.frames.size()==3 && pool.frames[0]==net::Packet({8,0,2,2,42,0,0,0}),"Shared pool cell publication lost recipients or frame");
        check(pool.frames[1][2]==1 && pool.frames[1][3]==2 && pool.frames[2][2]==0x1c && pool.frames[2][3]==5 && std::none_of(pool.used.begin(),pool.used.end(),[](bool active){return active;}) && pool.frees==3,"Shared pool cell publication lost appearance/speed or references");
    }
    {
        auto recipient=std::make_shared<Actor>(43,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});
        std::weak_ptr<Actor> weak=recipient;ZoneUsers grid(10,10,0);grid.attach(*recipient,1,1,{0,0,0});
        CellServices services;services.retain_actor=[&](Actor& found){check(recipient.get()==&found,"Wrong retained recipient");return std::static_pointer_cast<void>(recipient);};
        services.on_enqueue=[&](Actor& found){check(grid.detach(found.id()),"Recipient detach failed");recipient.reset();check(!weak.expired(),"Current recipient destroyed during enqueue");};
        UserViewCellTransport transport(grid,services);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(10,10,{3,3},{4,3},publication);
        check(weak.expired() && services.delivered.size()==1 && services.delivered[0].second==user_view_leave_packet(42),"Detached recipient not released after delivery");
    }
    {
        Actor first(43,{}, {},{},scene,{definitions,enchant,mobs});Actor second(44,{}, {},{},scene,{definitions,enchant,mobs});
        ZoneUsers grid(10,10,0);grid.attach(first,1,1,{0,0,0});grid.attach(second,1,1,{0,0,0});
        DeferredCellServices services;UserViewCellTransport transport(grid,services);UserViewActorPublication publication(actor,transport);
        publish_user_view_difference(10,10,{3,3},{4,3},publication);
        check(services.queued.size()==2 && services.queued[0].second==services.queued[1].second && *services.queued[0].second==user_view_leave_packet(42),"Queued recipients did not retain the same leave payload");
        check(!services.allocated[0].expired(),"Shared payload freed before queue completion");
        services.queued.erase(services.queued.begin());check(!services.allocated[0].expired(),"First completion freed another recipient's payload");
        services.queued.clear();check(services.allocated[0].expired(),"Shared payload retained after all completions");
    }
    {
        EffectStatus speed;speed.skills.equipment.attack_speed=12;speed.skills.equipment.move_speed=8;
        check(character_speed(42,speed)==net::Packet({0x1c,5,42,0,0,0,9,4}),"Clamped view speeds differ");
        speed.skills.equipment.attack_speed=3;speed.skills.equipment.move_speed=2;
        speed.fixed_form=1;check(character_speed(42,speed)==net::Packet({0x1c,5,42,0,0,0,255,0}),"Fixed-form view speeds differ");
        speed.fixed_form=0;speed.movement_restricted=1;check(character_speed(42,speed)==net::Packet({0x1c,5,42,0,0,0,3,255}),"Restricted view speeds differ");
        speed.action_block_a=1;check(character_speed(42,speed)==net::Packet({0x1c,5,42,0,0,0,255,255}),"Blocked view speeds differ");
    }
    actor.erase_persisted({777,2,0});check(effect_database.packets==std::vector<net::Packet>{effect_database_removed_packet(314,777,2)},"Effect database route lost the actor removal");
    {
        PlayerCore core;core.user_id=314;core.character.id=42;InventoryReplicaPool inventory_pool;
        CharacterReplica replica(core,inventory_pool);const std::vector<PersistedEffect> effects{{777,2,100},{778,1,100}};
        check(replica.skills().load({},effects),"Cannot load effect replica");
        std::recursive_mutex mutex;EffectDiagnostics diagnostics;ActorEffectReplicaDelivery database(replica.skills(),mutex,diagnostics);
        VerticalScene real_scene(channel,zone,user,party_world,services,database);
        Actor real_actor(42,{}, {},{},real_scene,{definitions,enchant,mobs});real_actor.account_id()=314;
        real_actor.erase_persisted(effects[0]);const auto remaining=replica.skills().effects();
        check(remaining.size()==1 && remaining[0].id==778 && !diagnostics.rejected && !diagnostics.failed,"Actor removal did not reach the actual skill replica");
    }
    {
        storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
        storage::Row row(table.columns.size(),storage::Cell{"0"});row[0]="777";row[1]="1";row[2]="74";table.rows.push_back(row);row[1]="2";table.rows.push_back(row);
        const auto skills=SkillDefinitions::load(table);const StatusRules statuses;const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});
        const ActorLoadRules rules{statuses,definitions,enchant,skills,quests,mobs};
        PlayerSnapshot snapshot;snapshot.core.user_id=314;snapshot.core.character.id=42;snapshot.effects={{777,2,100}};
        InventoryReplicaPool inventory_pool;auto replica=std::make_unique<CharacterReplica>(snapshot.core,inventory_pool);
        check(replica->skills().load({},snapshot.effects),"Cannot load restore-time effect replica");
        std::recursive_mutex mutex;EffectDiagnostics diagnostics;ActorEffectReplicaDelivery database(replica->skills(),mutex,diagnostics);
        VerticalScene load_scene(channel,zone,user,party_world,services,database);
        VerticalSessionCharacter session({std::move(snapshot),std::move(replica)},rules,{},load_scene);
        check(session.replica().skills().effects().empty() && !diagnostics.rejected && !diagnostics.failed,"Restore-time removal did not reach retained replica");
    }
    scene.send_self({5,2,1});check(outgoing==std::vector<net::Packet>{{5,2,1}},"Wrong channel send");fail_self=true;scene.send_self({5,2,2});check(services.failures==1 && services.failure==VerticalSceneFailure::self_send && services.has_exception && outgoing.size()==1,"Self queue failure not reported");fail_self=false;
    check(!scene.user_zone(),"Unattached actor has zone");actor.status().in_zone=true;bool foreign=false;try {scene.user_zone();} catch(const std::logic_error&) {foreign=true;}actor.status().in_zone=false;check(foreign,"Foreign zone binding silently accepted");scene.send_nearby({1,2});check(services.calls.empty(),"Detached view was sent");
    check(zone.attach_at_position(actor,{20,0,20}),"Cannot attach actor");scene.send_nearby({1,2});scene.send_cell_view({3,4});check(scene.user_zone()==&zone && services.calls==std::vector<std::string>{"near","cell"},"Zone views not routed");
    bool threw=false;try {scene.unbind();} catch(const std::logic_error&) {threw=true;}check(threw,"Attached unbind accepted");
    PartyRegistryParty party;PartyMembershipUser a,b,c;a.id=43;b.id=44;c.id=45;a.send_socket=1;b.send_socket=-1;c.send_socket=2;user.send_socket=3;user.party=&party;party.slots[0]=&user;party.slots[1]=&a;party.slots[2]=&b;party.extra_slots[0]=&c;party.count=3;services.group=&party;
    check(scene.party_kind()==false,"Normal kind lost");scene.send_party({1,2,3});check(party_world.sent==std::vector<std::uint32_t>{43} && party_world.frees==1 && !party_world.active,"Normal party source exclusion or buffer ownership failed");
    party.experience.kind=1;party.count=8;check(scene.party_kind()==true,"Raid kind lost");party_world.on_enqueue=[&]{party.experience.kind=0;};scene.send_party({9,8});check(party_world.sent==std::vector<std::uint32_t>({43,43,45}) && party_world.frees==2,"Raid extended slots or sender exclusion failed");
    party_world.deny_allocation=true;scene.send_party({1,2});check(services.failures==2 && services.failure==VerticalSceneFailure::party_buffer_unavailable && !services.has_exception && !party_world.active,"Pool exhaustion not reported");party_world.deny_allocation=false;
    party_world.fail_allocation=true;scene.send_party({1,2});check(services.failures==3 && services.failure==VerticalSceneFailure::party_send && services.has_exception && !party_world.active,"Pool exception not reported");party_world.fail_allocation=false;
    party_world.fail_enqueue=true;scene.send_party({1,2});check(services.failures==4 && services.failure==VerticalSceneFailure::party_send && services.has_exception && !party_world.active && party_world.locks==0 && party_world.buffer.references==0,"Queue failure leaked lock or references");party_world.fail_enqueue=false;
    const auto before=party_world.sent.size();party_world.on_enqueue=[&]{party_world.fail_enqueue=true;};scene.send_party({1,2});check(services.failures==5 && services.failure==VerticalSceneFailure::party_send && party_world.sent.size()==before+1 && !party_world.active && party_world.locks==0 && party_world.buffer.references==0,"Partial queue failure cleanup changed");party_world.fail_enqueue=false;
    party.experience.kind=2;check(!scene.party_kind() && services.failures==6 && services.failure==VerticalSceneFailure::party_state && services.has_exception && party_world.locks==0,"Party state failure was silent or locked");party.experience.kind=0;
    party_world.fail_free=true;scene.send_party({1,2});check(services.failures==7 && services.failure==VerticalSceneFailure::party_send && services.has_exception && !party_world.active && party_world.locks==0,"Buffer release failure was not reported");party_world.fail_free=false;
    scene.reset_damage();scene.stop_target_mob(44);scene.taunt(5,60);check(scene.allocate_effect_id()==19 && scene.tick_ms()==101,"Effect ID or clock delegation failed");scene.release_effect_id(19);check(services.freed==19 && scene.user_shape_social().guild_name=="guild","Effect release or social owner failed");
    EffectStatus status;SkillDefinition skill;EffectSource source;scene.finish_periodic_damage(status,skill,source,7);check(services.calls==std::vector<std::string>({"near","cell","reset","stop","taunt","damage"}),"Scene required service dispatch failed");
    check(zone.detach(actor.id()),"Cannot detach actor");scene.unbind();
    {auto owned_scene=std::make_shared<VerticalScene>(channel,zone,user,party_world,services,effect_database);auto binding=vertical_world_actor_scene(owned_scene);
     Actor bound_actor(42,{}, {},{},*owned_scene,{definitions,enchant,mobs});binding.bind(bound_actor);
     check(zone.attach_at_position(bound_actor,{20,0,20}),"Cannot attach bound scene actor");bool blocked=false;try{binding.unbind();}catch(const std::logic_error&){blocked=true;}
     check(blocked && zone.detach(bound_actor.id()),"Bound scene allowed unbind while attached");binding.unbind();}
    {ZoneUsers second_zone(3,3,0);ZoneUsers* current=nullptr;auto dynamic=std::make_shared<VerticalScene>(channel,[&]{return current;},user,party_world,services,effect_database);
     auto binding=vertical_world_actor_scene(dynamic);Actor moving(42,{}, {},{},*dynamic,{definitions,enchant,mobs});binding.bind(moving);check(!dynamic->user_zone(),"Detached scene selected a zone");
     current=&zone;check(zone.attach_at_position(moving,{20,0,20}) && dynamic->user_zone()==&zone,"Dynamic scene lost first zone");
     check(zone.detach(moving.id()),"Cannot leave first dynamic zone");current=&second_zone;second_zone.attach(moving,1,1,{1,0,1});check(dynamic->user_zone()==&second_zone,"Dynamic scene lost transferred zone");
     check(second_zone.detach(moving.id()),"Cannot leave second dynamic zone");current=nullptr;moving.status().in_zone=true;bool missing=false;try{(void)dynamic->user_zone();}catch(const std::logic_error&){missing=true;}moving.status().in_zone=false;
     check(missing,"Dynamic scene accepted missing attached zone");binding.unbind();}
    network_channel();std::cout<<"Scene channel, actual zone membership, normal/raid party buffer ownership and required services passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
