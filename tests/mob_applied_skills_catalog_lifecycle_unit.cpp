#include "eternity/game/mob_applied_skills_catalog.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value){if(!value)throw std::runtime_error("Catalog recursive lifecycle mismatch");}
void put(std::span<std::byte> b,unsigned at,unsigned value,unsigned count){for(unsigned i=0;i<count;++i)b[at+i]=static_cast<std::byte>((value>>(8*i))&255);}
void define(MobTargetingBindingCatalog& c,unsigned id,unsigned detail,const std::string& name){
    auto b=c.mutable_skill_record(id);put(b,0,id,2);put(b,2,1,1);put(b,49,3,1);put(b,50,detail,2);put(b,92,10,4);
    for(unsigned i=0;i<32;++i)b[3+i]=i<name.size()?static_cast<std::byte>(name[i]):std::byte{0};
    put(b,110,0,1);put(b,112,9,2);put(b,114,4,2);put(b,116,3,2);c.publish_skill_record(id);
}
struct Factory:MobAppliedSkillPoolServices {
    unsigned calls=0;bool seeded=false;
    std::unique_ptr<MobAppliedSkill> construct()override{check(!seeded);++calls;return std::make_unique<MobAppliedSkill>();}
};
struct Ports:MobAppliedSkillsCatalogPorts {
    const MobTargetingBindingCatalog& catalog;MobAppliedSkillPool& pool;MobRuntime& mob;
    MobAppliedSkillsCatalog *bridge=nullptr,*other=nullptr;MobAppliedSkill *selected=nullptr,*other_skill=nullptr,*nested_skill=nullptr;
    unsigned now=100,mode=0,hits=0,deaths=0,views=0,initial_hp=0;bool kill=false,nested=false,periodic=true,named=false;
    std::vector<char> stages;
    Ports(const MobTargetingBindingCatalog& c,MobAppliedSkillPool& p,MobRuntime& m):catalog(c),pool(p),mob(m){}
    bool free(const MobAppliedSkill* s)const{auto v=pool.available();return std::find(v.begin(),v.end(),s)!=v.end();}
    bool active(const MobAppliedSkill* s)const{auto v=bridge->owner().records();return std::find(v.begin(),v.end(),s)!=v.end();}
    void retained()const{
        check(other->owner().records()==std::vector<MobAppliedSkill*>{other_skill});check(!free(other_skill));
        if(mode!=0)check(!active(selected) && !free(selected));
    }
    std::uint32_t tick()noexcept override{return now;}
    void log(const MobAppliedSkill& s,std::string_view name,std::uint8_t action)noexcept override{
        auto* slot=catalog.from_definition(s.definition);check(slot && name==slot->name() && (action==5 || action==6));
    }
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    void hit(MobRuntime& m,unsigned type,unsigned id){
        check(&m==&mob && type==1 && id==42);retained();check(mode!=0 || active(selected));
        const auto* slot=catalog.from_definition(selected->definition);check(slot);
        if(named)check(std::string(reinterpret_cast<const char*>(mob.previous_attack_skill.data()))==slot->name());
        ++hits;stages.push_back('H');mob.hp=1;
    }
    bool damage_periodic(MobRuntime& m,std::uint32_t t,std::uint32_t id,MobAppliedSkillDamage& d)noexcept override{
        check(periodic && d==MobAppliedSkillDamage({9,4,3}));hit(m,t,id);d={4,2,1};return kill;
    }
    bool damage_hp(MobRuntime& m,std::uint32_t t,std::uint32_t id,std::uint32_t hp)noexcept override{
        check(!periodic && hp==initial_hp);hit(m,t,id);return kill;
    }
    void death(MobRuntime& m,std::uint32_t t,std::uint32_t id)noexcept override{
        check(&m==&mob && t==1 && id==42 && hits==1);retained();++deaths;stages.push_back('D');
        bridge->clear();check(bridge->owner().records().empty());
        if(mode!=0)check(!free(selected));
        if(nested){check(bridge->apply(3,2,77));nested_skill=bridge->owner().records().at(0);if(mode!=0)check(nested_skill!=selected);}
        retained();
    }
    void send_result(MobRuntime& m,const MobAppliedSkillDamagePacket& packet,float radius,std::uint8_t priority)noexcept override{
        check(&m==&mob && radius==60 && priority==6 && hits==1);retained();++views;stages.push_back('V');
        check(packet[0]==7 && packet[1]==6 && packet[6]==(mode==0 && kill && nested?3:1) && packet[7]==0 && packet[8]==1);
        auto word=[&](unsigned at){return unsigned(packet[at])+(unsigned(packet[at+1])<<8);};
        check(word(9)==(periodic?4:initial_hp) && word(11)==(periodic?2:0) && word(13)==(periodic?1:0));
        if(named)check(mob.previous_attack_skill[0]==0 && mob.previous_attack_user[0]==0);
    }
};
void run(unsigned mode,bool periodic,bool kill,bool nested,unsigned flags){
    auto catalog=MobTargetingBindingCatalog::load(0,4,{},{});
    define(catalog,1,periodic?0x3d:0x3f,"Selected");define(catalog,2,99,"Filler");define(catalog,3,99,"Nested");define(catalog,4,99,"Other");
    Factory factory;MobAppliedSkillPool pool(factory);MobRuntime mob,other_mob;MobRuntimeTemplate maximum;maximum.hp=2000;maximum.sp=100;maximum.mp=100;
    mob.definition=&maximum;other_mob.definition=&maximum;mob.hp=1000+flags;mob.sp=100;mob.mp=100;mob.id=7;mob.lua_ai=static_cast<int>(flags&1);
    MobRespawn respawn;respawn.both=true;mob.respawn=flags&2?&respawn:nullptr;static unsigned zone;mob.zone=flags&4?reinterpret_cast<ZoneMobs*>(&zone):nullptr;
    std::atomic<std::uint32_t> ids{0};Ports ports(catalog,pool,mob),other_ports(catalog,pool,other_mob);
    MobAppliedSkillsCatalog bridge(catalog,pool,mob,ids,ports),other(catalog,pool,other_mob,ids,other_ports);
    ports.bridge=&bridge;ports.other=&other;ports.mode=mode;ports.kill=kill;ports.nested=nested;ports.periodic=periodic;ports.named=(flags&3)!=0;ports.initial_hp=mob.hp;
    check(other.apply(4,0,0));ports.other_skill=other.owner().records().at(0);
    // Keep selected after filler so recursive Clear and shared-cursor traversal
    // both encounter the real existing owner, not a replacement test container.
    check(bridge.apply(2,1,43));auto* filler=bridge.owner().records().at(0);check(bridge.apply(1,1,42));ports.selected=bridge.owner().records().at(1);
    auto* selected=ports.selected;auto* identity=selected->definition;const auto instance=selected->instance_id;
    factory.seeded=true;ports.now=3100;
    // A real in-place publication after admission must leave old node fields
    // intact until the existing owner performs replacement admission.
    auto raw=catalog.mutable_skill_record(1);if(mode==2)put(raw,2,2,1);put(raw,92,20,4);catalog.publish_skill_record(1);
    check(selected->definition==identity && identity==catalog.skill_definition(1) && selected->end_tick==10100);
    if(mode==0)bridge.keep(3100);
    else if(mode==1){selected->end_tick=3100;bridge.expire(3100);}
    else check(bridge.apply(1,1,42));
    check(ports.hits==1 && ports.deaths==unsigned(kill) && ports.views==unsigned((flags&4)!=0));
    std::vector<char> expected{'H'};if(kill)expected.push_back('D');if(flags&4)expected.push_back('V');check(ports.stages==expected);
    auto active=bridge.owner().records();check(other.owner().records()==std::vector<MobAppliedSkill*>{ports.other_skill});
    check(factory.calls==3 && ids==3 && selected->instance_id==instance && selected->definition==(mode==0 && kill && nested?catalog.skill_definition(3):identity));
    if(mode==2){
        check(active.back()==selected && selected->level==2 && selected->end_tick==23100 && !ports.free(selected));
        check(active.size()==(kill?(nested?2U:1U):2U));
    }else if(kill){check(active.size()==unsigned(nested) && ports.free(selected)==!(mode==0 && nested));}
    else if(mode==1){check(active==std::vector<MobAppliedSkill*>{filler} && ports.free(selected));}
    else check(active==std::vector<MobAppliedSkill*>({filler,selected}) && selected->previous_tick==3100);
    if(kill && nested)check(active.front()==ports.nested_skill && ports.nested_skill==(mode==0?selected:filler) && ports.nested_skill->definition==catalog.skill_definition(3));
    if(!kill && nested)check(!ports.nested_skill);
}
}
int main(){try{unsigned count=0;for(unsigned mode=0;mode<3;++mode)for(bool periodic:{false,true}){
    if(mode==0 && !periodic)continue;
    for(bool kill:{false,true})for(bool nested:{false,true})for(unsigned flags=0;flags<8;++flags){run(mode,periodic,kill,nested,flags);++count;}
}check(count==160);std::cout<<count<<" real catalog/owner recursive damage, clear, nested admission and packet lifecycles matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
