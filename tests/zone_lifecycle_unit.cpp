#include "eternity/game/zone_lifecycle.hpp"
#include "eternity/game/effect_cycle.hpp"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& packet) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto byte:packet) out<<std::setw(2)<<unsigned(byte);return out.str();}
struct World final:EffectRemovalWorld,EffectObserver {
    std::vector<std::string> events;
    std::optional<bool> party;
    std::uint32_t tick=0;
    std::array<std::int32_t,3> healed{};
    std::uint32_t tick_ms() const noexcept override {return tick;}
    std::optional<std::uint32_t> allocate_id() noexcept override {return {};}
    void release_id(std::uint32_t id) noexcept override {events.push_back("FREE "+std::to_string(id));}
    void erase_persisted(const PersistedEffect& effect) noexcept override {
        net::Packet packet{2,9,42,0,0,0,static_cast<std::uint8_t>(effect.skill_id),static_cast<std::uint8_t>(effect.skill_id>>8),effect.level};events.push_back("DB "+hex(packet));
    }
    void send_self(const net::Packet& packet) noexcept override {events.push_back("SELF "+hex(packet));}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {
        require(packet.size()==9 && packet[0]==5 && packet[1]==12,"Map check party layout changed");
        events.push_back("PARTY "+std::to_string(packet[6]+256U*packet[7])+" "+std::to_string(packet[8]));
    }
    void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>& value) override {healed=value;}
    void unexpected() {throw std::runtime_error("Unexpected effect unapply in zero-effect fixture");}
    void maximum_changed(EffectStatus&,std::uint8_t) override {unexpected();}
    void speed_changed(EffectStatus&) override {unexpected();}
    void shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void rebuild_attack(EffectStatus&,bool) override {unexpected();}
    void user_shape_changed(EffectStatus&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
std::string snapshot(const ZoneLifecycleState& state,const NProtectState& protect) {
    std::ostringstream out;const auto& move=state.movement;
    out<<unsigned(state.motion)<<' '<<state.target_kind<<' '<<move.back_time<<' '<<move.check_time;
    for(auto value:{move.back.x,move.back.y,move.back.z}) out<<' '<<std::bit_cast<std::uint32_t>(value);
    out<<' '<<move.previous_time;
    for(auto value:{move.previous.x,move.previous.y,move.previous.z,move.total_distance,move.maximum_speed}) out<<' '<<std::bit_cast<std::uint32_t>(value);
    out<<' '<<protect.received<<' '<<protect.receive_time;return out.str();
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original zone lifecycle fixture");std::ifstream input(argv[1]);require(input.good(),"Cannot open zone lifecycle fixture");
        std::string line,actual_state;std::vector<std::string> events;std::size_t cases=0,next=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                unsigned int count=0,map=0;bool enabled=false,present=false,received=false,party=false;std::uint32_t error=0,tick=0;std::array<std::uint32_t,3> position{};std::array<std::uint32_t,4> query{};
                row>>count>>map>>enabled>>present>>received>>error>>party>>tick;for(auto& value:position) row>>value;for(auto& value:query) row>>value;
                World world;world.party=party?std::optional{false}:std::nullopt;EffectCycle cycle(9001);std::vector<SkillDefinition> definitions(count);EffectStatus status;ItemDefinitions items;
                for(unsigned int i=0;i<count;++i) {
                    auto& skill=definitions[i];skill.id=std::array<std::uint16_t,3>{111,124,200}[i%3];skill.level=std::array<std::uint8_t,3>{1,2,100}[(i/3)%3];skill.detail=std::array<std::uint16_t,3>{0,61,81}[i%3];
                    // Unapply is an original oracle port. Zero-effect definitions
                    // let this native fixture isolate the actual owner's control.
                    cycle.append(1000+i,{{skill.id,skill.level,100},777,999999,false},skill);
                }
                ZoneLifecycleState state;state.motion=1;state.target_kind=99;state.position={std::bit_cast<float>(position[0]),std::bit_cast<float>(position[1]),std::bit_cast<float>(position[2])};
                NProtectState protect{present,received,77};
                auto clock=[&]{const auto value=tick;tick+=7;world.events.push_back("TICK "+std::to_string(value));return value;};
                NProtectPorts auth;
                auth.query=[&](auto& output){world.events.push_back("QUERY");output=query;return error;};auth.tick=clock;
                auth.send=[&](net::Packet packet){world.events.push_back("SELF "+hex(packet));};auth.diagnostic=[&](std::uint32_t code){world.events.push_back("ERROR "+std::to_string(code));};auth.close=[&](std::uint32_t reason){world.events.push_back("CLOSE "+std::to_string(reason));};
                ZoneLifecyclePorts port{clock,[&](std::uint32_t value){cycle.reset_keep_tick(value);},[&]{cycle.check_map(static_cast<std::uint16_t>(map),status,{},items,world,world);},[&]{send_nprotect_query(enabled,protect,auth);}};
                enter_zone_lifecycle(state,port);actual_state=snapshot(state,protect);events=std::move(world.events);next=0;++cases;
            } else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original lifecycle event differs at case "+std::to_string(cases)+" event "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="STATE") require(actual_state==line.substr(6),"Original lifecycle state differs at case "+std::to_string(cases)+": "+actual_state+" vs "+line.substr(6));
            else if(tag=="END") require(next==events.size(),"Native lifecycle emitted extra events");
            else throw std::runtime_error("Unknown lifecycle fixture row");
        }
        require(cases==320,"Lifecycle oracle coverage changed");
        // A real periodic heal removed by map restriction must use the reset
        // previous tick, not accumulate time spent loading/transferring zones.
        World world;world.tick=13000;EffectCycle cycle(9001);SkillDefinition heal;heal.id=124;heal.level=1;heal.effect=4;heal.detail=81;heal.time_heal={300,600,900};
        cycle.append(77,{{124,1,100},1000,50000,false},heal);cycle.reset_keep_tick(10000);EffectStatus status;ItemDefinitions items;
        cycle.check_map(18,status,{},items,world,world);require(world.healed==std::array<std::int32_t,3>{300,600,900} && cycle.size()==0,"Map removal did not reverse periodic effect using reset keep tick");
        NProtectState disabled;require(send_nprotect_query(false,disabled,{}),"Disabled NProtect required a provider");
        std::cout<<cases<<" original zone lifecycle/movement/map-effect/NProtect cases and periodic timer composition passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
