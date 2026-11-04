#include "eternity/game/mob_ai_states_move.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include "mob_damage_fixture.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok){if(!ok)throw std::runtime_error("Original move-state mismatch");}
std::string points(std::span<const MapTile> values,bool reverse=false){std::string result;for(std::size_t i=0;i<values.size();++i){auto p=values[reverse?values.size()-1-i:i];if(!result.empty())result+=',';result+=std::to_string(static_cast<std::uint32_t>(p.x))+','+std::to_string(static_cast<std::uint32_t>(p.z));}return result.empty()?"-":result;}
std::array<std::uint32_t,3> bits(UserPosition p){return {std::bit_cast<std::uint32_t>(p.x),std::bit_cast<std::uint32_t>(p.y),std::bit_cast<std::uint32_t>(p.z)};}
UserPosition position(std::array<std::uint32_t,3> p){return {std::bit_cast<float>(p[0]),std::bit_cast<float>(p[1]),std::bit_cast<float>(p[2])};}
struct Services:MobAiMoveServices,MobAiStateServices{
    MobRuntime& mob;MobAiMoveDefinition& definition;const MapDefinition& map;unsigned mutation=0,clocks=0;std::uint32_t now=0,random_value=0;bool success=false,found=false,route=false;std::string trace;
    Services(MobRuntime& m,MobAiMoveDefinition& d,const MapDefinition& terrain):mob(m),definition(d),map(terrain){}
    void emit(char tag,std::initializer_list<std::uint32_t> values={}){if(!trace.empty())trace+=';';trace+=tag;trace+=':';bool first=true;for(auto value:values){if(!first)trace+=',';trace+=std::to_string(value);first=false;}}
    std::uint32_t tick() override {const auto index=clocks++;const auto value=now+(index?17U:0U);emit('T',{value});if(index==0&&mutation==3)mob.next_chase=now+1;if(index==0&&mutation==4){definition.normal_step=9;definition.size=2;}return value;}
    bool find_target(MobRuntime& m) override {emit('F');if(mutation==2){m.move_mode=1;m.target_id=0xabcdef01;m.next_chase=now+1;}return found;}
    std::int32_t random() override {emit('Q',{random_value});if(mutation==5){mob.move_target={1,0,1};definition.normal_step=0;}return static_cast<std::int32_t>(random_value);}
    void reset_attack(MobRuntime& m) override{emit('A');reset_mob_attack(m);}
    void dispatch(MobRuntime& m,std::int32_t event,std::uint32_t arg) override {emit('D',{static_cast<std::uint32_t>(event),arg});if(mutation==1){m.position={11,4,12};m.move_mode=3;}if(route)dispatch_mob_ai_event(m,event,arg,*this);}
    bool move(MobRuntime& m,UserPosition at,std::int32_t mode) override {auto b=bits(at);emit('M',{b[0],b[1],b[2],static_cast<std::uint32_t>(mode)});if(success)m.position=at;if(mutation==2){m.position={11,5,12};definition.normal_time=333;}return success;}
    void update(MobRuntime& m,MobAiState state) override {check(state==MobAiState::move);update_mob_ai_move(m,definition,map,*this);}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected stack diagnostic");}
    void lua_call(MobRuntime& m,std::string_view name,std::uint32_t at,std::optional<std::uint32_t> arg) override {check(name=="OnMoveEnd"&&!arg);emit('L',{at});if(mutation==6){m.function_type=111;m.move_mode=4;}}
};
int main(int argc,char**argv){try{
 check(argc==2);std::ifstream file(argv[1]);check(file.good());std::array<MapDefinition,4> maps;std::string line;unsigned cases=0;
 while(std::getline(file,line)){
    if(line.empty()||line[0]=='#')continue;std::istringstream row(line);char kind;row>>kind;
    if(kind=='N'){unsigned id;std::string hex;row>>id>>maps[id].size>>hex;for(std::size_t i=0;i<hex.size();i+=2)maps[id].tiles.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));continue;}
    unsigned id,map_id,mutation,success,stop,sleep,stun,step,size,path_mode,trail_count,found;std::uint32_t now,deadline,speed,interval,mode,lua,random;
    std::array<std::uint32_t,3> initial{},target{},final{},final_home{},final_target{};MapTile point;
    row>>id>>map_id>>mutation>>now>>success>>deadline>>speed>>stop>>sleep>>stun>>step>>size>>interval>>mode>>lua>>found>>random;for(auto& v:initial)row>>v;for(auto& v:target)row>>v;row>>path_mode>>point.x>>point.z>>trail_count;
    std::vector<MapTile> trail(trail_count);for(auto& p:trail)row>>p.x>>p.z;
    std::string trace,cursor,chain,final_trail;std::uint32_t final_deadline,final_speed,damage_resets,final_time,final_mode,final_function,final_id;unsigned final_step,final_size,path_found;
    row>>trace;for(auto& v:final)row>>v;for(auto& v:final_home)row>>v;for(auto& v:final_target)row>>v;row>>final_deadline>>final_speed>>damage_resets>>final_time>>final_step>>final_size>>path_found>>final_mode>>final_function>>final_id>>cursor>>chain>>final_trail;check(!row.fail());
    MobDamageFixture heap;MobRuntime m;MobRuntimeTemplate base;base.level=1;m.definition=&base;m.damage=std::make_unique<MobDamageLedger>(heap.pool);m.damage->fair_combat=false;m.damage->add(1,42,100,0,0);
    m.position=position(initial);m.respawn_position={1,2,3};m.move_target=position(target);m.next_chase=deadline;m.move_speed=std::bit_cast<std::int32_t>(speed);m.stop=static_cast<std::int32_t>(stop);m.sleep=static_cast<std::int32_t>(sleep);m.stun=static_cast<std::int32_t>(stun);m.state_stack=std::vector<std::int32_t>{0,4};m.move_mode=std::bit_cast<std::int32_t>(mode);m.lua_ai=std::bit_cast<std::int32_t>(lua);m.function_type=99;m.target_id=0x80000000U+id;
    if(path_mode)m.path.set_path(point);for(auto p:trail)m.trail.add(maps[map_id],p);
    MobAiMoveDefinition d{static_cast<std::uint8_t>(step),static_cast<std::uint8_t>(size),interval};Services services(m,d,maps[map_id]);services.mutation=mutation;services.now=now;services.success=success!=0;services.found=found!=0;services.random_value=random;
    check(poll_mob_ai_stack(m,services));
    std::string actual_cursor="-";if(auto p=m.path.current())actual_cursor=points(std::span(&*p,1));const auto nodes=m.path.nodes();
    bool ok=bits(m.position)==final && bits(m.respawn_position)==final_home && bits(m.move_target)==final_target && m.next_chase==final_deadline && static_cast<std::uint32_t>(m.move_speed)==final_speed && m.damage->records().empty()==(damage_resets!=0) && d.normal_time==final_time && d.normal_step==final_step && d.size==final_size && m.path.found()==(path_found!=0) && static_cast<std::uint32_t>(m.move_mode)==final_mode && m.function_type==final_function && m.target_id==final_id && actual_cursor==cursor && points(nodes)==chain && points(m.trail.nodes(),true)==final_trail && services.trace==trace;
    if(!ok)throw std::runtime_error("Case "+std::to_string(id)+" actual="+services.trace+" expected="+trace+" deadline="+std::to_string(m.next_chase)+"/"+std::to_string(final_deadline)+" path="+points(nodes)+"/"+chain);
    if(damage_resets)check(m.damage->fair_combat&&heap.pool.available().size()==1);++cases;
 }
 check(cases==1536);
 // Actual event 6 clears move_mode before completion: the pre-event mode must
 // still select damage cleanup (3) versus Lua notification (all other modes).
 for(auto mode:{1,3}){
    MobDamageFixture heap;MobRuntime m;MobRuntimeTemplate base;base.level=1;m.definition=&base;m.damage=std::make_unique<MobDamageLedger>(heap.pool);m.damage->add(1,42,100,0,0);m.state_stack=std::vector<std::int32_t>{0,4};m.move_mode=mode;m.lua_ai=1;m.function_type=77;
    MobAiMoveDefinition definition;Services world(m,definition,maps[0]);world.route=true;world.random_value=3;
    check(poll_mob_ai_stack(m,world));check(m.state_stack->back()==0&&m.move_mode==0&&m.function_type==77);
    check(m.damage->records().empty()==(mode==3));check((world.trace.find("L:")!=std::string::npos)==(mode!=3));
 }
 std::cout<<cases<<" original WhileMove cases; real map/path/trail/damage and event completion composition passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
