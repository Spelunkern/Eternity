#include "eternity/game/skills.hpp"
#include <algorithm>
#include <bit>
#include <charconv>

namespace eternity::game {
namespace {
std::int32_t add(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));
}
std::size_t column(const storage::ResultSet& table,std::string_view name) {
    const auto found=std::find(table.columns.begin(),table.columns.end(),name);
    if(found==table.columns.end()) throw std::runtime_error("Missing original skill definition column");
    return static_cast<std::size_t>(found-table.columns.begin());
}
unsigned int number(const storage::Row& row,std::size_t index,unsigned int maximum) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing original skill definition value");
    const auto& text=*row[index];unsigned int value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>maximum)
        throw std::runtime_error("Invalid original skill definition value");
    return value;
}
}
SkillDefinitions SkillDefinitions::load(const storage::ResultSet& table) {
    // CGameData::LoadConstSkill 004bffe0: only three skill levels have runtime tables.
    SkillDefinitions definitions;
    for(const auto& row:table.rows) {
        auto n=[&](std::string_view name,unsigned int maximum=255){return number(row,column(table,name),maximum);};
        SkillDefinition skill;skill.id=static_cast<std::uint16_t>(n("SkillID",65535));skill.level=static_cast<std::uint8_t>(n("SkillLevel"));
        if(skill.level==0 || skill.level>3) continue;
        skill.detail=static_cast<std::uint16_t>(n("TypeDetail",65535));skill.effect=static_cast<std::uint8_t>(n("TypeEffect"));
        if(skill.detail==93) skill.effect=4;else if(skill.detail==122) skill.effect=6;
        skill.weapon1=static_cast<std::uint8_t>(std::min(n("Weapon1"),19U));skill.weapon2=static_cast<std::uint8_t>(std::min(n("Weapon2"),19U));
        skill.weapon_value=static_cast<std::uint8_t>(n("Weaponvalue"));skill.bag=static_cast<std::uint8_t>(n("Bag"));
        skill.attribute=static_cast<std::uint8_t>(n("AttrType"));if(skill.attribute>8) skill.attribute=0;
        skill.defense_type=static_cast<std::uint8_t>(std::clamp(n("DefenceType"),1U,3U)-1);
        skill.defense_value=static_cast<std::uint8_t>(n("DefenceValue"));skill.apply_range=static_cast<std::uint8_t>(n("ApplyRange"));
        skill.aggro=static_cast<std::uint16_t>(n("Arrow",65535));skill.change_type=static_cast<std::uint16_t>(n("ChangeType",65535));skill.change_level=static_cast<std::uint16_t>(n("ChangeLevel",65535));
        for(std::size_t i=0;i<3;++i) {
            auto& ability=skill.abilities[i];ability.type=static_cast<std::uint8_t>(n("AbilityType"+std::to_string(i+1)));
            ability.value=static_cast<std::uint16_t>(n("AbilityValue"+std::to_string(i+1),65535));
            if(ability.type!=0 && ability.value==0) ability.value=1;
        }
        definitions.skills_[{skill.id,skill.level}]=skill;
    }
    return definitions;
}
const SkillDefinition* SkillDefinitions::find(std::uint16_t id,std::uint8_t level) const {
    if(id==0 || level==0 || level>3 || !skills_.contains({id,1})) return nullptr;
    const auto found=skills_.find({id,level});return found==skills_.end()?nullptr:&found->second;
}
void add_skill_ability(SkillStatus& status,std::uint8_t effect,std::uint8_t type,std::int32_t value) {
    // CUser::SetSkillAbility 00485e00, ps_game.exe. Ability IDs are one-based.
    auto& equipment=status.equipment;auto& attributes=equipment.base.attributes;auto& maximum=equipment.base.maximum;
    auto& attack=equipment.attack;auto& benefits=status.benefits;
    const auto resource=[&](std::uint32_t& target,unsigned int scale){if(effect!=3) target+=static_cast<std::uint32_t>(value)*scale;};
    switch(type) {
        case 1:resource(maximum.hp,1);break;
        case 2:resource(maximum.mp,1);break;
        case 3:resource(maximum.sp,1);break;
        case 4:attributes.strength=add(attributes.strength,value);break;
        case 5:attributes.recovery=add(attributes.recovery,value);resource(maximum.hp,5);break;
        case 6:attributes.intelligence=add(attributes.intelligence,value);break;
        case 7:attributes.wisdom=add(attributes.wisdom,value);resource(maximum.mp,5);break;
        case 8:attributes.dexterity=add(attributes.dexterity,value);resource(maximum.sp,5);break;
        case 9:attributes.luck=add(attributes.luck,value);break;
        case 10:status.sitting_recovery_percent[0]=add(status.sitting_recovery_percent[0],value);break;
        case 11:status.sitting_recovery_percent[1]=add(status.sitting_recovery_percent[1],value);break;
        case 12:status.sitting_recovery_percent[2]=add(status.sitting_recovery_percent[2],value);break;
        case 13:status.sp_cost_reduction=add(status.sp_cost_reduction,value);break;
        case 14:status.mp_cost_reduction=add(status.mp_cost_reduction,value);break;
        case 15:equipment.range=add(equipment.range,value);break;
        case 16:equipment.attack_speed=add(equipment.attack_speed,value);break;
        case 17:equipment.move_speed=add(equipment.move_speed,value);break;
        case 18:attack.critical=add(attack.critical,value);break;
        case 19:status.cool_time=add(status.cool_time,value);break;
        case 20:case 21:case 22:attack.categories[type-20].accuracy=add(attack.categories[type-20].accuracy,value);break;
        case 23:case 24:case 25:attack.categories[type-23].attack=add(attack.categories[type-23].attack,value);break;
        case 26:case 27:case 28:attack.categories[type-26].defense=add(attack.categories[type-26].defense,value);break;
        case 29:case 30:case 31:attack.categories[type-29].evasion=add(attack.categories[type-29].evasion,value);break;
        case 32:case 33:case 34:status.attack_disabled[type-32]=add(status.attack_disabled[type-32],value);break;
        case 35:benefits.experience_double=add(benefits.experience_double,value);break;
        case 36:benefits.no_durability_loss=add(benefits.no_durability_loss,value);break;
        case 37:benefits.no_item_loss=add(benefits.no_item_loss,value);break;
        case 38:benefits.no_exp_loss=add(benefits.no_exp_loss,value);break;
        case 39:benefits.bank_enabled=add(benefits.bank_enabled,value);break;
        case 40:case 41: {
            const auto mode=static_cast<std::int32_t>(type-39);
            if(value>0) benefits.amulet=mode;else if(benefits.amulet==mode) benefits.amulet=0;
            break;
        }
        case 42:if(value>0) benefits.amulet=3;else if(benefits.amulet==3) benefits.amulet=0;
            benefits.bank_double=add(benefits.bank_double,value);break;
        case 43:benefits.bank_double=add(benefits.bank_double,value);break;
        case 44:benefits.mob_gold=std::max(value,0);break;
        case 45:benefits.no_equipped_item_loss=add(benefits.no_equipped_item_loss,value);break;
        case 46:benefits.rebirth=add(benefits.rebirth,value);break;
    }
}
void add_passive(SkillStatus& status,const SkillDefinition& skill,std::optional<std::uint8_t> weapon_type,std::optional<std::uint8_t> shield_type) {
    // CUser::AddSkillPassive 00489a80, ps_game.exe. Masteries add the updated table total.
    auto& equipment=status.equipment;
    const auto mastery=[&](auto& table,std::int32_t& applied){
        for(const auto type:{skill.weapon1,skill.weapon2}) if(type!=0) {
            if(type>=table.size()) throw std::invalid_argument("Passive weapon type exceeds original table");
            table[type]=static_cast<std::uint8_t>(table[type]+skill.weapon_value);
        }
        if(weapon_type && (*weapon_type==skill.weapon1 || *weapon_type==skill.weapon2)) {
            if(*weapon_type>=table.size()) throw std::invalid_argument("Equipped weapon type exceeds original passive table");
            applied=add(applied,table[*weapon_type]);
        }
    };
    switch(skill.detail) {
        case 1:mastery(equipment.weapon_damage,equipment.attack.item_attack);break;
        case 2:status.max_bags=skill.bag;break;
        case 4:for(const auto& ability:skill.abilities) add_skill_ability(status,skill.effect,ability.type,ability.value);break;
        case 5:status.max_move_speed=skill.level;break;
        case 6:status.hp_ability_applied=false;status.hp_ability_id=skill.id;status.hp_ability_level=skill.level;status.hp_ability_previous=0;break;
        case 8:mastery(equipment.weapon_speed,equipment.attack_speed);break;
        case 9:status.cross_road=add(status.cross_road,1);break;
        case 10:
            if(skill.weapon1!=0) equipment.shield_value=static_cast<std::uint8_t>(equipment.shield_value+skill.weapon_value);
            if(shield_type && *shield_type==skill.weapon1) equipment.attack.physical_defense=add(equipment.attack.physical_defense,equipment.shield_value);
            break;
    }
}
void initialize_passives(SkillStatus& status,const std::vector<PlayerSkill>& skills,const std::vector<PlayerItem>& items,
    const SkillDefinitions& definitions,const ItemDefinitions& item_definitions) {
    // CUser::InitPassive 004899a0 processes learned skills in their original load order.
    std::optional<std::uint8_t> weapon,shield;
    for(const auto& item:items) if(item.bag==0 && (item.slot==5 || item.slot==6)) {
        const auto* definition=item_definitions.find(item.type,item.type_id);
        if(!definition) throw std::runtime_error("Missing original equipped skill item definition");
        (item.slot==5?weapon:shield)=definition->real_type;
    }
    for(const auto& skill:skills) if(const auto* definition=definitions.find(skill.id,skill.level);definition && definition->effect==0)
        add_passive(status,*definition,weapon,shield);
}
void apply_billing(SkillStatus& status,std::int32_t type) {
    // CUser::BillingApply 00457a70, ps_game.exe.
    if(type<1 || type>3) return;
    if(type>=2) {status.benefits.no_durability_loss=add(status.benefits.no_durability_loss,1);status.benefits.experience_half_bonus=add(status.benefits.experience_half_bonus,1);}
    status.benefits.bank_double=add(status.benefits.bank_double,1);
}
}
