#include "eternity/game/resources.hpp"
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t signed_value(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int bytes) {
    for(unsigned int i=0;i<bytes;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
net::Packet identified(std::uint16_t opcode,std::uint32_t id,std::size_t size) {
    net::Packet packet(size,0);put(packet,0,opcode,2);put(packet,2,id,4);return packet;
}
}
void ActorResources::party_energy() {
    // CUser::SendEnergyAllToParty 00482360: group payload orders HP, SP, MP.
    if(const auto raid=sink_.party_kind()) {
        auto packet=identified(*raid?0xc0c:0xc03,id_,12);put(packet,6,current_.hp,2);put(packet,8,current_.sp,2);put(packet,10,current_.mp,2);
        sink_.send_party(packet);
    }
}
std::array<std::int32_t,3> ActorResources::heal(PlayerResources maximum,std::array<std::int32_t,3> amounts) {
    // CUser::Heal 004505a0, ps_game.exe. Signed comparisons, DWORD addition and no lower clamp.
    const auto before=current_;
    std::array<std::uint32_t*,3> current={&current_.hp,&current_.sp,&current_.mp};
    const std::array<std::uint32_t,3> limits={maximum.hp,maximum.sp,maximum.mp};
    for(std::size_t i=0;i<3;++i) {
        if(signed_value(*current[i]+static_cast<std::uint32_t>(amounts[i]))>signed_value(limits[i])) amounts[i]=signed_value(limits[i]-*current[i]);
        *current[i]+=static_cast<std::uint32_t>(amounts[i]);
    }
    if(current_.hp==maximum.hp && before.hp!=current_.hp) sink_.reset_damage();
    if(before.hp!=current_.hp || before.sp!=current_.sp || before.mp!=current_.mp) party_energy();
    return amounts;
}
void ActorResources::recover(PlayerResources maximum,std::array<std::int32_t,3> amounts,bool send_set,std::int32_t attack_mode,bool recovering) {
    // SendRecoverAdd 00481f20 / SendRecoverAddSet 00482060 save only WORD-sized previous values.
    const PlayerResources before{static_cast<std::uint16_t>(current_.hp),static_cast<std::uint16_t>(current_.mp),static_cast<std::uint16_t>(current_.sp)};
    current_.hp+=static_cast<std::uint32_t>(amounts[0]);current_.sp+=static_cast<std::uint32_t>(amounts[1]);current_.mp+=static_cast<std::uint32_t>(amounts[2]);
    if(signed_value(current_.hp)>signed_value(maximum.hp)) current_.hp=maximum.hp;
    if(signed_value(current_.sp)>signed_value(maximum.sp)) current_.sp=maximum.sp;
    if(signed_value(current_.mp)>signed_value(maximum.mp)) current_.mp=maximum.mp;
    if(before.hp==current_.hp && before.sp==current_.sp && before.mp==current_.mp) return;
    if(sink_.in_zone()) {
        auto packet=identified(0x505,id_,12);put(packet,6,current_.hp-before.hp,2);put(packet,8,current_.mp-before.mp,2);put(packet,10,current_.sp-before.sp,2);
        sink_.send_view(packet);
    }
    party_energy();
    if(current_.hp==maximum.hp && signed_value(before.hp)<signed_value(current_.hp)) sink_.reset_damage();
    if(send_set && attack_mode==0 && (current_.hp==maximum.hp || current_.sp==maximum.sp || current_.mp==maximum.mp || recovering)) {
        net::Packet packet(8,0);put(packet,0,0x521,2);put(packet,2,current_.hp,2);put(packet,4,current_.mp,2);put(packet,6,current_.sp,2);sink_.send_self(packet);
    }
}
void ActorResources::maximum_changed(PlayerResources maximum,std::uint8_t resource,bool initializing) {
    // SendMaxHP 00481990 / SendMaxSP 00481aa0 / SendMaxMP 00481ba0.
    if(resource>2) throw std::invalid_argument("Invalid original resource identifier");
    if(initializing) return;
    recover(maximum,{0,0,0});
    const auto wire_type=static_cast<std::uint8_t>(resource==1?2:resource==2?1:0);
    const auto amount=resource==0?maximum.hp:resource==1?maximum.mp:maximum.sp;
    auto packet=identified(0x50b,id_,9);packet[6]=wire_type;put(packet,7,amount,2);
    if(resource==0) {if(sink_.in_zone()) sink_.send_view(packet);}else sink_.send_self(packet);
    if(const auto raid=sink_.party_kind()) {put(packet,0,*raid?0xc0b:0xc02,2);sink_.send_party(packet);}
}
}
