#include "eternity/game/mob_applied_skills.hpp"
#include "eternity/game/mobs.hpp"
#include "eternity/game/skills.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void require(bool b){if(!b)throw std::runtime_error("Vision cleanup mismatch");}
std::string seq(const std::vector<unsigned>& v){std::ostringstream o;for(auto n:v){if(o.tellp()>0)o<<',';o<<n;}return v.empty()?"-":o.str();}
const std::array<std::vector<unsigned>,8> layouts{{{}, {0x48},{0x42},{0x48,0x42,0x48},{99,0x48,0x40,0x48},{0x48,0x48,0x48},{99,0x48,99,0x42,0x48,99},{0x48,99,0x48,99,0x48,99,0x42}}};
struct Services:MobAppliedSkillPoolServices,MobAppliedSkillLogServices {
    MobRuntime mob;MobRespawn respawn;MobAppliedSkills *owner=nullptr,*other=nullptr;MobAppliedSkillPool* pool=nullptr;
    std::array<SkillDefinition,10> defs{};std::vector<MobAppliedSkill*> objects;std::vector<std::string> events;
    bool seeding=true,mutate=false,mutated=false;unsigned count=0;
    std::unique_ptr<MobAppliedSkill> construct()override{require(seeding);auto p=std::make_unique<MobAppliedSkill>();objects.push_back(p.get());return p;}
    unsigned index(const MobAppliedSkill* p)const{auto it=std::find(objects.begin(),objects.end(),p);require(it!=objects.end());return static_cast<unsigned>(it-objects.begin());}
    std::string pointers(const std::vector<MobAppliedSkill*>& v)const{std::vector<unsigned> ids;for(auto* p:v)ids.push_back(index(p));return seq(ids);}
    std::string core()const{
        std::ostringstream o;constexpr char digits[]="0123456789abcdef";
        for(auto v:std::array<std::uint32_t,12>{mob.vision_fixed_user,std::bit_cast<unsigned>(mob.target_kind),mob.target_id,std::bit_cast<unsigned>(mob.status),mob.next_move,mob.hp,std::bit_cast<unsigned>(mob.stun),std::bit_cast<unsigned>(mob.stop),std::bit_cast<unsigned>(mob.sleep),std::bit_cast<unsigned>(mob.lua_ai),unsigned(mob.respawn!=nullptr),unsigned(respawn.both)})
            for(unsigned i=0;i<4;++i){auto b=(v>>(i*8))&255;o<<digits[b>>4]<<digits[b&15];}
        return o.str();
    }
    std::string snapshot()const{
        std::ostringstream o;o<<core()<<'|'<<pool->construction_attempts()<<'|'<<pointers(owner->records())<<'|'<<pointers(other->records())<<'|'<<pointers(pool->available())<<'|';
        for(unsigned i=0;i<objects.size();++i){if(i)o<<';';auto& s=*objects[i];o<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick<<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id<<':'<<(s.definition-defs.data());}
        std::vector<unsigned> details;for(auto& d:defs)details.push_back(d.detail);o<<'|'<<seq(details);return o.str();
    }
    void removed(const MobAppliedSkill& s,std::uint8_t action)noexcept override{
        require(action==6);auto active=owner->records(),free=pool->available();require(std::find(active.begin(),active.end(),&s)!=active.end() && std::find(free.begin(),free.end(),&s)==free.end());
        events.push_back("L"+std::to_string(index(&s))+':'+snapshot());
        if(mutate && !mutated){mutated=true;mob.vision_fixed_user=0x11223344;mob.lua_ai^=1;defs[count-1].detail=0x48;}
    }
    void add(MobAppliedSkills& list,unsigned slot,unsigned id,unsigned instance){auto* s=list.append_from_pool();require(s);s->instance_id=instance;s->skill_id=id;s->level=slot%3+1;s->number=0x80000000+instance;s->attack_tick=60+slot;s->reset_tick=70+slot;s->end_tick=0xffffffff;s->previous_tick=slot*100;s->target_type=slot%3;s->target_id=slot==0?42:999+slot;s->definition=&defs[slot];s->name="Def"+std::to_string(slot);}
};
}
int main(int argc,char** argv){try{
    require(argc==2);std::ifstream in(argv[1]);require(in.good());unsigned cases=0,operations=0;
    std::string line;
    while(std::getline(in,line)){
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);char kind;unsigned id,flags;std::string initial;row>>kind>>id>>flags>>initial;require(kind=='S' && !row.fail());
        Services s;MobAppliedSkillPool pool(s);MobAppliedSkills owner(pool,s),other(pool,s);s.owner=&owner;s.other=&other;s.pool=&pool;
        s.count=static_cast<unsigned>(layouts[id%8].size());s.mutate=(id&256)!=0;
        s.mob.vision_fixed_user=std::array<unsigned,4>{0,42,0xffffffff,0x80000000}[(id/8)%4];s.mob.target_kind=static_cast<int>(id%3);s.mob.target_id=0xcafef00d;s.mob.status=id&1?2:0;s.mob.next_move=0xabcdef01;s.mob.hp=id&2?0:1000;s.mob.stun=2;s.mob.stop=3;s.mob.sleep=4;s.mob.lua_ai=static_cast<int>(flags&1);s.respawn.both=(flags&4)!=0;s.mob.respawn=flags&2?&s.respawn:nullptr;
        require(s.core()==initial);s.defs[9].detail=99;s.defs[8].detail=0x48;for(unsigned i=0;i<s.count;++i)s.defs[i].detail=static_cast<std::uint16_t>(layouts[id%8][i]);
        for(unsigned i=0;i<3;++i)s.add(other,9,900,9000+i);other.clear({});for(unsigned i=0;i<s.count;++i)s.add(owner,i,100+i,1000+i);s.add(other,8,800,3000);s.seeding=false;
        for(unsigned op=0;op<6;++op){require(bool(std::getline(in,line)));std::istringstream r(line);std::string expected,events;r>>kind;s.events.clear();
            if(kind=='V'){r>>events>>expected;owner.remove_vision_fix(s.mob);}else if(kind=='F'){unsigned value;r>>value>>expected;s.mob.vision_fixed_user=value;}else{require(kind=='R');r>>expected;require(other.append_from_pool()!=nullptr);}
            require(!r.fail());std::string actual_events;for(auto& e:s.events){if(!actual_events.empty())actual_events+=';';actual_events+=e;}if(actual_events.empty())actual_events="-";
            if(s.snapshot()!=expected || (kind=='V' && actual_events!=events)){std::cerr<<"case "<<id<<" operation "<<op<<"\nexpected "<<expected<<"\nactual "<<s.snapshot()<<"\nexpected events "<<events<<"\nactual events "<<actual_events<<'\n';return 1;}++operations;
        }++cases;
    }
    require(cases==512 && operations==3072);std::cout<<cases<<" vision-cleanup scenarios / "<<operations<<" original operations matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
