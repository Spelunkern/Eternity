#include "eternity/game/user_skill_runtime_ready.hpp"
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
struct World:UserSkillReadyWorld {
    UserSkillRuntime& owner;unsigned profile;std::uint32_t now;bool mutation;std::string events;
    World(UserSkillRuntime& o,unsigned p,std::uint32_t n,bool m):owner(o),profile(p),now(n),mutation(m){}
    std::string snapshot()const{auto p=owner.slot(0),q=owner.slot(1);return std::to_string(owner.learned_count())+','+std::to_string(owner.selection())+','+std::to_string(p->skill_id)+','+std::to_string(p->level)+','+std::to_string(q->reset_tick);}
    void event(std::string tag){if(!events.empty())events+='|';events+=tag+':'+snapshot();}
    void mutate(unsigned id){owner.reset_character();auto p=owner.slot(0);p->skill_id=id;p->level=3;}
    std::uint32_t tick_ms()override{event("T");if(profile==8)owner.slot(1)->reset_tick=now;if(profile==9)owner.slot(1)->reset_tick=now+1;if(profile==10){mutate(99);owner.slot(1)->reset_tick=now;}return profile==15?now+2:now;}
    bool range(std::uint8_t r){event("R"+std::to_string(r));if(mutation)mutate(77);return profile!=12;}
    bool in_user_range(Actor&,Actor&,std::uint8_t r)override{return range(r);}
    bool in_mob_range(Actor&,MobRuntime&,std::uint8_t r)override{return range(r);}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,mob,profile,mutation,result;std::uint32_t now;std::string expected,events;std::istringstream row(line);row>>n>>mob>>profile>>now>>mutation>>result>>expected>>events;check(bool(row),"invalid fixture");
        Scene scene;auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Actor peer(77,{}, {},{},scene,rules);MobRuntime target;Manager manager;manager.info.attack_kind=1;manager.info.attack_range=7;manager.info.required_skill=profile==7?33:(profile==5||profile==6||profile==8||profile==9||profile==10||profile==14||profile==15)?22:0;
        UserSkillRuntime owner(actor,manager);net::Packet wire{4,5,42,0,0,0,static_cast<std::uint8_t>(profile==14?3:2),11,0,2,0,0,0,22,0,1,1,0,0};if(profile==14){net::Packet more{22,0,1,2,0,0};wire.insert(wire.end(),more.begin(),more.end());}check(owner.load(wire,now)==UserSkillLoadResult::loaded,"seed failed");owner.selection()=17;
        auto active=owner.slot(0);active->reset_tick=0;
        if(profile==1||profile==3)active->reset_tick=now+1;
        if(profile==2||profile==4){active->reset_tick=now+1000;actor->status().skills.cool_time=profile==2?1:0x7fffffff;}
        if(profile==3)actor->status().skills.cool_time=-1;
        owner.slot(1)->reset_tick=profile==6||profile==9||profile==14?now:now+1;if(profile==14)owner.slot(2)->reset_tick=now+1;if(profile==11)actor->status().skills.attack_disabled[1]=-1;
        World world(owner,profile,now,mutation!=0);scene.on_send=[&](const net::Packet& p){world.event("S"+hex(p));if(mutation)world.mutate(88);};
        const bool actual=mob?user_skill_runtime_ready_mob_after_equipment(owner,active,manager.info,now,profile==13?nullptr:&target,world):user_skill_runtime_ready_user_after_equipment(owner,active,manager.info,now,profile==13?nullptr:&peer,world);
        if(events=="-")events.clear();check(actual==bool(result)&&world.snapshot()==expected&&world.events==events,"readiness mismatch case "+std::to_string(n)+" actual "+world.snapshot()+" expected "+expected+"\nactual events "+world.events+"\nexpected events "+events);++cases;
    }
    check(cases==256,"incomplete readiness fixture");std::cout<<cases<<" complete original overloads / retained-runtime suffix comparisons passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
