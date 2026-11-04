#pragma once
#include "eternity/game/effect_cycle.hpp"
#include "eternity/game/resources.hpp"
#include "eternity/game/experience_award.hpp"
#include "eternity/game/user_shape.hpp"
#include "eternity/game/zone_users.hpp"
#include "eternity/game/actor_actions.hpp"
#include "eternity/game/duel.hpp"
#include "eternity/game/item_exchange.hpp"
#include "eternity/game/zone_lifecycle.hpp"
#include "eternity/game/mob_drops_effect_transport.hpp"

namespace eternity::game {
class MoneyWorld;
class GroundItemPool;
class InventoryWorld;
class ItemPickupWorld;
class InventoryRemovalWorld;
class ItemDropWorld;
enum class ItemDropResult;
enum class InventoryReceipt;
// Original constructor/reset preserve three inherited DWORDs; only deadline is
// cleared. Raw code is kept unsigned even where original callers interpret it.
struct ActorDeathMetadata {
    std::int32_t penalty;
    std::uint32_t soul,deadline,code;
};
struct ActorInventoryItem {GroundItem* item;GroundItemPool* pool;};
// Required scene/transport services. Implementations own zone binding, combat
// and connection queues; none default to no-op.
class ActorScene {
public:
    virtual ~ActorScene() = default;
    virtual void send_self(const net::Packet&) noexcept = 0;
    virtual void send_nearby(const net::Packet&) = 0; // PSendView, radius 60, original channel 5.
    virtual void send_cell_view(const net::Packet&) = 0; // SendView, current zone grid coordinates.
    virtual std::optional<bool> party_kind() const noexcept = 0;
    virtual void send_party(const net::Packet&) noexcept = 0;
    virtual void reset_damage() = 0;
    virtual std::optional<std::uint32_t> allocate_effect_id() noexcept = 0;
    virtual void release_effect_id(std::uint32_t) noexcept = 0;
    virtual std::uint32_t tick_ms() const noexcept = 0;
    virtual void stop_target_mob(std::uint32_t id) = 0;
    virtual void taunt(std::uint16_t aggro,float range) = 0;
    virtual ZoneUsers* user_zone() = 0;
    virtual UserShapeSocial user_shape_social() = 0;
    virtual void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed) = 0;
};
struct ActorAppearance {
    bool visible=true;
    std::int32_t vehicle_mode=0,vehicle_shape=0,vehicle_addition=0;
    std::uint32_t vehicle_partner=0;
    UserShapeIdentity identity;
    std::optional<CopiedUserShape> copied_user;
    bool attackable=true;
};
struct ActorAttackState {
    std::int32_t mode=0,recovery=1;
    std::uint32_t previous_tick=0,next_tick=0;
    std::uint8_t motion=0; // CUser+1448 attack motion; not presentation+1434.
    std::uint32_t resume_tick=0; // CUser+1450.
};
struct ActorEquipmentRules {
    // Immutable definition/enchant sources must outlive the actor; its inventory is owned separately.
    const ItemDefinitions& items;
    const EnchantValues& enchant;
    const MobAttackTemplates& mobs;
};
// Single actor executor; its owner must serialize commands, ticks and scene changes.
class Actor final:public EffectRemovalWorld,public EffectActivationObserver,private ResourceSink {
public:
    Actor(std::uint32_t id,EffectStatus status,PlayerResources current,std::vector<PersistedEffect> replica,ActorScene& scene,
        ActorEquipmentRules equipment,std::vector<PlayerItem> inventory = {});
    // Explicit DB binding, or compose ActorScene with ActorEffectDatabase for
    // the source-compatible constructor (also used by ActorLoadPipeline).
    // An unbound inert actor may be inspected/shut down; effect-removal execution
    // requires the transport. Observer callbacks are noexcept and fail fast if
    // called unbound, rather than silently losing immediate DB requests.
    Actor(std::uint32_t id,EffectStatus status,PlayerResources current,std::vector<PersistedEffect> replica,ActorScene& scene,
        ActorEquipmentRules equipment,ActorEffectDatabase& database,std::vector<PlayerItem> inventory = {});
    bool effect_database_bound() const noexcept {return effect_database_!=nullptr;}
    ~Actor(); // Bound scene and pools owning retained inventory must outlive this actor.
    EffectStatus& status() noexcept {return status_;}
    const EffectStatus& status() const noexcept {return status_;}
    ActorAppearance& appearance() noexcept {return appearance_;}
    const ActorAppearance& appearance() const noexcept {return appearance_;}
    std::uint32_t id() const noexcept {return id_;}
    std::uint32_t& account_id() noexcept {return account_id_;}
    std::uint32_t account_id() const noexcept {return account_id_;}
    std::uint32_t& money() noexcept {return money_;}
    std::uint32_t money() const noexcept {return money_;}
    void send_money_database(MoneyWorld&);
    std::uint32_t receive_money(std::uint32_t amount,MoneyWorld&);
    std::uint32_t receive_money_item(GroundItem&,MoneyWorld&,GroundItemPool&);
    bool load_inventory_item(PlayerItem,GroundItemPool&); // Resolved SQL row, no DB/client notices.
    // Serialized raw ownership primitives, QuestAddItem 0047e6f0 / QuestFindItem
    // 0047f790/0047f8e0. Borrowed pointers survive only until release/reuse; retain
    // Actor, definition and owning pool through callbacks, and revalidate identity.
    // inventory() value-only rows still occupy slots but have no retained borrow.
    bool inventory_slot_empty(std::uint8_t bag,std::uint16_t slot) const;
    std::optional<ActorInventoryItem> retained_inventory_item(std::uint8_t bag,std::uint16_t slot);
    // Claim an exclusively owned, detached, already allocated item from this pool.
    // Caller guarantees pool membership and no other Actor/zone ownership. No
    // allocation, merge, craft decoding, packets, exchange or equipment effects.
    bool claim_inventory_item(std::uint8_t bag,std::uint16_t slot,GroundItem&,GroundItemPool&);
    // Pointer payload remains authoritative after claim (default craft follows
    // publication). Sync full mirror before DB send; count sync precedes both
    // DB/client callbacks. Neither operation sends/applies persistence packets.
    bool sync_inventory_item(std::uint8_t bag,std::uint16_t slot,GroundItem* expected=nullptr);
    bool set_inventory_item_count(std::uint8_t bag,std::uint16_t slot,std::uint8_t count,GroundItem* expected=nullptr);
    // Release directly through the bound pool, preserving the pointer's current
    // count. Invoke after required DB/client sends. No ItemRemove side effects.
    bool release_inventory_item(std::uint8_t bag,std::uint16_t slot,GroundItem* expected=nullptr);
    ItemDropResult drop_item(std::uint8_t bag,std::uint16_t slot,std::int32_t count,ItemDropWorld&,GroundItemPool&);
    bool remove_item(std::uint8_t bag,std::uint16_t slot,InventoryRemovalWorld&,bool notify=true);
    bool delete_items(std::int32_t type,std::int32_t type_id,InventoryRemovalWorld&);
    std::array<std::uint16_t,13>& equipment_database_quality() noexcept {return equipment_database_quality_;}
    const std::array<std::uint16_t,13>& equipment_database_quality() const noexcept {return equipment_database_quality_;}
    void send_equipment(std::uint8_t slot);
    InventoryReceipt receive_item(GroundItem&,InventoryWorld&,GroundItemPool&);
    InventoryReceipt pickup_item(ZoneItems&,std::uint32_t id,bool checks,const ItemQuestLinks&,ItemPickupWorld&,GroundItemPool&);
    UserRelations& relations() noexcept {return relations_;}
    const UserRelations& relations() const noexcept {return relations_;}
    ActorAttackState& attack_state() noexcept {return attack_;}
    const ActorAttackState& attack_state() const noexcept {return attack_;}
    // Bind inherited allocation/pool values explicitly; no invented zero seed.
    // One-time construction projection clears deadline like CUser004499d0.
    void initialize_death_metadata(ActorDeathMetadata inherited);
    ActorDeathMetadata& death_metadata();
    const ActorDeathMetadata& death_metadata() const;
    bool death_metadata_bound() const noexcept {return death_metadata_.has_value();}
    // Metadata projection ONLY of ResetCharacter0044a050, not a full reset.
    void reset_death_metadata();
    ActorActionState& actions() noexcept {return actions_;}
    const ActorActionState& actions() const noexcept {return actions_;}
    std::uint32_t& guard_target_after() noexcept {return guard_target_after_;}
    std::uint32_t guard_target_after() const noexcept {return guard_target_after_;}
    const PlayerResources& resources() const noexcept {return resources_.current();}
    // Rebirth bridge: silent raw stores to the existing resource owner. No heal,
    // clamp, notifications, damage reset or change to attack_state.mode (+142c).
    void replace_resources_raw(PlayerResources) noexcept;
    // Pure original SendRecoverChange, not recover/heal: view snapshot followed
    // by live current-energy group notice. Existing scene transports are required.
    // Execute on the actor serialized owner; actor/scene/group leases must survive
    // both synchronous sends (including reentry). This method retains no state.
    void send_recover_change(PlayerResources);
    // Existing ZoneLifecycleState owns original CUser+1434. Distinct from +1448.
    std::uint8_t presentation_motion() const noexcept {return zone_lifecycle_.motion;}
    void presentation_motion(std::uint8_t value) noexcept {zone_lifecycle_.motion=value;}
    EffectCycle& effects() noexcept {return effects_;}
    const EffectReplica& replica() const noexcept {return replica_;}
    std::vector<PlayerItem>& inventory() noexcept {return inventory_;}
    const std::vector<PlayerItem>& inventory() const noexcept {return inventory_;}
    void initializing(bool value) noexcept {initializing_=value;}
    void gain_experience(PlayerCore&,const StatusRules&,const Experience&,std::uint32_t source,std::int32_t amount,ExperienceAwardOptions,ExperienceAwardPorts&);
    void announce_maximum_resources() {resources_.announce_maximum(status_.skills.equipment.base.maximum);}
    void finish_loading_resources() {resources_.clamp(status_.skills.equipment.base.maximum);}
    std::array<std::int32_t,3> heal(std::array<std::int32_t,3> amounts);
    void recover(std::array<std::int32_t,3> amounts,bool send_set=false,bool recovering=false);
    void wear_defense();
    void remove_equipment_modifiers(std::uint8_t slot,const PlayerItem&,EquipmentRemovalDiagnostics&);
    bool check_hp_ability(const SkillDefinitions& definitions);
    bool damage(std::int32_t kind,std::uint32_t source,std::int32_t amount,const SkillDefinitions& definitions);
    void clear_effects_on_death(const std::int32_t& death_penalty);
    const DamageLedger& damage_ledger() const noexcept {return damage_;}
    // CDamage::Reset004b8aa0 on this actor ledger only. No Scene callback,
    // resource/effect/life writes or notifications. Serialized actor owner only.
    void reset_damage_ledger() noexcept;
    std::int32_t& defense_wear_cursor() noexcept {return defense_wear_cursor_;}
    std::int32_t defense_wear_cursor() const noexcept {return defense_wear_cursor_;}
    void send_shape_self();
    void send_vehicle_partner();
    bool reveal();
    bool cancel_shape_on_attack();
    void remove_sleep();
    void cancel_move(ActorActionWorld&);
    void cancel_vehicle(ActorActionWorld&);
    void stop_casting(ActorActionWorld&);
    void clear_duel_debuffs();
    void enter_zone_lifecycle(UserPosition,std::uint16_t map,const std::function<void()>& nprotect);
    const ZoneMovementCheck& movement_check() const noexcept {return zone_lifecycle_.movement;}
    void end_duel();
    void leave_duel_on_attack(DuelWorld&);
    ItemExchangeState& exchange() noexcept {return exchange_;}
    const ItemExchangeState& exchange() const noexcept {return exchange_;}
    ItemExchangeState& duel_exchange() noexcept {return duel_exchange_;}
    const ItemExchangeState& duel_exchange() const noexcept {return duel_exchange_;}
    void cancel_exchange_ready(Actor& peer,bool duel);
    void remove_exchange_item(std::int32_t bag,std::int32_t slot,ItemExchangeWorld&);
    DuelWager& duel_wager() noexcept {return duel_wager_;}
    const DuelWager& duel_wager() const noexcept {return duel_wager_;}
    void maximum_changed(EffectStatus&,std::uint8_t resource) override;
    void speed_changed(EffectStatus&) override;
    void shape_changed(EffectStatus&) override;
    void run_mode_changed(EffectStatus&,bool running) override;
    void stop_attack(EffectStatus&) override;
    void stop_target_mob(EffectStatus&,std::uint32_t id) override;
    void taunt(EffectStatus&,std::uint16_t aggro,float range) override;
    void detect_hidden(EffectStatus&,const SkillDefinition&) override;
    void rebuild_attack(EffectStatus&,bool equipment) override;
    void user_shape_changed(EffectStatus&) override;
    void replace_attack_elements(EffectStatus&,std::uint8_t attribute) override;
    void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>& amounts) override;
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed) override;
    std::optional<std::uint32_t> allocate_id() noexcept override;
    void release_id(std::uint32_t id) noexcept override;
    std::uint32_t tick_ms() const noexcept override;
    void erase_persisted(const PersistedEffect&) noexcept override;
    void persist_applied(const PersistedEffect&) noexcept override;
    void send_self(const net::Packet&) noexcept override;
    // CUser::GetShape, used by zone admission before optional shape delivery.
    std::uint8_t effective_shape() const;
    std::optional<bool> party_kind() const noexcept override;
    void send_party(const net::Packet&) noexcept override;
private:
    void require_effect_database() const;
    void prepare_equipment_removal(std::uint16_t slot,InventoryRemovalWorld&);
    InventoryReceipt admit_item(GroundItem&,InventoryWorld&,GroundItemPool&,bool pickup);
    std::uint32_t credit_money(std::uint32_t amount,std::uint8_t type_id,std::uint8_t count,MoneyWorld&);
    bool in_zone() const noexcept override {return status_.in_zone;}
    void send_view(const net::Packet&) override;
    void reset_damage() override;
    std::uint32_t id_;
    std::uint32_t account_id_=0,money_=0;
    EffectStatus status_;
    ActorAppearance appearance_;
    ActorAttackState attack_;
    ActorActionState actions_;
    std::optional<ActorDeathMetadata> death_metadata_; // Actual owner; no second life word.
    ZoneLifecycleState zone_lifecycle_;
    DuelWager duel_wager_;
    ItemExchangeState exchange_,duel_exchange_;
    std::uint32_t guard_target_after_=0;
    std::int32_t defense_wear_cursor_=0;
    UserRelations relations_;
    ActorScene& scene_;
    ActorEffectDatabase* effect_database_=nullptr; // Required before effect removal; borrowed lifetime.
    ActorResources resources_;
    DamageLedger damage_;
    EffectCycle effects_;
    EffectReplica replica_;
    ActorEquipmentRules equipment_;
    std::vector<PlayerItem> inventory_;
    std::array<std::uint16_t,13> equipment_database_quality_{}; // +1aa, separate from quality tiers.
    struct RetainedInventory {std::uint8_t bag;std::uint16_t slot;GroundItem* item;GroundItemPool* pool;};
    std::vector<RetainedInventory> retained_inventory_; // Bound pools must outlive the actor.
    bool initializing_=false;
};
}
