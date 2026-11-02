#include "eternity/game/entry_packets.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int bytes) {
    for(unsigned int i=0;i<bytes;++i) packet.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
}
std::int32_t signed_value(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}
net::Packet header(std::uint16_t opcode,std::size_t size=3) {net::Packet packet(size,0);put(packet,0,opcode,2);return packet;}
void item_data(net::Packet& packet,std::size_t offset,const PlayerItem& item) {
    if(item.craft.size()>20 || item.craft.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid original item craft string");
    packet.at(offset)=item.type;packet.at(offset+1)=item.type_id;put(packet,offset+2,item.quality,2);
    std::copy(item.gems.begin(),item.gems.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset+4));packet.at(offset+10)=item.count;
    // Original strcpy leaves bytes after NUL unspecified. Native output zeroes this unused padding.
    std::copy(item.craft.begin(),item.craft.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset+11));
}
template<class Entry,class Append>
std::vector<net::Packet> batches(std::span<const Entry> entries,std::uint16_t opcode,std::size_t limit,std::size_t width,Append append) {
    std::vector<net::Packet> result;
    for(std::size_t begin=0;begin<entries.size();) {
        const auto count=std::min(limit,entries.size()-begin);auto packet=header(opcode,3+count*width);packet[2]=static_cast<std::uint8_t>(count);
        for(std::size_t i=0;i<count;++i) append(packet,3+i*width,entries[begin+i]);
        result.push_back(std::move(packet));begin+=count;
    }
    return result;
}
}
std::vector<net::Packet> character_items(std::span<const PlayerItem> items,std::int32_t maximum_bag) {
    // ps_game.exe CUser::SendCharacterItem 004832b0: bag-major order, 24 slots, up to 50 records per packet.
    if(maximum_bag>5) throw std::invalid_argument("Maximum bag exceeds original inventory storage");
    std::array<const PlayerItem*,144> slots{};
    for(const auto& item:items) {
        if(item.bag>5 || item.slot>=24) throw std::invalid_argument("Invalid original inventory position");
        auto& slot=slots[item.bag*24+item.slot];if(slot) throw std::invalid_argument("Duplicate inventory position");slot=&item;
    }
    std::vector<const PlayerItem*> visible;
    for(std::size_t i=0;i<slots.size();++i) if(slots[i] && static_cast<std::int32_t>(i/24)<=maximum_bag) visible.push_back(slots[i]);
    return batches<const PlayerItem*>(visible,0x106,50,34,[](net::Packet& packet,std::size_t offset,const PlayerItem* item) {
        packet[offset]=item->bag;packet[offset+1]=static_cast<std::uint8_t>(item->slot);item_data(packet,offset+2,*item);
    });
}
std::vector<net::Packet> character_bank(std::span<const PlayerItem> items,std::uint64_t money) {
    // CUser::SendCharacterBank 00483410: the low DWORD of stored money accompanies every populated batch.
    std::array<const PlayerItem*,240> slots{};
    for(const auto& item:items) {if(item.slot>=240) throw std::invalid_argument("Invalid original bank position");auto& slot=slots[item.slot];if(slot) throw std::invalid_argument("Duplicate bank position");slot=&item;}
    std::vector<const PlayerItem*> ordered;for(auto* item:slots) if(item) ordered.push_back(item);
    std::vector<net::Packet> result;
    for(std::size_t begin=0;begin<ordered.size();) {
        const auto count=std::min<std::size_t>(50,ordered.size()-begin);auto packet=header(0x711,7+count*33);put(packet,2,static_cast<std::uint32_t>(money),4);packet[6]=static_cast<std::uint8_t>(count);
        for(std::size_t i=0;i<count;++i) {packet[7+i*33]=static_cast<std::uint8_t>(ordered[begin+i]->slot);item_data(packet,8+i*33,*ordered[begin+i]);}
        result.push_back(std::move(packet));begin+=count;
    }
    return result;
}
RestoredSkills restore_skills(std::span<const PlayerSkill> skills,const SkillDefinitions& definitions,std::uint32_t tick) {
    // PacketUserDBSetInfo 0046d780 case 0504: rows already renumbered by the DB-agent.
    RestoredSkills result;result.active.reserve(skills.size());
    for(const auto& skill:skills) {
        if(!definitions.find(skill.id,skill.level)) {result.invalid=skill;break;}
        if(skill.number!=result.active.size()) throw std::invalid_argument("Learned skills are not in original DB-agent load order");
        result.active.push_back({skill.id,skill.level,skill.number,tick+static_cast<std::uint32_t>(skill.delay)*1000U});
    }
    return result;
}
net::Packet character_skills(std::span<const LearnedSkillPacketEntry> skills,std::uint32_t tick) {
    // SendCharacterSkill 00483540 always sends, even with no learned skills.
    if(skills.size()>255) throw std::invalid_argument("Original skill packet count exceeded");
    auto packet=header(0x108,3+skills.size()*8);packet[2]=static_cast<std::uint8_t>(skills.size());
    for(std::size_t i=0;i<skills.size();++i) {const auto& skill=skills[i];const auto offset=3+i*8;
        put(packet,offset,skill.id,2);packet[offset+2]=skill.level;packet[offset+3]=skill.number;
        put(packet,offset+4,static_cast<std::uint32_t>(std::max(signed_value(skill.end_tick-tick)/1000,0)),4);
    }
    return packet;
}
std::vector<net::Packet> character_effects(std::span<const ActiveEffectPacketEntry> effects,std::uint32_t tick) {
    // SendCharacterSetSkill 00483890: 128 records, negative remaining time becomes 1; zero stays zero.
    return batches<ActiveEffectPacketEntry>(effects,0x10a,128,11,[tick](net::Packet& packet,std::size_t offset,const auto& effect) {
        auto remaining=effect.offline_countdown?signed_value(effect.end_tick-tick/1000):signed_value(effect.end_tick-tick)/1000;
        if(remaining<0) remaining=1;
        put(packet,offset,effect.instance_id,4);put(packet,offset+4,effect.skill_id,2);packet[offset+6]=effect.level;put(packet,offset+7,static_cast<std::uint32_t>(remaining),4);
    });
}
net::Packet character_quick_slots(std::span<const QuickSlot> slots) {
    // SendCharacterQuick 00483800 packs the runtime six-byte records into five bytes.
    if(slots.size()>255) throw std::invalid_argument("Original quick slot packet count exceeded");
    auto packet=header(0x10b,3+slots.size()*5);packet[2]=static_cast<std::uint8_t>(slots.size());
    for(std::size_t i=0;i<slots.size();++i) {const auto& slot=slots[i];const auto offset=3+i*5;packet[offset]=slot.bar;packet[offset+1]=slot.slot;packet[offset+2]=slot.bag;put(packet,offset+3,slot.number,2);}
    return packet;
}
std::vector<net::Packet> character_quests(std::span<const QuestPacketEntry> quests,std::uint32_t tick) {
    return batches<QuestPacketEntry>(quests,0x901,255,7,[tick](net::Packet& packet,std::size_t offset,const auto& quest) {
        const auto remaining=quest.timed?std::max(signed_value(quest.end_tick-tick)/60000,0):0;
        put(packet,offset,quest.id,2);put(packet,offset+2,static_cast<std::uint32_t>(remaining),2);std::copy(quest.counters.begin(),quest.counters.end(),packet.begin()+static_cast<std::ptrdiff_t>(offset+4));
    });
}
std::vector<net::Packet> character_finished_quests(std::span<const FinishedQuest> quests) {
    return batches<FinishedQuest>(quests,0x906,255,3,[](net::Packet& packet,std::size_t offset,const auto& quest) {put(packet,offset,quest.id,2);packet[offset+2]=quest.success;});
}
net::Packet character_speed(std::uint32_t id,const EffectStatus& status) {
    auto packet=header(0x51c,8);put(packet,2,id,4);packet[6]=attack_speed(status);packet[7]=move_speed(status);return packet;
}
}
