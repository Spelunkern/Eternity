#include "eternity/game/mob_log.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
std::string_view prefix(std::string_view value) {
    const auto end=value.find('\0');if(end!=std::string_view::npos) value=value.substr(0,end);
    if(value.size()>31) throw std::invalid_argument("Mob log string exceeds its original 32-byte slot");
    return value;
}
}
bool write_mob_log(std::span<std::uint8_t> packet,const MobRuntime& mob,MobLogContext context,
    std::uint8_t event,std::optional<std::string_view> user,std::optional<std::string_view> detail,std::uint32_t value) {
    if(!mob.zone) return false;
    if(packet.size()!=123 || !mob.definition) throw std::invalid_argument("Mob log requires its original packet and definition");
    const auto name=prefix(context.definition_name),user_name=prefix(user.value_or(std::string_view{})),description=prefix(detail.value_or(std::string_view{}));
    const auto put=[&](std::size_t offset,std::uint32_t field,unsigned bytes) {for(unsigned i=0;i<bytes;++i) packet[offset+i]=static_cast<std::uint8_t>(field>>(8*i));};
    const auto string=[&](std::size_t offset,std::string_view text) {std::copy(text.begin(),text.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset));packet[offset+text.size()]=0;};
    put(0,0xa04,2);put(2,context.map,2);
    put(4,std::bit_cast<std::uint32_t>(mob.position.x),4);put(8,std::bit_cast<std::uint32_t>(mob.position.y),4);put(12,std::bit_cast<std::uint32_t>(mob.position.z),4);put(16,event,1);
    string(17,name);string(49,user_name);string(81,description);put(113,value,4);put(117,mob.hp,4);put(121,mob.definition->id,2);return true;
}
bool send_mob_log(const MobRuntime& mob,MobLogContext context,std::uint8_t event,
    std::optional<std::string_view> user,std::optional<std::string_view> detail,std::uint32_t value,MobLogTransport& transport) {
    net::Packet packet(123);if(!write_mob_log(packet,mob,context,event,user,detail,value)) return false;
    transport.send_game_log(packet);return true;
}
bool send_mob_death_log(const MobRuntime& mob,MobLogContext context,std::optional<std::string_view> credited_name,bool fair,MobLogTransport& transport) {
    const std::string_view skill(reinterpret_cast<const char*>(mob.previous_attack_skill.data()),mob.previous_attack_skill.size());
    return send_mob_log(mob,context,4,credited_name,skill,fair?1U:0U,transport);
}
}
