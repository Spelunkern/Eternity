#include "eternity/game/effects.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity::game;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct World final:EffectRemovalWorld {
    std::vector<std::array<std::int64_t,3>> events;
    void maximum_changed(EffectStatus&,std::uint8_t resource) override {events.push_back({1,resource,0});}
    void speed_changed(EffectStatus&) override {events.push_back({2,0,0});}
    void shape_changed(EffectStatus&) override {events.push_back({3,0,0});}
    void run_mode_changed(EffectStatus&,bool running) override {events.push_back({4,running,0});}
    void stop_attack(EffectStatus&) override {events.push_back({5,0,0});}
    void stop_target_mob(EffectStatus&,std::uint32_t id) override {events.push_back({6,id,0});}
    void taunt(EffectStatus&,std::uint16_t aggro,float range) override {events.push_back({7,aggro,static_cast<std::int64_t>(range)});}
    void detect_hidden(EffectStatus&,const SkillDefinition& skill) override {events.push_back({8,skill.id,0});}
    // Record delegation; this fixture does not implement or verify world attack rebuilding.
    void rebuild_attack(EffectStatus&,bool equipment) override {events.push_back({9,equipment,0});}
    void user_shape_changed(EffectStatus&) override {events.push_back({10,0,0});}
    void replace_attack_elements(EffectStatus& status,std::uint8_t attribute) override {
        events.push_back({11,attribute,0});auto& equipment=status.skills.equipment;
        if(equipment.attack_attribute==0) status.zero_attack_attribute_buff=false;
        else equipment.attack_attribute_buffs[equipment.attack_attribute-1]=0;
        equipment.attack_attribute=0;
    }
    void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>& amounts) override {
        events.push_back({12,amounts[0],amounts[1]});events.push_back({13,amounts[2],0});
    }
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource& source,std::uint32_t elapsed) override {events.push_back({14,elapsed,source.attacker_id});}
};
ItemDefinitions items() {
    eternity::storage::ResultSet table;table.columns={"ItemID","Type","TypeID","Range","ReqWis","AttackTime","Speed","Attrib","Special","Exp","Server",
        "Quality","Effect1","Effect2","Effect3","Effect4","ConstStr","ConstDex","ConstRec","ConstInt","ConstWis","ConstLuc","ConstHP","ConstMP","ConstSP"};
    for(auto type:{5U,16U,30U}) {
        eternity::storage::Row row(table.columns.size(),eternity::storage::Cell{"0"});row[0]=std::to_string(type*1000+1);row[1]=std::to_string(type);row[2]="1";
        row[7]=type==5?"1":type==16?"2":"3";table.rows.push_back(std::move(row));
    }
    table.columns.push_back("Count");for(auto& row:table.rows) row.push_back("1");return ItemDefinitions::load(table);
}
std::vector<std::int64_t> fields(const EffectStatus& status) {
    const auto& equipment=status.skills.equipment;const auto& a=equipment.base.attributes;const auto& r=equipment.base.maximum;
    std::vector<std::int64_t> result={a.strength,a.dexterity,a.recovery,a.intelligence,a.wisdom,a.luck,r.hp,r.mp,r.sp,
        equipment.attack_speed,equipment.move_speed,equipment.attack_attribute,equipment.defense_attribute};
    for(const auto& defense:status.defense) for(auto value:{defense.block_mode,defense.block_value,defense.mirror_skill,defense.mirror_level}) result.push_back(value);
    for(auto value:{status.giveup_sp_mp,status.movement_block,status.action_block_a,status.action_block_b,status.fixed_form}) result.push_back(value);
    result.push_back(status.chase_id);result.push_back(status.shape_template);result.push_back(status.block_all);
    result.push_back(status.shape);result.push_back(status.form_mob);result.push_back(status.no_mob_targeting);
    result.push_back(status.zero_attack_attribute_buff);result.insert(result.end(),equipment.attack_attribute_buffs.begin(),equipment.attack_attribute_buffs.end());
    result.push_back(attack_speed(status));result.push_back(move_speed(status));
    for(const auto& value:status.calculated_attack) for(auto field:{value.accuracy,value.attack,value.evasion,value.defense,value.critical_attack,value.critical_defense}) result.push_back(field);
    return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Expected original apply/removal/heal oracle vectors");const auto definitions=items();
        for(const bool removal:{false,true}) {
            std::ifstream input(argv[removal?2:1]);require(input.good(),"Cannot open effect vectors");std::string line;unsigned int count=0;
            while(std::getline(input,line)) {
                if(line.empty() || line[0]=='#') continue;
                std::istringstream vector(line);unsigned int detail=0,effect=0,attribute=0,change=0,scenario=0,set=0;
                require(static_cast<bool>(vector>>detail>>effect>>attribute>>change>>scenario>>set),"Malformed effect vector");
                EffectStatus status;status.skills.equipment.base={{100,200,300,400,500,600},{10000,20000,30000}};
                status.skills.equipment.attack_attribute=1;status.skills.equipment.defense_attribute=7;status.form_mob=999;
                if(scenario==1 || scenario==2) status.skills.equipment.attack_attribute_buffs[0]=1;
                if(scenario==3) {status.current_target_type=2;status.current_target=777;status.in_zone=true;}
                PlayerItem weapon,armor;weapon.type=5;weapon.type_id=1;armor.type=16;armor.type_id=1;
                EffectEquipment equipped;
                if(removal) {
                    const std::array<std::int32_t,4> counts={0,1,-1,INT32_MIN};const auto value=counts[scenario];
                    status.giveup_sp_mp=value;status.movement_block=value;status.action_block_a=value;status.action_block_b=value;status.fixed_form=value;status.block_all=value;
                    status.shape=static_cast<std::uint8_t>(change);status.skills.equipment.transformed=true;status.chase_id=888;status.shape_template=123;status.no_mob_targeting=true;
                    for(std::size_t i=0;i<3;++i) status.defense[i]={2,19,i==1?4321:1234,static_cast<std::int32_t>(3-i)};
                    if(scenario) {equipped.weapon=&weapon;equipped.armor=&armor;}
                    if(scenario==2) weapon.gems[0]=1;
                    if(scenario==3) armor.gems[0]=1;
                }
                SkillDefinition skill;skill.id=1234;skill.level=3;skill.detail=static_cast<std::uint16_t>(detail);skill.effect=static_cast<std::uint8_t>(effect);
                skill.attribute=static_cast<std::uint8_t>(attribute);skill.change_type=static_cast<std::uint16_t>(change);skill.change_level=123;
                skill.apply_range=12;skill.aggro=37;skill.defense_type=static_cast<std::uint8_t>(scenario%3);skill.defense_value=19;
                skill.abilities=set==0?std::array{SkillAbility{5,13},SkillAbility{16,17},SkillAbility{17,19}}:
                    std::array{SkillAbility{1,0},SkillAbility{2,23},SkillAbility{3,29}};
                World world;
                if(removal) {
                    skill.time_heal={3,7,65535};const std::array<std::uint32_t,4> ages={0,1001,3000,UINT32_MAX};
                    EffectSource source;source.attacker_id=888;source.previous_tick=10000U-ages[scenario];
                    remove_active_effect(status,skill,source,10000,equipped,definitions,world);
                } else apply_active_effect(status,skill,888,world);
                require(status.skills.equipment.transformed==(status.shape!=0),"Effect shape and equipment transformation diverged");
                if(!removal && effect==4 && detail==95) require(status.skills.equipment.defense_attribute_skill,"Active defense-element skill did not protect future armor changes");
                const auto result=fields(status);
                for(std::size_t i=0;i<result.size();++i) {
                    std::int64_t expected=0;require(static_cast<bool>(vector>>expected),"Missing original effect result");
                    if(result[i]!=expected) throw std::runtime_error("Effect oracle mismatch vector "+std::to_string(count)+", field "+std::to_string(i)+": "+std::to_string(result[i])+" != "+std::to_string(expected));
                }
                unsigned int events=0;
                if(!(vector>>events) || events!=world.events.size()) throw std::runtime_error("Original effect world-operation count differs in vector "+std::to_string(count));
                for(const auto& event:world.events) for(auto value:event) {
                    std::int64_t expected=0;require(static_cast<bool>(vector>>expected),"Missing world-operation argument");
                    if(expected!=value) throw std::runtime_error("Original effect world-operation differs in vector "+std::to_string(count)+": "+std::to_string(value)+" != "+std::to_string(expected));
                }
                std::string extra;require(!(vector>>extra),"Trailing effect vector data");++count;
            }
            require(count==(removal?288U:296U),"Original effect vector count changed");
        }
        EffectStatus damage;SkillDefinition periodic;periodic.effect=3;periodic.detail=61;EffectSource source;source.attacker_id=12;source.previous_tick=100;
        World world;remove_active_effect(damage,periodic,source,501,{},definitions,world);
        require(world.events==std::vector<std::array<std::int64_t,3>>{{14,401,12}},"Periodic damage removal did not delegate to combat");
        std::ifstream heals(argv[3]);require(heals.good(),"Cannot open original periodic heal vectors");std::string heal_line;unsigned int heal_count=0;
        while(std::getline(heals,heal_line)) {
            if(heal_line.empty() || heal_line[0]=='#') continue;
            std::istringstream vector(heal_line);std::uint32_t elapsed=0;std::array<std::uint16_t,3> rates{};
            require(static_cast<bool>(vector>>elapsed>>rates[0]>>rates[1]>>rates[2]),"Malformed periodic heal vector");
            const auto amounts=periodic_heal(elapsed,rates);
            for(auto amount:amounts) {std::int32_t expected=0;require(static_cast<bool>(vector>>expected),"Missing original periodic heal result");
                if(amount!=expected) throw std::runtime_error("Periodic heal oracle mismatch in vector "+std::to_string(heal_count)+": "+std::to_string(amount)+" != "+std::to_string(expected));}
            std::string extra;require(!(vector>>extra),"Trailing periodic heal data");++heal_count;
        }
        require(heal_count==200,"Original periodic heal vector count changed");
        EffectStatus speed;speed.skills.equipment.attack_speed=-1;speed.skills.equipment.move_speed=9;
        require(attack_speed(speed)==0 && move_speed(speed)==4,"Original speed clamp changed");
        speed.fixed_form=1;require(attack_speed(speed)==255 && move_speed(speed)==0,"Fixed-form speed behavior changed");
        speed.casting=1;require(attack_speed(speed)==255 && move_speed(speed)==255,"Casting did not block original speeds");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
