#include "eternity/game/mob_applied_skills_time_damage.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value){if(!value)throw std::runtime_error("Mob skill TimeDamage mismatch");}
struct Composed final:MobAppliedSkillDamageServices {
    MobAppliedSkillTimeDamageDefinition definition{0,{600,60,30}};
    unsigned calculations=0,hits=0,packets=0;
    std::uint32_t tick()noexcept override{return 2500;}
    MobAppliedSkillDamage time_damage(MobRuntime& mob,const SkillDefinition& skill,std::uint32_t elapsed)noexcept override{
        require(skill.detail==0x3d && elapsed==1500 && mob.definition);++calculations;
        return calculate_mob_applied_skill_time_damage(definition,project_mob_applied_skill_time_damage(mob,*mob.definition),elapsed);
    }
    bool damage_periodic(MobRuntime& mob,std::uint32_t kind,std::uint32_t id,MobAppliedSkillDamage& values)noexcept override{
        require(kind==1 && id==42 && values==MobAppliedSkillDamage({300,30,15}));++hits;
        mob.hp-=300;mob.sp=static_cast<std::uint16_t>(mob.sp-30);mob.mp=static_cast<std::uint16_t>(mob.mp-15);
        values[0]=299;return false;
    }
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime& mob,const MobAppliedSkillDamagePacket& packet,float radius,std::uint8_t priority)noexcept override{
        require(radius==60 && priority==6 && mob.hp==700 && mob.sp==70 && mob.mp==35);
        require(packet==MobAppliedSkillDamagePacket({7,6,7,0,0,0,9,0,2,43,1,30,0,15,0}));++packets;
    }
};
}
int main(int argc,char** argv){
    try{
        require(argc==2);std::ifstream input(argv[1]);require(input.good());
        const auto initial_rounding=std::fegetround();require(initial_rounding==FE_TONEAREST);
        unsigned cases=0,negative=0,capped=0,zero=0;
        for(std::string line;std::getline(input,line);){
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);unsigned kind;row>>kind;
            MobAppliedSkillTimeDamageDefinition definition;definition.type=static_cast<std::uint8_t>(kind);
            for(auto& value:definition.damage){unsigned word;row>>word;value=static_cast<std::uint16_t>(word);}
            unsigned sp,mp;MobRuntime mob;MobRuntimeTemplate maximum;
            row>>mob.hp>>sp>>mp;mob.sp=static_cast<std::uint16_t>(sp);mob.mp=static_cast<std::uint16_t>(mp);
            row>>maximum.hp>>sp>>mp;maximum.sp=static_cast<std::uint16_t>(sp);maximum.mp=static_cast<std::uint16_t>(mp);
            std::uint32_t elapsed;std::array<std::uint32_t,3> expected{};row>>elapsed;for(auto& value:expected)row>>value;require(!row.fail());
            const auto actual=calculate_mob_applied_skill_time_damage(definition,project_mob_applied_skill_time_damage(mob,maximum),elapsed);
            for(std::size_t i=0;i<actual.size();++i){
                if(std::bit_cast<std::uint32_t>(actual[i])!=expected[i]){
                    std::cerr<<"case "<<cases<<" type "<<kind<<" slot "<<i<<" expected "<<expected[i]<<" actual "<<std::bit_cast<std::uint32_t>(actual[i])<<'\n';return 1;
                }
                negative+=actual[i]<0?1U:0U;capped+=actual[i]==30000?1U:0U;zero+=actual[i]==0?1U:0U;
            }
            ++cases;
        }
        require(cases==12544 && negative>0 && capped>0 && zero>0);
        require(std::fegetround()==initial_rounding);
        // Compose the real calculator with final periodic removal and packet packing.
        static std::uint64_t zone_identity;
        MobRuntimeTemplate maximum;maximum.hp=2000;maximum.sp=200;maximum.mp=150;
        MobRuntime mob;mob.definition=&maximum;mob.id=7;mob.hp=1000;mob.sp=100;mob.mp=50;mob.lua_ai=1;
        mob.zone=reinterpret_cast<ZoneMobs*>(&zone_identity);
        MobAppliedSkill skill;skill.skill_id=9;skill.level=2;skill.target_type=1;skill.target_id=42;skill.previous_tick=1000;skill.name="Burn";
        SkillDefinition definition;definition.detail=0x3d;Composed services;
        finish_mob_applied_skill_damage(mob,skill,definition,services);
        require(services.calculations==1 && services.hits==1 && services.packets==1);
        require(mob.previous_attack_user[0]==0 && mob.previous_attack_skill[0]==0 && skill.previous_tick==1000);
        std::cout<<cases<<" original complete TimeDamage cases and native removal composition matched\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
