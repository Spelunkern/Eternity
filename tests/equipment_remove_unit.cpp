#include "equipment_fixture.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene,EquipmentRemovalDiagnostics {
    std::vector<std::string> events;std::optional<bool> party;
    void record(char route,const net::Packet& packet) {static constexpr char digits[]="0123456789abcdef";std::string value(1,route);for(auto byte:packet) {value+=digits[byte>>4];value+=digits[byte&15];}events.push_back(std::move(value));}
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet&) override {throw std::runtime_error("Unexpected removal cell packet");}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void reset_damage() override {events.emplace_back("X");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected removal target");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected removal taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected removal zone query");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected removal social query");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected removal damage");}
    void negative_craft_reduction(const PlayerItem&,std::int32_t value) override {events.push_back("D"+std::to_string(value));}
};
struct World final:EquipmentRemovalWorld {
    EquipmentStatus& status;std::vector<std::string> events;explicit World(EquipmentStatus& value):status(value) {}
    void remove_weapon_elements() override {events.push_back("W"+std::to_string(status.attack.item_attack)+","+std::to_string(status.variance)+","+std::to_string(std::bit_cast<std::int32_t>(status.base.maximum.hp))+","+std::to_string(std::bit_cast<std::int32_t>(status.attack_attribute)));}
    void negative_craft_reduction(const PlayerItem&,std::int32_t value) override {events.push_back("D"+std::to_string(value));}
    void equipment_speed_changed() override {events.push_back("V");}
    void equipment_maximum_changed(std::uint8_t resource) override {events.push_back(resource==0?"H":resource==2?"S":"M");}
    void equipment_recover() override {events.push_back("R");}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected original equipment removal vectors and optional actor mode");const bool actor_mode=argc==3;require(!actor_mode || std::string_view(argv[2])=="actor","Unknown equipment removal mode");std::ifstream input(argv[1]);require(input.good(),"Cannot open equipment removal vectors");
        const auto definitions=ItemDefinitions::load(item_definitions());EnchantValues enchant;for(std::size_t i=0;i<21;++i) enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};
        std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int slot=0,type=0,quality=0,tier=0,shape=0,initializing=0;std::uint32_t seed=0;PlayerItem item;
            row>>slot>>type>>quality>>tier>>shape>>initializing>>seed>>item.craft;item.type=static_cast<std::uint8_t>(type);item.type_id=1;item.quality=static_cast<std::uint16_t>(quality);
            for(auto& gem:item.gems) {unsigned int value=0;row>>value;gem=static_cast<std::uint8_t>(value);}
            bool bound=false;int group=-1;PlayerResources resources{};if(actor_mode) row>>bound>>group>>resources.hp>>resources.mp>>resources.sp;
            const auto signed_seed=std::bit_cast<std::int32_t>(seed);EquipmentStatus status;auto& a=status.base.attributes;a={signed_seed,signed_seed,signed_seed,signed_seed,signed_seed,signed_seed};status.base.maximum={seed,seed,seed};
            status.attack.item_attack=signed_seed;status.variance=signed_seed;status.attack.physical_defense=signed_seed;status.attack.magic_defense=signed_seed;
            status.range=signed_seed;status.attack_speed=signed_seed;status.move_speed=signed_seed;status.damage_reduction=signed_seed;status.attack_attribute=seed;status.defense_attribute=seed;
            status.standing_recovery.fill(signed_seed);status.sitting_recovery.fill(signed_seed);status.state_blocks.fill(signed_seed);status.quality[slot]=static_cast<std::uint8_t>(tier);status.transformed=shape!=0;status.shield_value=9;status.weapon_speed.fill(3);status.weapon_damage.fill(7);
            World world(status);Scene scene;std::optional<Actor> actor;const MobAttackTemplates mobs;
            if(actor_mode) {EffectStatus full;full.skills.equipment=status;full.shape=static_cast<std::uint8_t>(shape);full.in_zone=bound;if(group>=0) scene.party=group!=0;actor.emplace(9001,full,resources,std::vector<PersistedEffect>{},scene,ActorEquipmentRules{definitions,enchant,mobs});actor->initializing(initializing!=0);actor->remove_equipment_modifiers(static_cast<std::uint8_t>(slot),item,scene);}
            else remove_equipment(status,static_cast<std::uint8_t>(slot),item,definitions,enchant,initializing!=0,world);
            const auto& observed=actor_mode?actor->status().skills.equipment:status;const auto& attributes=observed.base.attributes;const auto& m=observed.base.maximum;
            std::vector<std::int64_t> actual{attributes.strength,attributes.dexterity,attributes.recovery,attributes.intelligence,attributes.wisdom,attributes.luck,m.hp,m.mp,m.sp,observed.attack.item_attack,observed.variance,observed.attack.physical_defense,observed.attack.magic_defense,observed.range,observed.attack_speed,observed.move_speed,observed.damage_reduction,observed.attack_attribute,observed.defense_attribute};
            actual.insert(actual.end(),observed.standing_recovery.begin(),observed.standing_recovery.end());actual.insert(actual.end(),observed.sitting_recovery.begin(),observed.sitting_recovery.end());actual.insert(actual.end(),observed.state_blocks.begin(),observed.state_blocks.end());actual.push_back(observed.quality[slot]);
            if(actor_mode) {actual.push_back(actor->resources().hp);actual.push_back(actor->resources().mp);actual.push_back(actor->resources().sp);}
            for(std::size_t i=0;i<actual.size();++i) {std::int64_t original=0;row>>original;require(actual[i]==original,"Removal vector "+std::to_string(checked)+" field "+std::to_string(i)+": "+std::to_string(actual[i])+" != "+std::to_string(original));}
            const auto& events=actor_mode?scene.events:world.events;std::size_t count=0;row>>count;require(events.size()==count,"Equipment removal notification count differs at "+std::to_string(checked));
            for(const auto& event:events) {std::string original;row>>original;require(event==original,"Equipment removal order differs at "+std::to_string(checked)+": "+event+" != "+original);}
            require(static_cast<bool>(row),"Truncated removal vector");std::string extra;require(!(row>>extra),"Trailing removal data");++checked;
        }
        // The concrete actor couples modifier reversal, element cancellation,
        // replica erasure, deferred party notices and resource clamps.
        {
            Scene scene;scene.party=false;const MobAttackTemplates mobs;EffectStatus full;
            full.skills.equipment.base.maximum={100,200,300};PlayerItem weapon;weapon.type=5;weapon.type_id=1;weapon.slot=5;weapon.quality=1000;
            add_equipment(full.skills.equipment,5,weapon,definitions,enchant);const auto current=full.skills.equipment.base.maximum;
            full.in_zone=true;full.skills.equipment.attack_attribute=2;full.skills.equipment.attack_attribute_buffs[1]=1;full.skills.equipment.defense_attribute=3;full.skills.equipment.defense_attribute_skill=true;
            SkillDefinition element; element.id=321;element.level=1;element.effect=4;element.detail=94;element.attribute=2;
            SkillDefinition armor;armor.id=322;armor.level=1;armor.effect=4;armor.detail=95;armor.attribute=3;
            Actor actor(9001,full,current,{{321,1,30},{322,1,30}},scene,{definitions,enchant,mobs},{weapon});
            actor.effects().append(100,{{321,1,30},0,30000,false},element);actor.effects().append(101,{{322,1,30},0,30000,false},armor);
            actor.remove_equipment_modifiers(5,actor.inventory().front(),scene);
            const auto& equipment=actor.status().skills.equipment;
            require(!actor.effects().contains(100) && actor.effects().contains(101) && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].skill_id==322,"Actor weapon removal lost elemental cancellation/replica state");
            require(equipment.attack_attribute==0 && equipment.attack_attribute_buffs[1]==0 && equipment.defense_attribute==3 && equipment.defense_attribute_skill,"Actor weapon removal changed the armor element");
            require(equipment.base.maximum.hp==100 && equipment.base.maximum.mp==200 && equipment.base.maximum.sp==300 && actor.resources().hp==100 && actor.resources().mp==200 && actor.resources().sp==300,"Actor equipment removal failed to reverse and clamp resources");
            require(actor.inventory().size()==1 && actor.inventory().front().type==5,"Modifier helper removed the caller-owned inventory pointer");
            require(scene.events.size()>3 && scene.events[0]=="S0e0564000000" && scene.events[1]=="F100" && scene.events[2]=="P050c29230000410101","Actor equipment cancellation/group order changed");
        }
        require(checked==1024,"Equipment removal oracle coverage changed");std::cout<<checked<<" original equipment removal cases passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
