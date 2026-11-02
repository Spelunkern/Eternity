#include "equipment_fixture.hpp"
#include "eternity/game/effect_cycle.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct World final:EffectWorld,EffectObserver {
    EffectStatus& status;std::vector<std::string> events;
    explicit World(EffectStatus& value):status(value) {}
    void record(char kind) {events.push_back(std::string(1,kind)+std::to_string(status.skills.hp_ability_applied?1:0)+","+std::to_string(status.shape)+","+std::to_string(status.shape_template));}
    void maximum_changed(EffectStatus&,std::uint8_t resource) override {require(resource==0,"Unexpected death cleanup resource");record('M');}
    void speed_changed(EffectStatus&) override {record('V');}
    void rebuild_attack(EffectStatus&,bool equipment) override {require(equipment,"Unexpected death transformation rebuild");record('R');}
    void user_shape_changed(EffectStatus&) override {record('U');}
    void erase_persisted(const PersistedEffect& effect) noexcept override {events.push_back("D"+std::to_string(effect.skill_id)+","+std::to_string(effect.level));}
    void release_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::optional<std::uint32_t> allocate_id() noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void send_self(const net::Packet&) noexcept override {std::terminate();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void unexpected() {throw std::runtime_error("Unexpected death cleanup world operation");}
    void shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
};
std::vector<std::int64_t> fields(const EffectStatus& status) {
    const auto& equipment=status.skills.equipment;
    std::vector<std::int64_t> result{equipment.base.maximum.hp,equipment.base.attributes.strength,equipment.attack_speed,equipment.attack_attribute,equipment.defense_attribute,
        status.giveup_sp_mp,status.movement_block,status.action_block_a,status.action_block_b,status.fixed_form,status.chase_id,status.shape_template,status.block_all,status.casting,status.movement_restricted};
    for(const auto& defense:status.defense) for(auto value:{defense.block_mode,defense.block_value,defense.mirror_skill,defense.mirror_level}) result.push_back(value);
    for(auto value:{static_cast<int>(status.form_mob),static_cast<int>(status.shape),status.no_mob_targeting?1:0,status.block_death?1:0,status.block_dying?1:0,status.skills.hp_ability_applied?1:0,status.zero_attack_attribute_buff?1:0}) result.push_back(value);
    result.insert(result.end(),equipment.attack_attribute_buffs.begin(),equipment.attack_attribute_buffs.end());return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected death effect oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open death effect oracle");
        const auto items=ItemDefinitions::load(item_definitions());std::string line;unsigned int count=0;
        PlayerItem weapon,armor;weapon.type=5;armor.type=16;weapon.type_id=armor.type_id=1;weapon.gems[0]=armor.gems[0]=3;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::int32_t penalty=0;unsigned int mask=0,nodes=0;row>>penalty>>mask>>nodes;
            require(static_cast<bool>(row) && nodes<=8,"Malformed death effects input");
            EffectStatus status;auto& equipment=status.skills.equipment;
            equipment.base.maximum.hp=10000;equipment.base.attributes.strength=100;equipment.attack_speed=2;equipment.attack_attribute=7;equipment.defense_attribute=8;
            status.giveup_sp_mp=status.movement_block=status.action_block_a=status.action_block_b=status.fixed_form=status.block_all=status.casting=status.movement_restricted=3;
            status.chase_id=3;status.shape_template=123;status.shape=99;status.form_mob=987;status.no_mob_targeting=status.block_death=status.block_dying=true;
            status.zero_attack_attribute_buff=true;equipment.attack_attribute_buffs.fill(1);equipment.transformed=true;status.skills.hp_ability_applied=true;
            for(auto& defense:status.defense) defense={3,3,3,3};
            std::vector<SkillDefinition> definitions;definitions.reserve(nodes);EffectCycle cycle(9001);World world(status);
            for(unsigned int i=0;i<nodes;++i) {
                unsigned int detail=0,effect=0,id=0,level=0,preserve=0,change=0;row>>detail>>effect>>id>>level>>preserve>>change;
                SkillDefinition skill;skill.detail=static_cast<std::uint16_t>(detail);skill.effect=static_cast<std::uint8_t>(effect);skill.id=static_cast<std::uint16_t>(id);skill.level=static_cast<std::uint8_t>(level);
                skill.preserve_on_death=preserve!=0;skill.offline_countdown=preserve==1;skill.change_type=static_cast<std::uint16_t>(change);skill.abilities={{{1,17},{4,31},{16,9}}};
                definitions.push_back(skill);cycle.append(1000+i,{{skill.id,skill.level,30},0,30000,false},definitions.back());
            }
            cycle.clear_on_death(penalty,status,{mask&2?&weapon:nullptr,mask&1?&armor:nullptr},items,world,world);
            const auto context=" at death effects vector "+std::to_string(count);
            unsigned int field=0;for(const auto value:fields(status)) {
                std::int64_t original=0;row>>original;require(value==original,"Death state differs"+context+" field "+std::to_string(field)+": "+std::to_string(value)+" versus "+std::to_string(original));++field;
            }
            std::size_t retained=0;row>>retained;require(cycle.size()==retained,"Death retention count differs"+context);
            for(std::size_t i=0;i<retained;++i) {std::uint32_t id=0;row>>id;require(cycle.contains(id),"Wrong effect retained after death"+context);}
            std::size_t events=0;row>>events;require(world.events.size()==events,"Death event count differs"+context);
            for(const auto& event:world.events) {std::string original;row>>original;require(event==original,"Death event differs"+context+": "+event+" versus "+original);}
            require(static_cast<bool>(row),"Truncated death effects vector");std::string extra;require(!(row>>extra),"Trailing death effects data");++count;
        }
        require(count==728,"Death effects coverage changed");std::cout<<count<<" original death effect cleanup cases passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
