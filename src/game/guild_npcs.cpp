#include "eternity/game/guild_npcs.hpp"

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t value,unsigned int n=4) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint16_t word(std::span<const std::uint8_t> p,std::size_t at) {return static_cast<std::uint16_t>(p[at]|(std::uint16_t{p[at+1]}<<8));}
template<class Slots> bool records(net::Packet& p,std::size_t count,const Slots& slots) {
    for(std::size_t i=0;i<slots.size();++i) if(slots[i].type!=0) {append(p,slots[i].type,2);p.push_back(static_cast<std::uint8_t>(slots[i].level));p.push_back(static_cast<std::uint8_t>(i));++p[count];}
    return p[count]!=0;
}
}
void GuildNpcReplica::load(std::uint8_t slot,std::uint16_t type,std::uint8_t level) {if(slot<8) slots[slot]={{type,level},0};}
std::optional<net::Packet> GuildNpcReplica::packet(std::uint32_t guild) const {net::Packet p{0x0e,0x0b};append(p,guild);p.push_back(0);if(!records(p,6,slots)) return {};return p;}
bool GuildNpcs::apply(std::span<const std::uint8_t> p) {
    if(p.size()<7 || word(p,0)!=0xb0e || p.size()<7+std::size_t{p[6]}*4) return false;
    for(std::size_t i=0;i<p[6];++i) {const auto at=7+i*4;const auto slot=p[at+3];if(slot>=8) return false;slots[slot]={word(p,at),p[at+2]};}return true;
}
std::optional<net::Packet> GuildNpcs::client_packet() const {net::Packet p{0x24,0x0d,0};if(!records(p,2,slots)) return {};return p;}
}
