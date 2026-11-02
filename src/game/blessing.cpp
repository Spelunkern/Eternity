#include "eternity/game/blessing.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t signed_value(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}
std::int32_t add(std::int32_t a,std::int32_t b) {return signed_value(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));}
std::int32_t percent(std::uint32_t base,std::int32_t rate) {
    // InitKCStatus: low DWORD IMUL, signed conversion spilled to float, FMUL 0.01F, _ftol2.
    const auto product=signed_value(base*static_cast<std::uint32_t>(rate));
    return static_cast<std::int32_t>(std::trunc(static_cast<double>(static_cast<float>(product))*0.01F));
}
}
BlessingDefinitions BlessingDefinitions::decode(std::span<const std::uint8_t> data) {
    // ps_game.exe CGameData::LoadKCStatus 004bf4a0: packed count + 25-byte records.
    std::size_t cursor=0;
    auto read=[&](unsigned int bytes) {
        if(data.size()-cursor<bytes) throw std::runtime_error("Truncated KillStatus.SData");
        std::uint32_t value=0;for(unsigned int i=0;i<bytes;++i) value|=static_cast<std::uint32_t>(data[cursor++])<<(8*i);return value;
    };
    const auto count=read(4);if(count>512) throw std::runtime_error("Invalid original blessing record count");
    BlessingDefinitions result;for(auto& country:result.countries_) country.emplace_back();
    for(std::uint32_t i=0;i<count;++i) {
        const auto country=read(1);if(country>1) throw std::runtime_error("Invalid original blessing country");
        auto& records=result.countries_[country];if(records.size()>=256) throw std::runtime_error("Too many original country blessing records");
        BlessingDefinition record;record.index=static_cast<std::uint32_t>(records.size());record.kill_count=read(4);record.kill_value=static_cast<std::int32_t>(read(2));
        for(unsigned int j=0;j<6;++j) {const auto type=read(1),value=read(2);if(type>0 && type<28) record.modifiers[type]=static_cast<std::int32_t>(value);}
        records.push_back(record);
    }
    for(auto& records:result.countries_) for(std::size_t i=1;i<records.size();++i)
        for(std::size_t j=0;j<28;++j) records[i].modifiers[j]=std::max(records[i].modifiers[j],records[i-1].modifiers[j]);
    return result;
}
const BlessingDefinition* BlessingDefinitions::find(std::uint8_t country,std::int32_t index) const {
    if(country>1 || index<0 || static_cast<std::size_t>(index)>=countries_[country].size()) return nullptr;
    return &countries_[country][static_cast<std::size_t>(index)];
}
const BlessingDefinition* BlessingDefinitions::by_count(std::uint8_t country,std::uint32_t kills) const {
    if(country>1) return nullptr;
    for(auto record=countries_[country].rbegin();record!=countries_[country].rend();++record) if(record->kill_count<=kills) return &*record;
    return nullptr;
}
std::size_t BlessingDefinitions::size(std::uint8_t country) const {return country<2?countries_[country].size():0;}
std::array<std::int32_t,28> initialize_blessing(SkillStatus& status,std::array<std::int32_t,3>& attack_recovery,
    const BlessingDefinition& definition,const std::array<std::int32_t,2>& defenses) {
    // ps_game.exe CUser::InitKCStatus 0048be90. Only the original dispatched indices are applied.
    std::array<std::int32_t,28> applied{};const auto& rates=definition.modifiers;auto& equipment=status.equipment;
    auto& maximum=equipment.base.maximum;auto& attributes=equipment.base.attributes;auto& attack=equipment.attack;
    const std::array<std::uint32_t*,3> resources={&maximum.hp,&maximum.mp,&maximum.sp};
    for(std::size_t i=0;i<3;++i) {applied[i+1]=percent(*resources[i],rates[i+1]);*resources[i]+=static_cast<std::uint32_t>(applied[i+1]);}
    const std::array<std::uint32_t,3> limits={maximum.hp,maximum.sp,maximum.mp};
    for(std::size_t i=0;i<3;++i) {
        applied[i+10]=percent(limits[i],rates[i+10]);
        equipment.sitting_recovery[i]=add(equipment.sitting_recovery[i],applied[i+10]);equipment.standing_recovery[i]=add(equipment.standing_recovery[i],applied[i+10]);
        applied[i+13]=percent(limits[i],rates[i+13]);attack_recovery[i]=add(attack_recovery[i],applied[i+13]);
    }
    for(std::size_t i=0;i<2;++i) {applied[i+18]=percent(static_cast<std::uint32_t>(defenses[i]),rates[i+18]);attack.categories[i].defense=add(attack.categories[i].defense,applied[i+18]);}
    auto direct=[&](std::size_t index,std::int32_t& target) {applied[index]=rates[index];target=add(target,rates[index]);};
    direct(20,attack.categories[2].defense);
    direct(4,attributes.strength);direct(5,attributes.recovery);maximum.hp+=static_cast<std::uint32_t>(rates[5])*5;
    direct(6,attributes.intelligence);direct(7,attributes.wisdom);maximum.mp+=static_cast<std::uint32_t>(rates[7])*5;
    // Original 0048c268..0048c277 adds dexterity's resource bonus to MP, unlike ordinary stat modifiers.
    direct(8,attributes.dexterity);maximum.mp+=static_cast<std::uint32_t>(rates[8])*5;
    direct(9,attributes.luck);direct(16,attack.critical);direct(17,attack.categories[0].accuracy);
    direct(21,attack.categories[0].evasion);direct(22,attack.categories[1].evasion);direct(23,attack.categories[2].evasion);
    return applied;
}
net::Packet blessing_packet(std::uint8_t country,std::uint32_t kills,std::uint32_t end,std::uint32_t tick) {
    // CUser::SendKCStatus 00481920, signed wrap-aware remaining time, no deadline means zero.
    auto remaining=end==0?0:signed_value(end-tick);if(remaining<0) remaining=0;
    net::Packet packet{0x11,2,country};for(const auto value:{kills,static_cast<std::uint32_t>(remaining)})
        for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    return packet;
}
}
