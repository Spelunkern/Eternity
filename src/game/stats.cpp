#include "eternity/game/stats.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace eternity::game {
namespace {
std::size_t column(const storage::ResultSet& table,std::string_view name) {
    const auto found=std::find(table.columns.begin(),table.columns.end(),name);
    if(found==table.columns.end()) throw std::runtime_error("Missing original status definition column");
    return static_cast<std::size_t>(found-table.columns.begin());
}
unsigned int number(const storage::Row& row,std::size_t index,unsigned int maximum) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing original status definition value");
    const auto& text=*row[index];unsigned int value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>maximum)
        throw std::runtime_error("Invalid original status definition value");
    return value;
}
// FISTP with the original truncation control word returns integer-indefinite on overflow.
std::int32_t integer(long double value) {
    value=std::trunc(value);
    if(!std::isfinite(value) || value<std::numeric_limits<std::int32_t>::min() || value>std::numeric_limits<std::int32_t>::max())
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
std::int32_t sum(std::int32_t a,std::int32_t b,std::int32_t c) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b)+static_cast<std::uint32_t>(c));
}
long double f32(std::int32_t value) {return static_cast<float>(value);}
}
StatusRules StatusRules::load(const storage::ResultSet& definitions) {
    // ps_game.exe CGameData::LoadStatus 004bfbc0: six jobs, levels 0..60, unsigned words.
    StatusRules rules;
    const auto level=column(definitions,"Level"),job=column(definitions,"Job"),hp=column(definitions,"HP"),
        mp=column(definitions,"MP"),sp=column(definitions,"SP");
    for(const auto& row:definitions.rows)
        rules.levels_[number(row,job,5)][number(row,level,60)]={number(row,hp,65535),number(row,mp,65535),number(row,sp,65535)};
    return rules;
}
BaseStatus StatusRules::base(const CharacterSummary& character) const {
    // ps_game.exe CUser::InitEquipment 00454560, assembly 004546a2..00454b0d.
    if(character.grow>3) throw std::invalid_argument("Unsupported original grow mode");
    BaseStatus status{{character.strength,character.dexterity,character.recovery,character.intelligence,character.wisdom,character.luck},
        character.job<6 && character.level<=60?levels_[character.job][character.level]:levels_[0][60]};
    const long double scale=character.grow==2?static_cast<long double>(1.1F):character.grow==3?1.5L:1.0L;
    status.maximum.hp=static_cast<std::uint32_t>(integer(status.maximum.hp*scale));
    status.maximum.mp=static_cast<std::uint32_t>(integer(status.maximum.mp*scale));
    status.maximum.sp=static_cast<std::uint32_t>(integer(status.maximum.sp*scale));
    switch(character.family) {
        case 0:status.maximum.sp+=10;break;
        case 1:status.maximum.mp+=10;break;
        case 2:status.maximum.sp+=5;status.maximum.mp+=5;break;
        case 3:status.maximum.hp+=10;break;
    }
    status.maximum.hp+=static_cast<std::uint32_t>(character.recovery)*5;
    status.maximum.sp+=static_cast<std::uint32_t>(character.dexterity)*5;
    status.maximum.mp+=static_cast<std::uint32_t>(character.wisdom)*5;
    return status;
}

std::array<AttackValues,3> calculate_attack(std::uint16_t level,const Attributes& a,const AttackModifiers& modifiers) {
    // ps_game.exe CUser::SetAttack 00454280. Keep the x87 float spills and extended intermediates.
    const auto dex=f32(a.dexterity),str=f32(a.strength),wis=f32(a.wisdom),intel=f32(a.intelligence),luck=f32(a.luck);
    const auto half_dex=static_cast<float>(dex*0.5F);
    const auto defense_base=static_cast<float>(luck*0.125F+half_dex);
    const auto fifth_dex=static_cast<float>(dex*0.2F);
    const auto item=f32(modifiers.item_attack);
    const auto critical=integer(luck*0.19F+f32(modifiers.critical));
    std::array<AttackValues,3> result{};
    for(std::size_t category=0;category<2;++category) {
        const auto& add=modifiers.categories[category];auto& value=result[category];
        value.accuracy=integer(f32(add.accuracy)+level+half_dex);
        value.evasion=integer(f32(add.evasion)+defense_base+level);
        value.defense=sum(add.defense,a.recovery,modifiers.physical_defense);
        value.critical_attack=critical;
    }
    result[0].attack=integer(str*1.3F+f32(modifiers.categories[0].attack)+item+fifth_dex);
    result[1].attack=integer(luck*0.3F+f32(modifiers.categories[1].attack)+item+str+fifth_dex);
    const auto& magic=modifiers.categories[2];
    result[2]={magic.accuracy,magic.evasion,integer(intel*1.3F+wis*0.2F+f32(magic.attack)+item),
        sum(magic.defense,modifiers.magic_defense,a.wisdom),critical,0};
    return result;
}
}
