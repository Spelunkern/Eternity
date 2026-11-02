#pragma once
#include "eternity/game/player_packets.hpp"
#include "eternity/game/damage.hpp"
#include <array>
#include <optional>

namespace eternity::game {
class ResourceSink {
public:
    virtual ~ResourceSink() = default;
    virtual bool in_zone() const noexcept = 0;
    virtual std::optional<bool> party_kind() const noexcept = 0; // party=false, raid=true.
    virtual void send_view(const net::Packet&) = 0; // Original PSendView, radius 60 around this actor.
    virtual void send_party(const net::Packet&) = 0; // Excludes this actor.
    virtual void send_self(const net::Packet&) = 0;
    virtual void reset_damage() = 0;
};
class ActorResources {
public:
    ActorResources(std::uint32_t character_id,PlayerResources current,ResourceSink& sink):id_(character_id),current_(current),sink_(sink) {}
    const PlayerResources& current() const noexcept {return current_;}
    void clamp(PlayerResources maximum); // Final character-load clamp, without notifications.
    // Requested/actual amounts use HP, SP, MP order, matching CUser::Heal's in/out buffer.
    std::array<std::int32_t,3> heal(PlayerResources maximum,std::array<std::int32_t,3> amounts);
    void recover(PlayerResources maximum,std::array<std::int32_t,3> amounts,bool send_set = false,std::int32_t attack_mode = 0,bool recovering = false);
    void maximum_changed(PlayerResources maximum,std::uint8_t resource,bool initializing); // HP=0, MP=1, SP=2.
    bool damage(ScalarDamageState state,std::int32_t kind,std::uint32_t source,std::int32_t amount,DamageObserver& observer);
private:
    void party_energy();
    std::uint32_t id_;
    PlayerResources current_;
    ResourceSink& sink_;
};
}
