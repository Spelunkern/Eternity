#include "eternity/game/mob_ai_states_return.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include "mob_damage_fixture.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok){if(!ok)throw std::runtime_error("Original return-home mismatch");}
std::string points(std::span<const MapTile> values,bool reverse=false){std::string result;for(std::size_t i=0;i<values.size();++i){auto p=values[reverse?values.size()-1-i:i];if(!result.empty())result+=',';result+=std::to_string(static_cast<std::uint32_t>(p.x))+','+std::to_string(static_cast<std::uint32_t>(p.z));}return result.empty()?"-":result;}
std::array<std::uint32_t,3> bits(UserPosition p){return {std::bit_cast<std::uint32_t>(p.x),std::bit_cast<std::uint32_t>(p.y),std::bit_cast<std::uint32_t>(p.z)};}
UserPosition position(std::array<std::uint32_t,3> p){return {std::bit_cast<float>(p[0]),std::bit_cast<float>(p[1]),std::bit_cast<float>(p[2])};}
struct Services:MobAiReturnServices,MobAiStateServices{
    MobRuntime& mob;MobAiReturnDefinition& definition;const MapDefinition& map;unsigned mutation=0;std::uint32_t now=0;bool success=false,route=false;std::string trace;
    Services(MobRuntime& m,MobAiReturnDefinition& d,const MapDefinition& terrain):mob(m),definition(d),map(terrain){}
    void emit(char tag,std::initializer_list<std::uint32_t> values={}){if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;for(auto value:values){if(!first)trace+=',';trace+=std::to_string(value);first=false;}}
    std::uint32_t tick() override {emit('T');if(mutation==3)mob.next_chase=now+1;if(mutation==4){definition.chase_step=9;definition.size=2;}return now;}
    void reset_attack(MobRuntime& m) override{emit('A');reset_mob_attack(m);if(mutation==1)m.respawn_position={7,8,9};}
    void dispatch(MobRuntime& m,std::int32_t event,std::uint32_t arg) override {emit('D',{static_cast<std::uint32_t>(event),arg});if(route)dispatch_mob_ai_event(m,event,arg,*this);}
    bool move(MobRuntime& m,UserPosition at,std::int32_t mode) override {auto b=bits(at);emit('M',{b[0],b[1],b[2],static_cast<std::uint32_t>(mode)});if(success)m.position=at;if(mutation==2){m.position={11,5,12};definition.chase_time=333;}return success;}
    void update(MobRuntime& m,MobAiState state) override {check(state==MobAiState::return_home);update_mob_ai_return_home(m,definition,map,*this);}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected stack diagnostic");}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::runtime_error("Unexpected Lua");}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::array<MapDefinition,4> maps;std::string line;unsigned cases=0;
 while(std::getline(file,line)){
    if(line.empty()||line[0]=='#')continue;std::istringstream row(line);char kind;row>>kind;
    if(kind=='N'){unsigned id;std::string hex;row>>id>>maps[id].size>>hex;for(std::size_t i=0;i<hex.size();i+=2)maps[id].tiles.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));continue;}
    unsigned id,map_id,mutation,success,stop,sleep,stun,step,size,mode,trail_count;std::uint32_t now,deadline,speed,interval;
    std::array<std::uint32_t,3> initial{},home{},final{},final_home{};MapTile point;
    row>>id>>map_id>>mutation>>now>>success>>deadline>>speed>>stop>>sleep>>stun>>step>>size>>interval;for(auto& v:initial)row>>v;for(auto& v:home)row>>v;row>>mode>>point.x>>point.z>>trail_count;
    std::vector<MapTile> trail(trail_count);for(auto& p:trail)row>>p.x>>p.z;
    std::string trace,cursor,chain,final_trail;std::uint32_t final_deadline,final_speed,damage_resets,final_time;unsigned final_step,final_size,found;
    row>>trace;for(auto& v:final)row>>v;for(auto& v:final_home)row>>v;row>>final_deadline>>final_speed>>damage_resets>>final_time>>final_step>>final_size>>found>>cursor>>chain>>final_trail;check(!row.fail());
    MobDamageFixture heap;MobRuntime m;MobRuntimeTemplate base;base.level=1;m.definition=&base;m.damage=std::make_unique<MobDamageLedger>(heap.pool);m.damage->fair_combat=false;m.damage->add(1,42,100,0,0);
    m.position=position(initial);m.respawn_position=position(home);m.next_chase=deadline;m.move_speed=std::bit_cast<std::int32_t>(speed);m.stop=static_cast<std::int32_t>(stop);m.sleep=static_cast<std::int32_t>(sleep);m.stun=static_cast<std::int32_t>(stun);m.state_stack=std::vector<std::int32_t>{0,3};
    if(mode)m.path.set_path(point);for(auto p:trail)m.trail.add(maps[map_id],p);
    MobAiReturnDefinition d{static_cast<std::uint8_t>(step),static_cast<std::uint8_t>(size),interval};Services services(m,d,maps[map_id]);services.mutation=mutation;services.now=now;services.success=success!=0;
    check(poll_mob_ai_stack(m,services));
    std::string actual_cursor="-";if(auto p=m.path.current())actual_cursor=points(std::span(&*p,1));
    const auto nodes=m.path.nodes();
    bool ok=bits(m.position)==final && bits(m.respawn_position)==final_home && m.next_chase==final_deadline && static_cast<std::uint32_t>(m.move_speed)==final_speed && m.damage->records().empty()==(damage_resets!=0) && d.chase_time==final_time && d.chase_step==final_step && d.size==final_size && m.path.found()==(found!=0) && actual_cursor==cursor && points(nodes)==chain && points(m.trail.nodes(),true)==final_trail && services.trace==trace;
    if(!ok)throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace+" deadline="+std::to_string(m.next_chase)+"/"+std::to_string(final_deadline)+" path="+points(nodes)+"/"+chain);
    if(damage_resets)check(m.damage->fair_combat && heap.pool.available().size()==1);++cases;
 }
 check(cases==1536);
 // Arrival composes the real stack reset event and real pooled damage release.
 MobDamageFixture heap;MobRuntime m;MobRuntimeTemplate base;base.level=1;m.definition=&base;m.damage=std::make_unique<MobDamageLedger>(heap.pool);m.damage->add(1,42,100,0,0);m.state_stack=std::vector<std::int32_t>{0,3};m.current_attack=7;m.combat_timeout=123;m.only_lua_attack=1;
 MobAiReturnDefinition definition;Services world(m,definition,maps[0]);world.route=true;check(poll_mob_ai_stack(m,world));check(m.state_stack->back()==0 && m.current_attack==0 && m.combat_timeout==0 && m.only_lua_attack==0 && m.damage->records().empty());
 std::cout<<cases<<" original return-home cases; real map/path/trail/damage and stack composition passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
