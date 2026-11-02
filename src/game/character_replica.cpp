#include "eternity/game/character_replica.hpp"
#include <bit>

namespace eternity::game {
InventoryReplicaResult CharacterReplica::apply(std::span<const std::uint8_t> packet) {
    // ps_dbAgent.exe CUser::PacketUserSetStatus 00405b00. The owning dispatcher
    // validates identity and the packed PDB envelopes before applying writes.
    if(packet.size()<6) return InventoryReplicaResult::rejected;
    const auto word=[&](std::size_t at){return static_cast<std::uint16_t>(packet[at]|(static_cast<unsigned int>(packet[at+1])<<8));};
    const auto dword=[&](std::size_t at){return static_cast<std::uint32_t>(word(at))|(static_cast<std::uint32_t>(word(at+2))<<16);};
    if(dword(2)!=core_.user_id) return InventoryReplicaResult::rejected;
    const auto op=word(0);
    if(op>=0x700 && op<=0x7ff) return inventory_.apply(packet);
    if(op==0x603) return inventory_.apply_money(packet)?InventoryReplicaResult::applied:InventoryReplicaResult::rejected;
    std::size_t size=0;
    switch(op) {
        case 0x601:size=34;break;case 0x602:case 0x60b:size=10;break;
        case 0x604:size=20;break;case 0x605:case 0x610:size=9;break;
        case 0x606:size=12;break;case 0x607:size=22;break;case 0x608:size=18;break;
        case 0x60a:case 0x60e:case 0x60f:size=8;break;
        case 0x60c:size=11;break;case 0x60d:size=7;break;case 0x615:size=10;break;
        default:return InventoryReplicaResult::unsupported;
    }
    if(packet.size()!=size || (op==0x60c && packet[6]>=6)) return InventoryReplicaResult::rejected;
    auto& c=core_.character;
    const auto position=[&](std::size_t at){core_.x=std::bit_cast<float>(dword(at));core_.y=std::bit_cast<float>(dword(at+4));core_.z=std::bit_cast<float>(dword(at+8));};
    switch(op) {
        case 0x601:c.level=word(6);core_.experience=dword(8);core_.stat_points=word(12);core_.skill_points=word(14);
            c.strength=word(16);c.dexterity=word(18);c.intelligence=word(20);c.wisdom=word(22);c.recovery=word(24);c.luck=word(26);c.hp=word(28);c.mp=word(30);c.sp=word(32);break;
        case 0x602:core_.experience=dword(6);break;
        case 0x604:core_.stat_points=word(6);c.strength=word(8);c.dexterity=word(10);c.recovery=word(12);c.intelligence=word(14);c.wisdom=word(16);c.luck=word(18);break;
        case 0x605:switch(packet[6]) {case 1:core_.hunger=word(7);break;case 2:core_.vigor=std::bit_cast<std::int16_t>(word(7));break;case 3:core_.cg=packet[7];break;case 4:core_.og=packet[7];break;case 5:core_.ig=packet[7];break;default:break;}break;
        case 0x606:c.hp=word(6);c.mp=word(8);c.sp=word(10);break;
        case 0x607:c.map=word(6);core_.direction=word(8);position(10);break;
        case 0x608:position(6);break;
        case 0x60a:c.level=word(6);break;
        case 0x60b:core_.stored_money=dword(6);break;
        case 0x60c:if(packet[6]<4) core_.kills[packet[6]]=dword(7);else if(packet[6]==4) core_.kill_level=dword(7);else core_.dead_level=dword(7);break;
        case 0x60d:c.grow=packet[6];break;
        case 0x60e:core_.stat_points=word(6);break;
        case 0x60f:core_.skill_points=word(6);break;
        case 0x610:switch(packet[6]) {case 1:c.strength=word(7);break;case 2:c.dexterity=word(7);break;case 3:c.intelligence=word(7);break;case 4:c.wisdom=word(7);break;case 5:c.recovery=word(7);break;case 6:c.luck=word(7);break;default:break;}break;
        case 0x615:c.hair=packet[6];c.face=packet[7];c.size=packet[8];c.sex=packet[9];body_changed_=true;break;
        default:break;
    }
    return InventoryReplicaResult::applied;
}
}
