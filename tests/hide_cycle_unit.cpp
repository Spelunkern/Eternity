#include "eternity/game/actor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::uint32_t get(const net::Packet& packet,std::size_t offset) {std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=static_cast<std::uint32_t>(packet.at(offset+i))<<(8*i);return value;}
struct Scene final:ActorScene,EffectObserver {
    std::vector<std::string> events;std::optional<bool> party;unsigned int scenario=0;
    void record(char route,const net::Packet& packet) {
        static constexpr char digits[]="0123456789abcdef";std::string result(1,route);
        for(auto byte:packet) {result+=digits[byte>>4];result+=digits[byte&15];}events.push_back(std::move(result));
    }
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_nearby(const net::Packet&) override {throw std::runtime_error("Unexpected proximity route");}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    std::optional<std::uint32_t> allocate_id() noexcept override {return {};}
    void release_id(std::uint32_t id) noexcept override {release_effect_id(id);}
    void release_effect_id(std::uint32_t id) noexcept override {
        events.push_back("F"+std::to_string(id));if(scenario&16) party.reset();else if(scenario&32) party=true;
    }
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {
        net::Packet packet{2,9,0x29,0x23,0,0,static_cast<std::uint8_t>(effect.skill_id),static_cast<std::uint8_t>(effect.skill_id>>8),effect.level};record('B',packet);
    }
    void unexpected() {throw std::runtime_error("Unexpected hide scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected hide oracle path");std::ifstream input(argv[1]);require(input.good(),"Cannot open hide oracle");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        const std::array<std::vector<std::pair<std::uint16_t,std::uint8_t>>,4> variants={std::vector<std::pair<std::uint16_t,std::uint8_t>>{},
            {{82,4}},{{82,4},{88,4},{88,3},{82,4}},{{88,0},{82,4},{88,4}}};
        std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int s=0,shape=0,count=0;
            require(static_cast<bool>(row>>s>>shape>>count),"Malformed hide oracle row");
            std::vector<std::uint32_t> remaining(count);for(auto& id:remaining) row>>id;
            std::size_t event_count=0;row>>event_count;std::vector<std::string> events(event_count);for(auto& event:events) row>>event;
            Scene scene;scene.scenario=s;if(s&4) scene.party=(s&8)!=0;
            const auto& variant=variants[s>>6];std::vector<SkillDefinition> definitions(variant.size());
            EffectStatus status;status.shape=static_cast<std::uint8_t>(s&1?100+s%3:0);status.in_zone=(s&2)!=0;
            status.shape_template=321;status.form_mob=42;status.skills.equipment.transformed=status.shape!=0;
            Actor actor(9001,status,{0,0,0},{},scene,{items,enchant,mobs});
            for(std::size_t i=0;i<definitions.size();++i) {
                auto& skill=definitions[i];skill.id=static_cast<std::uint16_t>(200+i);skill.level=1;skill.detail=variant[i].first;skill.effect=variant[i].second;
                actor.effects().append(static_cast<std::uint32_t>(1000+i),{{skill.id,1,30},10000,12000,false},skill);
            }
            const bool removed=actor.effects().remove_hide(actor.status(),actor,scene);const auto context=" at hide vector "+std::to_string(s);
            require(removed==(actor.effects().size()!=definitions.size()),"Hide removal return differs"+context);
            require(actor.status().shape==shape && actor.status().shape_template==321 && actor.status().form_mob==42,"Hide transformation state differs"+context);
            require(actor.status().skills.equipment.transformed==(shape!=0),"Hide equipment transform flag differs"+context);
            std::vector<std::uint32_t> actual;
            for(const auto& packet:actor.effects().initial_packets(10000)) for(std::size_t at=3;at<packet.size();at+=11) actual.push_back(get(packet,at));
            require(actual==remaining,"Hide surviving list order differs"+context);require(scene.events==events,"Hide notification order differs"+context);
            std::string extra;require(!(row>>extra),"Unexpected hide oracle data");++checked;
        }
        require(checked==256,"Hide oracle coverage changed");
        // The public actor operation updates its own replica and releases only the first matching node.
        Scene scene;std::array<SkillDefinition,2> skills{};for(std::size_t i=0;i<2;++i){skills[i].id=static_cast<std::uint16_t>(300+i);skills[i].level=1;skills[i].detail=88;skills[i].effect=4;}
        EffectStatus status;status.shape=13;status.skills.equipment.transformed=true;status.in_zone=true;
        Actor actor(9001,status,{0,0,0},{{300,1,30},{301,1,30}},scene,{items,enchant,mobs});
        for(std::size_t i=0;i<2;++i) actor.effects().append(static_cast<std::uint32_t>(2000+i),{{skills[i].id,1,30},10000,12000,false},skills[i]);
        require(actor.reveal() && !actor.reveal() && actor.effects().size()==1 && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].skill_id==301,"Actor reveal lost first-node replica behavior");
        require(scene.events==std::vector<std::string>{"S0e05d0070000","C1d052923000000","S100201","F2000"},"Actor reveal packet order changed");
        std::cout<<checked<<" original hide-removal scenarios and concrete actor reveal passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
