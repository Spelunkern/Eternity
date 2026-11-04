#include "eternity/game/user_skill_runtime_spend_group.hpp"
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

struct World;
struct Group final:UserSkillSpendGroup {
    World& world;unsigned id;std::uint8_t type;
    Group(World& w,unsigned i,std::uint8_t t):world(w),id(i),type(t){}
    void lock() override;void unlock() noexcept override;
    std::uint8_t kind() const override{return type;}
};
struct World final:UserSkillSpendGroupWorld {
    std::array<std::shared_ptr<Group>,4> groups{};unsigned member,lock_member,after;
    std::string trace;bool throw_send=false;
    World(unsigned k,unsigned m,unsigned l,unsigned a):member(m),lock_member(l),after(a){for(unsigned i=1;i<4;++i)groups[i]=std::make_shared<Group>(*this,i,static_cast<std::uint8_t>((k+i-1)%2));}
    void event(std::string s){if(!trace.empty())trace+=',';trace+=s;}
    UserSkillSpendGroupLease membership(Actor&)override{return groups[member];}
    void send(UserSkillSpendGroup& g,Actor& actor,const net::Packet& packet) override {
        const auto& group=static_cast<Group&>(g);event("P"+std::to_string(group.id)+":"+hex(packet));
        if(throw_send)throw std::runtime_error("transport failed");
        if(packet[6]==1){member=after;actor.heal({0,0,static_cast<std::int32_t>(0x12345678-actor.resources().mp)});}
    }
};
void Group::lock(){world.event("L"+std::to_string(id));world.member=world.lock_member;}
void Group::unlock()noexcept{world.event("U"+std::to_string(id));}
}
int main(int argc,char** argv){try{
    check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};
    std::string line;unsigned count=0;
    while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        unsigned n,k,m,l,a,sc,mc;std::string expected;std::istringstream row(line);row>>n>>k>>m>>l>>a>>sc>>mc>>expected;check(bool(row),"invalid fixture");
        const auto debit=user_skill_runtime_spend_transition(100,100,static_cast<std::uint16_t>(sc),static_cast<std::uint16_t>(mc),0,0);
        Scene scene;EffectStatus status;status.skills.equipment.base.maximum={1,0x7fffffff,0x7fffffff};
        Actor actor(42,status,PlayerResources{1,debit.mp,debit.sp},std::vector<PersistedEffect>{},scene,rules);
        World world(k,0,l,a);scene.on_send=[&](const net::Packet& p){world.event("S"+hex(p));world.member=m;};
        actor.send_self(debit.self_packet);
        user_skill_runtime_spend_group(actor,debit,world);
        check(world.trace==expected,"original group trace differs case "+std::to_string(n)+" actual "+world.trace+" expected "+expected);++count;
    }
    check(count==480,"incomplete fixture");
    Scene scene;EffectStatus status;Actor actor(42,status,PlayerResources{1,97,98},std::vector<PersistedEffect>{},scene,rules);
    const auto debit=user_skill_runtime_spend_transition(100,100,2,3,0,0);
    World invalid(0,1,1,0);bool failed=false;
    try{user_skill_runtime_spend_group(actor,debit,invalid);}catch(const std::logic_error&){failed=true;}
    check(failed&&invalid.trace.ends_with("U1"),"null MP dispatch did not fail and release lock");
    World throwing(0,1,1,1);throwing.throw_send=true;failed=false;
    try{user_skill_runtime_spend_group(actor,debit,throwing);}catch(const std::runtime_error&){failed=true;}
    check(failed&&throwing.trace.ends_with("U1"),"transport failure leaked lock");
    World unknown(0,1,1,1);unknown.groups[1]->type=2;failed=false;
    try{user_skill_runtime_spend_group(actor,debit,unknown);}catch(const std::logic_error&){failed=true;}
    check(failed&&unknown.trace=="L1,U1","unknown original opcode silently fabricated");
    std::cout<<count<<" whole original group coordinator traces and 3 failure probes passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
