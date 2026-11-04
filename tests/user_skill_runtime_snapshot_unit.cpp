#include "eternity/game/user_skill_runtime_snapshot.hpp"
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
struct Clock:UserSkillSnapshotClock {
    UserSkillRuntime& owner;std::span<std::uint8_t,2051> scratch;std::string& events;std::uint32_t tick;unsigned flags,calls=0;
    Clock(UserSkillRuntime& o,std::span<std::uint8_t,2051> s,std::string& e,std::uint32_t t,unsigned f):owner(o),scratch(s),events(e),tick(t),flags(f){}
    std::uint32_t tick_ms()override{++calls;events="T,"+hex(scratch.first(3));if(flags&1)owner.reset_character();return tick;}
};
void put(net::Packet& p,unsigned v,unsigned bytes){for(unsigned i=0;i<bytes;++i)p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};std::string line;unsigned cases=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;unsigned n,count,pattern,flags,fill,final_count,selection;std::uint32_t tick,expected_hash;std::string expected,expected_events;
        std::istringstream row(line);row>>n>>count>>pattern>>tick>>flags>>fill>>final_count>>selection>>expected_hash>>expected>>expected_events;check(bool(row),"invalid fixture");
        Scene scene;auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules);Manager manager;UserSkillRuntime owner(actor,manager);
        for(unsigned start=0;start<count;start+=255){const auto end=std::min(count,start+255);net::Packet packet{4,5,42,0,0,0,static_cast<std::uint8_t>(end-start)};
            for(unsigned i=start;i<end;++i){const auto slot=pattern==0?i:pattern==1?(i*3+7)%256:pattern==2?255:i%3;put(packet,11,2);put(packet,1,1);put(packet,slot,1);put(packet,0,2);}check(owner.load(packet,tick)==UserSkillLoadResult::loaded,"seed failed");
        }
        owner.selection()=17;constexpr std::uint32_t deltas[]={0,999,1000,0xffffffff,0x7fffffff,0x80000000,0x800003e8,0xfffffc18};
        for(unsigned i=0;i<256;++i)if(auto p=owner.slot(i)){p->skill_id=0x12340000+i;p->level=0xff000203;p->number=0x1fe + i;p->reset_tick=tick+deltas[(i+n)%8];}
        std::array<std::uint8_t,2051> scratch;scratch.fill(static_cast<std::uint8_t>(fill));std::string actual,events;Clock clock(owner,scratch,events,tick,flags);
        scene.on_send=[&](const net::Packet& p){actual=hex(p);events+="|S";if(flags&2)owner.reset_character();};
        user_skill_runtime_snapshot(owner,clock,scratch);std::uint32_t hash=2166136261U;for(auto b:scratch){hash^=b;hash*=16777619U;}
        check(actual==expected&&hash==expected_hash&&events==expected_events&&clock.calls==1&&static_cast<unsigned>(owner.learned_count())==final_count&&owner.selection()==selection,"original snapshot mismatch case "+std::to_string(n)+" actual "+actual+" expected "+expected);++cases;
    }
    check(cases==192,"incomplete snapshot fixture");std::cout<<cases<<" complete original admission/sparse learned snapshot sequences passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
