#include "eternity/game/quest_start_log.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
namespace {
std::string_view prefix(std::string_view s,std::size_t capacity) {
    s=s.substr(0,s.find('\0'));
    if(s.size()>=capacity) throw std::invalid_argument("Quest start log string exceeds original slot");
    return s;
}
}
std::uint32_t quest_start_log_time(std::uint16_t year,std::uint16_t month,
    std::uint16_t day,std::uint16_t hour,std::uint16_t minute,std::uint16_t second) {
    std::uint32_t value=std::uint32_t(year)-16u;
    value=value*16u+month;value=value*32u+day;value=value*32u+hour;
    value=value*64u+minute;return value*64u+second;
}
void write_quest_start_log(std::span<std::uint8_t,144> packet,const QuestStartLogContext& c,
    const QuestStartRecord& q,const std::optional<QuestStartLogNpc>& npc) {
    const auto account=prefix(c.account_name,19),character=prefix(c.character_name,21);
    const auto quest=prefix(std::string_view(reinterpret_cast<const char*>(q.data()+2),256),32);
    const auto name=prefix(npc?npc->name:std::string_view{},32);
    const auto put=[&](std::size_t p,std::uint32_t v,unsigned bytes) {for(unsigned i=0;i<bytes;++i) packet[p+i]=static_cast<std::uint8_t>(v>>(8*i));};
    const auto string=[&](std::size_t p,std::string_view s) {std::copy(s.begin(),s.end(),packet.begin()+static_cast<std::ptrdiff_t>(p));packet[p+s.size()]=0;};
    put(0,0x501,2);put(2,c.user_id,4);string(6,account);put(25,c.character_id,4);string(29,character);
    put(50,c.level,2);put(52,c.experience,4);put(56,c.map,2);
    for(unsigned i=0;i<3;++i) put(58+4*i,c.position_bits[i],4);
    put(70,c.system_time,4);put(74,std::uint32_t(q[0])|(std::uint32_t(q[1])<<8),2);
    string(76,quest);put(108,npc?npc->type:0,4);string(112,name);
}
std::array<std::uint8_t,144> quest_start_log_packet(const QuestStartLogContext& c,
    const QuestStartRecord& q,const std::optional<QuestStartLogNpc>& npc) {
    std::array<std::uint8_t,144> packet{};write_quest_start_log(packet,c,q,npc);return packet;
}
}
