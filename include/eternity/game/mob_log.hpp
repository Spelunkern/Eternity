#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/net/framing.hpp"
#include <string_view>

namespace eternity::game {
struct MobLogContext {
    std::uint16_t map;
    std::string_view definition_name;
};
class MobLogTransport {
public:
    virtual ~MobLogTransport()=default;
    virtual void send_game_log(const net::Packet&)=0;
};
// ps_game.exe CMob::SendLogBothMob 004a1810. The supplied packet is initialized
// storage: C-string writes retain bytes after their terminator. An absent zone
// returns false before inspecting the definition, context or packet.
bool write_mob_log(std::span<std::uint8_t> packet,const MobRuntime&,MobLogContext,
    std::uint8_t event,std::optional<std::string_view> user,std::optional<std::string_view> detail,std::uint32_t value);
// Fresh packets start with zeroed string tails; actual delivery is mandatory.
bool send_mob_log(const MobRuntime&,MobLogContext,std::uint8_t event,
    std::optional<std::string_view> user,std::optional<std::string_view> detail,std::uint32_t value,MobLogTransport&);
// Direct provider for MobDeathWorld::log_death: event 4, retained previous skill
// text and the fair flag observed before the damage owner is reset.
bool send_mob_death_log(const MobRuntime&,MobLogContext,std::optional<std::string_view> credited_name,bool fair,MobLogTransport&);
}
