#include "eternity/game/skills.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
eternity::game::SkillStatus initial() {
    eternity::game::SkillStatus status;status.equipment.base={{100,200,300,400,500,600},{10000,20000,30000}};return status;
}
std::vector<std::int64_t> fields(const eternity::game::SkillStatus& status) {
    const auto& equipment=status.equipment;const auto& a=equipment.base.attributes;const auto& r=equipment.base.maximum;const auto& b=status.benefits;
    std::vector<std::int64_t> values={a.strength,a.dexterity,a.recovery,a.intelligence,a.wisdom,a.luck,r.hp,r.mp,r.sp};
    values.insert(values.end(),status.sitting_recovery_percent.begin(),status.sitting_recovery_percent.end());
    for(auto value:{status.mp_cost_reduction,status.sp_cost_reduction,equipment.range,equipment.attack_speed,equipment.move_speed,equipment.attack.critical,status.cool_time}) values.push_back(value);
    for(const auto& category:equipment.attack.categories) for(auto value:{category.accuracy,category.attack,category.evasion,category.defense}) values.push_back(value);
    values.insert(values.end(),status.attack_disabled.begin(),status.attack_disabled.end());
    for(auto value:{b.amulet,b.mob_gold,b.no_durability_loss,b.no_exp_loss,b.no_item_loss,b.no_equipped_item_loss,b.bank_enabled,b.bank_double,
        b.experience_double,b.experience_half_bonus,b.rebirth,status.max_move_speed,status.max_bags,status.cross_road,equipment.attack.item_attack,equipment.attack.physical_defense}) values.push_back(value);
    values.insert(values.end(),equipment.weapon_speed.begin(),equipment.weapon_speed.end());values.insert(values.end(),equipment.weapon_damage.begin(),equipment.weapon_damage.end());
    values.push_back(equipment.shield_value);values.push_back(status.hp_ability_applied);values.push_back(status.hp_ability_id);values.push_back(status.hp_ability_level);values.push_back(status.hp_ability_previous);
    return values;
}
void oracle(const char* path,bool passive) {
    using namespace eternity::game;
    std::ifstream input(path);require(input.good(),"Cannot open skill oracle vectors");std::string line;unsigned int count=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream vector(line);auto status=initial();
        if(passive) {
            unsigned int detail=0,weapon=0,shield=0,first=0,second=0,amount=0;
            require(static_cast<bool>(vector>>detail>>weapon>>shield>>first>>second>>amount),"Malformed passive oracle input");
            SkillDefinition skill;skill.id=1234;skill.level=3;skill.detail=static_cast<std::uint16_t>(detail);skill.bag=5;
            skill.weapon1=static_cast<std::uint8_t>(first);skill.weapon2=static_cast<std::uint8_t>(second);skill.weapon_value=static_cast<std::uint8_t>(amount);
            skill.abilities={SkillAbility{5,13},SkillAbility{22,19},SkillAbility{40,1}};
            add_passive(status,skill,weapon?std::optional<std::uint8_t>{weapon}:std::nullopt,shield?std::optional<std::uint8_t>{shield}:std::nullopt);
        } else {
            unsigned int kind=0,effect=0;std::int32_t amount=0,amulet=0;
            require(static_cast<bool>(vector>>kind>>effect>>amount>>amulet),"Malformed ability oracle input");
            status.benefits.amulet=amulet;add_skill_ability(status,static_cast<std::uint8_t>(effect),static_cast<std::uint8_t>(kind),amount);
        }
        const auto values=fields(status);
        for(std::size_t i=0;i<values.size();++i) {
            std::int64_t expected=0;require(static_cast<bool>(vector>>expected),"Missing original skill result");
            if(values[i]!=expected) throw std::runtime_error(std::string(passive?"Passive":"Ability")+" oracle mismatch vector "+std::to_string(count)+", field "+std::to_string(i)+": "+std::to_string(values[i])+" != "+std::to_string(expected));
        }
        std::string extra;require(!(vector>>extra),"Trailing skill oracle data");++count;
    }
    require(count==(passive?144U:1152U),"Original skill oracle count changed");
}
int main(int argc,char** argv) {
    using namespace eternity;using namespace eternity::game;
    try {
        require(argc==3,"Expected ability and passive oracle files");oracle(argv[1],false);oracle(argv[2],true);
        storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3"};
        table.rows={{"10","1","1","0","5","99","17","2","0","0","0","0","0","0"},
            {"11","1","8","0","5","0","2","0","0","0","0","0","0","0"},
            {"12","1","4","0","0","0","0","0","5","0","22","0","40","0"},
            {"13","1","93","0","0","0","0","0","0","0","0","0","0","0"},
            {"14","1","122","0","0","0","0","0","0","0","0","0","0","0"},
            {"10","4","1","0","5","0","17","0","0","0","0","0","0","0"},
            {"15","2","1","0","5","0","17","0","0","0","0","0","0","0"}};
        for(const auto name:{"AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime"}) {
            table.columns.emplace_back(name);for(auto& row:table.rows) row.emplace_back("0");
        }
        const auto definitions=SkillDefinitions::load(table);
        require(definitions.find(10,1)->weapon2==19 && definitions.find(12,1)->abilities[0].value==1 &&
            definitions.find(13,1)->effect==4 && definitions.find(14,1)->effect==6,"Original skill definition normalizations changed");
        require(!definitions.find(10,4) && !definitions.find(0,1) && !definitions.find(15,2),"Original skill level/base lookup checks changed");
        auto initialized=initial();initialize_passives(initialized,{{10,0,1},{10,0,1},{11,0,1},{12,0,1},{13,0,1},{14,0,1},{15,0,2}}, {},definitions,ItemDefinitions{});
        require(initialized.equipment.weapon_damage[5]==34 && initialized.equipment.weapon_speed[5]==2 &&
            initialized.equipment.base.maximum.hp==10005 && initialized.equipment.attack.categories[2].accuracy==1 && initialized.benefits.amulet==1,
            "Original passive load ordering or effect/definition filtering changed");
        SkillDefinition mastery=*definitions.find(10,1);mastery.weapon2=0;
        auto status=initial();add_passive(status,mastery,5,std::nullopt);add_passive(status,mastery,5,std::nullopt);
        require(status.equipment.weapon_damage[5]==34 && status.equipment.attack.item_attack==51,"Original cumulative mastery application changed");
        status=initial();add_passive(status,mastery,std::nullopt,std::nullopt);
        require(status.equipment.weapon_damage[5]==17 && status.equipment.attack.item_attack==0,"Unequipped mastery changed attack");
        for(std::int32_t type=-1;type<6;++type) {status=initial();apply_billing(status,type);
            require(status.benefits.bank_double==(type>=1 && type<=3?1:0) && status.benefits.no_durability_loss==(type>=2 && type<=3?1:0) &&
                status.benefits.experience_half_bonus==(type>=2 && type<=3?1:0),"Original billing entitlement initialization changed");}
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
