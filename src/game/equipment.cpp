#include "eternity/game/equipment.hpp"
#include "eternity/game/item_quests.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <fstream>

namespace eternity::game {
namespace {
std::int32_t add(std::int32_t a,std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));
}
std::size_t column(const storage::ResultSet& table,std::string_view name) {
    const auto found=std::find(table.columns.begin(),table.columns.end(),name);
    if(found==table.columns.end()) throw std::runtime_error("Missing original item definition column");
    return static_cast<std::size_t>(found-table.columns.begin());
}
unsigned int number(const storage::Row& row,std::size_t index,unsigned int maximum) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing original item definition value");
    const auto& text=*row[index];unsigned int value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>maximum)
        throw std::runtime_error("Invalid original item definition value");
    return value;
}
std::uint8_t real_type(std::uint8_t type) {
    // GetItemRType 004bdad0, LITEM_TYPE 00542188 and its lists at 00531914, ps_game.exe.
    static const std::array<std::vector<std::uint8_t>,27> groups={{{1,45},{2,46},{3,47},{4,48},{5,49,50},{6,51,52},
        {7,53,54},{8,55,56},{9,57},{10,58},{11,59},{12,60,61},{13,62,63},{14,64},{15,65},
        {16,31,66,72,81,87},{17,32,67,73,82,88},{18,33,68,74,83,89},{19,34,69,75,84,90},
        {20,35,70,76,85,91},{21,36,71,77,86,92},{22,37},{23,38},{24,39},{40,41},{25,44,100},{27,28,29}}};
    for(std::size_t i=0;i<groups.size();++i) if(std::find(groups[i].begin(),groups[i].end(),type)!=groups[i].end())
        return static_cast<std::uint8_t>(i+1);
    return 0;
}
void constants(EquipmentStatus& status,const ItemDefinition& item,bool resources) {
    auto& a=status.base.attributes;const auto& b=item.attributes;
    a.strength=add(a.strength,b.strength);a.dexterity=add(a.dexterity,b.dexterity);a.recovery=add(a.recovery,b.recovery);
    a.intelligence=add(a.intelligence,b.intelligence);a.wisdom=add(a.wisdom,b.wisdom);a.luck=add(a.luck,b.luck);
    auto& maximum=status.base.maximum;
    maximum.hp+=static_cast<std::uint32_t>(b.recovery)*5;
    maximum.sp+=static_cast<std::uint32_t>(b.dexterity)*5;
    maximum.mp+=static_cast<std::uint32_t>(b.wisdom)*5;
    if(resources) {maximum.hp+=item.resources.hp;maximum.mp+=item.resources.mp;maximum.sp+=item.resources.sp;}
}
std::uint32_t attribute(const ItemDefinition& definition,const PlayerItem& item,const ItemDefinitions& definitions) {
    auto value=definition.attribute;
    for(const auto gem_id:item.gems) if(const auto* gem=definitions.find(30,gem_id);gem && gem->attribute!=0 && gem->attribute!=value) {
        if(value!=0) return 0;
        value=gem->attribute;
    }
    return value;
}
void gem(EquipmentStatus& status,std::uint8_t slot,const ItemDefinition& item) {
    // CUser::GemEquipmentAdd 004560c0, ps_game.exe.
    status.range=add(status.range,item.range);status.attack_speed=add(status.attack_speed,item.attack_time);status.move_speed=add(status.move_speed,item.speed);
    if(!status.transformed) {
        status.attack.item_attack=add(status.attack.item_attack,item.effects[0][0]);status.variance=add(status.variance,item.effects[1][0]);
        status.attack.physical_defense=add(status.attack.physical_defense,item.effects[2][0]);status.attack.magic_defense=add(status.attack.magic_defense,item.effects[3][0]);
    }
    constants(status,item,item.special!=16);
    if(item.special==16) {
        const std::array<std::uint32_t,3> values={item.resources.hp,item.resources.sp,item.resources.mp};
        for(std::size_t i=0;i<3;++i) {status.standing_recovery[i]=add(status.standing_recovery[i],static_cast<std::int32_t>(values[i]));
            status.sitting_recovery[i]=add(status.sitting_recovery[i],static_cast<std::int32_t>(values[i]));}
    }
    status.damage_reduction=add(status.damage_reduction,item.experience);
    if(slot!=5 && item.special>0 && item.special<16) status.state_blocks[item.special]=add(status.state_blocks[item.special],1);
}
std::string trim(std::string value) {
    const auto start=value.find_first_not_of(" \t\r\n");
    return start==std::string::npos?std::string{}:value.substr(start,value.find_last_not_of(" \t\r\n")-start+1);
}
}
ItemDefinitions ItemDefinitions::load(const storage::ResultSet& table) {
    // CGameData::LoadConstItem 004c6420, ps_game.exe; retain unsigned SQL word/byte contracts.
    ItemDefinitions result;
    for(const auto& row:table.rows) {
        auto n=[&](std::string_view key,unsigned int maximum=65535){return number(row,column(table,key),maximum);};
        ItemDefinition item;item.id=n("ItemID",2147483647);item.type=static_cast<std::uint8_t>(n("Type",255));item.type_id=static_cast<std::uint8_t>(n("TypeID",255));
        item.range=static_cast<std::uint16_t>(n("Range"));item.required_wisdom=static_cast<std::uint16_t>(n("ReqWis"));
        item.attack_time=static_cast<std::uint8_t>(n("AttackTime",255));item.speed=static_cast<std::uint8_t>(n("Speed",255));
        item.attribute=static_cast<std::uint8_t>(n("Attrib",255));if(item.attribute>8) item.attribute=0;
        item.special=static_cast<std::uint8_t>(n("Special",255));item.experience=static_cast<std::uint8_t>(n("Exp",255));
        item.stack_count=static_cast<std::uint8_t>(n("Count",255));
        item.server=static_cast<std::uint8_t>(n("Server",255));if(item.server>=10) item.server=0;
        const auto quality=n("Quality");item.quality={static_cast<std::uint16_t>(quality),static_cast<std::uint16_t>(quality/2),
            static_cast<std::uint16_t>(static_cast<long double>(quality)*0.1F)};
        for(std::size_t i=0;i<4;++i) {const auto effect=n("Effect"+std::to_string(i+1));item.effects[i]={static_cast<std::uint16_t>(effect),static_cast<std::uint16_t>(effect/2),0};}
        item.attributes={static_cast<std::int32_t>(n("ConstStr")),static_cast<std::int32_t>(n("ConstDex")),static_cast<std::int32_t>(n("ConstRec")),
            static_cast<std::int32_t>(n("ConstInt")),static_cast<std::int32_t>(n("ConstWis")),static_cast<std::int32_t>(n("ConstLuc"))};
        item.resources={n("ConstHP"),n("ConstMP"),n("ConstSP")};
        if(item.server!=0 && item.required_wisdom==0) item.resources.mp=1;
        item.real_type=real_type(item.type);
        result.items_[{item.type,item.type_id}]=item;
    }
    return result;
}
const ItemDefinition* ItemDefinitions::find(std::uint8_t type,std::uint8_t id) const {
    if(type==0 || id==0) return nullptr;
    const auto found=items_.find({type,id});return found==items_.end()?nullptr:&found->second;
}
void ItemDefinitions::initialize_quest_flags(const ItemQuestLinks& links) {
    // CGameData::CalcQuestItem 004bf400: clear each flag, then test countStart.
    for(auto& [key,item]:items_) {const auto* link=links.find(key.first,key.second);item.quest_start=link && !link->start.empty();}
}
EnchantValues EnchantValues::read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open original enchant configuration");
    EnchantValues result;std::string line;bool additions=false;
    while(std::getline(input,line)) {
        if(const auto comment=line.find(';');comment!=std::string::npos) line.erase(comment);
        line=trim(line);if(line.empty()) continue;
        std::transform(line.begin(),line.end(),line.begin(),[](unsigned char c){return c>='A' && c<='Z'?static_cast<char>(c+32):static_cast<char>(c);});
        if(line.front()=='[') {additions=line=="[lapisianenchantaddvalue]";continue;}
        if(!additions) continue;
        const auto equal=line.find('=');if(equal==std::string::npos) continue;
        const auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
        std::size_t category=0,digits=0;
        if(key.starts_with("weaponstep")) digits=10;else if(key.starts_with("defensestep")) {digits=11;category=1;}else continue;
        unsigned int step=0;const auto parsed=std::from_chars(key.data()+digits,key.data()+key.size(),step);
        if(parsed.ec!=std::errc{} || parsed.ptr!=key.data()+key.size() || step>20) continue;
        // GetPrivateProfileInt accepts signed integers; the original stores their low word.
        std::int32_t amount=0;std::from_chars(value.data(),value.data()+value.size(),amount);
        result.additions[step][category]=static_cast<std::uint16_t>(amount);
    }
    return result;
}
CraftValues decode_craft(const ItemDefinition& item,std::string_view name,const EnchantValues& enchant) {
    // CItem::CopyCraftExpassion 004ba670 and GetEnchantStep 004ba9b0, ps_game.exe.
    CraftValues result;
    if(item.server==0 || item.required_wisdom==0 || item.required_wisdom>=100) return result;
    name=name.substr(0,name.find('\0'));
    if(name.size()!=20) {result.name=std::string(20,'0');return result;}
    result.name=name;
    for(std::size_t i=0;i<10;++i) {
        const auto a=name[2*i],b=name[2*i+1];if(a<'0' || a>'9' || b<'0' || b>'9') continue;
        auto value=static_cast<unsigned int>((a-'0')*10+b-'0');if(value==0) continue;
        if(i<9) result.additions[i]=static_cast<std::uint16_t>(value);
        else {result.enchant_step=static_cast<std::uint8_t>(value);const auto defense=value>50;if(defense) value-=50;
            if(value<=20) result.additions[defense?10:9]=enchant.additions[value][defense?1:0];}
    }
    return result;
}
std::uint32_t item_attribute(const PlayerItem& item,const ItemDefinitions& definitions) {
    // CUser::SetAttrAtk 00455f80 / SetAttrDef 00456020 use the same original item/gem rule.
    const auto* definition=definitions.find(item.type,item.type_id);
    if(!definition) throw std::runtime_error("Missing original element item definition");
    return attribute(*definition,item,definitions);
}
EquipmentWear decrease_equipment_quality(EquipmentStatus& status,std::uint8_t slot,PlayerItem* item,std::int32_t amount,const ItemDefinitions& definitions) {
    // ps_game.exe CUser::QualityDec 0045b350. WORD storage follows a wrapping
    // DWORD subtraction and sign-bit clamp; negative decrements can raise quality.
    if(!item || item->quality==0) return {};
    if(slot>=status.quality.size()) throw std::invalid_argument("Invalid original durability slot");
    const auto* info=definitions.find(item->type,item->type_id);
    if(!info) throw std::runtime_error("Missing original durability item definition");
    const auto remaining=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(item->quality)-static_cast<std::uint32_t>(amount));
    item->quality=remaining<0?0:static_cast<std::uint16_t>(remaining);
    const std::uint8_t next=item->quality<=info->quality[2]?2:item->quality<=info->quality[1]?1:0;
    const auto previous=status.quality[slot];
    if(next==previous) return {true,false};
    if(previous>2) throw std::invalid_argument("Invalid cached original durability tier");
    // ItemEquipmentAttackRemQ/AddQ 004566d0/00456620 affect only these four
    // accumulators. Craft attack is removed and added unchanged, so cancels.
    const auto old_index=slot>=8?0:previous,new_index=slot>=8?0:next;
    const auto change=[&](std::int32_t& value,std::size_t effect) {
        value=add(value,static_cast<std::int32_t>(info->effects[effect][new_index])-info->effects[effect][old_index]);
    };
    if(slot==5 || slot>=8) {change(status.attack.item_attack,0);change(status.variance,1);}
    if(slot!=5) {change(status.attack.physical_defense,2);change(status.attack.magic_defense,3);}
    status.quality[slot]=next;
    return {true,true};
}
EquipmentWear wear_defense(EquipmentStatus& status,std::vector<PlayerItem>& items,const ItemDefinitions& definitions,
    std::int32_t& cursor,std::int32_t no_durability_loss,std::uint8_t shape) {
    // QualityByDefense 0045b290 scans only the remaining suffix this invocation;
    // wrapping the saved cursor does not restart this scan at the first slot.
    if(no_durability_loss!=0 || shape!=0 || cursor>=6) return {};
    if(cursor<0) throw std::invalid_argument("Invalid original defense durability cursor");
    constexpr std::array<std::uint8_t,6> order{6,0,1,3,2,4};
    for(auto index=cursor;index<6;++index) {
        const auto slot=order[static_cast<std::size_t>(index)];PlayerItem* equipped=nullptr;
        for(auto& item:items) if(item.bag==0 && item.slot==slot) {
            if(equipped) throw std::invalid_argument("Unresolved duplicate equipment during durability wear");
            equipped=&item;
        }
        const auto result=decrease_equipment_quality(status,slot,equipped,1,definitions);
        if(++cursor>=6) cursor=0;
        if(result.applied) return result;
    }
    return {};
}
void add_equipment(EquipmentStatus& status,std::uint8_t slot,const PlayerItem& item,const ItemDefinitions& definitions,const EnchantValues& enchant) {
    // CUser::ItemEquipmentAdd 00454c70, ps_game.exe. Called for equipped slots 0..12.
    if(slot>=13) throw std::invalid_argument("Invalid original equipment slot");
    const auto* definition=definitions.find(item.type,item.type_id);if(!definition) throw std::runtime_error("Missing original equipped item definition");
    const auto& info=*definition;
    const std::uint8_t quality=item.quality<=info.quality[2]?2:item.quality<=info.quality[1]?1:0;
    status.quality[slot]=quality;
    const bool weapon=slot==5,accessory=slot>=8;
    if(weapon || accessory) {
        status.range=add(status.range,info.range);status.attack_speed=add(status.attack_speed,info.attack_time);
        if(accessory) status.move_speed=add(status.move_speed,info.speed);
        if(weapon) {
            if(info.real_type>=20) throw std::runtime_error("Original weapon type exceeds passive table");
            status.attack_speed=add(status.attack_speed,status.weapon_speed[info.real_type]);
        }
    }
    if(!status.transformed) {
        if(weapon || accessory) {
            const auto index=accessory?0:quality;
            status.attack.item_attack=add(status.attack.item_attack,info.effects[0][index]);status.variance=add(status.variance,info.effects[1][index]);
            if(weapon) status.attack.item_attack=add(status.attack.item_attack,status.weapon_damage[info.real_type]);
        }
        if(!weapon) {
            const auto index=accessory?0:quality;
            status.attack.physical_defense=add(status.attack.physical_defense,info.effects[2][index]);status.attack.magic_defense=add(status.attack.magic_defense,info.effects[3][index]);
        }
    }
    constants(status,info,true);
    if(weapon && std::all_of(status.attack_attribute_buffs.begin(),status.attack_attribute_buffs.end(),[](auto value){return value==0;}))
        status.attack_attribute=attribute(info,item,definitions);
    if(slot==1 && !status.defense_attribute_skill) status.defense_attribute=attribute(info,item,definitions);
    if(slot==6) status.attack.physical_defense=add(status.attack.physical_defense,status.shield_value);
    const auto craft=decode_craft(info,item.craft,enchant);
    if(info.server!=0) {
        const auto& c=craft.additions;auto& a=status.base.attributes;auto& maximum=status.base.maximum;
        a.strength=add(a.strength,c[0]);a.dexterity=add(a.dexterity,c[1]);a.recovery=add(a.recovery,c[2]);
        a.intelligence=add(a.intelligence,c[3]);a.wisdom=add(a.wisdom,c[4]);a.luck=add(a.luck,c[5]);
        maximum.hp+=static_cast<std::uint32_t>(c[2])*5+static_cast<std::uint32_t>(c[6])*100;
        maximum.sp+=static_cast<std::uint32_t>(c[1])*5+static_cast<std::uint32_t>(c[8])*100;
        maximum.mp+=static_cast<std::uint32_t>(c[4])*5+static_cast<std::uint32_t>(c[7])*100;
        if(!status.transformed) status.attack.item_attack=add(status.attack.item_attack,c[9]);
        status.damage_reduction=add(status.damage_reduction,c[10]);
    }
    if(slot!=7) for(const auto gem_id:item.gems) if(const auto* info_gem=definitions.find(30,gem_id)) gem(status,slot,*info_gem);
}
void remove_equipment(EquipmentStatus& status,std::uint8_t slot,const PlayerItem& item,const ItemDefinitions& definitions,const EnchantValues& enchant,
    bool initializing,EquipmentRemovalWorld& world) {
    // ps_game.exe CUser::ItemEquipmentRem 004553c0 and GemEquipmentRem 00456280.
    if(slot>=13) throw std::invalid_argument("Invalid original equipment removal slot");
    const auto* definition=definitions.find(item.type,item.type_id);if(!definition) throw std::runtime_error("Missing original removed item definition");
    const auto& info=*definition;const auto before=status;const bool weapon=slot==5,accessory=slot>=8;
    const auto quality=accessory?0:status.quality[slot];if(quality>2) throw std::invalid_argument("Invalid cached original removal quality tier");
    auto sub=[](std::int32_t& value,std::int32_t amount) {value=add(value,std::bit_cast<std::int32_t>(0U-static_cast<std::uint32_t>(amount)));};
    const auto subtract_constants=[&](const ItemDefinition& value,bool resources) {
        auto& a=status.base.attributes;const auto& b=value.attributes;
        sub(a.strength,b.strength);sub(a.dexterity,b.dexterity);sub(a.recovery,b.recovery);sub(a.intelligence,b.intelligence);sub(a.wisdom,b.wisdom);sub(a.luck,b.luck);
        auto& maximum=status.base.maximum;maximum.hp-=static_cast<std::uint32_t>(b.recovery)*5U;maximum.sp-=static_cast<std::uint32_t>(b.dexterity)*5U;maximum.mp-=static_cast<std::uint32_t>(b.wisdom)*5U;
        if(resources) {maximum.hp-=value.resources.hp;maximum.sp-=value.resources.sp;maximum.mp-=value.resources.mp;}
    };
    if(weapon || accessory) {
        sub(status.range,info.range);sub(status.attack_speed,info.attack_time);
        if(accessory) sub(status.move_speed,info.speed);
        if(weapon) {if(info.real_type>=20) throw std::runtime_error("Invalid original removal weapon mastery index");sub(status.attack_speed,status.weapon_speed[info.real_type]);}
    }
    if(!status.transformed) {
        if(weapon || accessory) {sub(status.attack.item_attack,info.effects[0][quality]);sub(status.variance,info.effects[1][quality]);if(weapon) sub(status.attack.item_attack,status.weapon_damage[info.real_type]);}
        if(!weapon) {sub(status.attack.physical_defense,info.effects[2][quality]);sub(status.attack.magic_defense,info.effects[3][quality]);}
    }
    if(slot==6) sub(status.attack.physical_defense,status.shield_value);
    subtract_constants(info,true);
    if(weapon) {world.remove_weapon_elements();status.attack_attribute=0;}
    else if(slot==1) status.defense_attribute=0;
    if(info.server!=0) {
        const auto c=decode_craft(info,item.craft,enchant).additions;auto& a=status.base.attributes;auto& maximum=status.base.maximum;
        sub(a.strength,c[0]);sub(a.dexterity,c[1]);sub(a.recovery,c[2]);sub(a.intelligence,c[3]);sub(a.wisdom,c[4]);sub(a.luck,c[5]);
        maximum.hp-=static_cast<std::uint32_t>(c[2])*5U+static_cast<std::uint32_t>(c[6])*100U;maximum.sp-=static_cast<std::uint32_t>(c[1])*5U+static_cast<std::uint32_t>(c[8])*100U;maximum.mp-=static_cast<std::uint32_t>(c[4])*5U+static_cast<std::uint32_t>(c[7])*100U;
        if(!status.transformed) sub(status.attack.item_attack,c[9]);
        if(c[10]!=0) {sub(status.damage_reduction,c[10]);if(status.damage_reduction<0) {world.negative_craft_reduction(item,status.damage_reduction);status.damage_reduction=0;}}
    }
    if(slot!=7) for(const auto gem_id:item.gems) if(const auto* gem_info=definitions.find(30,gem_id)) {
        const auto& g=*gem_info;sub(status.range,g.range);sub(status.attack_speed,g.attack_time);sub(status.move_speed,g.speed);
        if(!status.transformed) {sub(status.attack.item_attack,g.effects[0][0]);sub(status.variance,g.effects[1][0]);sub(status.attack.physical_defense,g.effects[2][0]);sub(status.attack.magic_defense,g.effects[3][0]);}
        subtract_constants(g,g.special!=16);
        if(g.special==16) {const std::array<std::uint32_t,3> values{g.resources.hp,g.resources.sp,g.resources.mp};for(std::size_t i=0;i<3;++i) {sub(status.standing_recovery[i],std::bit_cast<std::int32_t>(values[i]));sub(status.sitting_recovery[i],std::bit_cast<std::int32_t>(values[i]));}}
        sub(status.damage_reduction,g.experience);if(!weapon && g.special>0 && g.special<16) sub(status.state_blocks[g.special],1);
    }
    status.quality[slot]=0;
    if(!initializing) {
        if(before.attack_speed!=status.attack_speed || before.move_speed!=status.move_speed) world.equipment_speed_changed();
        if(before.base.maximum.hp!=status.base.maximum.hp) world.equipment_maximum_changed(0);
        if(before.base.maximum.sp!=status.base.maximum.sp) world.equipment_maximum_changed(2);
        if(before.base.maximum.mp!=status.base.maximum.mp) world.equipment_maximum_changed(1);
        world.equipment_recover();
    }
}
EquipmentStatus initialize_equipment(BaseStatus base,const std::vector<PlayerItem>& items,const ItemDefinitions& definitions,const EnchantValues& enchant) {
    EquipmentStatus result;result.base=base;
    std::array<const PlayerItem*,13> slots{};
    for(const auto& item:items) if(item.bag==0 && item.slot<slots.size()) {
        if(slots[item.slot]) throw std::invalid_argument("Inventory must resolve duplicate equipment slots before initialization");
        slots[item.slot]=&item;
    }
    // InitEquipment 00454b27..00454b56 processes exactly slots 0..12, before faction blessing.
    for(std::uint8_t slot=0;slot<13;++slot) if(slots[slot]) add_equipment(result,slot,*slots[slot],definitions,enchant);
    return result;
}
MobAttackTemplates MobAttackTemplates::load(const storage::ResultSet& table) {
    // LoadConstMob 004c1720, ODBC columns 1/3/9/18/19/49/50 retain WORD/DWORD values.
    MobAttackTemplates result;
    for(const auto& row:table.rows) {
        auto n=[&](std::string_view key) {
            const auto index=column(table,key);if(index>=row.size() || !row[index]) throw std::runtime_error("Missing original mob WORD");
            const auto& text=*row[index];std::int64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<INT16_MIN || value>UINT16_MAX) throw std::runtime_error("Invalid original mob WORD");
            return static_cast<std::uint16_t>(value);
        };
        MobAttackTemplate mob;mob.id=static_cast<std::uint16_t>(n("MobID"));mob.level=static_cast<std::uint16_t>(n("Level"));
        // SQL HP is signed int while original storage is a DWORD.
        const auto hp_column=column(table,"HP");if(hp_column>=row.size() || !row[hp_column]) throw std::runtime_error("Missing original mob HP");
        const auto& text=*row[hp_column];std::int64_t hp=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),hp);
        if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || hp<INT32_MIN || hp>UINT32_MAX) throw std::runtime_error("Invalid original mob HP");
        mob.hp=hp==0?1U:static_cast<std::uint32_t>(hp);mob.attack=static_cast<std::uint16_t>(n("Attack1"));mob.variance=static_cast<std::uint16_t>(n("Attackplus1"));
        mob.physical_defense=static_cast<std::uint16_t>(n("Defense"));mob.magic_defense=static_cast<std::uint16_t>(n("Magic"));result.templates_[mob.id]=mob;
    }
    return result;
}
const MobAttackTemplate* MobAttackTemplates::find(std::uint32_t index) const {
    // GetMobInfo 004026d0 / InitEquipment 00454bf1: index, embedded ID, HP and level must be valid.
    if(index>65535) return nullptr;
    const auto found=templates_.find(static_cast<std::uint16_t>(index));
    return found!=templates_.end() && found->second.id!=0 && found->second.level!=0 && found->second.hp!=0?&found->second:nullptr;
}
void rebuild_transformed_attack(EquipmentStatus& status,const MobAttackTemplate* mob) {
    // CUser::InitEquipment(bool=false) 00454ba0: clear four modifiers, even when lookup fails.
    status.attack.item_attack=mob?mob->attack:0;status.variance=mob?mob->variance:0;
    status.attack.physical_defense=mob?mob->physical_defense:0;status.attack.magic_defense=mob?mob->magic_defense:0;
}
void rebuild_equipment_attack(EquipmentStatus& status,const std::vector<PlayerItem>& items,const ItemDefinitions& definitions,const EnchantValues& enchant) {
    // InitEquipment(bool=true) 00454ba0 and ItemEquipmentAttackAdd 00456450 use cached quality tiers.
    rebuild_transformed_attack(status,nullptr);std::array<const PlayerItem*,13> slots{};
    for(const auto& item:items) if(item.bag==0 && item.slot<slots.size()) {
        if(slots[item.slot]) throw std::invalid_argument("Unresolved duplicate equipment during attack rebuild");
        slots[item.slot]=&item;
    }
    for(std::size_t slot=0;slot<slots.size();++slot) if(const auto* item=slots[slot]) {
        const auto* definition=definitions.find(item->type,item->type_id);if(!definition) throw std::runtime_error("Missing original attack rebuild item definition");
        const auto& info=*definition;const auto quality=slot>=8?0:status.quality[slot];
        if(quality>2) throw std::invalid_argument("Invalid cached original equipment quality tier");
        if(slot==5 || slot>=8) {
            status.attack.item_attack=add(status.attack.item_attack,info.effects[0][quality]);status.variance=add(status.variance,info.effects[1][quality]);
            if(slot==5) {if(info.real_type>=20) throw std::runtime_error("Invalid original weapon mastery index");status.attack.item_attack=add(status.attack.item_attack,status.weapon_damage[info.real_type]);}
        }
        if(slot!=5) {
            status.attack.physical_defense=add(status.attack.physical_defense,info.effects[2][quality]);status.attack.magic_defense=add(status.attack.magic_defense,info.effects[3][quality]);
            if(slot==6) status.attack.physical_defense=add(status.attack.physical_defense,status.shield_value);
        }
        if(info.server!=0) status.attack.item_attack=add(status.attack.item_attack,decode_craft(info,item->craft,enchant).additions[9]);
        if(slot!=7) for(const auto id:item->gems) if(const auto* gem_info=definitions.find(30,id)) {
            status.attack.item_attack=add(status.attack.item_attack,gem_info->effects[0][0]);status.variance=add(status.variance,gem_info->effects[1][0]);
            status.attack.physical_defense=add(status.attack.physical_defense,gem_info->effects[2][0]);status.attack.magic_defense=add(status.attack.magic_defense,gem_info->effects[3][0]);
        }
    }
}
}
