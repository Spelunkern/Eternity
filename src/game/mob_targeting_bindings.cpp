#include "eternity/game/mob_targeting_bindings.hpp"
#include <charconv>
namespace eternity::game {
namespace {
std::uint32_t number(const storage::Row& row,std::size_t column,unsigned width) {
    if(column==0 || column>row.size()) throw std::runtime_error("Missing positional mob targeting binding column");
    const auto& cell=row[column-1];if(!cell) return 0;
    std::int64_t n=0;const auto& text=*cell;const auto result=std::from_chars(text.data(),text.data()+text.size(),n);
    const std::int64_t low=width==1?0:width==2?INT16_MIN:INT32_MIN;
    const std::int64_t high=width==1?UINT8_MAX:width==2?UINT16_MAX:UINT32_MAX;
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || n<low || n>high) throw std::runtime_error("Invalid positional mob targeting binding value");
    return static_cast<std::uint32_t>(n)&(width==1?0xffU:width==2?0xffffU:0xffffffffU);
}
}
MobTargetingBindings MobTargetingBindingDefinition::targeting() const {
    MobTargetingBindings result;result.ai=ai;
    for(std::size_t i=0;i<attacks.size();++i) {
        const auto& a=attacks[i];result.attacks[i]={a.enabled!=0,a.skill!=nullptr,a.skill?a.skill->cooldown_seconds:std::uint16_t{0}};
    }
    return result;
}
MobTargetingBindingCatalog MobTargetingBindingCatalog::load(std::uint16_t mob_max_id,std::uint16_t skill_max_id,
    const storage::ResultSet& mob_rows,const storage::ResultSet& skill_rows) {
    MobTargetingBindingCatalog result;result.skills_.resize(static_cast<std::size_t>(skill_max_id)+1);result.mobs_.resize(static_cast<std::size_t>(mob_max_id)+1);
    for(const auto& row:skill_rows.rows) {
        if(row.size()<82) throw std::runtime_error("Expected 82 positional MobSkills columns");
        MobTargetingBindingSkill s;s.id=static_cast<std::uint16_t>(number(row,1,2));s.level=100;
        s.effect=static_cast<std::uint8_t>(number(row,17,1));s.detail=static_cast<std::uint16_t>(number(row,18,2));s.cooldown_seconds=static_cast<std::uint16_t>(number(row,38,2));
        if(s.id>=result.skills_.size()) throw std::runtime_error("Mob skill row exceeds original MaxID allocation");
        result.skills_[s.id]=s;
    }
    for(const auto& row:mob_rows.rows) {
        if(row.size()<69) throw std::runtime_error("Expected 69 positional Mobs columns");
        MobTargetingBindingDefinition d;d.id=static_cast<std::uint16_t>(number(row,1,2));d.ai=static_cast<std::uint8_t>(number(row,5,1));d.chase_range=static_cast<std::uint8_t>(number(row,45,1));
        for(std::size_t i=0;i<d.attacks.size();++i) {
            auto& a=d.attacks[i];const auto c=46+i*8;
            a.type=static_cast<std::uint16_t>(number(row,c,2));a.time=number(row,c+1,4);a.range=static_cast<std::uint8_t>(number(row,c+2,1));
            a.damage=static_cast<std::uint16_t>(number(row,c+3,2));a.damage_add=static_cast<std::uint16_t>(number(row,c+4,2));
            a.attribute=static_cast<std::uint8_t>(number(row,c+5,1));a.special=static_cast<std::uint8_t>(number(row,c+6,1));a.enabled=static_cast<std::uint8_t>(number(row,c+7,1));
            if(a.type==0) {if(a.enabled!=0 && a.range>60) a.range=60;}
            else {
                // 004c1d7a..004c1de8 saves/restores the requested skill ID and
                // original enabled BYTE, not the mob ID or the attack index.
                a.skill=result.skill(a.type);
                if(!a.skill || !(a.skill->effect==1 || a.skill->effect==3 || (a.skill->effect==2 && (a.skill->detail==41 || a.skill->detail==42)))) {
                    a.type=0;a.enabled=0;a.skill=nullptr;
                }
            }
            if(i!=2 && a.attribute>4) a.attribute=0;
            if(i==2 && a.special>15) a.special=0;
        }
        const auto country_code=d.attacks[2].attribute;d.country=country_code==1?0:country_code==2?1:2;
        if(d.id>=result.mobs_.size()) throw std::runtime_error("Mob row exceeds original MaxID allocation");
        result.mobs_[d.id]=d;
    }
    return result;
}
const MobTargetingBindingDefinition* MobTargetingBindingCatalog::mob(std::uint32_t id) const noexcept {
    return id<mobs_.size() && mobs_[id].id!=0?&mobs_[id]:nullptr;
}
const MobTargetingBindingSkill* MobTargetingBindingCatalog::skill(std::uint32_t id) const noexcept {
    // Original GetMobSkillInfo 004027e0 allows <= stored COUNT (MaxID+1),
    // including a one-past-allocation read. Never manufacture a native pointer
    // from that undefined boundary; real in-range sparse slots retain ID guards.
    return id>0 && id<skills_.size() && skills_[id].id!=0?&skills_[id]:nullptr;
}
}
