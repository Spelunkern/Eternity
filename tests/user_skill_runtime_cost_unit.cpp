#include "eternity/game/user_skill_runtime_cost.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
struct Scene final:ActorScene {
    mutable unsigned calls=0;std::function<void(const net::Packet&)> on_send;
    void send_self(const net::Packet& p) noexcept override{++calls;if(on_send)on_send(p);}
    void send_nearby(const net::Packet&) override{++calls;}
    void send_cell_view(const net::Packet&) override{++calls;}
    std::optional<bool> party_kind() const noexcept override{++calls;return {};}
    void send_party(const net::Packet&) noexcept override{++calls;}
    void reset_damage() override{++calls;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{++calls;return 1;}
    void release_effect_id(std::uint32_t) noexcept override{++calls;}
    std::uint32_t tick_ms() const noexcept override{++calls;return 123;}
    void stop_target_mob(std::uint32_t) override{++calls;}
    void taunt(std::uint16_t,float) override{++calls;}
    ZoneUsers* user_zone() override{++calls;return nullptr;}
    UserShapeSocial user_shape_social() override{++calls;return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{++calls;}
};
std::string hex(std::span<const std::uint8_t> p){constexpr char d[]="0123456789abcdef";std::string out;for(auto b:p){out+=d[b>>4];out+=d[b&15];}return out;}
struct Manager:UserSkillRuntimeServices {
    SkillDefinition effect;UserSkillDefinition info{effect,"Snapshot"};std::uint32_t counter=500;std::vector<UserSkillHandle> stranded;
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{return &info;}
    UserSkillHandle allocate()override{auto p=std::make_shared<UserSkillRuntimeSkill>();p->instance_id=++counter;return p;}
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("unexpected missing definition");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected disconnect");}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,mob,sp_cost,mp_cost,result;std::uint32_t sr,mr,sp,mp,need_sp,need_mp;std::string packet;std::istringstream row(line);row>>n>>mob>>sp_cost>>mp_cost>>sr>>mr>>sp>>mp>>need_sp>>need_mp>>result>>packet;check(bool(row),"invalid fixture");
        check(std::bit_cast<std::uint32_t>(user_skill_runtime_resource_cost(static_cast<std::uint16_t>(sp_cost),std::bit_cast<std::int32_t>(sr)))==need_sp&&std::bit_cast<std::uint32_t>(user_skill_runtime_resource_cost(static_cast<std::uint16_t>(mp_cost),std::bit_cast<std::int32_t>(mr)))==need_mp,"original cost arithmetic differs case "+std::to_string(n));
        Scene scene;EffectStatus status;status.skills.sp_cost_reduction=std::bit_cast<std::int32_t>(sr);status.skills.mp_cost_reduction=std::bit_cast<std::int32_t>(mr);auto actor=std::make_shared<Actor>(42,status,PlayerResources{1,mp,sp},std::vector<PersistedEffect>{},scene,rules);Manager manager;manager.info.sp_cost=static_cast<std::uint16_t>(sp_cost);manager.info.mp_cost=static_cast<std::uint16_t>(mp_cost);UserSkillRuntime owner(actor,manager);net::Packet seed{4,5,42,0,0,0,1,11,0,2,0,0,0};check(owner.load(seed,0)==UserSkillLoadResult::loaded,"seed failed");std::string actual="-";unsigned sends=0;scene.on_send=[&](const net::Packet& p){actual=hex(p);++sends;};
        const auto rounding=std::fegetround();const bool ready=user_skill_runtime_check_resources(owner,owner.slot(0),manager.info,mob!=0);check(ready==bool(result)&&actual==packet&&sends==unsigned(packet!="-")&&actor->resources().sp==sp&&actor->resources().mp==mp&&std::fegetround()==rounding,"original resource prefix differs case "+std::to_string(n));++cases;
    }
    check(cases==4096,"incomplete cost fixture");
    const auto before=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"cannot probe rounding contract");bool rejected=false;try{static_cast<void>(user_skill_runtime_resource_cost(29,100));}catch(const std::logic_error&){rejected=true;}const bool preserved=std::fegetround()==FE_DOWNWARD;check(std::fesetround(before)==0,"cannot restore rounding");check(rejected&&preserved,"unsupported rounding silently accepted or changed");
    std::cout<<cases<<" original CRT53 resource arithmetic/full-overload comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
