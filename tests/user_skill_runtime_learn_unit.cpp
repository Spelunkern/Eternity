#include "eternity/game/user_skill_runtime_learn.hpp"
#include <bit>
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
std::string hex(const net::Packet& p){constexpr char d[]="0123456789abcdef";std::string out;for(auto b:p){out+=d[b>>4];out+=d[b&15];}return out;}
struct Manager:UserSkillRuntimeServices {
    unsigned profile=0,queries=0,counter=500;bool seed=true;UserSkillHandle last;std::vector<UserSkillHandle> stranded;std::function<void(std::string)> event;
    std::array<SkillDefinition,5> effects{};
    std::array<UserSkillDefinition,5> defs{{{effects[0],"Requested"},{effects[1],"Validation"},{effects[2],"Old"},{effects[3],"After"},{effects[4],"Context"}}};
    Manager(){for(auto& d:defs){d.required_level=10;d.grow=1;d.learn_points=7;d.type_show=1;d.jobs[2]=1;}defs[1].learn_points=0;}
    const UserSkillDefinition* definition(std::uint16_t,std::uint8_t)override{
        if(seed)return &defs[2];++queries;event("Q"+std::to_string(queries));if(profile==4||(profile==21&&queries==2))return nullptr;return &defs[profile==20&&queries==2?1:0];
    }
    UserSkillHandle allocate()override{
        if(!seed){event("H");if(profile==7)return {};}
        last=std::make_shared<UserSkillRuntimeSkill>();last->instance_id=++counter;last->skill_id=last->level=last->number=last->attack_tick=last->reset_tick=last->end_tick=last->previous_tick=last->attacker_type=last->attacker_id=0x11223344;return last;
    }
    void destroy(UserSkillHandle)noexcept override{}
    void strand(UserSkillHandle p)noexcept override{stranded.push_back(std::move(p));}
    void invalid_definition(std::uint16_t,std::uint8_t)override{throw std::runtime_error("unexpected load diagnostic");}
    void disconnect(std::uint32_t,std::uint32_t)override{throw std::runtime_error("unexpected load disconnect");}
};
struct World:UserSkillLearnWorld {
    UserSkillRuntime& owner;PlayerCore& player;Manager& manager;unsigned profile,existing;std::uint32_t tick;std::string events;
    World(UserSkillRuntime& o,PlayerCore& p,Manager& m,unsigned pr,unsigned ex,std::uint32_t t):owner(o),player(p),manager(m),profile(pr),existing(ex),tick(t){}
    std::string snapshot()const{
        std::string out=std::to_string(player.skill_points)+'/'+std::to_string(owner.learned_count())+'/'+std::to_string(owner.selection());
        for(unsigned i=0;i<3;++i){out+='/';auto p=owner.slot(i);if(!p){out+='-';continue;}bool first=true;
            for(auto v:{p->instance_id,p->skill_id,p->level,p->number,p->attack_tick,p->reset_tick,p->end_tick,p->previous_tick,p->attacker_type,p->attacker_id,static_cast<std::uint32_t>(p->definition-manager.defs.data())}){if(!first)out+=',';first=false;out+=std::to_string(v);}
        }return out;
    }
    void event(std::string tag){if(!events.empty())events+='|';events+=tag+':'+snapshot();}
    UserSkillHandle target(){return existing==1?owner.slot(0):manager.last;}
    std::uint32_t tick_ms()override{event("T");if(profile==22)owner.reset_character();return tick;}
    void apply_passive(UserSkillRuntime&,const UserSkillDefinition&)override{event("B");if(profile==18){auto p=target();p->skill_id=99;p->level=3;p->number=9;p->definition=&manager.defs[3];player.skill_points=55;}}
    void send_database(const net::Packet& p)override{event("D"+hex(p));if(profile==19){target()->definition=&manager.defs[3];player.skill_points=55;}}
    void self(const net::Packet& p){event("S"+hex(p));if(profile==24)target()->definition=&manager.defs[3];}
    void populate_log(Actor&,std::span<std::uint8_t> p)override{
        event("L");if(profile==23)target()->definition=&manager.defs[4];
        const auto put=[&](unsigned offset,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)p[offset-2+i]=static_cast<std::uint8_t>(v>>(8*i));};
        put(2,42,4);const std::string account="Account",name="Hero";std::copy(account.begin(),account.end(),p.begin()+4);put(25,99,4);std::copy(name.begin(),name.end(),p.begin()+27);
        put(50,profile==30?65535:10,2);put(52,1234,4);put(56,7,2);put(58,0x3f800000,4);put(62,0x40000000,4);put(66,0x40400000,4);put(70,0x12345678,4);
    }
    void send_log(const net::Packet& p)override{event("G"+hex(p));}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned rows=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,profile,existing,counter;int level;std::uint32_t tick;std::string final,events;std::istringstream row(line);row>>n>>profile>>existing>>level>>tick>>final>>counter>>events;check(bool(row),"invalid fixture");
        Scene scene;EffectStatus status;status.level=profile==30?65535:10;auto actor=std::make_shared<Actor>(42,status,PlayerResources{},std::vector<PersistedEffect>{},scene,rules);actor->account_id()=42;actor->appearance().identity.job=2;actor->appearance().identity.grow=1;
        Manager manager;manager.profile=profile;auto& d=manager.defs[0];auto& identity=actor->appearance().identity;
        if(profile==9)d.required_level=11;if(profile==10)d.country=1;if(profile==11)d.jobs[2]=0;if(profile==12)d.grow=2;if(profile==13)identity.grow=0;if(profile==14){identity.grow=2;d.grow=3;}if(profile==15)d.type_show=0;
        if(profile==16){identity.family=1;d.country=2;}if(profile==17){identity.grow=3;d.grow=3;}if(profile==25)d.country=6;if(profile==26){d.country=3;identity.family=3;}if(profile==27){d.country=4;identity.family=2;}if(profile==28){d.country=5;identity.family=3;}if(profile==29){d.country=5;identity.family=1;}if(profile==30)d.required_level=65535;
        UserSkillRuntime owner(actor,manager);if(existing){net::Packet packet{4,5,42,0,0,0,1,static_cast<std::uint8_t>(existing==1?11:12),0,1,0,0,0};check(owner.load(packet,tick)==UserSkillLoadResult::loaded,"seed load failed");auto p=owner.slot(0);p->number=0xf1000005;p->attack_tick=71;p->reset_tick=72;p->end_tick=73;p->previous_tick=74;p->attacker_type=75;p->attacker_id=76;if(profile==31)p->level=0xffffffff;}
        owner.selection()=17;PlayerCore player;player.character.id=42;player.skill_points=profile==8||profile==20?1:20;World world(owner,player,manager,profile,existing,tick);manager.seed=false;manager.event=[&](std::string s){world.event(std::move(s));};scene.on_send=[&](const net::Packet& p){world.self(p);};
        owner.learn(player,11,level,world);if(events=="-")events.clear();check(world.snapshot()==final&&world.events==events&&manager.counter==counter,"learn mismatch case "+std::to_string(n)+"\nactual "+world.snapshot()+"\nexpected "+final+"\nactual events "+world.events+"\nexpected events "+events);++rows;
    }
    check(rows==192,"incomplete learned fixture");std::cout<<rows<<" complete original learned admission/upgrade/notification/log sequences passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
