#pragma once
#include "eternity/game/mob_death.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <functional>
#include <stdexcept>

// Compose the recovered death coordinator with a concrete reward owner. Every
// unrequested dependency fails, so integration cannot silently skip a service.
struct MobDeathRewardFixture final:eternity::game::MobDeathWorld,eternity::game::ZoneMobServices {
    using Mob=eternity::game::MobRuntime;using User=eternity::game::MobDeathUser;using Party=eternity::game::MobDeathParty;
    User user;Party* group=nullptr;std::vector<char> order;
    std::function<void(User&,std::uint32_t,std::int32_t)> reward;
    std::function<void(const eternity::net::Packet&)> death_packet;
    bool locked=false;
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected mob reward integration service");}
    void send_death(Mob&,const eternity::net::Packet& packet,std::uint8_t priority) override {if(priority!=6) unexpected();order.push_back('V');if(death_packet) death_packet(packet);}
    User* find_user(Mob&,std::uint32_t id) override {order.push_back('F');return id==user.id?&user:nullptr;}
    Party* party(User&) override {return group;}
    void lock_party(Party&) override {if(locked) unexpected();locked=true;order.push_back('[');}
    void unlock_party(Party&) noexcept override {locked=false;order.push_back(']');}
    bool in_reward_range(User&,User&,std::int32_t radius) override {if(!locked || radius!=100) unexpected();return true;}
    void add_experience(User& recipient,std::uint32_t source,std::int32_t amount) override {order.push_back('E');reward(recipient,source,amount);}
    void drop_items(Mob&,User* credit,Party* party) override {if(credit!=&user || party!=group || (party && !locked)) unexpected();order.push_back('D');}
    void quest_kill(User&,Mob&) override {order.push_back('Q');}
    std::optional<std::int32_t> obelisk_country(Mob&,std::int32_t) override {unexpected();}
    void destroy_obelisk(Mob&,std::int32_t,std::int32_t) override {unexpected();}
    void destroy_linked_group(Mob&,std::int32_t) override {unexpected();}
    void remove_obelisk_member(Mob&,void*) override {unexpected();}
    void remove_linked_member(Mob&,void*) override {unexpected();}
    void log_death(Mob&,User*,bool) override {unexpected();}
    void clear_applied_skills(Mob& mob) override {if(!mob.damage->fair_combat || !mob.damage->records().empty()) unexpected();order.push_back('C');}
    std::uint32_t tick_ms() override {return 1000;}
    void send_cell_view(eternity::game::CellCoordinates,const eternity::net::Packet&) override {unexpected();}
    std::unique_ptr<eternity::game::MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {unexpected();}
    void lua_init(Mob&) override {unexpected();}
};
