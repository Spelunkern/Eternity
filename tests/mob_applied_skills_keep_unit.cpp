#include "eternity/game/mob_applied_skills_time_damage.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value){if(!value)throw std::runtime_error("Mob keep/sleep ownership mismatch");}
std::uint32_t bits(std::int32_t n){return std::bit_cast<std::uint32_t>(n);}
std::string seq(const std::vector<std::uint32_t>& values){std::ostringstream out;for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}return values.empty()?"-":out.str();}
std::string hex(std::span<const std::uint8_t> bytes){constexpr char digits[]="0123456789abcdef";std::string out;for(auto byte:bytes){out+=digits[byte>>4];out+=digits[byte&15];}return out;}
std::string packed(std::initializer_list<std::uint32_t> values){std::vector<std::uint8_t> bytes;for(auto value:values)for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));return hex(bytes);}
const std::array<std::vector<std::uint16_t>,8> layouts{{{}, {0x3d}, {0x42}, {99,0x3d,0x42}, {0x3d,0x42,0x3d}, {0x42,0x3d,99,0x42,0x3d}, {99,99}, {0x3d,0x3d,0x42,99}}};
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,MobAppliedSkillDamageServices,MobAppliedSkillEffectServices {
    MobRuntime& mob;MobAppliedSkills* owner=nullptr;MobAppliedSkillPool* pool=nullptr;
    std::vector<MobAppliedSkill*> objects;std::array<SkillDefinition,5> definitions{};
    MobAppliedSkillTimeDamageDefinition calculation;unsigned mode=0,hits=0,current=0;
    std::vector<std::string> events;
    explicit Services(MobRuntime& m):mob(m){}
    std::unique_ptr<MobAppliedSkill> construct()override{auto p=std::make_unique<MobAppliedSkill>();objects.push_back(p.get());return p;}
    std::uint32_t index(const MobAppliedSkill* p)const{const auto it=std::find(objects.begin(),objects.end(),p);require(it!=objects.end());return static_cast<std::uint32_t>(it-objects.begin());}
    std::string pointers(const std::vector<MobAppliedSkill*>& values)const{std::vector<std::uint32_t> ids;for(auto* p:values)ids.push_back(index(p));return seq(ids);}
    std::string snapshot()const{
        std::ostringstream out;
        out<<seq({mob.id,mob.hp,bits(mob.sleep),bits(mob.status),static_cast<std::uint32_t>(mob.state_stack.has_value()),static_cast<std::uint32_t>(mob.state_stack?mob.state_stack->size():0),bits(mob.lua_ai)})
           <<'|'<<pointers(owner->records())<<'|'<<pointers(pool->available())<<'|';
        for(std::size_t i=0;i<objects.size();++i){if(i)out<<';';const auto& s=*objects[i];out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick<<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id;}
        if(objects.empty())out<<'-';std::vector<std::uint8_t> names(mob.previous_attack_user.begin(),mob.previous_attack_user.end());names.insert(names.end(),mob.previous_attack_skill.begin(),mob.previous_attack_skill.end());out<<'|'<<hex(names);return out.str();
    }
    void event(const std::string& tag){events.push_back(tag+':'+snapshot());}
    void removed(const MobAppliedSkill& skill,std::uint8_t action)noexcept override{require(action==6);event("L"+std::to_string(index(&skill)));}
    std::uint32_t tick()noexcept override{std::terminate();}
    MobAppliedSkillDamage time_damage(MobRuntime& m,const SkillDefinition& definition,std::uint32_t elapsed)noexcept override{
        const auto it=std::find_if(objects.begin(),objects.end(),[&](const auto* s){return s->definition==&definition;});require(it!=objects.end());current=index(*it);
        event("T"+std::to_string(current)+packed({elapsed}));
        return calculate_mob_applied_skill_time_damage(calculation,project_mob_applied_skill_time_damage(m,*m.definition),elapsed);
    }
    bool damage_periodic(MobRuntime&,std::uint32_t kind,std::uint32_t id,MobAppliedSkillDamage& values)noexcept override{
        event("P"+std::to_string(current)+packed({kind,id,bits(values[0]),bits(values[1]),bits(values[2])}));
        if(mode==3 && hits==0){mob.state_stack.reset();mob.status=2;objects[current]->skill_id=0xffff1234;objects[current]->level=0xffffff80;objects[current]->target_type=9;objects[current]->target_id=123;}
        const bool killed=(mode==1 || mode==2) && hits==0;++hits;return killed;
    }
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t kind,std::uint32_t id)noexcept override{
        event("X"+std::to_string(current)+packed({kind,id}));if(mode==1){mob.state_stack.reset();mob.status=2;}
        owner->clear({mob.lua_ai!=0,mob.respawn!=nullptr,mob.respawn && mob.respawn->both});
    }
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket& packet,float radius,std::uint8_t priority)noexcept override{require(radius==60 && priority==6);event("V"+hex(packet));}
    void send_speed(MobRuntime& m)noexcept override{require(m.sleep==0);event("S");}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&)noexcept override{std::terminate();}
};
}
int main(int argc,char** argv){
    try{
        require(argc==2);std::ifstream input(argv[1]);require(input.good());unsigned cases=0;
        for(std::string line;std::getline(input,line);){
            if(line.empty() || line[0]=='#')continue;
            char kind;unsigned case_id,stack,mode,flags;std::uint32_t now,sleep;std::string expected_events,expected_final;
            std::istringstream row(line);row>>kind>>case_id>>now>>sleep>>stack>>mode>>flags>>expected_events>>expected_final;require(!row.fail());
            MobRuntime mob;MobRuntimeTemplate maximum;maximum.hp=2000+case_id;maximum.sp=200;maximum.mp=150;mob.definition=&maximum;
            mob.id=7;mob.hp=1000+case_id;mob.sp=100;mob.mp=50;mob.sleep=std::bit_cast<std::int32_t>(sleep);mob.status=(stack==1 || stack==2)?2:0;
            if(stack==2)mob.state_stack=std::vector<std::int32_t>{};else if(stack==3)mob.state_stack=std::vector<std::int32_t>{2};
            MobRespawn respawn;respawn.both=true;mob.respawn=(flags&2)?&respawn:nullptr;mob.lua_ai=static_cast<std::int32_t>(flags&1);
            static std::uint64_t zone_identity;mob.zone=(case_id%2)?reinterpret_cast<ZoneMobs*>(&zone_identity):nullptr;
            mob.previous_attack_user.fill('U');mob.previous_attack_skill.fill('S');
            const std::string user="BeforeUser",skill_name="BeforeSkill";
            std::copy(user.begin(),user.end(),mob.previous_attack_user.begin());mob.previous_attack_user[user.size()]=0;
            std::copy(skill_name.begin(),skill_name.end(),mob.previous_attack_skill.begin());mob.previous_attack_skill[skill_name.size()]=0;
            Services services(mob);services.mode=mode;services.calculation={static_cast<std::uint8_t>(case_id%6),{1,5,33}};
            MobAppliedSkillPool pool(services);MobAppliedSkills owner(pool,services);services.pool=&pool;services.owner=&owner;
            const auto& layout=layouts[case_id%8];
            for(std::size_t i=0;i<layout.size();++i){
                auto* s=owner.append_from_pool();require(s);const auto n=static_cast<std::uint32_t>(i);
                s->instance_id=n+1;s->skill_id=100+n;s->level=n%3+1;s->number=50+n;s->attack_tick=60+n;s->reset_tick=70+n;s->end_tick=0;s->previous_tick=100*n+case_id%17;s->target_type=1;s->target_id=42+n;
                services.definitions[i].detail=layout[i];s->definition=&services.definitions[i];s->name="Skill"+std::to_string(i);
            }
            if(kind=='K')owner.keep(now,mob,services,services);else{require(kind=='S');owner.remove_sleep(mob,services);}
            std::string events;for(const auto& event:services.events){if(!events.empty())events+=';';events+=event;}if(events.empty())events="-";
            if(events!=expected_events || services.snapshot()!=expected_final){std::cerr<<kind<<" case "<<case_id<<" mismatch\nexpected events "<<expected_events<<"\nactual events "<<events<<"\nexpected state "<<expected_final<<"\nactual state "<<services.snapshot()<<'\n';return 1;}
            ++cases;
        }
        require(cases==640);std::cout<<cases<<" original keep/sleep traces through live native owners matched\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
