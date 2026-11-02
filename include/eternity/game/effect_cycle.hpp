#pragma once
#include "eternity/game/effect_timers.hpp"
#include "eternity/game/effects.hpp"

namespace eternity::game {
net::Packet effect_removed_packet(std::uint32_t instance_id);
net::Packet party_effect_removed_packet(std::uint32_t character_id,std::uint16_t skill_id,std::uint8_t level,bool raid);
class EffectObserver {
public:
    virtual ~EffectObserver() = default;
    virtual std::optional<std::uint32_t> allocate_id() noexcept = 0;
    virtual void release_id(std::uint32_t id) noexcept = 0;
    virtual std::uint32_t tick_ms() const noexcept = 0;
    // Enqueue changes to the DB-agent replica; SQL is replaced later at character save.
    // Transient backend failures must not stop effect reversal.
    virtual void erase_persisted(const PersistedEffect&) noexcept = 0;
    virtual void send_self(const net::Packet&) noexcept = 0;
    virtual std::optional<bool> party_kind() const noexcept = 0; // absent, party=false, raid=true.
    virtual void send_party(const net::Packet&) noexcept = 0; // excludes the affected character.
};
class EffectCycle {
public:
    explicit EffectCycle(std::uint32_t character_id):character_id_(character_id) {}
    EffectCycle(const EffectCycle&) = delete;
    EffectCycle& operator=(const EffectCycle&) = delete;
    // Definitions must outlive the cycle; ordered map nodes keep their addresses stable on reload.
    void append(std::uint32_t instance_id,EffectTimer timer,const SkillDefinition& definition);
    void restore(const std::vector<PersistedEffect>& effects,const SkillDefinitions& definitions,std::uint16_t map,
        std::uint32_t disconnected_seconds,std::uint32_t tick_ms,EffectObserver& observer);
    void initialize(EffectStatus& status,EffectWorld& world);
    void expire(std::uint32_t tick_ms,EffectStatus& status,EffectEquipment equipment,const ItemDefinitions& definitions,
        EffectRemovalWorld& world,EffectObserver& observer);
    std::size_t size() const noexcept {return effects_.size();}
    bool contains(std::uint32_t instance_id) const noexcept;
private:
    struct Active {std::uint32_t id;EffectTimer timer;const SkillDefinition* definition;};
    // Insertion order permits callbacks to append/remove entries without invalidating the traversal cursor.
    std::map<std::uint64_t,Active> effects_;
    std::uint64_t sequence_ = 0;
    std::uint32_t character_id_;
};
class EffectReplica {
public:
    explicit EffectReplica(std::vector<PersistedEffect> effects):effects_(std::move(effects)) {}
    bool erase(std::uint16_t skill_id,std::uint8_t level);
    const std::vector<PersistedEffect>& snapshot() const noexcept {return effects_;}
private:
    std::vector<PersistedEffect> effects_;
};
class EffectStore {
public:
    EffectStore(storage::SqlServer& database,std::uint8_t server_id,std::string catalog = "PS_GameData");
    bool replace(std::uint32_t user_id,std::uint32_t character_id,const std::vector<PersistedEffect>& effects);
private:
    storage::SqlServer& database_;
    std::uint8_t server_id_;
    std::string prefix_;
};
}
