#include "eternity/game/actor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::uint32_t get(const net::Packet& packet,std::size_t offset) {std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=static_cast<std::uint32_t>(packet.at(offset+i))<<(8*i);return value;}
struct World final:EffectWorld,EffectObserver,ActorScene ,public ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    EffectStatus& status;unsigned int scenario=0;bool member=false,raid=false;std::vector<std::string> events;
    explicit World(EffectStatus& value):status(value) {}
    void record(char route,const net::Packet& packet) {
        static constexpr char digits[]="0123456789abcdef";std::string event(1,route);for(auto byte:packet) {event+=digits[byte>>4];event+=digits[byte&15];}events.push_back(std::move(event));
    }
    void state(char route) {events.push_back(std::string(1,route)+std::to_string(status.shape)+","+std::to_string(status.fixed_form)+","+std::to_string(status.action_block_b)+","+std::to_string(status.shape_template));}
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return member?std::optional<bool>{raid}:std::nullopt;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {
        net::Packet packet{2,9,0x29,0x23,0,0,static_cast<std::uint8_t>(effect.skill_id),static_cast<std::uint8_t>(effect.skill_id>>8),effect.level};record('B',packet);
    }
    void release_id(std::uint32_t id) noexcept override {
        events.push_back("F"+std::to_string(id));if(scenario&8) member=false;else if(scenario&16) raid=true;else if(scenario&32) {member=true;raid=false;}
    }
    void release_effect_id(std::uint32_t id) noexcept override {release_id(id);}
    std::optional<std::uint32_t> allocate_id() noexcept override {std::terminate();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void rebuild_attack(EffectStatus&,bool equipment) override {require(equipment,"Unexpected attack-cancel rebuild mode");state('R');}
    void user_shape_changed(EffectStatus&) override {state('U');}
    void speed_changed(EffectStatus&) override {state('V');if(scenario&64) member=!member;}
    void shape_changed(EffectStatus& value) override {if(value.in_zone) record('C',{0x1d,5,0x29,0x23,0,0,value.shape});}
    void run_mode_changed(EffectStatus&,bool running) override {record('S',{0x10,2,static_cast<std::uint8_t>(running)});}
    void unexpected() {throw std::runtime_error("Unexpected attack effect operation");}
    void maximum_changed(EffectStatus&,std::uint8_t) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
    void reset_damage() override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected attack effects oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open attack effects oracle");
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int op=0,scenario=0,shape=0,nodes=0;EffectStatus status;
            row>>op>>scenario>>shape>>status.fixed_form>>status.action_block_b>>nodes;require(static_cast<bool>(row) && op<2,"Malformed attack effects input");
            status.shape=static_cast<std::uint8_t>(shape);status.shape_template=321;status.form_mob=42;status.in_zone=(scenario&1)!=0;status.skills.equipment.transformed=shape!=0;
            std::vector<SkillDefinition> definitions(nodes);EffectCycle cycle(9001);World world(status);world.scenario=scenario;world.member=(scenario&2)!=0;world.raid=(scenario&4)!=0;
            for(unsigned int i=0;i<nodes;++i) {
                unsigned int id=0,level=0,detail=0,effect=0,change=0;row>>id>>level>>detail>>effect>>change;auto& skill=definitions[i];
                skill.id=static_cast<std::uint16_t>(id);skill.level=static_cast<std::uint8_t>(level);skill.detail=static_cast<std::uint16_t>(detail);skill.effect=static_cast<std::uint8_t>(effect);skill.change_type=static_cast<std::uint16_t>(change);
                cycle.append(1000+i,{{skill.id,skill.level,30},10000,12000,false},skill);
            }
            const auto removed=op==0?cycle.cancel_shape_on_attack(status,world,world):false;if(op==1) cycle.remove_sleep(status,world,world);
            unsigned int final_shape=0,form=0;std::int32_t fixed=0,sleep=0;std::uint32_t shape_template=0,copied=0;std::size_t remaining_count=0,event_count=0;
            row>>final_shape>>fixed>>sleep>>shape_template>>form>>copied>>remaining_count;std::vector<std::uint32_t> remaining(remaining_count);for(auto& id:remaining) row>>id;
            row>>event_count;std::vector<std::string> events(event_count);for(auto& event:events) row>>event;
            const auto context=" at attack effect vector "+std::to_string(count);
            require(status.shape==final_shape && status.fixed_form==fixed && status.action_block_b==sleep && status.shape_template==shape_template && status.form_mob==form,"Attack effect state differs"+context);
            require(copied==0x778899aa && status.skills.equipment.transformed==(final_shape!=0),"Copied profile or equipment transform state differs"+context);
            std::vector<std::uint32_t> actual;for(const auto& packet:cycle.initial_packets(10000)) for(std::size_t at=3;at<packet.size();at+=11) actual.push_back(get(packet,at));
            require(actual==remaining && (op!=0 || removed==(remaining_count!=nodes)),"Attack effect surviving order/result differs"+context);
            require(world.events==events,"Attack effect notice order differs"+context);
            require(static_cast<bool>(row),"Truncated attack effects vector");std::string extra;require(!(row>>extra),"Trailing attack effects data");++count;
        }
        require(count==2304,"Attack effects coverage changed");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        std::array<SkillDefinition,3> skills{};for(std::size_t i=0;i<skills.size();++i) {skills[i].id=static_cast<std::uint16_t>(300+i);skills[i].level=1;skills[i].effect=3;skills[i].detail=i==0?90:66;}
        skills[0].change_type=101;EffectStatus status;status.shape=101;status.shape_template=123;status.fixed_form=1;status.action_block_b=2;status.in_zone=true;
        World scene(status);Actor actor(9001,status,{0,0,0},{{300,1,30},{301,1,30},{302,1,30}},scene,{items,enchant,mobs});actor.appearance().copied_user.emplace();
        for(std::size_t i=0;i<skills.size();++i) actor.effects().append(static_cast<std::uint32_t>(2000+i),{{skills[i].id,1,30},10000,12000,false},skills[i]);
        require(actor.cancel_shape_on_attack() && actor.status().shape==0 && actor.status().fixed_form==0 && actor.status().shape_template==0 && actor.appearance().copied_user.has_value(),"Actor did not cancel fixed transformation or retain cached copied profile");
        actor.remove_sleep();require(actor.effects().size()==0 && actor.replica().snapshot().empty() && actor.status().action_block_b==0,"Actor did not clear all sleep nodes and replicas");
        require(scene.events.size()==9 && scene.events[0]=="S0e05d0070000" && scene.events[1]=="C1d052923000000" && scene.events[3]=="F2000" && scene.events[4]=="S0e05d1070000" && scene.events[5]=="F2001" && scene.events[6]=="S0e05d2070000" && scene.events[7]=="F2002","Actor attack effect cancellation ordering changed");
        std::cout<<count<<" original attack shape/sleep removal cases and native actor integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
