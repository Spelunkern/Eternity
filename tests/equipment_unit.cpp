#include "eternity/game/equipment.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
eternity::storage::ResultSet item_definitions() {
    using namespace eternity::storage;
    ResultSet table;table.columns={"ItemID","Type","TypeID","Range","ReqWis","AttackTime","Speed","Attrib","Special","Exp","Server",
        "Quality","Effect1","Effect2","Effect3","Effect4","ConstStr","ConstDex","ConstRec","ConstInt","ConstWis","ConstLuc","ConstHP","ConstMP","ConstSP"};
    for(unsigned int type:{5U,16U,21U,45U}) table.rows.push_back({std::to_string(type*1000+1),std::to_string(type),"1","4","50","2","1","1","0","0","1",
        "1000","1001","1002","1003","1004","1","2","3","4","5","6","101","303","202"});
    table.rows.push_back({"30001","30","1","2","0","1","3","1","4","5","0","0","31","32","33","34","11","12","13","14","15","16","21","23","22"});
    table.rows.push_back({"30002","30","2","0","0","0","0","0","16","0","0","0","0","0","0","0","1","2","3","4","5","6","7","9","8"});
    table.rows.push_back({"30003","30","3","0","0","0","0","2","1","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"});
    table.rows.push_back({"30004","30","4","0","0","0","0","1","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"});
    return table;
}
int main(int argc,char** argv) {
    using namespace eternity;using namespace eternity::game;
    try {
        require(argc==2,"Expected equipment oracle vectors");
        const auto table=item_definitions();const auto definitions=ItemDefinitions::load(table);
        require(!definitions.find(0,1) && !definitions.find(30,0) && !definitions.find(30,5),"Missing/invalid original item lookup changed");
        const auto& weapon=*definitions.find(5,1);
        require(weapon.quality==std::array<std::uint16_t,3>{1000,500,100} && weapon.effects[0]==std::array<std::uint16_t,3>{1001,500,0},
            "Original durability thresholds/effect arrays changed");
        require(definitions.find(45,1)->real_type==1 && definitions.find(16,1)->real_type==16,"Original real-type grouping changed");
        auto altered=table;altered.rows[0][4]="0";altered.rows[0][7]="9";
        auto normalized=ItemDefinitions::load(altered);
        require(normalized.find(5,1)->resources.mp==1 && normalized.find(5,1)->attribute==0,"Original server/attribute normalization changed");
        altered.rows[0][10]="10";normalized=ItemDefinitions::load(altered);
        require(normalized.find(5,1)->server==0 && normalized.find(5,1)->resources.mp==303,"Invalid server flag changed original MP");

        EnchantValues enchant;for(std::size_t i=0;i<21;++i) enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};
        require(decode_craft(weapon,"short",enchant).name==std::string(20,'0'),"Invalid eligible craft string was not reset");
        require(decode_craft(*definitions.find(30,1),"01020304050607080903",enchant).name.empty(),"Ineligible item received craft expansion");
        auto no_wis=weapon;no_wis.required_wisdom=100;require(decode_craft(no_wis,std::string(20,'9'),enchant).name.empty(),"ReqWis above craft limit accepted");
        const auto path=std::filesystem::temp_directory_path()/("eternity-enchant-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
        {std::ofstream file(path);file<<"[LapisianEnchantPercentRate]\nWeaponStep01=9999\n[LapisianEnchantAddValue]\n WeaponStep01 = 65543 ; low word\nDefenseStep01=-1\nWeaponStep21=9\n";}
        EnchantValues config;try{config=EnchantValues::read(path);}catch(...){std::filesystem::remove(path);throw;}
        std::filesystem::remove(path);require(config.additions[1][0]==7 && config.additions[1][1]==65535 && config.additions[0][0]==0,"Original enchant INI section/default/word conversion changed");

        std::ifstream vectors(argv[1]);require(vectors.good(),"Cannot open equipment vectors");std::string line;unsigned int count=0;
        while(std::getline(vectors,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream input(line);unsigned int slot=0,type=0,quality=0,transformed=0,skill=0,buff=0;PlayerItem item;
            require(static_cast<bool>(input>>slot>>type>>quality>>transformed>>skill>>buff>>item.craft),"Malformed equipment vector");
            item.type=static_cast<std::uint8_t>(type);item.type_id=1;item.quality=static_cast<std::uint16_t>(quality);
            for(auto& gem:item.gems) {unsigned int id=0;require(static_cast<bool>(input>>id),"Missing gem");gem=static_cast<std::uint8_t>(id);}
            EquipmentStatus status;status.base={Attributes{100,200,300,400,500,600},PlayerResources{10000,20000,30000}};
            status.attack.item_attack=10;status.variance=20;status.attack.physical_defense=30;status.attack.magic_defense=40;
            status.range=50;status.attack_speed=60;status.damage_reduction=70;status.attack_attribute=7;status.defense_attribute=8;
            status.shield_value=9;status.weapon_speed.fill(3);status.weapon_damage.fill(7);
            status.transformed=transformed!=0;status.defense_attribute_skill=skill!=0;status.attack_attribute_buffs[0]=static_cast<std::uint8_t>(buff);
            add_equipment(status,static_cast<std::uint8_t>(slot),item,definitions,enchant);
            const auto& a=status.base.attributes;const auto& maximum=status.base.maximum;
            std::vector<std::int64_t> values={a.strength,a.dexterity,a.recovery,a.intelligence,a.wisdom,a.luck,maximum.hp,maximum.mp,maximum.sp,
                status.attack.item_attack,status.variance,status.attack.physical_defense,status.attack.magic_defense,status.range,status.attack_speed,status.move_speed,status.damage_reduction,
                status.attack_attribute,status.defense_attribute};
            values.insert(values.end(),status.standing_recovery.begin(),status.standing_recovery.end());
            values.insert(values.end(),status.sitting_recovery.begin(),status.sitting_recovery.end());
            values.insert(values.end(),status.state_blocks.begin(),status.state_blocks.end());values.push_back(status.quality[slot]);
            const auto craft=decode_craft(*definitions.find(item.type,1),item.craft,enchant);
            values.insert(values.end(),craft.additions.begin(),craft.additions.end());values.push_back(craft.enchant_step);
            for(std::size_t i=0;i<values.size();++i) {
                std::int64_t expected=0;require(static_cast<bool>(input>>expected),"Missing original equipment result");
                if(values[i]!=expected) throw std::runtime_error("Equipment oracle mismatch vector "+std::to_string(count)+", field "+std::to_string(i)+": "+std::to_string(values[i])+" != "+std::to_string(expected));
            }
            std::string extra;require(!(input>>extra),"Trailing equipment vector data");++count;
        }
        require(count==138,"Original equipment oracle count changed");
        const BaseStatus base{Attributes{INT32_MAX,0,0,0,0,0},PlayerResources{UINT32_MAX,0,0}};
        PlayerItem equipped;equipped.type=5;equipped.type_id=1;equipped.slot=5;equipped.quality=1000;
        auto packed=equipped;packed.bag=1;
        const auto initialized=initialize_equipment(base,{packed,equipped},definitions,enchant);
        require(initialized.base.attributes.strength==INT32_MIN && initialized.base.maximum.hp==115 && initialized.attack.item_attack==1001,
            "Equipment initialization order, bag filtering or original 32-bit wrap changed");
        bool duplicate=false;try{initialize_equipment(base,{equipped,equipped},definitions,enchant);}catch(const std::invalid_argument&){duplicate=true;}
        require(duplicate,"Unresolved duplicate equipment accepted by initialization");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
