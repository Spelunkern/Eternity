#include "eternity/game/interface_replica.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
namespace {
void put(net::Packet& p,std::uint32_t value,unsigned int count) {for(unsigned int i=0;i<count;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet header(std::uint16_t op,std::uint32_t account) {net::Packet p;put(p,op,2);put(p,account,4);return p;}
}
bool InterfaceReplica::load_quick_slots(std::span<const QuickSlot> slots) {if(slots.size()>quick_.size()) return false;std::copy(slots.begin(),slots.end(),quick_.begin());quick_count_=slots.size();return true;}
bool InterfaceReplica::load(const PlayerSnapshot& player) {if(player.core.user_id!=account_ || !load_quick_slots(player.quick_slots)) return false;for(const auto& point:player.save_points) load_point(point.slot,point.map,point.x,point.y,point.z);return true;}
bool InterfaceReplica::load_point(std::uint8_t slot,std::uint16_t map,float x,float y,float z) {if(slot>=4) return false;points_[slot]={0,map,x,y,z};return true;}
InventoryReplicaResult InterfaceReplica::apply(std::span<const std::uint8_t> p) {
    if(p.size()<6) return InventoryReplicaResult::rejected;
    const auto word=[&](std::size_t at){return static_cast<std::uint16_t>(p[at]|(static_cast<unsigned int>(p[at+1])<<8));};
    const auto dword=[&](std::size_t at){return static_cast<std::uint32_t>(word(at))|(static_cast<std::uint32_t>(word(at+2))<<16);};
    if(dword(2)!=account_) return InventoryReplicaResult::rejected;
    const auto op=word(0);
    if(op==0x609) { // PacketUserSetStatus 00405b00, CUser+3814: 128 entries of six bytes.
        if(p.size()<7 || p[6]>quick_.size() || p.size()!=7+5U*p[6]) return InventoryReplicaResult::rejected;
        quick_count_=p[6];for(std::size_t i=0;i<quick_count_;++i) {const auto at=7+5*i;quick_[i]={p[at],p[at+1],p[at+2],word(at+3)};}
    } else if(op==0xd02) { // PacketUserSavePoint 00406c20.
        if(p.size()!=21 || p[6]>=4) return InventoryReplicaResult::rejected;
        auto& point=points_[p[6]];
        if(point.map==65535 || point.save_type==1) point.save_type=1;else if(point.save_type==0) point.save_type=2;
        point.map=word(7);point.x=std::bit_cast<float>(dword(9));point.y=std::bit_cast<float>(dword(13));point.z=std::bit_cast<float>(dword(17));
    } else return InventoryReplicaResult::unsupported;
    return InventoryReplicaResult::applied;
}
net::Packet InterfaceReplica::quick_slots_packet() const {
    auto p=header(0x508,account_);p.push_back(static_cast<std::uint8_t>(quick_count_));for(const auto& slot:quick_slots()) {p.insert(p.end(),{slot.bar,slot.slot,slot.bag});put(p,slot.number,2);}return p;
}
net::Packet InterfaceReplica::save_points_packet() const {
    auto p=header(0xd01,account_);p.push_back(0);
    for(std::size_t i=0;i<points_.size();++i) {const auto& point=points_[i];if(point.map==65535) continue;++p[6];p.push_back(static_cast<std::uint8_t>(i));put(p,point.map,2);for(float value:{point.x,point.y,point.z}) put(p,std::bit_cast<std::uint32_t>(value),4);}return p;
}
}
