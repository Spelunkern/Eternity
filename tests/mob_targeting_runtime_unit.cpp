#include "eternity/game/mob_targeting_runtime.hpp"
#include "eternity/game/mobs.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
struct Services final:MobTargetingRuntimeServices {
    MobTargetingBindings live;
    MobTargetingBindings bindings(const MobRuntime&) const override {return live;}
    std::uint32_t seed=0,tick=0,ticks=0;bool mutate=false;std::ostringstream events;
    std::int32_t random(MobRuntime&) override {seed=seed*214013U+2531011U;const auto v=(seed>>16)&32767U;events<<"R "<<v<<' ';return static_cast<std::int32_t>(v);}
    std::uint32_t tick_ms(MobRuntime&) override {const auto v=tick+ticks++*11U;events<<"T "<<v<<' ';return v;}
    void help(MobRuntime& m,const MobTargetingUser&,std::uint8_t kind) override {
        events<<"H "<<static_cast<unsigned>(kind)<<' '<<static_cast<std::uint32_t>(m.status)<<' '<<m.target_id<<' '<<m.combat_timeout<<' ';
        if(mutate) {m.target_id=987654321;live.attacks[1]={true,true,65};}
    }
    void dispatch(MobRuntime&,std::uint32_t event,std::uint32_t argument) override {events<<"D "<<event<<' '<<argument<<' ';}
};
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct LiveServices final:MobTargetingRuntimeServices {
    MobTargetingBindings live{2, {{{true,true,1},{true,true,1},{false,false,0}}}};
    unsigned calls=0;bool fail=false;
    MobTargetingBindings bindings(const MobRuntime&) const override {return live;}
    void help(MobRuntime& m,const MobTargetingUser& u,std::uint8_t kind) override {
        check(++calls==1 && kind==2 && u.id==17,"Help route");
        check(m.target_kind==1 && m.target_id==17 && m.combat_timeout==0 && m.status==42,"Help observes original writes");
        if(fail) throw std::runtime_error("provider failure");
        m.target_id=99;m.state_stack=std::vector<std::int32_t>{8,4};
    }
    std::int32_t random(MobRuntime& m) override {
        check(++calls==2 && m.target_id==99 && m.attack_times[0]==0,"Random observes prior timer writes");
        live.attacks[1].cooldown_seconds=65;m.attack_times[0]=888;return 32767;
    }
    std::uint32_t tick_ms(MobRuntime& m) override {
        check(++calls==3 && m.attack_times[0]==888,"Tick observes random mutations");
        live.attacks[2]={true,true,0};m.status=70;m.next_move=123;return 0xfffffff0;
    }
    void dispatch(MobRuntime& m,std::uint32_t event,std::uint32_t argument) override {
        check(++calls==4 && event==1 && argument==99,"Dispatch current target");
        check(m.attack_times==std::array<std::uint32_t,3>{888,32751,0} && m.status==70,"Dispatch observes timers and live fields");
        check(*m.state_stack==std::vector<std::int32_t>({8,4}),"Help scene transition retained");
        m.state_stack=std::vector<std::int32_t>{9,6};m.target_id=123;m.combat_timeout=55;m.vision_fixed_user=66;
    }
};
void live_callbacks() {
    MobRuntime m;m.status=42;m.state_stack=std::vector<std::int32_t>{};m.combat_timeout=55;m.next_move=777;m.attack_times={111,222,333};
    LiveServices s;mob_set_target(m,{17,0},false,s);
    check(s.calls==4 && m.target_id==123 && m.combat_timeout==55 && m.vision_fixed_user==66 && m.next_move==123,"Final provider fields retained");
    check(*m.state_stack==std::vector<std::int32_t>({9,6}),"Dispatch scene transition retained");
    MobRuntime failed;failed.status=42;failed.state_stack=std::vector<std::int32_t>{};failed.combat_timeout=55;
    LiveServices throwing;throwing.fail=true;bool caught=false;
    try {mob_set_target_ex(failed,{17,0},throwing);} catch(const std::runtime_error& e) {caught=std::string(e.what())=="provider failure";}
    check(caught && throwing.calls==1 && failed.target_id==17 && failed.combat_timeout==0,"Provider failure preserves committed writes");
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Cannot read fixture");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);MobRuntime m;MobTargetingUser u{0xf1234567,0};Services s;
            unsigned op,fsm,init,ai,mask,skills,mutate;std::int32_t raw_status=0;std::array<unsigned,3> cool{};
            row>>op>>fsm>>m.status>>m.move_mode>>u.life_status>>m.vision_fixed_user>>init>>ai>>mask>>skills>>cool[0]>>cool[1]>>cool[2]>>mutate>>s.seed>>s.tick>>raw_status;
            if(fsm) {m.state_stack=std::vector<std::int32_t>{-77,m.status};}
            m.status=raw_status;s.live.ai=static_cast<std::uint8_t>(ai);m.target_kind=7;m.target_id=12345;m.combat_timeout=8765;m.next_move=777;m.attack_times={111,222,333};s.mutate=mutate!=0;
            for(unsigned i=0;i<3;++i) s.live.attacks[i]={(mask&(1U<<i))!=0,(skills&(1U<<i))!=0,static_cast<std::uint16_t>(cool[i])};
            char separator;row>>separator;if(separator!='|') throw std::runtime_error("Malformed input");
            std::array<std::uint32_t,8> expected{};for(auto& v:expected) row>>v;
            row>>separator;if(separator!='|') throw std::runtime_error("Malformed output");
            std::string event_text;std::getline(row,event_text);
            if(op) mob_set_target_ex(m,u,s);else mob_set_target(m,u,init!=0,s);
            if(fsm && (m.state_stack->size()!=2 || m.state_stack->front()!=-77)) throw std::runtime_error("Scene ownership corrupted");
            const std::array<std::uint32_t,8> actual{static_cast<std::uint32_t>(m.status),static_cast<std::uint32_t>(m.target_kind),m.target_id,m.combat_timeout,m.next_move,m.attack_times[0],m.attack_times[1],m.attack_times[2]};
            std::istringstream a(s.events.str()),e(event_text);std::string av,ev;
            bool same=true;while(e>>ev) {if(!(a>>av) || av!=ev) same=false;}if(a>>av) same=false;
            if(actual!=expected || !same) throw std::runtime_error("Original targeting differs at case "+std::to_string(count));
            ++count;
        }
        live_callbacks();
        if(count!=4096) throw std::runtime_error("Coverage changed");
        std::cout<<count<<" original targeting comparisons through real MobRuntime passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
