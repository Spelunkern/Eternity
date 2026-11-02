#include "eternity/game/entry_packets.hpp"
#include "eternity/game/effect_cycle.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& text) {
    require(text.size()%2==0,"Odd packet hex length");net::Packet result;
    for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected entry packet and learned skill oracle files");std::ifstream input(argv[1]);require(input.good(),"Cannot open entry packet oracle");std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
            unsigned int op=0,count=0;std::uint32_t tick=0;std::int32_t parameter=0;std::size_t packet_count=0;row>>op>>count>>tick>>parameter>>packet_count;
            require(static_cast<bool>(row) && op<8,"Malformed entry packet vector");std::vector<net::Packet> result;
            if(op<=1) {
                std::vector<PlayerItem> items;
                for(unsigned int i=0;i<count;++i) {
                    PlayerItem item;const auto position=i*37%(op==0?144:240);item.bag=static_cast<std::uint8_t>(op==0?position/24:0);item.slot=static_cast<std::uint16_t>(op==0?position%24:position);
                    item.type=static_cast<std::uint8_t>(i*7+1);item.type_id=static_cast<std::uint8_t>(i*3+2);item.count=static_cast<std::uint8_t>(i*5+1);item.quality=static_cast<std::uint16_t>(i*997);
                    for(unsigned int j=0;j<6;++j) item.gems[j]=static_cast<std::uint8_t>(i+j*13);
                    item.craft=i%3==0?"":i%3==1?"1234567":"01234567890123456789";items.push_back(item);
                }
                result=op==0?character_items(items,parameter):character_bank(items,0x78563412dcba9876ULL);
            } else if(op==2) {
                std::vector<LearnedSkillPacketEntry> skills;
                for(unsigned int i=0;i<count;++i) skills.push_back({static_cast<std::uint16_t>(1000+i),static_cast<std::uint8_t>(i%3+1),static_cast<std::uint8_t>(i),tick+static_cast<std::uint32_t>((static_cast<std::int32_t>(i%9)-4)*1001)});
                result.push_back(character_skills(skills,tick));
            } else if(op==3) {
                std::vector<ActiveEffectPacketEntry> effects;
                for(unsigned int i=0;i<count;++i) effects.push_back({0x12340000+i,static_cast<std::uint16_t>(1000+i),static_cast<std::uint8_t>(i%3+1),(i%2?tick/1000:tick)+static_cast<std::uint32_t>((static_cast<std::int32_t>(i%9)-4)*1001),i%2!=0});
                result=character_effects(effects,tick);
                EffectCycle cycle(9001);std::vector<SkillDefinition> definitions(count);
                for(std::size_t i=0;i<effects.size();++i) {
                    const auto& effect=effects[i];auto& definition=definitions[i];definition.id=effect.skill_id;definition.level=effect.level;definition.offline_countdown=effect.offline_countdown;
                    // Packet serialization follows the live definition, independently of the timer's stored flag.
                    cycle.append(effect.instance_id,{{effect.skill_id,effect.level,0},0,effect.end_tick,!effect.offline_countdown},definition);
                }
                require(cycle.initial_packets(tick)==result,"Effect cycle did not serialize its live insertion order");
            } else if(op==4) {
                std::vector<QuickSlot> slots;
                for(unsigned int i=0;i<count;++i) slots.push_back({static_cast<std::uint8_t>(i%8),static_cast<std::uint8_t>(i%24),static_cast<std::uint8_t>(i%4),static_cast<std::uint16_t>(i*777+123)});
                result.push_back(character_quick_slots(slots));
            } else if(op==5) {
                std::vector<QuestPacketEntry> quests;
                for(unsigned int i=0;i<count;++i) quests.push_back({static_cast<std::uint16_t>(1000+i),tick+static_cast<std::uint32_t>((static_cast<std::int32_t>(i%9)-4)*60001),i%3!=0,{static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(i+13),static_cast<std::uint8_t>(i+26)}});
                result=character_quests(quests,tick);
            } else if(op==6) {
                std::vector<FinishedQuest> quests;
                for(unsigned int i=0;i<count;++i) quests.push_back({static_cast<std::uint16_t>(1000+i),static_cast<std::uint8_t>(i*7)});
                result=character_finished_quests(quests);
            } else {
                EffectStatus status;status.movement_block=parameter&1;status.action_block_a=(parameter>>1)&1;status.action_block_b=(parameter>>2)&1;status.casting=(parameter>>3)&1;status.fixed_form=(parameter>>4)&1;status.movement_restricted=(parameter>>5)&1;
                status.skills.equipment.attack_speed=parameter%13-2;status.skills.equipment.move_speed=parameter%8-2;result.push_back(character_speed(9001,status));
            }
            const auto context=" at entry vector "+std::to_string(checked)+" operation "+std::to_string(op);
            require(result.size()==packet_count,"Packet count differs"+context);
            for(const auto& packet:result) {std::string hex;row>>hex;const auto original=unhex(hex);require(packet==original,"Packet bytes differ"+context);}
            std::string extra;require(!(row>>extra),"Unexpected trailing entry packet vector");++checked;
        }
        require(checked==321,"Entry packet coverage changed");std::cout<<checked<<" original initial character packet scenarios passed\n";
        storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
        for(unsigned int id=1000;id<1255;++id) for(unsigned int level=1;level<=3;++level) {storage::Row row(table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]=std::to_string(level);table.rows.push_back(std::move(row));}
        const auto definitions=SkillDefinitions::load(table);std::ifstream learned(argv[2]);require(learned.good(),"Cannot open learned skill oracle");unsigned int restored_count=0;
        while(std::getline(learned,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int count=0,loaded=0,invalid=0;std::uint32_t tick=0;int bad=-1;
            require(static_cast<bool>(row>>count>>tick>>bad>>loaded>>invalid),"Malformed learned skill vector");std::vector<PlayerSkill> skills;
            for(unsigned int i=0;i<count;++i) skills.push_back({static_cast<std::uint16_t>(static_cast<int>(i)==bad?999:1000+i),std::array<std::uint16_t,3>{0,1,65535}[i%3],static_cast<std::uint8_t>(i%3+1),static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(i)});
            const auto restored=restore_skills(skills,definitions,tick);require(restored.active.size()==loaded && (restored.invalid?restored.invalid->id:0)==invalid,"Original learned skill rejection differs");
            for(const auto& skill:restored.active) {unsigned int id=0,level=0,number=0;std::uint32_t end=0;require(static_cast<bool>(row>>id>>level>>number>>end) && skill.id==id && skill.level==level && skill.number==number && skill.end_tick==end,"Original learned skill restoration differs");}
            std::string extra;require(!(row>>extra),"Trailing learned skill vector data");++restored_count;
        }
        require(restored_count==40,"Learned skill coverage changed");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
