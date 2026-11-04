#pragma once
#include "eternity/game/actor.hpp"
#include <functional>
#include <stdexcept>
namespace rebirth_test {
struct Scene final:eternity::game::ActorScene {
    std::vector<eternity::net::Packet> packets;unsigned resets=0;mutable unsigned ticks=0,party_queries=0;
    std::function<void()> on_tick;
    void send_self(const eternity::net::Packet& p) noexcept override {packets.push_back(p);}
    void send_nearby(const eternity::net::Packet& p) override {packets.push_back(p);}
    void send_cell_view(const eternity::net::Packet& p) override {packets.push_back(p);}
    std::optional<bool> party_kind() const noexcept override {++party_queries;return {};}
    void send_party(const eternity::net::Packet& p) noexcept override {packets.push_back(p);}
    void reset_damage() override {++resets;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {if(on_tick) on_tick();return 1000+ticks++;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected targeting operation");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    eternity::game::ZoneUsers* user_zone() override {return nullptr;}
    eternity::game::UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(eternity::game::EffectStatus&,const eternity::game::SkillDefinition&,const eternity::game::EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected periodic damage");}
};
inline bool same(eternity::game::PlayerResources a,eternity::game::PlayerResources b) {return a.hp==b.hp && a.mp==b.mp && a.sp==b.sp;}
}
