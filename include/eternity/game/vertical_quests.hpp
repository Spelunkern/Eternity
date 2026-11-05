#pragma once
#include "eternity/game/quest_start_items_actor_world.hpp"
#include "eternity/game/quest_requests_actor.hpp"
#include "eternity/game/mob_quests.hpp"
#include "eternity/game/zone_npcs.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/mob_drops.hpp"
#include "eternity/game/vertical_move_interactions.hpp"
#include "eternity/game/character_entry.hpp"
#include "eternity/game/experience_award.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/vertical_inventory.hpp"
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace eternity::game {
class CharacterReplica;
class VerticalSessionCharacter;
struct VerticalQuestRestoreResult {
    std::shared_ptr<QuestRuntime> runtime;
    std::size_t skipped_active=0,skipped_finished=0;
};
// PacketUserDBSetInfo 0046d780 cases 0506/0507: materialize the already
// restored rows into the canonical pooled runtime in order; failed allocations
// are skipped. The returned runtime lease also retains the pool and definitions;
// the supplied pool lease must retain its QuestRuntimePoolServices owner.
VerticalQuestRestoreResult vertical_quests_restore_runtime(const RestoredQuests&,std::uint32_t account,
    std::shared_ptr<QuestRuntimePool>,std::shared_ptr<const QuestDefinitions>);
struct VerticalQuestReplicaRestoreResult {
    VerticalQuestRestoreResult restored;
    std::vector<std::uint16_t> invalid_active;
};
// Restore exactly the DB-agent's retained rows (including its allocation
// omissions), not the unfiltered SQL snapshot. The caller serializes the
// replica with its actual save mutex throughout the synchronous projection.
VerticalQuestReplicaRestoreResult vertical_quests_restore_replica(const CharacterReplica&,std::uint32_t tick,
    std::shared_ptr<QuestRuntimePool>,std::shared_ptr<const QuestDefinitions>);
struct VerticalQuestSessionRestoreResult {
    std::shared_ptr<Actor> actor;
    VerticalQuestReplicaRestoreResult quests;
};
// The supplied lease must retain this exact session character, Actor, use
// state and their dependencies. The pipeline must not release/move its Actor
// while this alias exists. Call once on the serialized session executor.
VerticalQuestSessionRestoreResult vertical_quests_restore_session(std::shared_ptr<void> session_lease,
    VerticalSessionCharacter&,std::recursive_mutex& replica_mutex,std::uint32_t tick,
    std::shared_ptr<QuestRuntimePool>,std::shared_ptr<const QuestDefinitions>);
// For a retained VerticalWorldSession-like owner, obtain the character from
// that exact owner instead of accepting an unrelated lease and character.
// Session and character must stay on their serialized executor. If the session
// stores this result, it must release quest owners during teardown to break the
// resulting strong ownership cycle.
template<class Session>
VerticalQuestSessionRestoreResult vertical_quests_restore_selected_session(const std::shared_ptr<Session>& session,
    std::recursive_mutex& replica_mutex,std::uint32_t tick,std::shared_ptr<QuestRuntimePool> pool,
    std::shared_ptr<const QuestDefinitions> definitions) {
    if(!session) throw std::invalid_argument("Quest restore requires a retained session");
    auto* character=session->character();
    if(!character) throw std::invalid_argument("Quest restore requires a selected session character");
    auto* actor=character->pipeline().result().actor.get();
    if(!actor || actor->account_id()!=session->account_id()) throw std::invalid_argument("Quest restore session account differs from actor");
    return vertical_quests_restore_session(session,*character,replica_mutex,tick,std::move(pool),std::move(definitions));
}
struct VerticalQuestEntryRows {
    std::vector<QuestPacketEntry> active;
    std::vector<FinishedQuest> finished;
};
// SendCharacterQuest 004835f0 / SendCharacterQuestFinish 00483730 use the
// actual allocated lists, including allocation omissions and live ordering.
VerticalQuestEntryRows vertical_quests_entry_rows(const QuestRuntime&);
class VerticalQuestEntryPorts {
public:
    virtual ~VerticalQuestEntryPorts()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual void lock_active()=0;
    virtual void unlock_active() noexcept=0;
    virtual void lock_finished()=0;
    virtual void unlock_finished() noexcept=0;
    virtual void send_self(const net::Packet&)=0;
};
// SendCharacterQuest 004835f0 / SendCharacterQuestFinish 00483730. The caller
// binds these ports to this runtime's user, retains all owners, and serializes
// callbacks; callbacks must not mutate either quest list during this operation.
// Returns false without side effects for other character-entry steps.
bool vertical_quests_send_entry_step(QuestRuntime&,CharacterEntryStep,VerticalQuestEntryPorts&);
class VerticalQuestEntryFinishedLock {
public:
    virtual ~VerticalQuestEntryFinishedLock()=default;
    virtual void lock_finished(Actor&)=0;
    virtual void unlock_finished(Actor&) noexcept=0;
};
// The runtime owns the distinct finished-list mutex; the Actor is borrowed
// only for identity checks while the caller retains it during each operation.
class VerticalQuestRuntimeFinishedLock final:public VerticalQuestEntryFinishedLock {
public:
    VerticalQuestRuntimeFinishedLock(Actor& actor,std::shared_ptr<QuestRuntime> runtime):actor_(&actor),runtime_(std::move(runtime)) {
        if(!runtime_) throw std::invalid_argument("Finished quest lock requires a runtime");
    }
    void lock_finished(Actor& actor) override {if(&actor!=actor_) throw std::logic_error("Finished quest lock actor mismatch");runtime_->finished_mutex().lock();}
    void unlock_finished(Actor& actor) noexcept override {if(&actor!=actor_) std::terminate();runtime_->finished_mutex().unlock();}
private:
    Actor* actor_;std::shared_ptr<QuestRuntime> runtime_;
};
// QuestSuccessResult calls the inner QuestAddItem overload, not the outer
// start-item wrapper. The name provider must resolve the actual definition's
// original item-name bytes; all owners outlive synchronous delivery.
class VerticalQuestRewardItems final {
public:
    using ItemName=std::function<std::string(const ItemDefinition&)>;
    VerticalQuestRewardItems(Actor&,QuestStartItemsActor&,ItemName);
    bool owns_actor(const Actor& actor) const noexcept {return items_.owns_actor(actor);}
    std::optional<QuestRewardItemReceipt> receive(std::uint8_t type,std::uint8_t id,std::uint8_t count);
private:
    QuestStartItemsActor& items_;ItemName name_;
};
// The options provider samples the live party and skill owners at award time.
// QuestSuccessResult supplies skip_modifiers; the other options remain live.
class VerticalQuestRewardExperience final {
public:
    using Options=std::function<ExperienceAwardOptions()>;
    VerticalQuestRewardExperience(Actor&,PlayerCore&,const StatusRules&,const Experience&,ExperienceAwardPorts&,Options);
    bool owns_actor(const Actor& actor) const noexcept {return &actor_==&actor;}
    void award(std::uint32_t source,std::int32_t amount,bool skip_modifiers);
private:
    Actor& actor_;PlayerCore& player_;const StatusRules& rules_;const Experience& curve_;ExperienceAwardPorts& ports_;Options options_;
};
class VerticalQuestRewardServices {
public:
    virtual ~VerticalQuestRewardServices()=default;
    virtual void add_experience(std::uint32_t,std::int32_t,bool)=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void fill_game_log_main(std::span<std::uint8_t>)=0;
    virtual std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t,std::uint8_t,std::uint8_t)=0;
    virtual void send_self(const net::Packet&)=0;
    virtual void send_game_log(const net::Packet&)=0;
};
// Bind reward completion to the same live Actor, quest heap and QuestRuntime.
// All other reward methods forward to the required actual reward services;
// their Actor/transport/item owners and these references must outlive calls.
// ps_game.exe QuestSuccessResult 0047ee40 calls QuestRemove 0047e150.
class VerticalQuestRewardBinding final:public QuestRewardWorld,private QuestRuntimeWorld {
public:
    VerticalQuestRewardBinding(Actor&,QuestRuntime&,QuestRuntimePool&,VerticalQuestRewardServices&);
    void add_experience(std::uint32_t,std::int32_t,bool) override;
    void send_database(const net::Packet&) override;
    void fill_game_log_main(std::span<std::uint8_t>) override;
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t,std::uint8_t,std::uint8_t) override;
    void send_self(const net::Packet&) override;
    void remove_quest(RestoredQuest&,bool) override;
    void send_game_log(const net::Packet&) override;
private:
    void validate() const;
    Actor& actor_;QuestRuntime& runtime_;QuestRuntimePool& pool_;VerticalQuestRewardServices& services_;
};
class VerticalQuestPorts : public QuestStartItemsActorPorts, public QuestRequestsActorTransport {
public:
    virtual void send_database(Actor&,const net::Packet&) override=0;
    virtual void lock_quests(Actor&)=0;
    virtual void unlock_quests(Actor&) noexcept=0;
    virtual std::uint32_t quest_tick_ms()=0;
    virtual void fill_game_log_main(Actor&,std::span<std::uint8_t>)=0;
    virtual void send_game_log(Actor&,const net::Packet&)=0;
    virtual QuestRewardWorld& reward_world(Actor&)=0;
    virtual QuestStartLogContext start_log_context(Actor&)=0;
    virtual void send_start_log(Actor&,const std::array<std::uint8_t,144>&)=0;
};
// Compose the actor's actual experience, inventory and quest transport owners.
// The ports implement real DB/log delivery; Actor owns the client transport.
class VerticalQuestRewardComposition final:public VerticalQuestRewardServices {
public:
    VerticalQuestRewardComposition(Actor&,VerticalQuestRewardExperience&,VerticalQuestRewardItems&,VerticalQuestPorts&);
    bool owns_actor(const Actor& actor) const noexcept {return &actor_==&actor;}
    void add_experience(std::uint32_t,std::int32_t,bool) override;
    void send_database(const net::Packet&) override;
    void fill_game_log_main(std::span<std::uint8_t>) override;
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t,std::uint8_t,std::uint8_t) override;
    void send_self(const net::Packet&) override;
    void send_game_log(const net::Packet&) override;
private:
    Actor& actor_;VerticalQuestRewardExperience& experience_;VerticalQuestRewardItems& items_;VerticalQuestPorts& ports_;
};
class VerticalQuestPacketActions {
public:
    virtual ~VerticalQuestPacketActions()=default;
    virtual void cancel_action_exchange(Actor&)=0;
    virtual void end_shop(Actor&)=0;
    virtual void close_connection(Actor&,std::uint32_t reason)=0;
};
enum class VerticalQuestPacketResult {unhandled,malformed,handled,closed};
class VerticalNpcPacketActions {
public:
    virtual ~VerticalNpcPacketActions()=default;
    virtual void close_connection(Actor&,std::uint32_t reason)=0;
};
enum class VerticalNpcPacketResult {unhandled,malformed,handled,closed};
// ps_game.exe CUser::PacketNpc 00467510. The same interaction state must be
// passed to movement and zone-leave handlers. The owner serializes NPC removal
// and retains the currently used NPC until that state is cleared or replaced.
VerticalNpcPacketResult vertical_npc_request(Actor&,std::span<const std::uint8_t>,
    ZoneNpcs&,VerticalNpcInteraction&,VerticalNpcPacketActions&);
// All owners, including use-state, zone and ports, refer to the same live user.
// Serialize calls; the canonical NPC pool must outlive this adapter and its calls.
// The reward provider must remove through this same QuestRuntime. The actor
// lease must also retain its use-state; definitions, pool and services must
// outlive all runtime and item leases. Actor account and runtime pool binding
// must remain stable through every callback.
// ps_game.exe: QuestStartRequest 0047f220, QuestUpdateKillMOB 00480140,
// QuestEndRequest 0047f2a0, QuestEnd 0047ec40, QuestEndSelect 0047ebe0.
class VerticalQuests final : private QuestStartItemsActorWorldPorts, private QuestRequestsActorPorts, private MobQuestWorld {
public:
    VerticalQuests(QuestStartItemsActorWorldOwners,QuestStartItemsActorUseState,ZoneNpcs&,NpcPool&,VerticalQuestPorts&);
    std::uint32_t actor_id() const noexcept {return owners_.actor->id();}
    bool owns_actor(const Actor& actor) const noexcept {return owners_.actor.get()==&actor;}
    bool owns_npc_zone(const ZoneNpcs& zone) const noexcept {return npcs_==&zone;}
    // The caller must retain this quest owner while using the runtime.
    QuestRuntime& runtime_for(Actor& actor);
    std::shared_ptr<QuestRuntime> runtime_owner_for(Actor& actor) {return (void)runtime_for(actor),owners_.runtime;}
    void lock_save_for(Actor& actor);
    void unlock_save_for(Actor& actor) noexcept;
    void deliver_save_for(Actor& actor,const net::Packet& packet);
    bool active(std::uint16_t quest);
    bool start(std::uint16_t row,std::uint32_t npc);
    void end(std::uint16_t row,std::uint32_t npc);
    void select_reward(std::uint16_t quest,std::uint8_t selection);
    void abandon(std::uint16_t quest);
    // PacketQuest 0046be10. The caller retains the packet and all owners across
    // callbacks and routes only the quest opcode family to this method.
    VerticalQuestPacketResult receive(std::span<const std::uint8_t>,VerticalQuestPacketActions&);
    void kill_mob(std::uint16_t mob,const MobQuestLinks&);
    void kill_player();
    void pick_item(std::int32_t type,std::int32_t id,const ItemQuestLinks&);
    void update_timers(std::uint32_t tick);
    // Save this same account's live runtime through its real quest lock and DB
    // port. The caller keeps all owners and packet consumers live during calls.
    void save_runtime(std::uint32_t tick);
    // Uses this actor's quest clock, active lock and client transport; the
    // required provider owns the same user's distinct finished-list lock.
    bool send_entry_step(CharacterEntryStep,VerticalQuestEntryFinishedLock&);
    // Zone traversal must supply the exact retained Actor object, not just an
    // account or character ID that could resolve to a different live session.
    void update_timers_for(Actor&,std::uint32_t tick);
private:
    friend class VerticalQuestNpcPackets;
    void validate_binding() const;
    std::optional<QuestStartNpc> find_start_npc(Actor&,std::uint32_t) override;
    bool require_start_item(Actor&,std::uint8_t,std::uint8_t,bool) override;
    QuestStartLogContext start_log_context(Actor&) override;
    QuestStartLogNpc start_log_npc(Actor&,const QuestStartNpc&) override;
    void send_start_log(Actor&,const std::array<std::uint8_t,144>&) override;
    std::uint32_t local_make_time(Actor&) override;
    std::uint32_t tick_ms(Actor&) override;
    void send_database(Actor&,const net::Packet&) override;
    void send_client(Actor&,const net::Packet&) override;
    void lock_quests(Actor&) override;
    void unlock_quests(Actor&) noexcept override;
    std::optional<QuestRequestNpc> find_npc(Actor&,std::uint32_t) override;
    void fill_game_log_main(Actor&,std::span<std::uint8_t>) override;
    void send_game_log(Actor&,const net::Packet&) override;
    QuestRewardWorld& reward_world(Actor&) override;
    std::uint32_t tick_ms() override;
    RestoredQuest* find_quest(std::uint16_t) override;
    void send_database(const net::Packet&) override;
    void send_self(const net::Packet&) override;
    void complete_quest(RestoredQuest&,std::int32_t,bool) override;
    QuestStartItemsActorWorldOwners owners_; ZoneNpcs* npcs_; NpcPool& npc_pool_; VerticalQuestPorts& ports_;
    QuestRequestsActorWorld requests_; QuestStartItemsActorWorld starts_;
    struct StartFrame {NpcPool::Lease npc;std::string name;}; StartFrame* frame_=nullptr;std::size_t operation_depth_=0;
};
enum class VerticalQuestNpcPacketResult {unhandled,malformed,handled,closed};
struct ZoneLeaveShopState;
class ZoneLeaveInteractionServices;
// One admitted session's quest/NPC families. The actor, quest owner, zone,
// interaction and actual packet actions remain live and serialized for calls;
// movement and zone leave must use this same interaction object.
// Clean up that interaction before destroying the adapter; synchronize after
// movement/leave so a detached NPC can be recycled as soon as it is unused.
class VerticalQuestNpcPackets final {
public:
    VerticalQuestNpcPackets(Actor&,VerticalQuests&,ZoneNpcs&,VerticalNpcInteraction&,VerticalQuestPacketActions&,VerticalNpcPacketActions&);
    VerticalQuestNpcPacketResult receive(std::span<const std::uint8_t>);
    void synchronize_interaction();
    // Replace the corresponding existing movement/leave interaction step;
    // do not run that step twice. All supplied services are real owners.
    void move_interactions(bool wide,VerticalMoveInteractionServices&);
    void leave_interactions(ZoneLeaveShopState&,ZoneLeaveInteractionServices&);
    // After old-zone interaction cleanup and outside quest callbacks, replace
    // both NPC lookups together; the caller retains both zones during transfer.
    void rebind_npc_zone(ZoneNpcs& expected,ZoneNpcs& destination);
    bool uses_interaction(const VerticalNpcInteraction& value) const noexcept {return &interaction_==&value;}
    const VerticalNpcInteraction& interaction() const noexcept {return interaction_;}
private:
    Actor& actor_;VerticalQuests& quests_;ZoneNpcs* zone_;VerticalNpcInteraction& interaction_;
    VerticalQuestPacketActions& quest_actions_;VerticalNpcPacketActions& npc_actions_;
    NpcPool::Lease npc_lease_;std::size_t dispatch_depth_=0;
};
// One session's existing quest, reward and NPC packet owners. Upstream ports
// supply real clock, DB, client and log operations; reward_world is bound to
// this runtime internally. All referenced services and use-state must outlive
// this object, and NPC interaction cleanup must precede its destruction.
class VerticalQuestSession final {
public:
    VerticalQuestSession(QuestStartItemsActorWorldOwners,QuestStartItemsActorUseState,ZoneNpcs&,NpcPool&,VerticalQuestPorts&,
        PlayerCore&,const StatusRules&,const Experience&,ExperienceAwardPorts&,VerticalQuestRewardExperience::Options,
        VerticalQuestRewardItems::ItemName,VerticalNpcInteraction&,VerticalQuestPacketActions&,VerticalNpcPacketActions&);
    ~VerticalQuestSession();
    VerticalQuestSession(const VerticalQuestSession&)=delete;
    VerticalQuestSession& operator=(const VerticalQuestSession&)=delete;
    VerticalQuests& quests() noexcept {return quests_;}
    VerticalQuestNpcPackets& packets() noexcept {return packets_;}
    const std::shared_ptr<Actor>& actor_owner() const noexcept {return owners_.actor;}
    bool uses_ports(const VerticalQuestPorts&) const noexcept;
    const VerticalQuestPorts& upstream_ports() const noexcept;
private:
    class Ports;
    QuestStartItemsActorWorldOwners owners_;std::unique_ptr<Ports> ports_;
    QuestStartItemsActor grants_;VerticalQuestRewardExperience experience_;VerticalQuestRewardItems items_;
    VerticalQuestRewardComposition reward_services_;VerticalQuestRewardBinding reward_;
    VerticalQuests quests_;VerticalQuestNpcPackets packets_;
};
// CZone::GetPickItem 004174a0 resolves linked quest IDs with QuestFind
// 0047e4b0. All non-quest pickup services remain the real inventory owner.
class VerticalQuestPickupWorld final:public ItemPickupWorld {
public:
    VerticalQuestPickupWorld(Actor&,VerticalQuests&,ItemPickupWorld&);
    std::uint32_t pick_tick_ms() override {return world_.pick_tick_ms();}
    std::uint32_t pick_party_id(Actor& a) override {return world_.pick_party_id(a);}
    UserPosition pick_position(const Actor& a) override {return world_.pick_position(a);}
    bool pick_quest_active(Actor&,std::uint16_t) override;
    std::uint32_t lock_inventory_party(Actor& a) override {return world_.lock_inventory_party(a);}
    void unlock_inventory_party(std::uint32_t n) noexcept override {world_.unlock_inventory_party(n);}
    std::optional<bool> inventory_party_kind(const Actor& a) override {return world_.inventory_party_kind(a);}
    void send_inventory_party(Actor& a,const net::Packet& p) override {world_.send_inventory_party(a,p);}
    void send_inventory_database(const net::Packet& p) override {world_.send_inventory_database(p);}
    std::string inventory_item_name(const ItemDefinition& i) override {return world_.inventory_item_name(i);}
    std::string inventory_drop_user_name(Actor& a,std::uint32_t id,bool zone) override {return world_.inventory_drop_user_name(a,id,zone);}
    bool return_inventory_item(Actor& a,GroundItem& i) override {return world_.return_inventory_item(a,i);}
    void send_money_database(const net::Packet& p) override {world_.send_money_database(p);}
    GameLogSubject money_log_subject(const Actor& a) override {return world_.money_log_subject(a);}
    std::chrono::sys_seconds money_local_calendar() override {return world_.money_local_calendar();}
    void send_money_log(const net::Packet& p) override {world_.send_money_log(p);}
private:
    Actor& actor_;VerticalQuests& quests_;ItemPickupWorld& world_;
};
// KillPCTo 0045aed0 runs QuestUpdateKillPC first. The required continuation
// executes the remainder of KillPCTo (log, counters and notices), not its
// complete entry point. Its owner retains both actors for the synchronous call.
class VerticalQuestKillPcContinuation {
public:
    virtual ~VerticalQuestKillPcContinuation()=default;
    virtual void after_quest(Actor& killer,Actor& victim)=0;
};
void vertical_quests_kill_pc_to(Actor& killer,Actor& victim,VerticalQuests&,VerticalQuestKillPcContinuation&);
// CZone::UpdateQuest 00422320 traverses the actual zone user list while
// holding its recursive lock. The returned aliasing lease must retain the same
// actor, scene, quest adapter and user-owned state through all callbacks. The
// resolver may not mutate zone membership while obtaining the lease.
class VerticalQuestZoneUsers {
public:
    virtual ~VerticalQuestZoneUsers()=default;
    virtual std::shared_ptr<VerticalQuests> retain_quests(Actor&)=0;
};
class VerticalQuestSessionResolver {
public:
    virtual ~VerticalQuestSessionResolver()=default;
    // Return the actual live session lease without changing zone membership.
    virtual std::shared_ptr<VerticalQuestSession> retain(Actor&)=0;
};
// Retain the matching session while dispatching its quest and NPC families.
VerticalQuestNpcPacketResult vertical_quests_session_receive(Actor&,std::span<const std::uint8_t>,VerticalQuestSessionResolver&);
// Replace the corresponding movement and leave interaction steps, not the
// entire movement or leave operation. Services must be the live owners.
void vertical_quests_session_move_interactions(Actor&,bool,VerticalMoveInteractionServices&,VerticalQuestSessionResolver&);
void vertical_quests_session_leave_interactions(Actor&,ZoneLeaveShopState&,ZoneLeaveInteractionServices&,VerticalQuestSessionResolver&);
bool vertical_quests_session_send_entry_step(Actor&,CharacterEntryStep,VerticalQuestEntryFinishedLock&,VerticalQuestSessionResolver&);
void vertical_quests_session_save_runtime(Actor&,std::uint32_t,VerticalQuestSessionResolver&);
// Retains the actor's matching quest session throughout the synchronous 0205
// handler and forwards all non-quest pickup operations to the live owner.
// The resolver must not mutate the Actor's attack state or the packet.
VerticalInventoryResult vertical_quests_session_pickup_request(Actor&,std::span<const std::uint8_t>,
    ZoneItems&,const ItemQuestLinks&,ItemPickupWorld&,GroundItemPool&,
    VerticalInventoryActions&,VerticalQuestSessionResolver&);
// Caller retains killer, victim and continuation for the synchronous call;
// the resolver must return the killer's actual live composed quest session.
void vertical_quests_session_kill_pc_to(Actor& killer,Actor& victim,VerticalQuestSessionResolver&,VerticalQuestKillPcContinuation&);
// Adapts a real session registry to the existing zone timer traversal; the
// alias retains the entire session and therefore all composed quest owners.
class VerticalQuestSessionZoneUsers final:public VerticalQuestZoneUsers {
public:
    explicit VerticalQuestSessionZoneUsers(VerticalQuestSessionResolver& resolver):resolver_(resolver) {}
    std::shared_ptr<VerticalQuests> retain_quests(Actor&) override;
private:
    VerticalQuestSessionResolver& resolver_;
};
class VerticalQuestMobActorResolver {
public:
    virtual ~VerticalQuestMobActorResolver()=default;
    // Resolve the retained actual Actor represented by this live death user.
    virtual std::shared_ptr<Actor> retain(MobDeathUser&)=0;
};
// Bind CMob::Death's quest callback to a retained actor and its exact composed
// quest session. The death/user/party and all resolver dependencies outlive
// the synchronous callback; no experience, drop or other service is replaced.
class VerticalQuestSessionMobCredit final {
public:
    VerticalQuestSessionMobCredit(VerticalQuestMobActorResolver& actors,VerticalQuestSessionResolver& sessions,const MobQuestLinks& links)
        :actors_(actors),sessions_(sessions),links_(links) {}
    void apply(MobDeathUser&,MobRuntime&);
private:
    VerticalQuestMobActorResolver& actors_;VerticalQuestSessionResolver& sessions_;const MobQuestLinks& links_;
};
class VerticalQuestLootActorResolver {
public:
    virtual ~VerticalQuestLootActorResolver()=default;
    // Resolve the actual Actor represented by this retained loot user.
    virtual std::shared_ptr<Actor> retain(MobLootUser&)=0;
};
// Required quest callbacks for CMob::ItemDrop. The item links, loot users,
// actors and sessions are retained and serialized by the caller; all actual
// drop selection, item creation, delivery, party and log services remain real.
class VerticalQuestSessionLoot final {
public:
    VerticalQuestSessionLoot(VerticalQuestLootActorResolver& actors,VerticalQuestSessionResolver& sessions,const ItemQuestLinks& links)
        :actors_(actors),sessions_(sessions),links_(links) {}
    std::span<const std::uint16_t> end_quests(const MobLootItem&) const;
    bool has_quest(MobLootUser&,std::uint16_t);
private:
    VerticalQuestLootActorResolver& actors_;VerticalQuestSessionResolver& sessions_;const ItemQuestLinks& links_;
};
// Wrap the existing complete loot world; only quest link/lookup callbacks are
// supplied here. The caller retains all owners across the serialized loot pass.
class VerticalQuestMobLootWorld final:public MobLootWorld {
public:
    VerticalQuestMobLootWorld(MobLootWorld& world,VerticalQuestSessionLoot& quests):world_(world),quests_(quests) {}
    const MobLootItem* item(std::uint8_t type,std::uint8_t id) override {return world_.item(type,id);}
    std::span<const std::uint16_t> end_quests(const MobLootItem& item) override {return quests_.end_quests(item);}
    bool has_quest(MobLootUser& user,std::uint16_t id) override {return quests_.has_quest(user,id);}
    const MobLootItem* grade_item(std::uint16_t grade) override {return world_.grade_item(grade);}
    const MobLootItem* sequence_item(std::uint16_t level,std::uint32_t object) override {return world_.sequence_item(level,object);}
    void deliver(const MobLootItem& item,MobLootUser* user,MobLootParty* party,bool sequence) override {world_.deliver(item,user,party,sequence);}
    void guild_ranking(MobLootUser& user,std::uint16_t points) override {world_.guild_ranking(user,points);}
    void log_money(std::int32_t amount) override {world_.log_money(amount);}
    void ground_money(std::uint32_t owner,std::int32_t amount) override {world_.ground_money(owner,amount);}
    void give_money(MobLootUser& user,std::int32_t amount) override {world_.give_money(user,amount);}
    bool zone_item(MobLootUser& credited,MobLootUser* candidate) override {return world_.zone_item(credited,candidate);}
private:
    MobLootWorld& world_;VerticalQuestSessionLoot& quests_;
};
// A single zone pass. The caller owns the CWorldThread minute cadence and must
// visit every active primary and dynamic zone under its world-list lock.
void vertical_quests_update_zone(ZoneUsers&,std::uint32_t tick,VerticalQuestZoneUsers&);
// Resolve each credited live user to that user's retained quest owner. The
// resolver and all returned owners survive the entire mob_death callback.
class VerticalQuestMobUsers {
public:
    virtual ~VerticalQuestMobUsers()=default;
    virtual VerticalQuests& quests_for(MobDeathUser&)=0;
};
// CMob::Death 004910f0 invokes QuestUpdateKillMOB 00480140 for each eligible
// recipient. Wrap the real death services; quest_kill is supplied by the same
// user's actual VerticalQuests, and all other services are forwarded unchanged.
class VerticalQuestMobDeath final:public MobDeathWorld {
public:
    VerticalQuestMobDeath(MobDeathWorld& world,VerticalQuestMobUsers& users,const MobQuestLinks& links):world_(world),users_(&users),links_(&links) {}
    VerticalQuestMobDeath(MobDeathWorld& world,VerticalQuestSessionMobCredit& credit):world_(world),credit_(&credit) {}
    void send_death(MobRuntime& m,const net::Packet& p,std::uint8_t n) override {world_.send_death(m,p,n);}
    MobDeathUser* find_user(MobRuntime& m,std::uint32_t id) override {return world_.find_user(m,id);}
    MobDeathParty* party(MobDeathUser& u) override {return world_.party(u);}
    void lock_party(MobDeathParty& p) override {world_.lock_party(p);}
    void unlock_party(MobDeathParty& p) noexcept override {world_.unlock_party(p);}
    bool in_reward_range(MobDeathUser& a,MobDeathUser& b,std::int32_t n) override {return world_.in_reward_range(a,b,n);}
    void add_experience(MobDeathUser& u,std::uint32_t id,std::int32_t n) override {world_.add_experience(u,id,n);}
    void drop_items(MobRuntime& m,MobDeathUser* u,MobDeathParty* p) override {world_.drop_items(m,u,p);}
    void quest_kill(MobDeathUser&,MobRuntime&) override;
    std::optional<std::int32_t> obelisk_country(MobRuntime& m,std::int32_t t) override {return world_.obelisk_country(m,t);}
    void destroy_obelisk(MobRuntime& m,std::int32_t c,std::int32_t t) override {world_.destroy_obelisk(m,c,t);}
    void destroy_linked_group(MobRuntime& m,std::int32_t t) override {world_.destroy_linked_group(m,t);}
    void remove_obelisk_member(MobRuntime& m,void* p) override {world_.remove_obelisk_member(m,p);}
    void remove_linked_member(MobRuntime& m,void* p) override {world_.remove_linked_member(m,p);}
    void log_death(MobRuntime& m,MobDeathUser* u,bool f) override {world_.log_death(m,u,f);}
    void clear_applied_skills(MobRuntime& m) override {world_.clear_applied_skills(m);}
private:
    MobDeathWorld& world_;VerticalQuestMobUsers* users_=nullptr;const MobQuestLinks* links_=nullptr;VerticalQuestSessionMobCredit* credit_=nullptr;
};
}
