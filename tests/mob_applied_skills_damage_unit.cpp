#include "eternity/game/mob_applied_skills_damage.hpp"
#include "eternity/game/mob_applied_skills_expiration.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value){if(!value)throw std::runtime_error("Mob skill damage coordination mismatch");}
std::uint32_t bits(std::int32_t value){return std::bit_cast<std::uint32_t>(value);}
ZoneMobs* zone_tag(){static std::uint64_t identity;return reinterpret_cast<ZoneMobs*>(&identity);}
std::string hex(std::span<const std::uint8_t> bytes){
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(auto byte:bytes){out+=digits[byte>>4];out+=digits[byte&15];}return out;
}
std::vector<std::uint8_t> decode(const std::string& text){
    require(text.size()%2==0);std::vector<std::uint8_t> out;
    for(std::size_t i=0;i<text.size();i+=2)out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));
    return out;
}
void put(std::vector<std::uint8_t>& out,std::uint32_t value){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
std::uint32_t get(std::span<const std::uint8_t> bytes,std::size_t at){
    std::uint32_t out=0;for(unsigned i=0;i<4;++i)out|=static_cast<std::uint32_t>(bytes[at+i])<<(i*8);return out;
}
std::string packed(std::initializer_list<std::uint32_t> values){std::vector<std::uint8_t> out;for(auto value:values)put(out,value);return hex(out);}
void name(std::array<std::uint8_t,32>& target,const std::string& text){
    require(text.size()<target.size());std::copy(text.begin(),text.end(),target.begin());target[text.size()]=0;
}
struct Services final:MobAppliedSkillDamageServices {
    MobRuntime& mob;MobAppliedSkill& skill;SkillDefinition& definition;MobRespawn& respawn;
    unsigned case_id=0,mode=0;bool killed=false;std::uint32_t now=0;MobAppliedSkillDamage amounts{};
    std::vector<std::string> events;
    Services(MobRuntime& m,MobAppliedSkill& s,SkillDefinition& d,MobRespawn& r):mob(m),skill(s),definition(d),respawn(r){}
    std::string snapshot()const{
        std::vector<std::uint8_t> bytes;
        for(auto value:{mob.id,mob.hp,bits(mob.lua_ai),static_cast<std::uint32_t>(mob.respawn!=nullptr),
                        static_cast<std::uint32_t>(respawn.both),static_cast<std::uint32_t>(mob.zone!=nullptr),
                        skill.skill_id,skill.level,skill.previous_tick,skill.target_type,skill.target_id,
                        static_cast<std::uint32_t>(definition.detail)})put(bytes,value);
        bytes.insert(bytes.end(),mob.previous_attack_user.begin(),mob.previous_attack_user.end());
        bytes.insert(bytes.end(),mob.previous_attack_skill.begin(),mob.previous_attack_skill.end());
        for(std::size_t i=0;i<32;++i)bytes.push_back(i<skill.name.size()?static_cast<std::uint8_t>(skill.name[i]):0);
        return hex(bytes);
    }
    void toggle_zone(){mob.zone=mob.zone?nullptr:zone_tag();}
    std::uint32_t tick()noexcept override{
        events.push_back("K"+snapshot());if(mode&8)skill.previous_tick^=0x80000000U;return now;
    }
    MobAppliedSkillDamage time_damage(MobRuntime&,const SkillDefinition&,std::uint32_t elapsed)noexcept override{
        events.push_back("T"+packed({elapsed})+snapshot());
        if(mode&1){mob.lua_ai=1;mob.hp=0x76543210;skill.previous_tick=0x7fffffff;skill.target_type=5;skill.target_id=0x12345678;skill.name="TimeChanged";}
        return amounts;
    }
    void mutate_damage(){
        if(!(mode&2))return;
        mob.id^=0x10203040;mob.hp=0x12340000+case_id;mob.lua_ai=0;
        skill.skill_id^=0xabcd0000;skill.level+=0x101;skill.target_type=9;skill.target_id=0x89abcdef;
        name(mob.previous_attack_user,"DamageUser");name(mob.previous_attack_skill,"DamageSkill");toggle_zone();
    }
    bool damage_periodic(MobRuntime&,std::uint32_t kind,std::uint32_t id,MobAppliedSkillDamage& values)noexcept override{
        events.push_back("P"+packed({kind,id,bits(values[0]),bits(values[1]),bits(values[2])})+snapshot());
        mutate_damage();if(mode&2){values[0]=std::bit_cast<std::int32_t>(bits(values[0])^0xffffU);values[1]=std::bit_cast<std::int32_t>(bits(values[1])^0x80000000U);values[2]=std::bit_cast<std::int32_t>(bits(values[2])^0xffff0000U);}return killed;
    }
    bool damage_hp(MobRuntime&,std::uint32_t kind,std::uint32_t id,std::uint32_t hp)noexcept override{
        events.push_back("H"+packed({kind,id,hp})+snapshot());mutate_damage();return killed;
    }
    void death(MobRuntime&,std::uint32_t kind,std::uint32_t id)noexcept override{
        events.push_back("X"+packed({kind,id})+snapshot());
        if(mode&4){mob.id=0x99887766;mob.hp=0;skill.skill_id=0xffff1234;skill.level=0xffffff80;name(mob.previous_attack_user,"DeathUser");name(mob.previous_attack_skill,"DeathSkill");toggle_zone();}
    }
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket& packet,float radius,std::uint8_t priority)noexcept override{
        require(mob.zone && radius==60 && priority==6);events.push_back("V"+hex(packet)+snapshot());
    }
};
struct Composed final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,MobAppliedSkillRemovalServices,
                      MobAppliedSkillEffectServices,MobAppliedSkillDamageServices{
    MobRuntime& mob;MobAppliedSkills* owner=nullptr;std::vector<MobAppliedSkill*> objects;std::vector<char> trace;
    explicit Composed(MobRuntime& m):mob(m){}
    std::unique_ptr<MobAppliedSkill> construct()override{auto object=std::make_unique<MobAppliedSkill>();objects.push_back(object.get());return object;}
    void removed(const MobAppliedSkill&,std::uint8_t action)noexcept override{require(action==6);trace.push_back('L');}
    void remove_effects(MobAppliedSkill& skill)noexcept override{trace.push_back('R');require(skill.definition);remove_mob_applied_skill_effects(mob,skill,*skill.definition,0,*this);}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    void finish_damage_skill(MobRuntime& m,MobAppliedSkill& s,const SkillDefinition& d)noexcept override{finish_mob_applied_skill_damage(m,s,d,*this);}
    std::uint32_t tick()noexcept override{std::terminate();}
    MobAppliedSkillDamage time_damage(MobRuntime&,const SkillDefinition&,std::uint32_t)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime& m,std::uint32_t kind,std::uint32_t id,std::uint32_t hp)noexcept override{
        require(kind==1 && id==42 && hp==1000);trace.push_back('H');m.hp=0;return true;
    }
    void death(MobRuntime& m,std::uint32_t kind,std::uint32_t id)noexcept override{
        require(kind==1 && id==42 && m.previous_attack_skill[0]=='F');trace.push_back('X');owner->clear({true,false,false});
    }
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket& p,float radius,std::uint8_t priority)noexcept override{
        require(radius==60 && priority==6 && p[0]==7 && p[1]==6 && p[9]==0xe8 && p[10]==3);trace.push_back('V');
    }
};
}
int main(int argc,char** argv){
    try{
        require(argc==2);std::ifstream input(argv[1]);require(input.good());unsigned cases=0;
        for(std::string line;std::getline(input,line);){
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);unsigned id,mode,killed;std::uint32_t now;
            std::string initial,amount_hex,expected_events,expected_final;row>>id>>now>>mode>>killed>>initial>>amount_hex>>expected_events>>expected_final;require(!row.fail());
            const auto state=decode(initial);require(state.size()==144);MobRuntime mob;MobAppliedSkill skill;SkillDefinition definition;MobRespawn respawn;
            mob.id=get(state,0);mob.hp=get(state,4);mob.lua_ai=std::bit_cast<std::int32_t>(get(state,8));mob.respawn=get(state,12)?&respawn:nullptr;
            respawn.both=get(state,16)!=0;mob.zone=get(state,20)?zone_tag():nullptr;skill.skill_id=get(state,24);skill.level=get(state,28);skill.previous_tick=get(state,32);skill.target_type=get(state,36);skill.target_id=get(state,40);definition.detail=static_cast<std::uint16_t>(get(state,44));
            std::copy_n(state.begin()+48,32,mob.previous_attack_user.begin());std::copy_n(state.begin()+80,32,mob.previous_attack_skill.begin());
            for(std::size_t at=112;at<144 && state[at];++at)skill.name+=static_cast<char>(state[at]);
            Services services(mob,skill,definition,respawn);services.case_id=id;services.now=now;services.mode=mode;services.killed=killed!=0;
            const auto amount=decode(amount_hex);require(amount.size()==12);for(unsigned i=0;i<3;++i)services.amounts[i]=std::bit_cast<std::int32_t>(get(amount,i*4));
            finish_mob_applied_skill_damage(mob,skill,definition,services);
            std::string events;for(const auto& event:services.events){if(!events.empty())events+=',';events+=event;}
            if(events!=expected_events || services.snapshot()!=expected_final){std::cerr<<"case "<<id<<" mismatch\nexpected events "<<expected_events<<"\nactual events "<<events<<"\nexpected state "<<expected_final<<"\nactual state "<<services.snapshot()<<'\n';return 1;}
            ++cases;
        }
        require(cases==1024);
        MobRuntime mob;mob.hp=1000;mob.lua_ai=1;mob.zone=zone_tag();Composed services(mob);MobAppliedSkillPool pool(services);MobAppliedSkills owner(pool,services);services.owner=&owner;
        SkillDefinition definition;definition.detail=0x3f;
        auto* dying=owner.append_from_pool();auto* survivor=owner.append_from_pool();require(dying && survivor);
        dying->definition=&definition;dying->target_type=1;dying->target_id=42;dying->name="Final";dying->end_tick=5;survivor->end_tick=100;
        owner.expire(5,{true,false,false},services);
        require(owner.records().empty() && pool.available()==std::vector<MobAppliedSkill*>({dying,survivor}));
        require(mob.hp==0 && mob.previous_attack_user[0]==0 && mob.previous_attack_skill[0]==0);
        require(services.trace==std::vector<char>({'L','R','H','X','L','V'}));
        std::cout<<cases<<" original damage-removal traces and recursive expiration composition matched\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
