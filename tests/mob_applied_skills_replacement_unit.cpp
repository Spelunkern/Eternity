#include "eternity/game/mob_applied_skills_application.hpp"
#include "eternity/game/mob_applied_skills_time_damage.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool b){if(!b)throw std::runtime_error("Mob replacement composition mismatch");}
std::uint32_t bits(std::int32_t n){return std::bit_cast<std::uint32_t>(n);}
std::string hex(std::span<const std::uint8_t> bytes){constexpr char d[]="0123456789abcdef";std::string out;for(auto b:bytes){out+=d[b>>4];out+=d[b&15];}return out;}
void put(std::vector<std::uint8_t>& out,std::uint32_t v){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(v>>(i*8)));}
std::string packed(std::initializer_list<std::uint32_t> words){std::vector<std::uint8_t> out;for(auto n:words)put(out,n);return hex(out);}
std::vector<std::uint32_t> decode(const std::string& text){require(text.size()%8==0);std::vector<std::uint32_t> out;for(std::size_t i=0;i<text.size();i+=8){std::uint32_t word=0;for(unsigned b=0;b<4;++b)word|=static_cast<std::uint32_t>(std::stoul(text.substr(i+b*2,2),nullptr,16))<<(b*8);out.push_back(word);}return out;}
std::string seq(const std::vector<std::uint32_t>& values){std::ostringstream out;for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}return values.empty()?"-":out.str();}
ZoneMobs* zone_tag(){static std::uint64_t identity;return reinterpret_cast<ZoneMobs*>(&identity);}
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,MobAppliedSkillApplicationServices,MobAppliedSkillEffectServices,MobAppliedSkillDamageServices {
    MobRuntime& mob;std::atomic<std::uint32_t>& counter;MobAppliedSkills* owner=nullptr;MobAppliedSkillPool* pool=nullptr;
    unsigned scenario=0,ticks=0;std::uint32_t clock=0;bool seeding=true;
    MobAppliedSkill* selected=nullptr;std::array<SkillDefinition,5> definitions{};std::array<std::string,5> names;
    MobAppliedSkillTimeDamageDefinition calculation;std::vector<MobAppliedSkill*> objects;std::vector<std::string> events;
    Services(MobRuntime& m,std::atomic<std::uint32_t>& c,unsigned n):mob(m),counter(c),scenario(n){
        constexpr std::array<std::uint16_t,5> ids{200,100,201,100,999};
        for(std::size_t i=0;i<definitions.size();++i){auto& d=definitions[i];d.id=ids[i];d.level=i==3?2:1;d.effect=3;d.keep_time_seconds=i==3?4294967:2;names[i]="Def"+std::to_string(i);}
        definitions[0].detail=0x42;definitions[1].detail=(n&1)?0x3f:0x3d;definitions[2].detail=0x40;definitions[3].detail=(n&8)?0x48:0x40;definitions[4].detail=0x44;
        calculation={static_cast<std::uint8_t>((n/5)%6),{1,3,5}};
    }
    std::unique_ptr<MobAppliedSkill> construct()override{require(seeding);auto p=std::make_unique<MobAppliedSkill>();objects.push_back(p.get());return p;}
    std::uint32_t index(const MobAppliedSkill* p)const{const auto it=std::find(objects.begin(),objects.end(),p);require(it!=objects.end());return static_cast<std::uint32_t>(it-objects.begin());}
    std::uint32_t definition_index(const MobAppliedSkill& s)const{const auto it=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){return &d==s.definition;});require(it!=definitions.end());return static_cast<std::uint32_t>(it-definitions.begin());}
    std::string pointers(const std::vector<MobAppliedSkill*>& records)const{std::vector<std::uint32_t> ids;for(auto* p:records)ids.push_back(index(p));return seq(ids);}
    std::string core()const{
        const std::vector<std::uint32_t> words{mob.id,mob.hp,mob.sp,mob.mp,bits(mob.stun),bits(mob.stop),bits(mob.sleep),bits(mob.attribute),mob.vision_fixed_user,bits(mob.status),mob.next_move,bits(mob.target_kind),mob.target_id,bits(mob.lua_ai),static_cast<std::uint32_t>(mob.zone!=nullptr),static_cast<std::uint32_t>(mob.state_stack.has_value()),static_cast<std::uint32_t>(mob.state_stack?mob.state_stack->size():0),static_cast<std::uint32_t>(mob.state_stack && !mob.state_stack->empty()?mob.state_stack->back():0)};
        std::vector<std::uint8_t> bytes;for(auto n:words)put(bytes,n);return hex(bytes);
    }
    std::string snapshot()const{
        std::ostringstream out;out<<core()<<'|'<<counter.load()<<'|'<<pool->construction_attempts()<<'|'<<pointers(owner->records())<<'|'<<pointers(pool->available())<<'|';
        for(std::size_t i=0;i<objects.size();++i){if(i)out<<';';const auto& s=*objects[i];out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick<<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id<<':'<<definition_index(s);}
        std::vector<std::uint8_t> bytes(mob.previous_attack_user.begin(),mob.previous_attack_user.end());bytes.insert(bytes.end(),mob.previous_attack_skill.begin(),mob.previous_attack_skill.end());out<<'|'<<hex(bytes);return out.str();
    }
    void event(const std::string& tag){events.push_back(tag+':'+snapshot());}
    void detached()const{const auto active=owner->records(),free=pool->available();require(std::find(active.begin(),active.end(),selected)==active.end() && std::find(free.begin(),free.end(),selected)==free.end());}
    std::uint32_t tick()noexcept override{event("T");return clock+17U*ticks++;}
    void log(const MobAppliedSkill& s,std::uint8_t action){const auto active=owner->records();require((std::find(active.begin(),active.end(),&s)!=active.end())==(action==6));require(s.name==names[definition_index(s)]);event("L"+std::to_string(action)+","+std::to_string(index(&s)));}
    void removed(const MobAppliedSkill& s,std::uint8_t action)noexcept override{require(action==6);log(s,action);}
    void added(const MobAppliedSkill& s,std::uint8_t action)noexcept override{require(action==5);log(s,action);}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{event("S");}
    void finish_damage_skill(MobRuntime& m,MobAppliedSkill& s,const SkillDefinition& d)noexcept override{require(&s==selected);detached();event("R");finish_mob_applied_skill_damage(m,s,d,*this);}
    MobAppliedSkillDamage time_damage(MobRuntime& m,const SkillDefinition&,std::uint32_t elapsed)noexcept override{detached();event("Q"+packed({elapsed}));return calculate_mob_applied_skill_time_damage(calculation,project_mob_applied_skill_time_damage(m,*m.definition),elapsed);}
    bool mutate(){
        if(scenario&8){mob.lua_ai^=1;mob.zone=mob.zone?nullptr:zone_tag();selected->skill_id=0xffff0abc;selected->level=0x1234;selected->target_type=5;selected->target_id=0x87654321;}
        const bool killed=(scenario&2)!=0;if(killed){mob.hp=0;mob.state_stack.reset();mob.status=2;}return killed;
    }
    bool damage_periodic(MobRuntime&,std::uint32_t kind,std::uint32_t id,MobAppliedSkillDamage& values)noexcept override{
        detached();event("P"+packed({kind,id,bits(values[0]),bits(values[1]),bits(values[2])}));
        if(scenario&8){values[0]=std::bit_cast<std::int32_t>(bits(values[0])^0xffffU);values[1]=std::bit_cast<std::int32_t>(bits(values[1])^0x80000000U);values[2]=std::bit_cast<std::int32_t>(bits(values[2])^0xffff0000U);}return mutate();
    }
    bool damage_hp(MobRuntime&,std::uint32_t kind,std::uint32_t id,std::uint32_t hp)noexcept override{detached();event("H"+packed({kind,id,hp}));return mutate();}
    void death(MobRuntime& m,std::uint32_t kind,std::uint32_t id)noexcept override{
        detached();event("X"+packed({kind,id}));owner->clear({m.lua_ai!=0,m.respawn!=nullptr,m.respawn && m.respawn->both});detached();
        if(scenario&4)owner->apply(m,{definitions[4],names[4],2,777},7,counter,*this,*this);
        detached();
    }
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket& p,float radius,std::uint8_t priority)noexcept override{detached();require(radius==60 && priority==6);event("V"+hex(p));}
};
}
int main(int argc,char** argv){
    try{
        require(argc==2);std::ifstream input(argv[1]);require(input.good());unsigned count=0;
        constexpr std::array<std::array<unsigned,3>,3> orders{{{1,0,2},{0,1,2},{0,2,1}}};
        for(std::string line;std::getline(input,line);){
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);unsigned scenario;std::uint32_t clock,counter_seed;std::string initial,expected_events,expected;
            row>>scenario>>clock>>counter_seed>>initial>>expected_events>>expected;require(!row.fail());const auto state=decode(initial);require(state.size()==18);
            MobRuntime mob;MobRuntimeTemplate maximum;maximum.hp=2000;maximum.sp=200;maximum.mp=150;maximum.attribute=7;mob.definition=&maximum;
            mob.id=state[0];mob.hp=state[1];mob.sp=static_cast<std::uint16_t>(state[2]);mob.mp=static_cast<std::uint16_t>(state[3]);mob.stun=std::bit_cast<std::int32_t>(state[4]);mob.stop=std::bit_cast<std::int32_t>(state[5]);mob.sleep=std::bit_cast<std::int32_t>(state[6]);mob.attribute=std::bit_cast<std::int32_t>(state[7]);mob.vision_fixed_user=state[8];mob.status=std::bit_cast<std::int32_t>(state[9]);mob.next_move=state[10];mob.target_kind=std::bit_cast<std::int32_t>(state[11]);mob.target_id=state[12];mob.lua_ai=std::bit_cast<std::int32_t>(state[13]);mob.zone=state[14]?zone_tag():nullptr;
            if(state[15]){mob.state_stack=std::vector<std::int32_t>{};if(state[16])mob.state_stack->push_back(static_cast<std::int32_t>(state[17]));}
            MobRespawn respawn;respawn.both=true;mob.respawn=(scenario/64)&2?&respawn:nullptr;
            mob.previous_attack_user.fill('U');mob.previous_attack_skill.fill('S');
            const std::string user="BeforeUser",previous_skill="BeforeSkill";
            std::copy(user.begin(),user.end(),mob.previous_attack_user.begin());mob.previous_attack_user[user.size()]=0;
            std::copy(previous_skill.begin(),previous_skill.end(),mob.previous_attack_skill.begin());mob.previous_attack_skill[previous_skill.size()]=0;
            std::atomic<std::uint32_t> counter{counter_seed};Services services(mob,counter,scenario);services.clock=clock;MobAppliedSkillPool pool(services);MobAppliedSkills owner(pool,services);services.pool=&pool;services.owner=&owner;
            for(std::size_t i=0;i<3;++i){const auto role=orders[scenario%3][i];auto* skill=owner.append_created(counter);require(skill);skill->definition=&services.definitions[role];skill->name=services.names[role];skill->skill_id=services.definitions[role].id;skill->level=1;skill->number=0x80000000U+static_cast<std::uint32_t>(i);skill->attack_tick=60+static_cast<std::uint32_t>(i);skill->reset_tick=70+static_cast<std::uint32_t>(i);skill->end_tick=9999;skill->previous_tick=42*static_cast<std::uint32_t>(i)+scenario%17;skill->target_type=1;skill->target_id=42+static_cast<std::uint32_t>(i);if(role==1)services.selected=skill;}
            services.seeding=false;const auto original=services.selected;const auto instance=original->instance_id;
            owner.apply(mob,{services.definitions[3],services.names[3],2,555},7,counter,services,services);
            std::string events;for(const auto& event:services.events){if(!events.empty())events+=';';events+=event;}
            if(events!=expected_events || services.snapshot()!=expected){std::cerr<<"case "<<scenario<<" mismatch\nexpected events "<<expected_events<<"\nactual events "<<events<<"\nexpected state "<<expected<<"\nactual state "<<services.snapshot()<<'\n';return 1;}
            require(owner.records().back()==original && original->instance_id==instance && original->definition==&services.definitions[3]);++count;
        }
        require(count==256);std::cout<<count<<" original damage/reentrant replacement compositions matched\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
