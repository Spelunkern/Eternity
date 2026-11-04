#include "eternity/game/mob_ai_states_idle.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok){if(!ok)throw std::runtime_error("Original idle state mismatch");}
using Fields=std::array<std::uint32_t,9>;
void assign(MobRuntime& m,const Fields& f){m.next_new_target=f[0];m.next_move=f[1];m.current_time=f[2];m.target_id=f[3];m.move_speed=std::bit_cast<std::int32_t>(f[4]);m.stop=std::bit_cast<std::int32_t>(f[5]);m.sleep=std::bit_cast<std::int32_t>(f[6]);m.stun=std::bit_cast<std::int32_t>(f[7]);m.hold_position=std::bit_cast<std::int32_t>(f[8]);}
Fields fields(const MobRuntime& m){return {m.next_new_target,m.next_move,m.current_time,m.target_id,static_cast<std::uint32_t>(m.move_speed),static_cast<std::uint32_t>(m.stop),static_cast<std::uint32_t>(m.sleep),static_cast<std::uint32_t>(m.stun),static_cast<std::uint32_t>(m.hold_position)};}
struct Services:MobAiIdleServices,MobAiStateServices {
    MobRuntime& mob;MobAiIdleProjection& projection;std::uint32_t now=0;unsigned mutation=0,index=0;bool found=false,line=false;std::array<std::uint32_t,3> randoms{};std::string trace;
    Services(MobRuntime& subject,MobAiIdleProjection& data):mob(subject),projection(data){}
    void emit(char tag,std::initializer_list<std::uint32_t> values={}){if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;for(auto value:values){if(!first)trace+=',';trace+=std::to_string(value);first=false;}}
    std::uint32_t tick() override {emit('T');if(mutation==1)mob.next_new_target=0xffffffff;return now;}
    bool find_target(MobRuntime& m) override {check(&m==&mob);emit('F');if(mutation==2){m.target_id=0xabcdef01;m.current_time=700;m.next_new_target=888;}return found;}
    void dispatch(MobRuntime& m,std::int32_t event,std::uint32_t arg) override {emit('D',{static_cast<std::uint32_t>(event),arg});dispatch_mob_ai_event(m,event,arg,*this);}
    std::int32_t random() override {
        check(index<3);auto value=randoms[index];emit('Q',{value});
        if(mutation==3 && index==0){mob.move_speed=4;projection.normal_step=9;}
        if(mutation==4 && index==1)projection.zone_available=false;
        if(mutation==5 && index==2){mob.current_time=0xfffffff0;projection.normal_time=0xffffffff;}
        ++index;return static_cast<std::int32_t>(value);
    }
    bool enable_line(MobRuntime& m,std::int32_t x,std::int32_t z,std::int32_t end_x,std::int32_t end_z) override {
        check(&m==&mob);emit('L',{static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(z),static_cast<std::uint32_t>(end_x),static_cast<std::uint32_t>(end_z)});if(mutation==6)m.current_time=900;return line;
    }
    void move(MobRuntime& m,UserPosition at,std::int32_t mode) override {check(&m==&mob);emit('M',{std::bit_cast<std::uint32_t>(at.x),std::bit_cast<std::uint32_t>(at.y),std::bit_cast<std::uint32_t>(at.z),static_cast<std::uint32_t>(mode)});if(mutation==7){m.current_time=1000;projection.normal_time=12345;}}
    void update(MobRuntime& m,MobAiState state) override {check(state==MobAiState::idle);update_mob_ai_idle(m,projection,*this);}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected FSM error");}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::runtime_error("Unexpected Lua call");}
};
int main(int argc,char**argv){
 try{
    check(argc==2);std::ifstream file(argv[1]);check(file.good());std::string line;unsigned count=0;
    while(std::getline(file,line)){
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);
        unsigned id,mutation,found,allowed,ai,speed,step,obelisk,chase,zone;std::uint32_t now,normal;Fields input{},expected{};std::array<std::uint32_t,3> position{},home{},randoms{};std::string trace;unsigned final_step,final_zone;std::uint32_t final_normal;
        row>>id>>mutation>>now>>found>>allowed>>ai>>speed>>step>>normal>>obelisk>>chase>>zone;for(auto& value:input)row>>value;for(auto& value:position)row>>value;for(auto& value:home)row>>value;for(auto& value:randoms)row>>value;row>>trace;for(auto& value:expected)row>>value;row>>final_normal>>final_step>>final_zone;check(!row.fail());
        MobRuntime mob;assign(mob,input);mob.position={std::bit_cast<float>(position[0]),std::bit_cast<float>(position[1]),std::bit_cast<float>(position[2])};mob.respawn_position={std::bit_cast<float>(home[0]),std::bit_cast<float>(home[1]),std::bit_cast<float>(home[2])};mob.state_stack=std::vector<std::int32_t>{0};
        MobAiIdleProjection data{static_cast<std::uint8_t>(ai),static_cast<std::uint8_t>(speed),static_cast<std::uint8_t>(step),normal,obelisk!=0,chase!=0,zone!=0};
        Services services(mob,data);services.now=now;services.found=found!=0;services.line=allowed!=0;services.mutation=mutation;services.randoms=randoms;
        check(poll_mob_ai_stack(mob,services));
        if(fields(mob)!=expected || services.trace!=trace || data.normal_time!=final_normal || data.normal_step!=final_step || data.zone_available!=(final_zone!=0))throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace);
        const auto target_state=trace.find("D:0,")!=std::string::npos?1:trace.find("D:4,")!=std::string::npos?3:0;
        check(mob.state_stack->back()==target_state);++count;
    }
    check(count==1536);std::cout<<count<<" original WhileIdle comparisons through native stack poll/dispatch passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
