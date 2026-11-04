#include "eternity/game/vertical_movement_prefix.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint32_t get(std::span<const std::uint8_t> p,std::size_t offset,unsigned bytes) {std::uint32_t v=0;for(unsigned i=0;i<bytes;++i)v|=static_cast<std::uint32_t>(p[offset+i])<<(8*i);return v;}
void put(net::Packet& p,std::uint32_t n,unsigned size) {for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void write(std::span<std::uint8_t> p,std::size_t offset,float v) {auto n=std::bit_cast<std::uint32_t>(v);for(unsigned i=0;i<4;++i)p[offset+i]=static_cast<std::uint8_t>(n>>(8*i));}
UserPosition destination(std::span<const std::uint8_t> p){return {std::bit_cast<float>(get(p,5,4)),std::bit_cast<float>(get(p,9,4)),std::bit_cast<float>(get(p,13,4))};}
UserPosition validate(Actor& actor,RuntimeZone& zone,const ZoneEntryState& entry,std::span<const std::uint8_t> p) {
    if(p.size()<17 || get(p,0,2)!=0x501)throw std::invalid_argument("Movement prefix requires a complete 0501 packet");
    if(!zone.map || !zone.users || !entry.bound || entry.character_id!=actor.id() || zone.map->settings.id<0 ||
       zone.map->settings.id>65535 || entry.map!=zone.map->settings.id || !actor.status().in_zone ||
       zone.users->find_actor(actor.id())!=&actor)throw std::logic_error("Movement prefix requires live actor admission");
    auto pos=zone.users->position(actor.id());if(!pos)throw std::logic_error("Movement actor has no live position");return *pos;
}
void correct(Actor& a,UserPosition p,VerticalMovementPrefixServices& s) {
    a.stop_attack(a.status());
    net::Packet view{0x0b,0x02};put(view,a.id(),4);put(view,s.direction(a),2);
    for(float v:{p.x,p.y,p.z})put(view,std::bit_cast<std::uint32_t>(v),4);
    s.send_view_action(a,view);
    net::Packet self{0x02,0x05,0x05};put(self,a.id(),4);self.resize(17,0);a.send_self(self);
}
}
VerticalMovementPrefixResult vertical_movement_prefix(Actor& a,RuntimeZone& z,const ZoneEntryState& e,
    std::span<const std::uint8_t> p,VerticalMovementPrefixServices& s) {
    validate(a,z,e,p);s.status_stand(a);
    if(a.attack_state().mode==1 && a.death_metadata().soul!=1)return VerticalMovementPrefixResult::life_blocked;
    const auto pos=validate(a,z,e,p); // StatusStand may change live state; retain the same zone and actor.
    if(vertical_movement_planar_distance(pos,destination(p))>35.0F) {correct(a,pos,s);return VerticalMovementPrefixResult::distance_corrected;}
    return VerticalMovementPrefixResult::check_required;
}
void vertical_movement_speed_correction(Actor& a,RuntimeZone& z,const ZoneEntryState& e,
    std::span<std::uint8_t> p,VerticalMovementPrefixServices& s) {
    validate(a,z,e,p);const auto back=a.movement_check().back;
    write(p,5,back.x);write(p,9,back.y);write(p,13,back.z);correct(a,back,s);
}
}
