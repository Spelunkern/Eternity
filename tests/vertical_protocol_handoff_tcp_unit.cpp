#include "eternity/game/vertical_move_runtime_composition.hpp"
#define main unused_vertical_gate_fixture_main
#include "vertical_protocol_gate_unit.cpp"
#undef main
#include "eternity/net/world_server.hpp"
#include "eternity/game/vertical_movement_dispatch.hpp"
#include "eternity/game/vertical_entry_admission.hpp"
#include "eternity/game/vertical_entry_catalog_restore.hpp"
#include "eternity/game/vertical_move_retained_session.hpp"
#include "eternity/game/vertical_move_composed_world.hpp"
#include "eternity/game/vertical_move_priority_channel.hpp"
#include "eternity/game/vertical_move_channel_send_world.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"
#include "login_socket.hpp"
#include <bit>
#include <condition_variable>
#include <mutex>
using namespace std::chrono_literals;
struct HandoffObserverResource {
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;
    std::shared_ptr<Actor> actor;ZoneEntryState entry;ZoneLeaveShopState shop;PartyMembershipUser connection;WorldAdmissionUser admission;std::unique_ptr<WorldUser> index;
    HandoffObserverResource():actor(std::make_shared<Actor>(43,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{items,enchant,mobs})) {
        actor->account_id()=12;entry.bound=true;entry.character_id=43;entry.map=20;entry.cell={5,2};entry.position={201,0,81};
        connection.id=43;connection.send_socket=1;admission.user_id=12;admission.location=4;
    }
};
struct HandoffState {
    std::mutex mutex;std::condition_variable cv;bool pause=false,waiting=false,first=false,second=false,location=false,done=false,closed=false,failed=false,cancelled=false;
    bool mode_first=false,bad_position=false,multi=false,observer_online=false,observer_closed=false,disconnect_observer=false,reset_observer=false,remove_observer=false;unsigned entries=0,entry_packets=0;net::WorldChannel channel,observer_channel;std::weak_ptr<HandoffObserverResource> observer_resource;
    void wait(const std::function<bool()>& condition){std::unique_lock lock(mutex);check(cv.wait_for(lock,3s,condition),"handoff timed out");}
    void release(bool& flag){std::lock_guard lock(mutex);flag=true;cv.notify_all();}
};
struct HandoffMoveView final:UserViewActorTransport {
    struct Buffer final:UserViewBuffer {};
    UserViewLiveAppearance appearance(const Actor&) override{return {255,0,{40,0,40},0};}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet) override{return std::make_unique<Buffer>();}
    void broadcast(CellCoordinates,UserViewBuffer&) override{}void leave_snapshot(CellCoordinates) override{}void enter_snapshot(CellCoordinates) override{}
};
struct HandoffPriorityPool final:PartyMembershipSendWorld {
    unsigned allocations=0,frees=0;
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override{++allocations;return new PartyMembershipSendBuffer;}
    void free_send_buffer(PartyMembershipSendBuffer& buffer) override {++frees;delete &buffer;}
    void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer&) override {throw std::logic_error("Unexpected immediate send");}
    std::uint32_t tick() override{throw std::logic_error("Unexpected pool tick");}
    void lock(PartyNormalMembership&) override{throw std::logic_error("Unexpected party lock");}
    void unlock(PartyNormalMembership&) noexcept override{std::terminate();}
    void lock_effects(PartyMembershipUser&) override{throw std::logic_error("Unexpected effect lock");}
    void unlock_effects(PartyMembershipUser&) noexcept override{std::terminate();}
    void send_user(PartyMembershipUser&,const net::Packet&) override{throw std::logic_error("Unexpected user send");}
    void send_scene(PartyMembershipUser&,const net::Packet&) override{throw std::logic_error("Unexpected scene send");}
    void unregister_party(PartyNormalMembership&) override{throw std::logic_error("Unexpected unregister");}
    void cleanup_guild_creation(PartyMembershipUser&) override{throw std::logic_error("Unexpected guild cleanup");}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override{throw std::logic_error("Unexpected log");}
    void send_log(const net::Packet&) override{throw std::logic_error("Unexpected log send");}
};
struct HandoffRetainedMoveOwner:std::enable_shared_from_this<HandoffRetainedMoveOwner> {
    ZoneEntryState* entry=nullptr;net::WorldChannel wire;UserPriorityQueue* queue=nullptr;PartyMembershipUser* connection=nullptr;ZoneLeaveShopState* shop=nullptr;
    WorldUser* world=nullptr;bool stopped=false;std::shared_ptr<HandoffObserverResource> observer_resource;
    WorldUser* world_user(){return world;}net::WorldChannel& channel(){return wire;}bool closing() const{return stopped;}
};
struct HandoffDrainGameplay {using Other=std::function<void()>;};
using HandoffMoveComposition=VerticalMoveRuntimeComposition<HandoffDrainGameplay,int,HandoffRetainedMoveOwner>;
struct HandoffMoveHolder {
    std::unique_ptr<VerticalMoveRetainedSession<HandoffRetainedMoveOwner>> value;
    std::unique_ptr<VerticalMoveComposedWorld<HandoffRetainedMoveOwner>> composed;
    auto receive(std::span<const std::uint8_t> packet){return composed?composed->receive(packet):value->receive(packet);}
};
struct HandoffDuelBackend final:DuelWorld {
    Actor* find_duel_user(Actor&,std::uint32_t) override{throw std::logic_error("Unexpected duel lookup");}
    UserPosition duel_position(const Actor&) override{throw std::logic_error("Unexpected duel position");}
    std::int32_t drop_random() override{throw std::logic_error("Unexpected duel random");}
    DuelDropItem* allocate_drop_item() override{throw std::logic_error("Unexpected duel allocation");}
    bool enter_duel_item(Actor&,DuelDropItem&,bool) override{throw std::logic_error("Unexpected duel drop");}
    GameLogSubject duel_log_subject(const Actor&) override{throw std::logic_error("Unexpected duel log");}
    std::chrono::sys_seconds local_calendar() override{throw std::logic_error("Unexpected duel clock");}
    void send_duel_log(const net::Packet&) override{throw std::logic_error("Unexpected duel logging");}
};
struct HandoffGuildMovement final:VerticalGuildDuelMovementServices {
    std::shared_ptr<void> capture_guild_lock(Actor&) override{throw std::logic_error("Unexpected guild lock");}
    void* current_guild(Actor&) override{throw std::logic_error("Unexpected guild lookup");}
    std::int32_t guild_duel_status(void*) override{throw std::logic_error("Unexpected guild duel");}
    void leave_guild_duel(void*,Actor&,std::uint8_t) override{throw std::logic_error("Unexpected guild leave");}
};
struct HandoffMoveServices final:VerticalMovePacketServices {
    net::WorldChannel channel;std::array<float,5> speeds{3,4,5,6,7};bool finished=false;std::function<void(Actor&,const net::Packet&)> priority_send;
    explicit HandoffMoveServices(net::WorldChannel c):channel(std::move(c)){}
    void status_stand(Actor&) override{}std::uint16_t direction(const Actor&) override{return 0;}
    void send_view_action(Actor&,const net::Packet&) override{throw std::logic_error("Unexpected correction");}
    std::uint32_t tick_ms() override{return 200;}
    bool send_priority_view(Actor& actor,const net::Packet& packet,std::uint8_t priority) override{check(priority==7 && bool(priority_send),"Movement priority differs");priority_send(actor,packet);return false;}
    void finish_move(Actor&,bool wide) override{check(!wide,"Unexpected wide pass");finished=true;}
    const std::array<float,5>& movement_speed_table() override{return speeds;}
    void close(Actor&,std::uint32_t) override{throw std::logic_error("Unexpected movement close");}
};
struct HandoffApp final:net::WorldApplication {
    static VerticalPersistenceSelection selection(InventoryReplicaPool& pool) {
        PlayerSnapshot snapshot;snapshot.core.user_id=11;snapshot.core.character.id=42;
        snapshot.core.character.hp=1;snapshot.core.character.mp=1;snapshot.core.character.sp=1;
        snapshot.catalogs["Friends"].columns.resize(7);snapshot.catalogs["Friends"].rows={{"55","Friend","1","2","3","original","0"}};
        snapshot.catalogs["Blocked"].columns.resize(3);
        auto replica=std::make_unique<CharacterReplica>(snapshot.core,pool);return {std::move(snapshot),std::move(replica)};
    }
    std::shared_ptr<HandoffState> state;net::WorldChannel channel;ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;
    StatusRules statuses;SkillDefinitions skills;QuestDefinitions quests=QuestDefinitions::decode(net::Packet{0,0,0,0});
    ActorLoadRules rules{statuses,items,enchant,skills,quests,mobs};InventoryReplicaPool replica_pool;
    VerticalSessionCharacter character;Actor& actor;
    Corpse corpse;Npc npc;CorpseServices corpse_services;MobServices mob_services;NpcServices npc_services;ItemServices item_services;WorldCorpses corpses;
    ConfiguredMap map;std::unique_ptr<MapGeometry> retained_geometry;RuntimeZone zone;ZoneEntryState pos;ZoneActorEntryServices entry;
    std::unique_ptr<UserCellSnapshotChannel> snapshot_channel;std::unique_ptr<CellSnapshot> snapshot;
    Services services;WorldAdmissionUser user;WorldAdmission world;VerticalEntrySession session;
    WorldUsers world_index{WorldUserPorts{[](WorldUser&){},[](WorldUser&){}}};WorldUser mover_index{actor,user};
    std::recursive_mutex ack_mutex;VerticalDatabaseAckUser move_ack{mover_index,ack_mutex};ZoneLeaveShopState move_shop;
    HandoffDuelBackend duel_backend;HandoffGuildMovement guild_movement;
    VerticalSessionRegistry<HandoffRetainedMoveOwner> move_registry;std::shared_ptr<HandoffMoveComposition> movement_composition;HandoffPriorityPool priority_pool;
    VerticalMoveChannelSendWorld<HandoffRetainedMoveOwner> channel_pool{priority_pool,world_index,move_registry,
        [](HandoffRetainedMoveOwner& owner)->PartyMembershipUser&{return *owner.connection;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;}};
    UserViewSharedBuffers priority_buffers{channel_pool};PartyMembershipUser priority_connection;VerticalMoveSessionResources movement_resources{channel_pool,priority_connection,move_shop};
    HandoffMoveView move_view;HandoffMoveServices move_services;VerticalMotionState motion;
    std::shared_ptr<HandoffRetainedMoveOwner> move_owner;HandoffMoveHolder move_session;
    Actor* observer_actor=nullptr;std::shared_ptr<HandoffRetainedMoveOwner> observer_owner;HandoffObserverResource* observer_data=nullptr;
    std::shared_ptr<VerticalMoveSessionResources> observer_movement;
    std::optional<net::Packet> pending_entry;std::optional<VerticalEntryRequest> outcome;
    HandoffApp(std::shared_ptr<HandoffState> s,net::WorldChannel c,std::optional<VerticalPersistenceSelection> loaded={}):state(std::move(s)),channel(std::move(c)),character(loaded?std::move(*loaded):selection(replica_pool),rules,{},scene),actor(*character.pipeline().result().actor),services(zone,actor,pos,entry),world(services),session(world,user),move_services(channel){
        map.settings.id=20;map.definition.size=state->multi?280:81;map.definition.doors={{{1,0,1},1,1}};zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);pos.character_id=42;pos.position=state->multi?UserPosition{110,0,81}:UserPosition{40,0,40};
        retained_geometry=std::make_unique<MapGeometry>(map.definition.geometry());const auto& geometry=*retained_geometry;zone.corpses=std::make_unique<ZoneCorpses>(geometry,corpses,corpse_services);
        zone.mobs=std::make_unique<ZoneMobs>(geometry,mob_services);zone.npcs=std::make_unique<ZoneNpcs>(geometry,npc_services);zone.items=std::make_unique<ZoneItems>(geometry,item_services);
        corpse.id=100;corpse.position={1,0,1};npc.id=101;npc.position={80,0,80};check(zone.corpses->enter(corpse) && zone.npcs->enter(npc),"TCP entity setup failed");
        snapshot_channel=std::make_unique<UserCellSnapshotChannel>(channel,[](Actor& a){return std::shared_ptr<void>(&a,[](void*){});},
            [this](const Actor& a){return capture_user_view_appearance(*zone.users,a,255,0,0);},[]{return std::array<std::uint8_t,4>{};});
        snapshot=std::make_unique<CellSnapshot>(*zone.users,*zone.corpses,*zone.mobs,*zone.npcs,*zone.items,*zone.environment,*snapshot_channel);
        entry.send_view=[](CellCoordinates,net::Packet){};entry.obelisks=[](){};entry.nprotect=[](){};entry.diagnostic=[](){};entry.close=[](std::uint32_t){};
        entry=bind_entry_cell_snapshots(std::move(entry),geometry.cell_count(),geometry.cell_count(),*snapshot);
        actor.account_id()=11;user.user_id=11;user.map=20;world.track_selection(user);world_index.enter(mover_index);
        movement_composition=HandoffMoveComposition::create(0,move_registry,
            [](HandoffRetainedMoveOwner&)->HandoffMoveComposition::Resources{throw std::logic_error("Unexpected resource acquisition");},
            [](HandoffRetainedMoveOwner&,RuntimeZone&,HandoffMoveComposition::Directory&)->VerticalMoveDirectoryServices<HandoffRetainedMoveOwner>{throw std::logic_error("Unexpected gameplay services");},[]{});
        move_owner=std::make_shared<HandoffRetainedMoveOwner>();move_owner->world=&mover_index;move_owner->entry=&pos;move_owner->wire=channel;move_owner->queue=&movement_resources.queue();move_owner->connection=&priority_connection;move_owner->shop=&move_shop;move_ack.socket_open=true;move_registry.retain(move_owner,actor);
        priority_connection.id=actor.id();priority_connection.send_socket=1;
        move_services.priority_send=[this](Actor& subject,const net::Packet& packet) {
            auto buffer=priority_buffers.allocate(packet);check(bool(buffer) && priority_buffers.enqueue_priority(priority_connection,*buffer,movement_resources.queue(),7,0),"TCP priority queue failed");
            buffer.reset();VerticalMovePriorityChannel<HandoffRetainedMoveOwner> drain(move_registry,channel_pool,
                [](HandoffRetainedMoveOwner& owner)->UserPriorityQueue&{return *owner.queue;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;});
            check(drain.drain(subject)==1 && movement_resources.queue().size()==0,"TCP priority drain failed");
        };
        move_session.value=std::make_unique<VerticalMoveRetainedSession<HandoffRetainedMoveOwner>>(move_registry,*move_owner,actor,zone,user,pos,motion,move_services,
            [](HandoffRetainedMoveOwner& owner)->ZoneEntryState&{return *owner.entry;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;},[]{return std::array<std::uint8_t,4>{};},
            &priority_buffers,[](HandoffRetainedMoveOwner& owner)->PartyMembershipUser&{return *owner.connection;});
        services.on_packets=[this](WorldAdmissionUser& queued){check(&queued==&user,"wrong admission user");if(pending_entry){auto packet=std::move(*pending_entry);pending_entry.reset();outcome=session.receive(packet);}};
    }
    void open() override {}
    void receive(const net::Packet& p) override {
        if(p==net::Packet{4,1,42,0,0,0}) {
            channel.pause_input([s=state](bool ok){std::lock_guard lock(s->mutex);s->pause=ok;s->cv.notify_all();});
            state->wait([&]{return state->pause;});
            CharacterEntryState entry_state;entry_state.user_id=11;entry_state.character_id=42;entry_state.level=character.snapshot().core.character.level;
            SocialCatalog social(11,42);auto catalogs=vertical_restore_entry_catalogs(character.replica());VerticalProtocolServices protocol;
            protocol.delivery.send_self=[&](net::Packet packet){++state->entry_packets;channel.send(std::move(packet));};
            protocol.delivery.send_peer=[](std::uint32_t,net::Packet){};protocol.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
            protocol.delivery.tick=[] {return 1U;};protocol.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
            protocol.lifecycle=[](CharacterEntryStep,Actor&){};protocol.send_manager=[](net::Packet){};protocol.world_day=[] {return 0U;};protocol.system_time=[] {return 0U;};protocol.lock_guild=[] {return false;};protocol.unlock_guild=[]{};
            check(deliver_vertical_entry_with_admission(entry_state,character,world,user,social,catalogs,{0,100},"",std::move(protocol)) && entry_state.ready,"real character delivery failed");
            const auto before=services.events.size();world.update_zone_wait_packets(zone);check(services.events.size()==before,"zone processed a world-wait user");state->release(state->waiting);
            state->wait([&]{return state->first;});
            if(state->mode_first){
                channel.enter_world();channel.send({0x77,0x77,1});state->wait([&]{return state->second;});
                update_vertical_admission(world);check(user.location==3,"zone publication failed");state->release(state->location);
                channel.set_admission_dispatch(true);pos.position.x=state->bad_position?81.0F:(state->multi?110.0F:40.0F);channel.send({0x78,0x77,1});channel.resume_input();
            } else {
                update_vertical_admission(world);check(user.location==3,"zone publication failed");state->release(state->location);
                channel.set_admission_dispatch(true);pos.position.x=state->bad_position?81.0F:(state->multi?110.0F:40.0F);channel.send({0x77,0x77,0});channel.resume_input();
            }
        } else if(p==net::Packet{1,2}) {
            pending_entry=p;outcome.reset();world.update_zone_wait_packets(zone);check(outcome.has_value(),"zone-wait owner did not dispatch frame");const auto result=*outcome;{std::lock_guard lock(state->mutex);++state->entries;state->failed=result==VerticalEntryRequest::rejected;check(result!=VerticalEntryRequest::unhandled,"gate ignored");if(state->failed)check(user.location==3 && world.zone_waiting_count(zone)==1,"failed entry lost queue");state->cv.notify_all();}
            if(result==VerticalEntryRequest::entered){check(zone.users->find_actor(42)==&actor && character.replica().core().character.id==actor.id(),"handoff lost session actor or replica");
                if(state->multi) {
                    state->wait([&]{return state->observer_online;});
                    std::shared_ptr<HandoffObserverResource> retained;{std::lock_guard lock(state->mutex);retained=state->observer_resource.lock();}
                    check(bool(retained),"observer application lost its actor owner");observer_data=retained.get();observer_actor=observer_data->actor.get();observer_data->admission.zone=&zone;
                    observer_data->index=std::make_unique<WorldUser>(WorldUser{*observer_actor,observer_data->admission});world_index.enter(*observer_data->index);
                    observer_owner=std::make_shared<HandoffRetainedMoveOwner>();observer_owner->world=observer_data->index.get();observer_owner->entry=&observer_data->entry;observer_owner->connection=&observer_data->connection;observer_owner->shop=&observer_data->shop;observer_owner->observer_resource=std::move(retained);
                    {std::lock_guard lock(state->mutex);observer_owner->wire=state->observer_channel;}
                    zone.users->attach(*observer_actor,5,2,observer_data->entry.position);move_registry.retain(observer_owner,*observer_actor);
                    observer_movement=movement_composition->directory().attach(observer_owner,*observer_actor,observer_owner->observer_resource,channel_pool,observer_data->connection,observer_data->shop);check(movement_composition->directory().find(*observer_actor)==observer_movement,"observer movement directory lost resource");observer_owner->queue=&observer_movement->queue();
                    move_session.value.reset();
                    move_session.composed=std::make_unique<VerticalMoveComposedWorld<HandoffRetainedMoveOwner>>(move_registry,*move_owner,actor,zone,user,pos,motion,movement_resources.cadence(),movement_resources.npc(),movement_resources.duel(),movement_resources.guild_duel(),duel_backend,guild_movement,
                        [](Actor&){throw std::logic_error("Unexpected guild refresh");},[]{return std::array<std::uint8_t,100>{};},priority_buffers,
                        [](HandoffRetainedMoveOwner& owner)->PartyMembershipUser&{return *owner.connection;},[](HandoffRetainedMoveOwner& owner)->UserPriorityQueue&{return *owner.queue;},
                        [](HandoffRetainedMoveOwner& owner)->ZoneLeaveShopState&{return *owner.shop;},move_ack,move_shop,move_services.speeds,[]{return 200U;},
                        [](HandoffRetainedMoveOwner& owner)->ZoneEntryState&{return *owner.entry;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;},[]{return std::array<std::uint8_t,4>{};});
                    if(state->disconnect_observer || state->reset_observer || state->remove_observer) {
                        auto pending=priority_buffers.allocate(net::Packet{0x31,0x77});check(bool(pending) && priority_buffers.enqueue_priority(observer_data->connection,*pending,observer_movement->queue(),7,20),"observer pending queue failed");
                        pending.reset();check(observer_movement->queue().size()==1 && priority_pool.allocations==priority_pool.frees+1,"observer pending reference was not retained");
                        if(state->reset_observer || state->remove_observer) {
                            VerticalMovePriorityChannel<HandoffRetainedMoveOwner> resetter(move_registry,channel_pool,
                                [](HandoffRetainedMoveOwner& owner)->UserPriorityQueue&{return *owner.queue;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;},
                                [](HandoffRetainedMoveOwner& owner)->PartyMembershipUser&{return *owner.connection;});
                            bool rejected=false;try{movement_composition->directory().close(*observer_actor);}catch(const std::logic_error&){rejected=true;}
                            check(rejected && observer_movement->queue().size()==1,"open observer queue reset was accepted");
                        }
                    }
                }
                channel.send({0x79,0x77});}else channel.disconnect();
        } else if(p==net::Packet{9,1,55}) {channel.send({0x7a,0x77});}
        else if(p.size()==17 && p[0]==1 && p[1]==5) {
            check(user.location==4,"movement arrived before zone admission");
            if(state->disconnect_observer || state->reset_observer || state->remove_observer) {
                state->wait([&]{return state->observer_closed;});
                check(!state->observer_resource.expired() && observer_owner->observer_resource.get()==observer_data,"registered observer actor did not survive socket close");
                VerticalMovePriorityChannel<HandoffRetainedMoveOwner> drain(move_registry,channel_pool,
                    [](HandoffRetainedMoveOwner& owner)->UserPriorityQueue&{return *owner.queue;},[](HandoffRetainedMoveOwner& owner)->net::WorldChannel&{return owner.wire;},
                    [](HandoffRetainedMoveOwner& owner)->PartyMembershipUser&{return *owner.connection;});
                if(state->reset_observer || state->remove_observer) {
                    observer_data->connection.send_socket=-1;
                    check(movement_composition->directory().close(*observer_actor)==1 && !movement_composition->directory().find(*observer_actor),"closed observer priority reset failed");
                } else check(drain.drain(*observer_actor)==1,"closed observer priority drain failed");
                if(state->reset_observer || state->remove_observer) {
                    bool closed=false;try{(void)observer_movement->queue();}catch(const std::logic_error&){closed=true;}
                    check(closed,"closed observer retained usable priority resources");
                } else check(observer_movement->queue().size()==0,"closed observer priority queue did not drain");
                check(priority_pool.allocations==priority_pool.frees,"closed observer priority reference leaked");
                if(state->remove_observer) {
                    remove_observer();check(state->observer_resource.expired(),"observer resource survived zone and registry removal");
                    check(!zone.users->find_actor(43) && !world_index.find(43),"closed observer remained indexed");
                }
            }
            const auto result=move_session.receive(p);
            check(result==VerticalMoveResult::moved && (state->multi || move_services.finished) && zone.users->position(actor.id())->x==(state->multi?140.F:45.F) && zone.users->find_actor(42)==&actor && character.replica().core().character.id==42,"movement dispatch lost admitted session actor");
            if(state->multi) {
                if(!state->remove_observer) {
                    if(!state->reset_observer)check(movement_composition->drain(*observer_owner)==1,"observer movement queue differs");
                    else {bool closed=false;try{(void)observer_movement->queue();}catch(const std::logic_error&){closed=true;}check(closed,"closed observer acquired movement queue");}
                }
                check(priority_pool.allocations>=4 && priority_pool.allocations==priority_pool.frees,"Shared TCP visibility or priority buffers leaked");
            }
            channel.send({0x7b,0x77});
        }
        else throw std::runtime_error("unexpected inbound packet");
    }
    void remove_observer() {
        if(!observer_actor)return;
        observer_owner->stopped=true;
        if(movement_composition->directory().find(*observer_actor)){observer_data->connection.send_socket=-1;movement_composition->directory().close(*observer_actor);}observer_movement.reset();check(zone.users->detach(43),"observer zone detach failed");world_index.leave(*observer_data->index);observer_data->index.reset();
        check(move_registry.release(12,*observer_owner),"observer release failed");observer_owner.reset();observer_actor=nullptr;observer_data=nullptr;
    }
    void close() override {remove_observer();move_owner->stopped=true;priority_connection.send_socket=-1;movement_resources.close_queue();if(user.location==4)zone.users->detach(42);world_index.leave(mover_index);world.untrack(user);move_session.composed.reset();move_session.value.reset();check(move_registry.release(11,*move_owner),"movement session release failed");move_owner.reset();std::lock_guard lock(state->mutex);state->closed=true;state->cv.notify_all();}
};
struct HandoffObserver final:net::WorldApplication {
    std::shared_ptr<HandoffState> state;net::WorldChannel channel;std::shared_ptr<HandoffObserverResource> resource;
    HandoffObserver(std::shared_ptr<HandoffState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)){}
    void open() override {resource=std::make_shared<HandoffObserverResource>();std::lock_guard lock(state->mutex);state->observer_channel=channel;state->observer_resource=resource;state->observer_online=true;state->cv.notify_all();}
    void receive(const net::Packet&) override {throw std::logic_error("Unexpected observer packet");}
    void close() override {resource.reset();std::lock_guard lock(state->mutex);state->observer_closed=true;state->cv.notify_all();}
};
void run(bool mode_first,bool fail,bool overflow,bool abort=false,bool multi=false,bool disconnect_observer=false,bool reset_observer=false,bool remove_observer=false){
    asio::io_context io;auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<HandoffState>();state->mode_first=mode_first;state->bad_position=fail;state->multi=multi;state->disconnect_observer=disconnect_observer;state->reset_observer=reset_observer;state->remove_observer=remove_observer;net::WorldServerOptions options;options.port=0;
    net::WorldServer server(io,options,registry,[state](const session::LoginSession& account,net::WorldChannel c)->std::unique_ptr<net::WorldApplication>{if(account.user_id==12)return std::make_unique<HandoffObserver>(state,std::move(c));state->channel=c;return std::make_unique<HandoffApp>(state,std::move(c));});server.start();std::jthread thread([&]{io.run();});
    struct Guard {net::WorldServer& server;std::jthread& thread;std::shared_ptr<HandoffState> state;~Guard(){{std::lock_guard lock(state->mutex);state->pause=true;state->first=true;state->second=true;state->cv.notify_all();}server.stop();if(thread.joinable())thread.join();}} guard{server,thread,state};
    session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
    std::unique_ptr<PeerSocket> observer;net::LegacyCipher observer_incoming(net::SessionSecrets{});
    if(multi) {
        session::LoginSession other{12,102,1,"observer",{}, {}};other.token.fill(0x43);registry->add(other);check(registry->select(12,102,0,now),"observer reservation failed");
        observer=std::make_unique<PeerSocket>(server.port());net::Packet request{1,0xa3,12,0,0,0};request.insert(request.end(),other.token.begin(),other.token.end());observer->write(net::frame(request));
        auto hello=observer->packet();observer_incoming.crypt(hello);check(hello.size()==20 && hello[0]==1 && hello[1]==0xa3,"observer greeting failed");state->wait([&]{return state->observer_online;});
    }
    PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});
    auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"greeting failed");std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());net::WorldXor xor_key(seed);
    auto send=[&](net::Packet p){outgoing.crypt(p);peer.write(net::frame(p));};send({4,1,42,0,0,0});auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0,42,0,0,0}),"selection acknowledgement AES differs");state->wait([&]{return state->waiting;});bool saw_data=false,saw_time=false;for(unsigned i=1;i<state->entry_packets;++i){auto packet=peer.packet();incoming.crypt(packet);check(packet.size()>=2,"truncated initial character packet");saw_data|=packet[0]==5 && packet[1]==1;saw_time|=packet[0]==0x14 && packet[1]==1;}check(saw_data && saw_time,"initial character data or clock not delivered");send({1,2});
    if(abort){state->channel.disconnect();state->channel.pause_input([state](bool active){std::lock_guard lock(state->mutex);state->cancelled=!active;state->cv.notify_all();});state->wait([&]{return state->cancelled;});state->release(state->first);state->release(state->second);state->wait([&]{return state->closed;});check(state->entries==0,"disconnected handoff entered world");return;}
    if(overflow){for(int i=0;i<32;++i){net::Packet packet(2046,0);packet[0]=9;packet[1]=1;send(std::move(packet));}peer.closed();state->release(state->first);state->release(state->second);state->wait([&]{return state->closed;});check(state->entries==0,"overflow entered world");return;}
    send({9,1,55});
    state->release(state->first);auto first=peer.packet();if(mode_first)xor_key.crypt(first);else incoming.crypt(first);check(first==net::Packet({0x77,0x77,static_cast<std::uint8_t>(mode_first)}),"first barrier or outbound cipher differs");
    {std::lock_guard lock(state->mutex);if(mode_first)check(state->entries==0,"entry ran before zone publication");else check(state->location,"zone-first barrier missing");}
    if(mode_first) {state->release(state->second);auto second=peer.packet();xor_key.crypt(second);check(second==net::Packet({0x78,0x77,1}),"zone-ready XOR marker differs");}
    if(fail){peer.closed();state->wait([&]{return state->closed;});check(state->failed && state->entries==1,"failed admission did not close or ran twice");return;}
    for(const auto expected:std::array<std::uint16_t,5>{0x405,0x403,0x201,0x51c,0xe01}) {
        auto packet=peer.packet();if(mode_first)xor_key.crypt(packet);else incoming.crypt(packet);
        check(packet.size()>=2 && (packet[0]|(packet[1]<<8))==expected,"TCP cell snapshot order or cipher differs");
    }
    auto entered=peer.packet();if(mode_first)xor_key.crypt(entered);else incoming.crypt(entered);check(entered==net::Packet({0x79,0x77}),"real actor did not enter in current cipher mode");
    auto continuity=peer.packet();if(mode_first)xor_key.crypt(continuity);else incoming.crypt(continuity);check(continuity==net::Packet({0x7a,0x77}),"inbound AES stream lost continuity");
    if(!mode_first){state->channel.enter_world();state->channel.send({0x78,0x77,1});auto switched=peer.packet();xor_key.crypt(switched);check(switched==net::Packet({0x78,0x77,1}),"outbound XOR transition differs");}
    net::Packet movement{1,5,0x81,0x34,0x12};for(float v:{multi?140.F:45.F,0.F,multi?81.F:40.F}){const auto bits=std::bit_cast<std::uint32_t>(v);for(unsigned i=0;i<4;++i)movement.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
    if(disconnect_observer || reset_observer || remove_observer){asio::error_code ignored;observer->socket.close(ignored);state->wait([&]{return state->observer_closed;});}
    send(movement);
    if(multi) {
        if(!disconnect_observer && !reset_observer && !remove_observer) {
        auto appeared=observer->packet();observer_incoming.crypt(appeared);auto speed=observer->packet();observer_incoming.crypt(speed);
        check(appeared.size()==29 && appeared[0]==1 && appeared[1]==2 && appeared[2]==42 && speed.size()==8 && speed[0]==0x1c && speed[1]==5 && speed[2]==42,"observer missed movement appearance/speed");
        auto observer_move=observer->packet();observer_incoming.crypt(observer_move);net::Packet expected_observer{1,5,42,0,0,0,0x81,0x34,0x12};expected_observer.insert(expected_observer.end(),movement.begin()+5,movement.end());
        check(observer_move==expected_observer,"observer missed retained priority movement");
        }
        auto corpse_leave=peer.packet();xor_key.crypt(corpse_leave);check(corpse_leave==corpse_removed(100),"mover missed old-cell corpse removal");
        if(!remove_observer) {
            auto observer_appearance=peer.packet();xor_key.crypt(observer_appearance);auto observer_speed=peer.packet();xor_key.crypt(observer_speed);
            check(observer_appearance.size()==29 && observer_appearance[0]==1 && observer_appearance[1]==2 && observer_appearance[2]==43 && observer_speed.size()==8 && observer_speed[0]==0x1c && observer_speed[1]==5,"mover missed observer snapshot");
        }
    }
    auto moved=peer.packet();xor_key.crypt(moved);
    net::Packet expected_move{1,5,42,0,0,0,0x81,0x34,0x12};expected_move.insert(expected_move.end(),movement.begin()+5,movement.end());
    check(moved==expected_move,"Composed movement packet did not reach TCP");
    auto checked=peer.packet();xor_key.crypt(checked);check(checked==net::Packet({0x7b,0x77}),"admitted movement check did not complete");
    {std::lock_guard lock(state->mutex);check(state->entries==1 && state->location,"entry did not run once after zone update");}asio::error_code ignored;peer.socket.close(ignored);
    if(multi){state->wait([&]{return state->closed;});observer->socket.close(ignored);state->wait([&]{return state->observer_closed;});check(state->observer_resource.expired(),"observer resource survived both owners");}
}
#ifndef ETERNITY_HANDOFF_FIXTURE_ONLY
int main(){try{run(false,false,false);run(true,false,false);run(false,true,false);run(false,false,true);run(false,false,false,true);run(false,false,false,false,true);run(false,false,false,false,true,true);run(false,false,false,false,true,false,true);run(false,false,false,false,true,false,false,true);std::cout<<"Two barrier orders, real zone admission/failure, AES continuity, bounded input overflow and disconnect passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#endif
