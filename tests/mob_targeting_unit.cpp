#include "eternity/game/mob_targeting.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
struct Services final:MobTargetingServices {
    std::uint32_t seed=0,tick=0,ticks=0;bool mutate=false;std::ostringstream events;
    std::int32_t random() override {seed=seed*214013U+2531011U;const auto v=(seed>>16)&32767U;events<<"R "<<v<<' ';return static_cast<std::int32_t>(v);}
    std::uint32_t tick_ms() override {const auto v=tick+ticks++*11U;events<<"T "<<v<<' ';return v;}
    void help(MobTargetingState& m,const MobTargetingUser&,std::uint8_t kind) override {
        events<<"H "<<static_cast<unsigned>(kind)<<' '<<static_cast<std::uint32_t>(m.status)<<' '<<m.target_id<<' '<<m.combat_timeout<<' ';
        if(mutate) {m.target_id=987654321;m.attacks[1]={true,true,65};}
    }
    void dispatch(MobTargetingState&,std::uint32_t event,std::uint32_t argument) override {events<<"D "<<event<<' '<<argument<<' ';}
};
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Cannot read fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);MobTargetingState m;MobTargetingUser u{0xf1234567,0};Services s;
            unsigned op,fsm,init,ai,mask,skills,mutate;std::int32_t raw_status=0;std::array<unsigned,3> cool{};
            row>>op>>fsm>>m.status>>m.move_mode>>u.life_status>>m.vision_fixed_user>>init>>ai>>mask>>skills>>cool[0]>>cool[1]>>cool[2]>>mutate>>s.seed>>s.tick>>raw_status;
            m.fsm_enabled=fsm!=0;m.scene_status=m.status;m.status=raw_status;m.ai=static_cast<std::uint8_t>(ai);m.target_type=7;m.target_id=12345;m.combat_timeout=8765;m.next_move_time=777;m.attack_times={111,222,333};s.mutate=mutate!=0;
            for(unsigned i=0;i<3;++i) m.attacks[i]={(mask&(1U<<i))!=0,(skills&(1U<<i))!=0,static_cast<std::uint16_t>(cool[i])};
            char separator;row>>separator;if(separator!='|') throw std::runtime_error("Malformed input");
            std::array<std::uint32_t,8> expected{};for(auto& v:expected) row>>v;
            row>>separator;if(separator!='|') throw std::runtime_error("Malformed output");
            std::string event_text;std::getline(row,event_text);
            if(op) mob_set_target_ex(m,u,s);else mob_set_target(m,u,init!=0,s);
            const std::array<std::uint32_t,8> actual{static_cast<std::uint32_t>(m.status),m.target_type,m.target_id,m.combat_timeout,m.next_move_time,m.attack_times[0],m.attack_times[1],m.attack_times[2]};
            std::istringstream a(s.events.str()),e(event_text);std::string av,ev;
            bool same=true;while(e>>ev) {if(!(a>>av) || av!=ev) same=false;}if(a>>av) same=false;
            if(actual!=expected || !same) throw std::runtime_error("Original targeting differs at case "+std::to_string(count));
            ++count;
        }
        if(count!=4096) throw std::runtime_error("Coverage changed");
        std::cout<<count<<" original targeting/scene transitions passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
