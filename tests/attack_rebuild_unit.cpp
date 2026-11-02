#include "equipment_fixture.hpp"
#include "eternity/game/actor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected attack rebuild oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open attack rebuild vectors");std::string line;unsigned int checked=0;
        const auto definitions=ItemDefinitions::load(item_definitions());EnchantValues enchant;for(std::size_t i=0;i<21;++i) enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};
        storage::ResultSet mob_table;mob_table.columns={"MobID","Level","HP","Attack1","Attackplus1","Defense","Magic"};
        for(unsigned int id=0;id<4;++id) mob_table.rows.push_back({std::to_string(id),id>=2?"0":"2",id==2?"0":"-1","34123","54234","65535","12345"});
        const auto mobs=MobAttackTemplates::load(mob_table);require(mobs.find(1) && mobs.find(1)->hp==UINT32_MAX && !mobs.find(0) && !mobs.find(2) && !mobs.find(3) && !mobs.find(UINT32_MAX),"Original mob attack-template lookup changed");
        auto signed_words=mob_table;signed_words.rows[1][1]="-1";signed_words.rows[1][3]="-1";
        const auto retained=MobAttackTemplates::load(signed_words);require(retained.find(1)->level==65535 && retained.find(1)->attack==65535,"Original signed SQL mob WORD bits changed");
        auto zero_hp=mob_table;zero_hp.rows[2][1]="2";const auto normalized=MobAttackTemplates::load(zero_hp);
        require(normalized.find(2) && normalized.find(2)->hp==1,"Original mob loader zero-HP normalization changed");
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int mode=0,quality=0,type=0,shape=0;int slot=0;std::string craft;std::array<std::uint8_t,6> gems{};std::uint32_t mob=0;
            row>>mode>>slot>>quality>>type>>shape>>craft;for(auto& gem:gems) {unsigned int value=0;row>>value;gem=static_cast<std::uint8_t>(value);}row>>mob;require(static_cast<bool>(row),"Malformed rebuild vector");
            EquipmentStatus status;status.base={{100,200,300,400,500,600},{10000,20000,30000}};status.attack.item_attack=INT32_MIN;status.variance=INT32_MAX;status.attack.physical_defense=-1;status.attack.magic_defense=23;
            status.range=50;status.attack_speed=60;status.move_speed=2;status.damage_reduction=70;status.attack_attribute=7;status.defense_attribute=8;status.shield_value=9;status.weapon_damage.fill(7);status.transformed=shape!=0;
            for(std::size_t i=0;i<3;++i) status.attack.categories[i]={static_cast<std::int32_t>(31+4*i),static_cast<std::int32_t>(32+4*i),static_cast<std::int32_t>(33+4*i),static_cast<std::int32_t>(34+4*i)};
            std::vector<PlayerItem> items;std::vector<int> positions;if(slot==-2) for(int i=0;i<13;++i) positions.push_back(i);else if(slot!=-1) positions.push_back(slot);positions.push_back(29);
            for(const auto position:positions) {PlayerItem item;item.bag=static_cast<std::uint8_t>(position/24);item.slot=static_cast<std::uint16_t>(position%24);
                item.type=static_cast<std::uint8_t>(slot==-2?(position==5?5:position<8?16:21):type);item.type_id=1;item.quality=0;item.craft=craft;item.gems=gems;items.push_back(item);
                if(position<13) status.quality[static_cast<std::size_t>(position)]=static_cast<std::uint8_t>(slot==-2?(quality+static_cast<unsigned int>(position))%3:quality);
            }
            if(mode) rebuild_equipment_attack(status,items,definitions,enchant);else rebuild_transformed_attack(status,mobs.find(mob));
            const auto& a=status.base.attributes;const auto& r=status.base.maximum;
            std::vector<std::int64_t> fields{status.attack.item_attack,status.variance,status.attack.physical_defense,status.attack.magic_defense,status.range,status.attack_speed,status.move_speed,status.damage_reduction,status.attack_attribute,status.defense_attribute,a.strength,a.dexterity,a.recovery,a.intelligence,a.wisdom,a.luck,r.hp,r.mp,r.sp};
            for(const auto& attack:calculate_attack(30,a,status.attack)) for(auto value:{attack.accuracy,attack.attack,attack.evasion,attack.defense,attack.critical_attack,attack.critical_defense}) fields.push_back(value);
            for(std::size_t i=0;i<fields.size();++i) {std::int64_t expected=0;require(static_cast<bool>(row>>expected),"Missing original attack rebuild output");require(fields[i]==expected,"Attack rebuild differs at vector "+std::to_string(checked)+" field "+std::to_string(i)+": "+std::to_string(fields[i])+" versus "+std::to_string(expected));}
            std::string extra;require(!(row>>extra),"Trailing attack rebuild vector data");++checked;
        }
        require(checked==192,"Original attack rebuild coverage changed");std::cout<<checked<<" original equipment and transformed attack rebuild scenarios passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
