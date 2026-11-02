#include "eternity/game/effect_timers.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
eternity::storage::ResultSet skill_table(unsigned int id,unsigned int level,unsigned int effect,unsigned int offline,std::uint32_t keep,bool available) {
    using namespace eternity::storage;
    ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
        "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
        "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP"};
    if(available) for(unsigned int current=1;current<=level;++current) {
        Row row(table.columns.size(),Cell{"0"});
        row[0]=std::to_string(id);row[1]=std::to_string(current);row[3]=std::to_string(effect);
        row[21]=std::to_string(offline);row[22]=std::to_string(keep);table.rows.push_back(std::move(row));
    }
    return table;
}
int main(int argc,char** argv) {
    using namespace eternity;using namespace eternity::game;
    try {
        require(argc==2,"Expected effect timer oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open timer vectors");
        unsigned int count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream vector(line);unsigned int id=0,level=0,effect=0,offline=0,map=0,available=0;
            std::uint32_t keep=0,remaining=0,disconnected=0,tick=0,disposition=0,previous=0,end=0,attacker=0;
            require(static_cast<bool>(vector>>id>>level>>effect>>offline>>keep>>remaining>>map>>disconnected>>tick>>available>>disposition>>previous>>end>>attacker),"Malformed timer vector");
            const auto definitions=SkillDefinitions::load(skill_table(id,level,effect,offline,keep,available!=0));
            const auto restored=restore_effects({PersistedEffect{static_cast<std::uint16_t>(id),static_cast<std::uint8_t>(level),remaining}},definitions,
                static_cast<std::uint16_t>(map),disconnected,tick);
            require(restored.active.size()==(disposition==1?1U:0U) && restored.erase.size()==(disposition==2?1U:0U),"Original effect restoration disposition differs");
            if(disposition==1) {
                const auto& timer=restored.active[0];
                if(timer.previous_tick!=previous || timer.end!=end || timer.attacker_kind!=attacker || timer.attacker_id!=0 || timer.offline_countdown!=(offline!=0))
                    throw std::runtime_error("Original timer mismatch in vector "+std::to_string(count));
                for(auto now:{tick,tick+1U,tick-1U}) {int expected=-1;require(static_cast<bool>(vector>>expected) && static_cast<int>(timer.expired(now))==expected,"Original timer expiry decision differs");}
            } else {
                for(unsigned int i=0;i<3;++i) {int expected=0;require(static_cast<bool>(vector>>expected) && expected==-1,"Inactive timer had expiry decision");}
                if(disposition==2) require(restored.erase[0].skill_id==id && restored.erase[0].level==level,"Expired SQL deletion key changed");
            }
            std::string extra;require(!(vector>>extra),"Trailing timer vector data");++count;
        }
        require(count==178,"Original timer vector count changed");
        require(effect_map_result(18,111,100)==0 && effect_map_result(18,111,3)==2 && effect_map_result(17,124,3)==0,"Original map/monster-level restriction changed");
        const auto definitions=SkillDefinitions::load(skill_table(137,1,4,1,2,true));
        require(definitions.find(137,1)->keep_time_seconds==7200 && definitions.find(137,1)->offline_countdown,"Original offline KeepTime hour conversion changed");
        auto signed_keep=skill_table(137,1,4,0,0,true);signed_keep.rows[0][22]="-1";
        require(SkillDefinitions::load(signed_keep).find(137,1)->keep_time_seconds==UINT32_MAX,"Original signed SQL KeepTime bits were not preserved");
        const auto restored=restore_effects({{137,1,99},{137,1,100},{137,1,101},{138,1,100}},definitions,1,100,1234567);
        require(restored.active.size()==2 && restored.erase.size()==1 && restored.active[0].effect.remaining==100 && restored.active[1].effect.remaining==101,
            "Persisted effect ordering, exact disconnect boundary or missing-definition filtering changed");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
