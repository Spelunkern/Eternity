#pragma once
#include "eternity/game/player_packets.hpp"
#include <span>
namespace eternity::game {
class LevelUpNoticeWorld {
public:
    virtual ~LevelUpNoticeWorld()=default;
    virtual void rebuild_attack()=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    virtual bool in_zone() const=0;
    virtual void send_scene(const net::Packet&,float radius,std::uint8_t priority)=0;
    // Owner acquires/rechecks membership and keeps the party stable through send.
    virtual bool acquire_party()=0;
    virtual std::uint8_t party_type() const=0;
    virtual void update_party_experience()=0;
    virtual void send_party(const net::Packet&)=0;
    virtual void release_party()=0;
    virtual void energy_all_max()=0;
    virtual void recover_add(PlayerResources)=0;
    virtual void fill_game_log_main(std::span<std::uint8_t>)=0;
    virtual void send_game_log(const net::Packet&)=0;
    virtual void update_guild_level(std::uint32_t actor_id,std::uint16_t level)=0; // Acquire/recheck membership internally.
};
// ps_game.exe CUser::LevelUp 0048ad70. Required world implements live actor,
// scene, party, guild, log and transport operations; no default no-op providers.
void notify_level_up(const PlayerCore&,std::uint32_t actor_id,const PlayerResources& maximum,const PlayerResources& base,const Experience&,std::uint32_t amount,std::uint32_t source,LevelUpNoticeWorld&);
}
