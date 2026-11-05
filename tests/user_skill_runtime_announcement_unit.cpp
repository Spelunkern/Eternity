#include "eternity/game/user_skill_runtime_announcement.hpp"
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
struct View final:UserSkillAnnouncementWorld {
    SkillDefinition& effect;UserSkillDefinition& info;std::string packet="-";std::uint32_t ex1=0,ex2=0;unsigned sends=0;
    View(SkillDefinition& e,UserSkillDefinition& i):effect(e),info(i){}
    void send(Actor& source,const net::Packet& p,std::uint32_t source_id,std::uint32_t target_id)override{
        ++sends;packet=hex(p);ex1=source_id;ex2=target_id;
        source.status().in_zone=false;effect.id=0xabcd;effect.level=0xfe;info.multi_attack=3;
    }
};
struct Strike final:UserSkillStrikeAnnouncementWorld,UserSkillAttackServices {
    unsigned hits=0;
    explicit Strike(View& view):UserSkillStrikeAnnouncementWorld(view){}
    void hit(Actor&,Actor&,const UserSkillDefinition&,bool first,bool second)override{check(!first&&second,"wrong multihit flags");++hits;}
    bool self_damage(Actor&,std::int32_t)override{throw std::runtime_error("unexpected scalar damage");}
    void self_death(Actor&,bool)override{throw std::runtime_error("unexpected death");}
    void range(Actor&,Actor*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected range");}
    void range_select(Actor&,Actor*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected range select");}
    void user_effect(UserSkillAction action,Actor& source,Actor* target,const UserSkillDefinition& info)override{check(action==UserSkillAction::attack,"wrong resolved action");user_skill_runtime_strike(source,target,info,*this);}
    void mob_effect(UserSkillAction,Actor&,MobRuntime*,const UserSkillDefinition&)override{throw std::runtime_error("unexpected mob effect");}
    void spend(Actor&,std::uint16_t,std::uint16_t)override{throw std::runtime_error("unexpected cost");}
    std::uint32_t tick_ms()override{throw std::runtime_error("unexpected clock");}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;std::vector<std::string> chains;
    while(std::getline(input,line)){
        if(line.starts_with("# chain\t")){chains.push_back(line.substr(8));continue;}
        if(line.empty()||line[0]=='#')continue;
        unsigned n,in_zone,id,level,reason,sends;std::uint32_t source_id,target_id,flag,ex1,ex2;std::string packet;
        std::istringstream row(line);row>>n>>in_zone>>source_id>>target_id>>id>>level>>reason>>flag>>packet>>ex1>>ex2>>sends;check(bool(row),"invalid fixture");
        Scene scene;EffectStatus status;status.in_zone=in_zone!=0;Actor source(source_id,status,PlayerResources{1,100,100},std::vector<PersistedEffect>{},scene,rules);
        SkillDefinition effect;effect.id=static_cast<std::uint16_t>(id);effect.level=static_cast<std::uint8_t>(level);UserSkillDefinition info{effect,"Announcement"};View view(effect,info);
        user_skill_runtime_announcement(source,info,static_cast<std::uint8_t>(reason),target_id,static_cast<std::uint8_t>(flag),view);
        check(view.packet==packet&&view.ex1==ex1&&view.ex2==ex2&&view.sends==sends&&scene.calls==0,"original announcement differs case "+std::to_string(n));++cases;
    }
    check(cases==2048,"incomplete fixture");
    check(chains.size()==2,"missing original attack chains");
    for(const auto& chain:chains){
        unsigned in_zone,reset,hits,sends;std::uint32_t ex1,ex2;std::string packet;
        std::istringstream row(chain);row>>in_zone>>reset>>hits>>packet>>ex1>>ex2>>sends;check(bool(row),"invalid chain");
        Scene scene;EffectStatus status;status.in_zone=in_zone!=0;auto source=std::make_shared<Actor>(42,status,PlayerResources{1,100,100},std::vector<PersistedEffect>{},scene,rules);
        Actor target(77,status,PlayerResources{1,100,100},std::vector<PersistedEffect>{},scene,rules);Manager manager;manager.effect.id=11;manager.effect.level=2;manager.effect.detail=22;manager.effect.effect=1;manager.info.target_kind=3;manager.info.multi_attack=1;manager.info.reset_seconds=7;
        UserSkillRuntime owner(source,manager);net::Packet seed{4,5,42,0,0,0,1,11,0,2,0,0,0};check(owner.load(seed,0)==UserSkillLoadResult::loaded,"seed failed");
        // Live runtime IDs deliberately differ; announcement reads definition IDs.
        owner.slot(0)->skill_id=60000;owner.slot(0)->level=99;
        View view(manager.effect,manager.info);Strike strike(view);
        user_skill_runtime_attack_user(owner,owner.slot(0),100,&target,strike);
        check(owner.slot(0)->reset_tick==reset&&strike.hits==hits&&view.sends==sends,"resolved announcement chain differs");
        check(view.packet==packet&&view.ex1==ex1&&view.ex2==ex2,"definition IDs/exclusions differ");
    }
    std::cout<<cases<<" complete original announcements and 2 resolved multihit compositions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
