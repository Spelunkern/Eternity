#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::array<std::int32_t,4> attack(const EquipmentStatus& status) {return {status.attack.item_attack,status.variance,status.attack.physical_defense,status.attack.magic_defense};}
void set_attack(EquipmentStatus& status,const std::array<std::int32_t,4>& values) {
    status.attack.item_attack=values[0];status.variance=values[1];status.attack.physical_defense=values[2];status.attack.magic_defense=values[3];
}
template<typename T,std::size_t N> void read(std::istream& row,std::array<T,N>& values) {for(auto& value:values) {int input=0;row>>input;value=static_cast<T>(input);}}
PlayerItem item(std::uint16_t slot,std::uint16_t quality) {PlayerItem result;result.type=5;result.type_id=1;result.slot=slot;result.quality=quality;return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected durability oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open durability oracle");
        const auto definitions=ItemDefinitions::load(item_definitions());unsigned int direct=0,defense=0;
        std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;row>>op;EquipmentStatus status;EquipmentWear result;
            std::array<std::int32_t,4> initial{},expected{};
            const auto context=" at durability vector "+std::to_string(direct)+"/"+std::to_string(defense);
            if(op=='Q') {
                unsigned int slot=0,present=0,quality=0,tier=0;std::int32_t amount=0;
                row>>slot>>present>>quality>>tier>>amount;read(row,initial);set_attack(status,initial);status.quality.at(slot)=static_cast<std::uint8_t>(tier);
                auto equipped=item(static_cast<std::uint16_t>(slot),static_cast<std::uint16_t>(quality));
                result=decrease_equipment_quality(status,static_cast<std::uint8_t>(slot),present?&equipped:nullptr,amount,definitions);
                int applied=0;unsigned int final_quality=0,final_tier=0;row>>applied>>final_quality>>final_tier;
                require(result.applied==(applied!=0) && equipped.quality==final_quality && status.quality[slot]==final_tier,"Quality result differs"+context);
                ++direct;
            } else if(op=='D') {
                std::int32_t cursor=0,no_wear=0;unsigned int shape=0,mask=0;
                row>>cursor>>no_wear>>shape>>mask;
                std::array<std::uint16_t,13> qualities{},final_qualities{};std::array<std::uint8_t,13> tiers{},final_tiers{};
                read(row,qualities);read(row,tiers);read(row,initial);set_attack(status,initial);status.quality=tiers;
                std::vector<PlayerItem> inventory;
                for(std::uint16_t slot=0;slot<13;++slot) if(mask&(1U<<slot)) inventory.push_back(item(slot,qualities[slot]));
                result=wear_defense(status,inventory,definitions,cursor,no_wear,static_cast<std::uint8_t>(shape));
                std::int32_t final_cursor=0;row>>final_cursor;read(row,final_qualities);read(row,final_tiers);
                require(cursor==final_cursor && status.quality==final_tiers,"Defense traversal differs"+context);
                for(const auto& equipped:inventory) qualities[equipped.slot]=equipped.quality;
                require(qualities==final_qualities,"Defense quality differs"+context);
                ++defense;
            } else throw std::runtime_error("Unknown durability operation");
            read(row,expected);int recalculations=0;row>>recalculations;
            require(attack(status)==expected,"Quality attack delta differs"+context);
            require(recalculations==(result.attack_changed?1:0),"Attack rebuild count differs"+context);
            if(recalculations) {std::array<std::int32_t,4> at_rebuild{};read(row,at_rebuild);require(attack(status)==at_rebuild,"Attack changed after recalculation"+context);}
            require(static_cast<bool>(row),"Truncated durability vector");std::string extra;require(!(row>>extra),"Trailing durability vector data");
        }
        require(direct==1136 && defense==1536,"Durability coverage changed");
        // A scan that reaches the end wraps only the next invocation's cursor.
        EquipmentStatus status;std::int32_t cursor=4;std::vector<PlayerItem> inventory{item(6,2)};
        require(!wear_defense(status,inventory,definitions,cursor,0,0).applied && cursor==0 && inventory[0].quality==2,"Defense suffix scan restarted at the beginning");
        require(wear_defense(status,inventory,definitions,cursor,0,0).applied && cursor==1 && inventory[0].quality==1,"Next defense scan did not use wrapped cursor");
        auto packed=item(0,1000);packed.bag=1;inventory.push_back(packed);
        require(!wear_defense(status,inventory,definitions,cursor,0,0).applied && cursor==0 && inventory[1].quality==1000,"Packed inventory item received wear");
        bool invalid=false;cursor=-1;try {wear_defense(status,inventory,definitions,cursor,0,0);}catch(const std::invalid_argument&) {invalid=true;}
        require(invalid,"Invalid negative durability cursor accepted");
        std::cout<<direct<<" direct quality mutations and "<<defense<<" defense scans match original instructions\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
