#include "eternity/game/mob_applied_skills_application.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool b){if(!b)throw std::runtime_error("Mob skill admission mismatch");}
std::uint32_t bits(std::int32_t n){return std::bit_cast<std::uint32_t>(n);}
std::string hex(const std::vector<std::uint32_t>& values){constexpr char digits[]="0123456789abcdef";std::string out;for(auto value:values)for(unsigned i=0;i<4;++i){const auto byte=(value>>(i*8))&255;out+=digits[byte>>4];out+=digits[byte&15];}return out;}
std::vector<std::uint32_t> decode(const std::string& text){require(text.size()%8==0);std::vector<std::uint32_t> out;for(std::size_t i=0;i<text.size();i+=8){std::uint32_t word=0;for(unsigned b=0;b<4;++b)word|=static_cast<std::uint32_t>(std::stoul(text.substr(i+b*2,2),nullptr,16))<<(b*8);out.push_back(word);}return out;}
std::string seq(const std::vector<std::uint32_t>& values){std::ostringstream out;for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}return values.empty()?"-":out.str();}
std::vector<std::uint32_t> core(const MobRuntime& m){
    std::vector<std::uint32_t> out{bits(m.wisdom),bits(m.dexterity),bits(m.luck),bits(m.attack_speed),bits(m.move_speed)};
    for(const auto& a:m.additions)out.insert(out.end(),{bits(a.accuracy),bits(a.attack),bits(a.evasion),bits(a.defense)});
    for(auto v:m.attack_disabled)out.push_back(bits(v));
    out.insert(out.end(),{bits(m.stun),bits(m.stop),bits(m.sleep),bits(m.attribute),m.vision_fixed_user,m.hp,bits(m.status),m.target_id,m.next_move,bits(m.target_kind),bits(m.lua_ai)});return out;
}
void assign(MobRuntime& m,const std::vector<std::uint32_t>& v){
    require(v.size()==31);std::size_t at=0;auto next=[&]{return std::bit_cast<std::int32_t>(v[at++]);};
    m.wisdom=next();m.dexterity=next();m.luck=next();m.attack_speed=next();m.move_speed=next();
    for(auto& a:m.additions){a.accuracy=next();a.attack=next();a.evasion=next();a.defense=next();}for(auto& n:m.attack_disabled)n=next();
    m.stun=next();m.stop=next();m.sleep=next();m.attribute=next();m.vision_fixed_user=v[at++];m.hp=v[at++];m.status=next();m.target_id=v[at++];m.next_move=v[at++];m.target_kind=next();m.lua_ai=next();
}
constexpr std::array<std::array<std::uint32_t,5>,17> specs{{
    {100,1,3,0x40,1},{100,1,3,0x41,3},{100,2,3,0x41,4294967},{100,1,3,0x42,2},
    {100,4,3,0x43,2},{100,4,3,0x48,0},{101,1,3,0x44,1},{101,2,3,0x48,1},
    {102,1,2,0x40,1},{103,0,3,0x3e,1},{103,3,3,0x43,1},{104,255,3,0x42,1},
    {104,1,3,0x44,1},{105,1,3,0x48,1},{105,1,3,0x48,9},{106,1,3,0x40,1},{106,1,3,0x40,2}}};
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,MobAppliedSkillApplicationServices,MobAppliedSkillEffectServices {
    MobRuntime& mob;std::atomic<std::uint32_t>& counter;MobAppliedSkills* owner=nullptr;MobAppliedSkillPool* pool=nullptr;
    unsigned scenario=0,incoming=0,ticks=0;std::uint32_t clock=0;bool fail=false;
    std::array<SkillDefinition,17> definitions{};std::array<std::string,17> names;
    std::vector<MobAppliedSkill*> objects;std::vector<std::string> events;
    Services(MobRuntime& m,std::atomic<std::uint32_t>& c):mob(m),counter(c){
        for(std::size_t i=0;i<specs.size();++i){const auto& p=specs[i];auto& d=definitions[i];
            d.id=static_cast<std::uint16_t>(p[0]);d.level=static_cast<std::uint8_t>(p[1]);d.effect=static_cast<std::uint8_t>(p[2]);d.detail=static_cast<std::uint16_t>(p[3]);d.keep_time_seconds=p[4];names[i]="Def"+std::to_string(i);
            d.abilities={SkillAbility{7,1},SkillAbility{8,2},SkillAbility{9,3}};
            if(i==9)d.abilities={SkillAbility{0,0},SkillAbility{16,65535},SkillAbility{32,65535}};
            if(i==10)d.abilities={SkillAbility{20,1},SkillAbility{27,32768},SkillAbility{31,65535}};
        }
    }
    std::unique_ptr<MobAppliedSkill> construct()override{
        if(fail)return nullptr;auto p=std::make_unique<MobAppliedSkill>();
        p->instance_id=p->skill_id=p->level=p->number=p->attack_tick=p->reset_tick=p->end_tick=p->previous_tick=p->target_type=p->target_id=0xa5a5a5a5U;
        objects.push_back(p.get());return p;
    }
    std::uint32_t index(const MobAppliedSkill* p)const{const auto it=std::find(objects.begin(),objects.end(),p);require(it!=objects.end());return static_cast<std::uint32_t>(it-objects.begin());}
    std::size_t definition_index(const MobAppliedSkill& s)const{if(!s.definition)return 255;const auto it=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){return &d==s.definition;});require(it!=definitions.end());return static_cast<std::size_t>(it-definitions.begin());}
    std::string pointers(const std::vector<MobAppliedSkill*>& records)const{std::vector<std::uint32_t> ids;for(auto* p:records)ids.push_back(index(p));return seq(ids);}
    std::string snapshot()const{
        std::ostringstream out;out<<hex(core(mob))<<'|'<<counter.load()<<'|'<<pool->construction_attempts()<<'|'<<pointers(owner->records())<<'|'<<pointers(pool->available())<<'|';
        for(std::size_t i=0;i<objects.size();++i){if(i)out<<';';const auto& s=*objects[i];out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick<<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id<<':'<<definition_index(s);}
        if(objects.empty())out<<'-';std::vector<std::uint32_t> durations;for(const auto& d:definitions)durations.push_back(d.keep_time_seconds);out<<'|'<<seq(durations);return out.str();
    }
    void event(const std::string& tag){events.push_back(tag+':'+snapshot());}
    std::uint32_t tick()noexcept override{event("T");if(scenario&1)++definitions[incoming].keep_time_seconds;return clock+(ticks++)*17U;}
    void log(const MobAppliedSkill& s,std::uint8_t action){
        const auto active=owner->records();require((std::find(active.begin(),active.end(),&s)!=active.end())==(action==6));
        require(s.name==names[definition_index(s)]);event("L"+std::to_string(action)+","+std::to_string(index(&s)));
    }
    void removed(const MobAppliedSkill& s,std::uint8_t action)noexcept override{require(action==6);log(s,action);}
    void added(const MobAppliedSkill& s,std::uint8_t action)noexcept override{require(action==5);log(s,action);}
    void rebuild_attack(MobRuntime&)noexcept override{event("B");if(scenario&2)mob.lua_ai^=1;}
    void send_speed(MobRuntime&)noexcept override{event("S");if(scenario&2)mob.lua_ai^=1;}
    void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&)noexcept override{std::terminate();}
};
}
int main(int argc,char** argv){
    try{
        require(argc==2);std::ifstream input(argv[1]);require(input.good());std::string line;unsigned cases=0,operations=0;
        std::unique_ptr<MobRuntime> mob;std::unique_ptr<Services> services;std::unique_ptr<MobAppliedSkillPool> pool;std::unique_ptr<MobAppliedSkills> owner;
        MobRuntimeTemplate base;base.attribute=7;MobRespawn respawn;respawn.both=true;std::atomic<std::uint32_t> counter{0};
        while(std::getline(input,line)){
            if(line.empty() || line[0]=='#')continue;std::istringstream row(line);char op;row>>op;
            if(op=='S'){
                unsigned scenario,flags,stack;std::uint32_t clock,initial_counter;std::string initial;row>>scenario>>flags>>stack>>clock>>initial_counter>>initial;require(!row.fail());
                owner.reset();pool.reset();services.reset();mob=std::make_unique<MobRuntime>();assign(*mob,decode(initial));mob->definition=&base;mob->respawn=(flags&2)?&respawn:nullptr;
                if(stack==2)mob->state_stack=std::vector<std::int32_t>{};else if(stack==3)mob->state_stack=std::vector<std::int32_t>{2};
                counter.store(initial_counter);services=std::make_unique<Services>(*mob,counter);services->scenario=scenario;services->clock=clock;
                pool=std::make_unique<MobAppliedSkillPool>(*services);owner=std::make_unique<MobAppliedSkills>(*pool,*services);services->pool=pool.get();services->owner=owner.get();++cases;continue;
            }
            services->events.clear();
            if(op=='A'){
                unsigned definition,fail;std::uint32_t kind,id;row>>definition>>kind>>id>>fail;services->incoming=definition;services->fail=fail!=0;
                owner->apply(*mob,{services->definitions[definition],services->names[definition],kind,id},7,counter,*services,*services);
            }else{require(op=='C');owner->clear({mob->lua_ai!=0,mob->respawn!=nullptr,mob->respawn && mob->respawn->both});}
            std::string expected_events,expected;row>>expected_events>>expected;require(!row.fail());std::string events;
            for(const auto& event:services->events){if(!events.empty())events+=';';events+=event;}if(events.empty())events="-";
            if(events!=expected_events || services->snapshot()!=expected){std::cerr<<"case "<<cases-1<<" operation "<<operations<<" mismatch\nexpected events "<<expected_events<<"\nactual events "<<events<<"\nexpected state "<<expected<<"\nactual state "<<services->snapshot()<<'\n';return 1;}++operations;
        }
        require(cases==64 && operations==1536);std::cout<<cases<<" original admission scenarios / "<<operations<<" operations matched\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
