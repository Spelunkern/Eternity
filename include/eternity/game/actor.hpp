#pragma once
#include "eternity/game/effect_cycle.hpp"
#include "eternity/game/resources.hpp"
#include "eternity/game/user_shape.hpp"

namespace eternity::game {
// Required scene/transport services. Implementations own spatial queries, combat
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
    virtual void detect_hidden(const SkillDefinition&) = 0;
    virtual UserShapeSocial user_shape_social() = 0;
    virtual void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed) = 0;
};
struct ActorAppearance {
    bool visible=true;
    std::int32_t vehicle_mode=0,vehicle_shape=0,vehicle_addition=0;
    std::uint32_t vehicle_partner=0;
    UserShapeIdentity identity;
    std::optional<CopiedUserShape> copied_user;
};
struct ActorAttackState {
    std::int32_t mode=0,recovery=1;
    std::uint32_t previous_tick=0,next_tick=0;
    std::uint8_t motion=0;
};
struct ActorEquipmentRules {
    // Immutable definition/enchant sources must outlive the actor; its inventory is owned separately.
    const ItemDefinitions& items;
    const EnchantValues& enchant;
    const MobAttackTemplates& mobs;
};
// Single actor executor; its owner must serialize commands, ticks and scene changes.
class Actor final:public EffectRemovalWorld,public EffectObserver,private ResourceSink {
public:
    Actor(std::uint32_t id,EffectStatus status,PlayerResources current,std::vector<PersistedEffect> replica,ActorScene& scene,
        ActorEquipmentRules equipment,std::vector<PlayerItem> inventory = {});
    ~Actor(); // Bound scene must outlive this actor.
    EffectStatus& status() noexcept {return status_;}
    const EffectStatus& status() const noexcept {return status_;}
    ActorAppearance& appearance() noexcept {return appearance_;}
    ActorAttackState& attack_state() noexcept {return attack_;}
    const PlayerResources& resources() const noexcept {return resources_.current();}
    EffectCycle& effects() noexcept {return effects_;}
    const EffectReplica& replica() const noexcept {return replica_;}
    std::vector<PlayerItem>& inventory() noexcept {return inventory_;}
    void initializing(bool value) noexcept {initializing_=value;}
    void finish_loading_resources() {resources_.clamp(status_.skills.equipment.base.maximum);}
    std::array<std::int32_t,3> heal(std::array<std::int32_t,3> amounts);
    void recover(std::array<std::int32_t,3> amounts,bool send_set=false,bool recovering=false);
    void send_shape_self();
    void send_vehicle_partner();
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
    void send_self(const net::Packet&) noexcept override;
    std::optional<bool> party_kind() const noexcept override;
    void send_party(const net::Packet&) noexcept override;
private:
    std::uint8_t effective_shape() const;
    bool in_zone() const noexcept override {return status_.in_zone;}
    void send_view(const net::Packet&) override;
    void reset_damage() override;
    std::uint32_t id_;
    EffectStatus status_;
    ActorAppearance appearance_;
    ActorAttackState attack_;
    ActorScene& scene_;
    ActorResources resources_;
    EffectCycle effects_;
    EffectReplica replica_;
    ActorEquipmentRules equipment_;
    std::vector<PlayerItem> inventory_;
    bool initializing_=false;
};
}
