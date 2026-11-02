#include "eternity/game/equipment.hpp"
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
}
