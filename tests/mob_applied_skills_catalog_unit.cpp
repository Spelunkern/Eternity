#include "eternity/game/mob_applied_skills_catalog.hpp"
#include "eternity/game/mob_targeting_reload.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool b){if(!b)throw std::runtime_error("Actual catalog/applied-owner composition mismatch");}
void put(std::span<std::byte> b,unsigned at,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)b[at+i]=static_cast<std::byte>((v>>(8*i))&255);}
void text(std::span<std::byte> b,const std::string& name){for(unsigned i=0;i<32;++i)b[3+i]=i<name.size()?static_cast<std::byte>(name[i]):std::byte{0};}
void init(MobTargetingBindingCatalog& c,unsigned level,unsigned detail,unsigned seconds,const std::string& name){auto b=c.mutable_skill_record(1);put(b,0,7,2);put(b,2,level,1);put(b,49,3,1);put(b,50,detail,2);put(b,92,seconds);text(b,name);c.publish_skill_record(1);}
// Required SQL boundary scripted with typed values; actual recovered R2 writes
// the existing catalog buffers. This is not a live SQL driver test.
struct Reload final:MobTargetingReloadServices {
    MobTargetingBindingCatalog& catalog;eternity::storage::Row row;unsigned reads=0,cleanup=0;int cursor=0;
    explicit Reload(MobTargetingBindingCatalog& c,unsigned n):catalog(c),row(82,std::string("0")){
        row[0]="1";row[1]="2";row[2]="Ignored replacement name";row[16]="3";row[17]="67";
        row[47]="3";row[64]="7";row[65]=std::to_string(9+n);
    }
    bool profile()override{return true;}void parse_connection()override{}
    bool connect()override{return true;}bool allocate()override{return true;}
    bool query(MobTargetingReloadKind k,std::uint16_t id)override{check(k==MobTargetingReloadKind::skill && id==1);return true;}
    std::int16_t fetch()override{return cursor++==0?0:100;}
    bool get_data(unsigned column,unsigned,int type,std::span<std::byte> out)override{
        ++reads;const auto& v=*row.at(column-1);
        if(type==1){auto n=std::min(v.size(),out.size()-1);for(std::size_t i=0;i<n;++i)out[i]=static_cast<std::byte>(v[i]);out[n]=std::byte{0};}
        else{auto n=static_cast<std::uint32_t>(std::stoll(v));for(std::size_t i=0;i<out.size();++i)out[i]=static_cast<std::byte>((n>>(8*i))&255);}
        return true;
    }
    void free_database()override{++cleanup;}void disconnect()override{++cleanup;}void release_pool()override{++cleanup;}
    void log_error()override{throw std::runtime_error("Unexpected scripted R2 error");}
    std::optional<MobTargetingReloadLink> skill(std::uint16_t)const override{std::terminate();}
    std::span<std::byte> target(MobTargetingReloadKind k,std::uint16_t id)override{check(k==MobTargetingReloadKind::skill && id==1 && cleanup==3);return catalog.mutable_skill_record(id);}
};
struct Ports final:MobAppliedSkillPoolServices,MobAppliedSkillsCatalogPorts {
    unsigned now=0,logs=0,rebuilds=0,speeds=0,hits=0,packets=0,deaths=0;bool kill=false;
    MobAppliedSkillDamage expected{};std::string expected_name;MobAppliedSkillsCatalog* bridge=nullptr;
    std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}
    std::uint32_t tick()noexcept override{return now;}
    void log(const MobAppliedSkill& s,std::string_view name,std::uint8_t action)noexcept override{check(name==expected_name && s.definition && (action==5 || action==6));++logs;}
    void rebuild_attack(MobRuntime&)noexcept override{++rebuilds;}
    void send_speed(MobRuntime&)noexcept override{++speeds;}
    bool damage_periodic(MobRuntime& m,std::uint32_t type,std::uint32_t id,MobAppliedSkillDamage& d)noexcept override{
        check(type==1 && id==42 && d==expected);check(std::string(reinterpret_cast<const char*>(m.previous_attack_skill.data()))==expected_name);++hits;return kill;
    }
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t t,std::uint32_t id)noexcept override{check(t==1 && id==42 && bridge);++deaths;bridge->clear();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket& packet,float radius,std::uint8_t priority)noexcept override{check(packet[0]==7 && packet[1]==6 && radius==60 && priority==6);++packets;}
};
}
int main(int argc,char** argv){try{
    check(argc==2);std::ifstream input(argv[1]);check(input.good());unsigned cases=0;
    for(std::string line;std::getline(input,line);){
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned type,hp,sp,mp,chp,csp,cmp,mhp,msp,mmp,elapsed,ehp,esp,emp;
        row>>type>>hp>>sp>>mp>>chp>>csp>>cmp>>mhp>>msp>>mmp>>elapsed>>ehp>>esp>>emp;check(!row.fail());
        auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});init(catalog,1,0x3d,2,"Before");
        Ports ports;ports.expected_name="Before";MobAppliedSkillPool pool(ports);MobRuntime mob;MobRuntimeTemplate maximum;
        maximum.hp=mhp;maximum.sp=static_cast<std::uint16_t>(msp);maximum.mp=static_cast<std::uint16_t>(mmp);mob.definition=&maximum;
        mob.hp=chp;mob.sp=static_cast<std::uint16_t>(csp);mob.mp=static_cast<std::uint16_t>(cmp);mob.lua_ai=1;
        static unsigned zone;mob.zone=reinterpret_cast<ZoneMobs*>(&zone);
        std::atomic<std::uint32_t> ids{0};MobAppliedSkillsCatalog bridge(catalog,pool,mob,ids,ports);ports.bridge=&bridge;
        check(!bridge.apply(0,1,42) && !bridge.apply(3,1,42));check(bridge.apply(1,1,42));
        auto* skill=bridge.owner().records().at(0);auto* definition=skill->definition;auto* name_address=catalog.skill(1)->name().data();auto original_end=skill->end_tick;
        // Publish into the actual catalog slot after admission. The already
        // retained pointer must read all new periodic/name/keep fields.
        auto raw=catalog.mutable_skill_record(1);put(raw,110,type,1);put(raw,112,hp,2);put(raw,114,sp,2);put(raw,116,mp,2);put(raw,92,9);text(raw,"Published");catalog.publish_skill_record(1);
        check(skill->definition==definition && definition==catalog.skill_definition(1) && catalog.from_definition(definition)==catalog.skill(1));
        check(definition->keep_time_seconds==9 && skill->end_tick==original_end && catalog.skill(1)->name().data()==name_address);
        ports.expected_name="Published";ports.expected={std::bit_cast<std::int32_t>(ehp),std::bit_cast<std::int32_t>(esp),std::bit_cast<std::int32_t>(emp)};
        bridge.keep(elapsed);check(ports.hits==1 && ports.packets==1 && skill->previous_tick==elapsed && skill->name=="Published");
        check(mob.previous_attack_skill[0]==0 && mob.previous_attack_user[0]==0);
        // Refresh uses published keep seconds without allocating/rebinding.
        ports.now=elapsed;check(bridge.apply(1,1,42));check(skill->end_tick==elapsed+9000U && ids==1 && pool.construction_attempts()==1);
        // Replacement reads the old node's same live catalog definition. Final
        // damage still uses original fixture elapsed; no temporary definition.
        skill->previous_tick=0;put(raw,2,2,1);text(raw,"Replacement");catalog.publish_skill_record(1);ports.expected_name="Replacement";
        check(bridge.apply(1,1,42));check(ports.hits==2 && ports.packets==2 && bridge.owner().records().at(0)==skill && skill->instance_id==1 && skill->level==2);
        check(skill->definition==definition && bridge.name(*definition)=="Replacement");
        // Actual expiry reaches the same pure calculator and mandatory ports.
        ports.expected=calculate_mob_applied_skill_time_damage({catalog.skill(1)->time_damage_type,catalog.skill(1)->time_damage},project_mob_applied_skill_time_damage(mob,maximum),0);
        skill->end_tick=elapsed;bridge.expire(elapsed);check(ports.hits==3 && ports.packets==3 && bridge.owner().records().empty() && pool.available().at(0)==skill);
        check(ports.logs==4 && ports.speeds==0 && ports.rebuilds==0);++cases;
    }
    check(cases==12544);
    for(unsigned i=0;i<256;++i){
        auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});init(catalog,1,0x43,2,"Ability");auto raw=catalog.mutable_skill_record(1);put(raw,124,7,1);put(raw,126,5+i,2);catalog.publish_skill_record(1);
        Ports ports;ports.expected_name="Ability";MobAppliedSkillPool pool(ports);MobRuntime mob;MobRuntimeTemplate maximum;mob.definition=&maximum;mob.wisdom=1000;mob.lua_ai=1;std::atomic<std::uint32_t> ids{0};MobAppliedSkillsCatalog bridge(catalog,pool,mob,ids,ports);
        bridge.apply(1,1,42);auto* skill=bridge.owner().records().at(0);check(mob.wisdom==995-static_cast<int>(i));
        put(raw,126,9+i,2);put(raw,92,3);catalog.publish_skill_record(1);check(skill->definition->abilities[0].value==9+i && skill->end_tick==2000);
        bridge.expire(2000);check(mob.wisdom==1004 && pool.available().at(0)==skill && ports.rebuilds==2);
        bridge.apply(1,1,42);check(bridge.owner().records().at(0)==skill && skill->instance_id==1 && skill->end_tick==3000);
        auto wisdom=mob.wisdom;put(raw,2,2,1);put(raw,126,11+i,2);text(raw,"LiveAbility");catalog.publish_skill_record(1);ports.expected_name="LiveAbility";
        bridge.apply(1,1,42);check(mob.wisdom==wisdom && skill->level==2 && skill->definition==catalog.skill_definition(1));
        bridge.expire(3000);check(mob.wisdom==1006 && ports.rebuilds==6 && pool.construction_attempts()==1);
    }
    for(unsigned i=0;i<256;++i){
        auto catalog=MobTargetingBindingCatalog::load(0,2,{},{});init(catalog,100,0x43,2,"Retained R2");
        auto raw=catalog.mutable_skill_record(1);put(raw,0,1,2);put(raw,124,7,1);put(raw,126,5+i,2);catalog.publish_skill_record(1);
        Ports ports;ports.expected_name="Retained R2";MobAppliedSkillPool pool(ports);MobRuntime mob;MobRuntimeTemplate maximum;mob.definition=&maximum;mob.wisdom=1000;mob.lua_ai=1;
        std::atomic<std::uint32_t> ids{0};MobAppliedSkillsCatalog bridge(catalog,pool,mob,ids,ports);bridge.apply(1,1,42);
        auto* skill=bridge.owner().records().at(0);auto* def=skill->definition;auto* raw_address=raw.data();auto* name_address=catalog.skill(1)->name().data();
        Reload reload(catalog,i);std::uint32_t changes=0;auto result=mob_targeting_reload(MobTargetingReloadKind::skill,1,changes,reload);
        check(result.success && changes>0 && reload.reads==82 && reload.cleanup==3);catalog.publish_skill_record(1);
        check(skill->definition==def && def==catalog.skill_definition(1) && raw.data()==raw_address && catalog.skill(1)->name().data()==name_address);
        check(def->level==100 && def->keep_time_seconds==3 && def->abilities[0].value==9+i && skill->end_tick==2000 && bridge.name(*def)=="Retained R2");
        bridge.expire(2000);check(mob.wisdom==1004 && ports.rebuilds==2 && pool.available().at(0)==skill);
        bridge.apply(1,1,42);check(skill->end_tick==3000 && skill->instance_id==1 && pool.construction_attempts()==1);bridge.clear();
    }
    std::cout<<cases<<" original TimeDamage rows through real catalog admission/reload/keep/replacement/expiry; 256 live ability cycles and 256 actual R2 publication cycles matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
