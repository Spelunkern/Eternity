#include "eternity/game/mobs.hpp"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool v,const std::string& message) {if(!v) throw std::runtime_error(message);}
struct Bytes {
    std::vector<std::uint8_t> data;
    void u(std::uint32_t value,unsigned int n=4) {for(unsigned int i=0;i<n;++i) data.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    void f(float v) {u(std::bit_cast<std::uint32_t>(v));}
    void pos(UserPosition p) {f(p.x);f(p.y);f(p.z);}
    std::string hex() const {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto v:data) out<<std::setw(2)<<unsigned(v);return out.str();}
};
std::string snapshot(const MobRuntime& m) {
    Bytes b;b.u(m.id);b.u(m.type,2);b.u(m.hp);b.u(m.sp,2);b.u(m.mp,2);b.u(m.dexterity);b.u(m.wisdom);b.u(m.luck);b.u(m.attribute);b.u(m.status);b.u(m.target_kind);b.u(m.target_id);
    for(std::size_t i=0;i<3;++i) {const auto& a=m.attack[i];b.u(m.attack_disabled[i]);b.u(a.accuracy);b.u(a.attack);b.u(a.evasion);b.u(a.defense);b.u(a.critical_attack);b.u(a.critical_defense);}
    for(const auto& a:m.additions) {b.u(a.accuracy);b.u(a.attack);b.u(a.evasion);b.u(a.defense);}for(auto v:m.attack_times) b.u(v);b.u(m.move_speed);b.u(m.attack_speed);b.u(m.current_attack);
    for(auto v:{m.use_pais,m.attacked,m.in_battle,m.attack_at_moment}) b.u(v,1);b.u(m.battle_type);b.u(m.check_attack_count,1);b.u(m.attacked_count);b.u(m.recover_time);b.u(m.recover_enabled,1);for(auto v:m.multipliers) b.f(v);for(auto v:m.attack_enabled) b.u(v,1);
    for(auto v:{m.next_recover,m.next_help,m.next_attack,m.next_chase,m.next_move,m.next_target,m.next_new_target}) b.u(v);b.u(m.death_remove,1);b.u(m.death_time);b.pos(m.previous_position);b.pos(m.move_direction);b.u(m.previous_move_time);b.f(m.move_range);b.u(m.stun);b.u(m.stop);b.u(m.sleep);
    for(auto v:m.previous_attack_user) b.u(v,1);for(auto v:m.previous_attack_skill) b.u(v,1);b.u(m.lock_on_tick);b.u(m.unbeatable);b.u(m.vision_fixed_user);b.pos(m.position);b.pos(m.respawn_position);b.pos(m.exit_position);b.pos(m.selected_position);
    b.u(m.combat_timeout);b.u(m.lua_ai);b.u(m.retarget_time);b.u(m.lua_attacked_count);b.u(m.only_lua_attack);b.u(m.hold_position);b.u(m.lua_order_time);b.u(m.move_mode);b.pos(m.move_target);b.u(m.function_type);b.u(m.current_time);b.u(m.path.found(),1);b.u(m.cell->x);b.u(m.cell->z);return b.hex();
}
struct Random final:RandomSource {std::int32_t value=0;std::vector<std::string>* events=nullptr;std::int32_t next() override {events->push_back("RANDOM "+std::to_string(value));return value;}};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original mob runtime oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open mob oracle");MobRuntimeTemplates definitions;MobRuntime m;MobRespawn respawn;Random random;std::vector<std::string> events;random.events=&events;
        unsigned int seed=0,ai_ok=0,has_respawn=0,clock=0;std::uint32_t type=0;std::array<std::uint32_t,2> ticks{};std::size_t cases=0,next_event=0,states=0;std::string line;bool initialize_pending=false;
        MobInitPorts ports{random,[&]{const auto t=ticks.at(clock++);events.push_back("TICK "+std::to_string(t));return t;},[&](MobRuntime& mob,std::uint8_t ai){events.push_back("AI "+std::to_string(ai)+' '+std::to_string(ai_ok));mob.status=17;mob.lock_on_tick=0x1234;mob.unbeatable=33;mob.vision_fixed_user=99;return ai_ok!=0;},[&](MobRuntime& mob){events.push_back("LUA");mob.lua_ai=1;mob.only_lua_attack=1;mob.lock_on_tick=0x76543210;mob.unbeatable=5;mob.vision_fixed_user=99;}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                unsigned int scenario=0;row>>scenario>>seed>>type>>has_respawn>>random.value>>ticks[0]>>ticks[1]>>ai_ok;m=MobRuntime{};events.clear();next_event=0;clock=0;initialize_pending=false;const auto bits=std::uint32_t{seed}*0x01010101U;const auto number=std::bit_cast<std::int32_t>(bits);const auto f=std::bit_cast<float>(bits);m.id=bits;m.zone=reinterpret_cast<ZoneMobs*>(static_cast<std::uintptr_t>(bits));m.cell=CellCoordinates{number,number};m.position=m.respawn_position=m.exit_position=m.selected_position=m.move_target={f,f,f};m.combat_timeout=m.retarget_time=m.lua_attacked_count=m.lua_order_time=m.function_type=m.current_time=bits;m.lua_ai=m.only_lua_attack=m.hold_position=m.move_mode=number;m.previous_attack_user.fill(static_cast<std::uint8_t>(seed));m.previous_attack_skill.fill(static_cast<std::uint8_t>(seed));m.path.set_path({12,34});MapDefinition map;map.size=8;map.tiles.resize(8);m.trail.add(map,{1,1});++cases;
            } else if(tag=="ROW") {
                storage::ResultSet table;table.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3"};storage::Row r;std::string value;while(row>>value) r.emplace_back(value=="N"?storage::Cell{}:storage::Cell{value});require(r.size()==13,"Original mob row width differs");const auto id=static_cast<std::uint32_t>(std::stoul(*r.front()));table.rows.push_back(std::move(r));definitions=MobRuntimeTemplates::load(table);m.definition=definitions.find(id);require(m.definition,"Original runtime definition was rejected");initialize_pending=true;
            } else if(tag=="DEF") {
                const auto& d=*m.definition;const std::array<std::uint32_t,13> values{d.id,d.level,d.ai,d.hp,d.sp,d.mp,d.dexterity,d.wisdom,d.luck,d.attribute,d.physical_defense,d.magic_defense,d.shield_defense};for(auto actual:values) {std::uint32_t expected=0;row>>expected;require(actual==expected,"Original mob definition projection differs");}
                initialize_mob(m,type,has_respawn?&respawn:nullptr,ports);require(m.path.nodes().empty() && m.trail.nodes().empty(),"Existing native path/trail owners were not reset");initialize_pending=false;
            } else if(tag=="EVENT") {require(next_event<events.size() && events[next_event]==line.substr(6),"Original mob clock/random/AI call order differs");++next_event;}
            else if(tag=="STATE") {require(!initialize_pending && next_event==events.size(),"Incomplete native mob initialization effects");std::string expected;row>>expected;require(snapshot(m)==expected,"Original mob state/attack bytes differ at case "+std::to_string(cases)+" state "+std::to_string(states));++states;}
            else if(tag=="BIND") {bool bound=false,zone=false;row>>bound>>zone;require(bool(m.respawn)==bound && bool(m.zone)==zone,"Original mob retained/respawn bindings differ");}
            else if(tag=="ATTACK") {std::uint32_t scale=0;row>>m.wisdom>>m.luck>>scale;m.multipliers[2]=std::bit_cast<float>(scale);for(auto& a:m.additions) row>>a.accuracy>>a.attack>>a.evasion>>a.defense;for(auto& v:m.attack_disabled) row>>v;rebuild_mob_attack(m);}
            else if(tag=="RESET_ATTACK") reset_mob_attack(m);else if(tag!="END") throw std::runtime_error("Unknown mob runtime row");
        }
        require(cases==384 && states==1152,"Original mob runtime coverage changed");require(!definitions.find(0) && !definitions.find(65536),"Invalid original mob lookup changed");std::cout<<cases<<" original mob definition/init scenarios and "<<states<<" initialization/attack snapshots passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
