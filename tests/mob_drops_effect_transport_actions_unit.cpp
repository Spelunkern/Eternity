#include "eternity/game/actor.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::uint32_t get(const net::Packet& packet,std::size_t offset) {std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=static_cast<std::uint32_t>(packet.at(offset+i))<<(8*i);return value;}
struct World final:EffectWorld,EffectObserver,ActorScene ,public ActorEffectDatabase {
    Actor* actor=nullptr;std::vector<PersistedEffect> expected;unsigned databases=0,clients=0,released=0;bool shutdown=false;
    void send_effect_database(const net::Packet& packet) noexcept override {
        if(packet.size()!=9 || packet[0]!=2 || packet[1]!=9 || get(packet,2)!=9001) std::terminate();
        const auto id=static_cast<std::uint16_t>(packet[6]|(packet[7]<<8));const auto level=packet[8];
        const auto first=std::find_if(expected.begin(),expected.end(),[&](const auto& effect){return effect.skill_id==id && effect.level==level;});
        if(first!=expected.end()) expected.erase(first);
        const auto& actual=actor->replica().snapshot();if(actual.size()!=expected.size()) std::terminate();
        for(std::size_t i=0;i<actual.size();++i) if(actual[i].skill_id!=expected[i].skill_id || actual[i].level!=expected[i].level || actual[i].remaining!=expected[i].remaining) std::terminate();
        ++databases;if(actor->effects().size()!=initial_nodes-databases || clients!=databases-1 || released!=clients) std::terminate();record('B',packet);
    }
    unsigned initial_nodes=0;
    EffectStatus& status;unsigned int scenario=0;bool member=false,raid=false;std::vector<std::string> events;
    explicit World(EffectStatus& value):status(value) {}
    void record(char route,const net::Packet& packet) {
        static constexpr char digits[]="0123456789abcdef";std::string event(1,route);for(auto byte:packet) {event+=digits[byte>>4];event+=digits[byte&15];}events.push_back(std::move(event));
    }
    void state(char route) {events.push_back(std::string(1,route)+std::to_string(status.shape)+","+std::to_string(status.fixed_form)+","+std::to_string(status.action_block_b)+","+std::to_string(status.shape_template));}
    void send_self(const net::Packet& packet) noexcept override {if(packet.size()==6 && packet[0]==14 && packet[1]==5) {if(databases!=clients+1 || released!=clients) std::terminate();++clients;}record('S',packet);}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return member?std::optional<bool>{raid}:std::nullopt;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {
        net::Packet packet{2,9,0x29,0x23,0,0,static_cast<std::uint8_t>(effect.skill_id),static_cast<std::uint8_t>(effect.skill_id>>8),effect.level};record('B',packet);
    }
    void release_id(std::uint32_t id) noexcept override {
        if(!shutdown) {if(clients!=released+1) std::terminate();++released;}events.push_back("F"+std::to_string(id));if(scenario&8) member=false;else if(scenario&16) raid=true;else if(scenario&32) {member=true;raid=false;}
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
            std::vector<SkillDefinition> definitions(nodes);World world(status);world.scenario=scenario;world.member=(scenario&2)!=0;world.raid=(scenario&4)!=0;
            for(unsigned int i=0;i<nodes;++i) {
                unsigned int id=0,level=0,detail=0,effect=0,change=0;row>>id>>level>>detail>>effect>>change;auto& skill=definitions[i];
                skill.id=static_cast<std::uint16_t>(id);skill.level=static_cast<std::uint8_t>(level);skill.detail=static_cast<std::uint16_t>(detail);skill.effect=static_cast<std::uint8_t>(effect);skill.change_type=static_cast<std::uint16_t>(change);
                world.expected.push_back({skill.id,skill.level,30});
            }
            const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
            Actor actor(9001,status,{},world.expected,world,{items,enchant,mobs});actor.account_id()=9001;world.actor=&actor;world.initial_nodes=nodes;auto& cycle=actor.effects();
            struct Closing {World& world;~Closing(){world.shutdown=true;}} closing{world};
            for(unsigned i=0;i<nodes;++i) {const auto& skill=definitions[i];cycle.append(1000+i,{{skill.id,skill.level,30},10000,12000,false},skill);}
            const auto removed=op==0?cycle.cancel_shape_on_attack(status,world,actor):false;if(op==1) cycle.remove_sleep(status,world,actor);
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
            world.shutdown=true;
        }
        require(count==2304,"Attack effects coverage changed");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        std::cout<<count<<" original shape/sleep traces through actual Actor DB transport and replica matched\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
