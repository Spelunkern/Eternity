#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/item_pool.hpp"
#include <stdexcept>
namespace inventory_test {
struct Scene final:eternity::game::ActorScene {
    std::vector<eternity::net::Packet> packets;
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected retained-inventory scene side effect");}
    void send_self(const eternity::net::Packet& packet) noexcept override {packets.push_back(packet);}
    void send_nearby(const eternity::net::Packet&) override {unexpected();}
    void send_cell_view(const eternity::net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    eternity::game::ZoneUsers* user_zone() override {unexpected();}
    eternity::game::UserShapeSocial user_shape_social() override {unexpected();}
    void finish_periodic_damage(eternity::game::EffectStatus&,const eternity::game::SkillDefinition&,const eternity::game::EffectSource&,std::uint32_t) override {unexpected();}
};
}
