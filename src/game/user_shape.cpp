#include "eternity/game/user_shape.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int width) {
    for(unsigned int i=0;i<width;++i) packet.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
}
void validate(std::string_view value,std::size_t maximum) {
    if(value.size()>maximum || value.find('\0')!=std::string_view::npos) throw std::invalid_argument("Invalid original user-shape wire string");
}
std::uint8_t enchant(const PlayerItem& item) {
    if(item.craft.size()!=20 || item.craft[18]<'0' || item.craft[18]>'9' || item.craft[19]<'0' || item.craft[19]>'9') return 0;
    return static_cast<std::uint8_t>((item.craft[18]-'0')*10+item.craft[19]-'0');
}
}
UserShapeIdentity user_shape_identity(const PlayerCore& player,std::uint8_t country) {
    const auto& c=player.character;
    return {country,c.family,c.hair,c.face,c.size,c.job,c.sex,c.grow,player.kills[0],c.name};
}
net::Packet character_user_shape(std::uint32_t id,const UserShapeIdentity& identity,bool dead,std::uint8_t motion,
    const UserShapeSocial& social,std::span<const PlayerItem> inventory) {
    // ps_game.exe CUser::SendUserShape 00482ad0, normal branch 00482cb5.
    validate(identity.name,20);validate(social.guild_name,24);
    const PlayerItem* cloak=nullptr;
    for(const auto& item:inventory) if(item.bag==0 && item.slot==7) cloak=&item;
    net::Packet packet(67+(cloak?6:0)+social.guild_name.size(),0);
    put(packet,0,0x303,2);put(packet,2,id,4);
    const std::array<std::uint8_t,11> appearance={static_cast<std::uint8_t>(dead),motion,identity.country,identity.family,
        identity.hair,identity.face,identity.size,identity.job,identity.sex,social.party_role,identity.grow};
    std::copy(appearance.begin(),appearance.end(),packet.begin()+6);put(packet,17,identity.kill_count,4);
    for(const auto& item:inventory) if(item.bag==0 && item.slot<8) {
        const auto at=21+item.slot*3;packet[at]=item.type;packet[at+1]=item.type_id;packet[at+2]=enchant(item);
    }
    std::copy(identity.name.begin(),identity.name.end(),packet.begin()+45);
    if(cloak) std::copy(cloak->gems.begin(),cloak->gems.end(),packet.begin()+66);
    std::copy(social.guild_name.begin(),social.guild_name.end(),packet.begin()+66+(cloak?6:0));
    return packet;
}
net::Packet character_equipment(std::uint32_t id,std::uint8_t slot,const PlayerItem* item) {
    // ps_game.exe SendEquipment 004839e0: an absent cloak uses the short form.
    if(slot>=8) throw std::invalid_argument("Invalid visible equipment slot");
    net::Packet packet(item && slot==7?16:10,0);put(packet,0,0x507,2);put(packet,2,id,4);packet[6]=slot;
    if(item) {packet[7]=item->type;packet[8]=item->type_id;packet[9]=enchant(*item);if(slot==7) std::copy(item->gems.begin(),item->gems.end(),packet.begin()+10);}
    return packet;
}
net::Packet character_copied_user_shape(std::uint32_t id,const CopiedUserShape& shape) {
    // 00482b11..00482ca1 copies the cached fields, forces name[20]=0, uses its cached length.
    if(shape.length<67 || shape.length>97) throw std::invalid_argument("Invalid original copied-user shape length");
    net::Packet packet(97,0);put(packet,0,0x303,2);put(packet,2,id,4);
    std::copy(shape.appearance.begin(),shape.appearance.end(),packet.begin()+6);put(packet,17,shape.kill_count,4);
    std::copy(shape.equipment.begin(),shape.equipment.end(),packet.begin()+21);
    std::copy_n(shape.name.begin(),20,packet.begin()+45);
    std::copy(shape.extra.begin(),shape.extra.end(),packet.begin()+66);packet.resize(shape.length);return packet;
}
}
