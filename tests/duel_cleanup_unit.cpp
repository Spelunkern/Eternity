#include "equipment_fixture.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct World final:EffectWorld,EffectObserver,ActorScene ,public ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    Actor* actor=nullptr;std::vector<std::string> events;
    void state(char kind) {
        const auto& status=actor->status();events.push_back(std::string(1,kind)+std::to_string(status.action_block_a)+","+std::to_string(status.movement_block)+","+
            std::to_string(status.action_block_b)+","+std::to_string(status.fixed_form)+","+std::to_string(status.shape)+","+std::to_string(status.shape_template));
    }
    void maximum_changed(EffectStatus&,std::uint8_t resource) override {require(resource==0,"Unexpected debuff resource");state('M');}
    void speed_changed(EffectStatus&) override {state('V');}
    void shape_changed(EffectStatus&) override {state('S');}
    void rebuild_attack(EffectStatus&,bool equipment) override {require(equipment,"Unexpected debuff rebuild mode");state('A');}
    void erase_persisted(const PersistedEffect& effect) noexcept override {events.push_back("D"+std::to_string(effect.skill_id)+","+std::to_string(effect.level));actor->erase_persisted(effect);}
    void release_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    void release_effect_id(std::uint32_t id) noexcept override {release_id(id);}
    void reset_damage() override {
        const auto& relation=actor->relations();events.push_back("H"+std::to_string(relation.duel_state)+","+std::to_string(relation.duel_type)+","+std::to_string(actor->status().current_target_type));
    }
    void packet(char kind,const net::Packet& data) {
        static constexpr char digits[]="0123456789abcdef";std::string value(1,kind);for(auto byte:data) {value+=digits[byte>>4];value+=digits[byte&15];}events.push_back(value);
    }
    void send_nearby(const net::Packet& data) override {packet('N',data);}
    void send_cell_view(const net::Packet& data) override {packet('C',data);}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_self(const net::Packet&) noexcept override {std::terminate();}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    std::optional<std::uint32_t> allocate_id() noexcept override {std::terminate();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void unexpected() {throw std::runtime_error("Unexpected duel cleanup operation");}
    void user_shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
std::vector<std::uint32_t> snapshot(const Actor& actor) {
    const auto& status=actor.status();const auto& equipment=status.skills.equipment;const auto& relation=actor.relations();
    std::vector<std::uint32_t> result;
    const auto put=[&](auto value) {result.push_back(static_cast<std::uint32_t>(value));};
    put(equipment.base.maximum.hp);put(equipment.base.attributes.strength);put(equipment.attack_speed);put(equipment.defense_attribute);
    put(status.action_block_a);put(status.movement_block);put(status.action_block_b);put(status.fixed_form);put(status.chase_id);put(status.shape_template);
    put(status.block_all);put(status.skills.hp_ability_previous);put(relation.duel_state);put(relation.duel_type);put(relation.duel_opponent);put(status.current_target_type);put(status.current_target);
    put(status.shape);put(status.form_mob);put(status.no_mob_targeting);put(status.skills.hp_ability_applied);put(status.skills.hp_ability_id);
    put(actor.appearance().copied_user?0x778899aaU:0);put(actor.damage_ledger().entries().size());return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected duel cleanup oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open duel cleanup oracle");
        const auto items=ItemDefinitions::load(item_definitions());const EnchantValues enchant;const MobAttackTemplates mobs;const SkillDefinitions no_skills;
        PlayerItem armor;armor.type=16;armor.type_id=1;armor.slot=1;armor.gems[0]=3;
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int op=0,equipped=0,shape=0,nodes=0;std::int32_t counter=0,duel_state=0,duel_type=0,target_type=0;
            row>>op>>equipped>>counter>>shape>>duel_state>>duel_type>>target_type>>nodes;require(static_cast<bool>(row) && op<=2,"Malformed duel cleanup input");
            std::vector<SkillDefinition> definitions(nodes);std::vector<PersistedEffect> replica;
            for(auto& skill:definitions) {
                unsigned int detail=0,effect=0,id=0,level=0;row>>detail>>effect>>id>>level;
                skill.id=static_cast<std::uint16_t>(id);skill.level=static_cast<std::uint8_t>(level);skill.detail=static_cast<std::uint16_t>(detail);skill.effect=static_cast<std::uint8_t>(effect);
                skill.abilities={{{1,17},{4,31},{16,9}}};replica.push_back({skill.id,skill.level,30});
            }
            World world;Actor actor(9001,{}, {100,0,0},replica,world,{items,enchant,mobs},equipped?std::vector<PlayerItem>{armor}:std::vector<PlayerItem>{});world.actor=&actor;
            actor.damage(1,1,0,no_skills);actor.damage(1,2,0,no_skills);require(world.events.empty(),"Damage fixture emitted notifications");
            auto& status=actor.status();status.skills.equipment.base.maximum.hp=10000;status.skills.equipment.base.attributes.strength=100;status.skills.equipment.attack_speed=2;status.skills.equipment.defense_attribute=8;
            status.action_block_a=status.movement_block=status.action_block_b=status.fixed_form=counter;status.chase_id=888;status.shape_template=321;status.block_all=4;
            status.skills.hp_ability_previous=123456;status.shape=static_cast<std::uint8_t>(shape);status.form_mob=42;status.no_mob_targeting=true;
            status.skills.hp_ability_applied=true;status.skills.hp_ability_id=200;status.skills.equipment.transformed=shape!=0;
            actor.relations().duel_state=duel_state;actor.relations().duel_type=duel_type;actor.relations().duel_opponent=777;status.current_target_type=target_type;status.current_target=999;
            actor.appearance().copied_user.emplace();
            for(std::size_t i=0;i<definitions.size();++i) actor.effects().append(static_cast<std::uint32_t>(1000+i),{{definitions[i].id,definitions[i].level,30},0,30000,false},definitions[i]);
            if(op==0) cancel_debuff(status,definitions.at(0),{nullptr,equipped?&armor:nullptr},items,world);
            else if(op==1) actor.effects().clear_duel_debuffs(status,{nullptr,equipped?&armor:nullptr},items,world,world);
            else actor.end_duel();
            const auto context=" at duel cleanup vector "+std::to_string(count);unsigned int field=0;
            for(auto value:snapshot(actor)) {std::uint32_t original=0;row>>original;require(value==original,"Duel cleanup state differs"+context+" field "+std::to_string(field));++field;}
            std::size_t remaining=0;row>>remaining;require(actor.effects().size()==remaining,"Duel surviving count differs"+context);
            for(std::size_t i=0;i<remaining;++i) {std::uint32_t id=0;row>>id;require(actor.effects().contains(id),"Duel surviving ID differs"+context);}
            std::size_t events=0;row>>events;require(world.events.size()==events,"Duel cleanup event count differs"+context);
            for(const auto& event:world.events) {std::string original;row>>original;require(event==original,"Duel cleanup event differs"+context+": "+event+" versus "+original);}
            require(actor.replica().snapshot().size()==remaining,"Duel replica count differs"+context); // One persisted row per original runtime node.
            require(static_cast<bool>(row),"Truncated duel cleanup vector");std::string extra;require(!(row>>extra),"Trailing duel cleanup data");++count;
        }
        require(count==868,"Duel cleanup coverage changed");
        // Concrete actor: debuffs reverse without self/group expiry notices;
        // buffs and a stale opponent/target ID remain, while history is reset last.
        World world;Actor actor(9001,{}, {100,0,0},{{310,1,30},{311,1,30},{312,1,30}},world,{items,enchant,mobs});world.actor=&actor;
        actor.damage(1,1,0,no_skills);actor.status().in_zone=true;actor.status().action_block_b=1;actor.status().skills.equipment.base.attributes.strength=69;
        actor.status().current_target_type=1;actor.status().current_target=999;actor.relations().duel_state=3;actor.relations().duel_type=2;actor.relations().duel_opponent=777;
        std::array<SkillDefinition,3> skills{};for(std::size_t i=0;i<3;++i) {auto& skill=skills[i];skill.id=static_cast<std::uint16_t>(310+i);skill.level=1;skill.effect=i==2?4:3;skill.detail=i==0?62:i==1?66:82;skill.abilities[0]={4,31};actor.effects().append(static_cast<std::uint32_t>(2000+i),{{skill.id,1,30},0,30000,false},skill);}
        actor.end_duel();
        require(actor.effects().size()==1 && actor.effects().contains(2002) && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].skill_id==312,"Concrete duel end did not preserve its buff");
        require(actor.status().skills.equipment.base.attributes.strength==100 && actor.status().action_block_b==0 && actor.damage_ledger().entries().empty(),"Concrete duel end did not reverse debuffs/history");
        require(actor.relations().duel_state==0 && actor.relations().duel_type==-1 && actor.relations().duel_opponent==777 && actor.status().current_target_type==0 && actor.status().current_target==999,"Concrete duel end reset the wrong target/relationship fields");
        require(world.events.size()==4 && world.events[0]=="F2000" && world.events[1].starts_with("N1c05") && world.events[2]=="F2001" && world.events[3]=="H3,2,1","Concrete duel end notification/history ordering changed");
        std::cout<<count<<" original debuff/duel cleanup cases and concrete actor duel end passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
