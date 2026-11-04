#include "eternity/game/mob_applied_skills_effects.hpp"
#include "eternity/game/mob_applied_skills_expiration.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value) {if(!value)throw std::runtime_error("Mob skill effects comparison failed");}
std::uint32_t bits(std::int32_t value) {return std::bit_cast<std::uint32_t>(value);}
std::vector<std::uint32_t> words(const MobRuntime& m,const SkillDefinition& definition) {
    std::vector<std::uint32_t> out{bits(m.wisdom),bits(m.dexterity),bits(m.luck),bits(m.attack_speed),bits(m.move_speed)};
    for(const auto& a:m.additions)out.insert(out.end(),{bits(a.accuracy),bits(a.attack),bits(a.evasion),bits(a.defense)});
    for(auto value:m.attack_disabled)out.push_back(bits(value));
    out.insert(out.end(),{bits(m.stun),bits(m.stop),bits(m.sleep),bits(m.attribute),m.vision_fixed_user,m.hp,bits(m.status),m.target_id});
    for(const auto& ability:definition.abilities)out.insert(out.end(),{ability.type,ability.value});
    return out;
}
std::string hex(const std::vector<std::uint32_t>& input) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(auto value:input)for(unsigned i=0;i<4;++i) {
        const auto byte=(value>>(i*8))&255;out+=digits[byte>>4];out+=digits[byte&15];
    }
    return out;
}
std::vector<std::uint32_t> decode(const std::string& input) {
    require(input.size()%8==0);std::vector<std::uint32_t> out;
    for(std::size_t i=0;i<input.size();i+=8) {
        std::uint32_t value=0;
        for(unsigned b=0;b<4;++b)value|=static_cast<std::uint32_t>(std::stoul(input.substr(i+b*2,2),nullptr,16))<<(b*8);
        out.push_back(value);
    }
    return out;
}
void assign(MobRuntime& m,const std::vector<std::uint32_t>& input) {
    require(input.size()==28);std::size_t at=0;
    auto next=[&] {return std::bit_cast<std::int32_t>(input[at++]);};
    m.wisdom=next();m.dexterity=next();m.luck=next();m.attack_speed=next();m.move_speed=next();
    for(auto& a:m.additions){a.accuracy=next();a.attack=next();a.evasion=next();a.defense=next();}
    for(auto& value:m.attack_disabled)value=next();
    m.stun=next();m.stop=next();m.sleep=next();m.attribute=next();
    m.vision_fixed_user=input[at++];m.hp=input[at++];m.status=next();m.target_id=input[at++];
}
struct Services final:MobAppliedSkillEffectServices {
    SkillDefinition& definition;bool mutate=false;unsigned damage=0;
    std::vector<std::string> events;
    explicit Services(SkillDefinition& d):definition(d) {}
    void event(MobRuntime& m,char kind) {
        events.push_back(std::string(1,kind)+hex(words(m,definition)));
        if(mutate) {
            m.wisdom=std::bit_cast<std::int32_t>(bits(m.wisdom)^0x11111111U);
            definition.abilities[1]={8,65535};
        }
    }
    void rebuild_attack(MobRuntime& m) noexcept override {event(m,'A');}
    void send_speed(MobRuntime& m) noexcept override {event(m,'S');}
    void finish_damage_skill(MobRuntime& m,MobAppliedSkill&,const SkillDefinition& d) noexcept override {
        require(d.detail==0x3d || d.detail==0x3f);++damage;event(m,'D');
    }
};
struct Composed final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,
                       MobAppliedSkillRemovalServices,MobAppliedSkillEffectServices {
    MobRuntime& mob;std::vector<char> trace;
    explicit Composed(MobRuntime& m):mob(m) {}
    std::unique_ptr<MobAppliedSkill> construct() override {return std::make_unique<MobAppliedSkill>();}
    void removed(const MobAppliedSkill&,std::uint8_t action) noexcept override {require(action==6);trace.push_back('L');}
    void remove_effects(MobAppliedSkill& skill) noexcept override {
        trace.push_back('R');require(skill.definition!=nullptr);
        remove_mob_applied_skill_effects(mob,skill,*skill.definition,mob.definition->attribute,*this);
    }
    void rebuild_attack(MobRuntime& m) noexcept override {trace.push_back('A');rebuild_mob_attack(m);}
    void send_speed(MobRuntime&) noexcept override {trace.push_back('S');}
    void finish_damage_skill(MobRuntime&,MobAppliedSkill&,const SkillDefinition&) noexcept override {std::terminate();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2);std::ifstream input(argv[1]);require(input.good());unsigned cases=0;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);char kind;std::uint32_t selector,delta;unsigned attribute,mutate;
            SkillDefinition definition;row>>kind>>selector>>delta>>attribute>>mutate;
            for(auto& ability:definition.abilities) {
                unsigned type,value;row>>type>>value;
                ability={static_cast<std::uint8_t>(type),static_cast<std::uint16_t>(value)};
            }
            definition.detail=static_cast<std::uint16_t>(selector);
            std::string initial,expected,events;row>>initial>>expected>>events;require(!row.fail());
            MobRuntime mob;assign(mob,decode(initial));MobAppliedSkill skill;skill.instance_id=123;
            Services services(definition);services.mutate=mutate!=0;
            if(kind=='A')set_mob_applied_skill_ability(mob,selector,std::bit_cast<std::int32_t>(delta),services);
            else {require(kind=='R');remove_mob_applied_skill_effects(mob,skill,definition,static_cast<std::uint8_t>(attribute),services);}
            std::string actual_events;
            for(const auto& event:services.events){if(!actual_events.empty())actual_events+=',';actual_events+=event;}
            if(actual_events.empty())actual_events="-";
            if(hex(words(mob,definition))!=expected || actual_events!=events) {
                std::cerr<<"case "<<cases<<" state/events mismatch\nexpected "<<expected<<'\n'
                         <<"actual   "<<hex(words(mob,definition))<<"\nexpected events "<<events
                         <<"\nactual events   "<<actual_events<<'\n';return 1;
            }
            require(skill.instance_id==123 && services.damage==0);++cases;
        }
        require(cases==1282);
        // The remaining damage branch must call its required provider explicitly.
        for(auto detail:{0x3d,0x3f}) {
            MobRuntime mob;MobAppliedSkill skill;SkillDefinition definition;
            definition.detail=static_cast<std::uint16_t>(detail);Services services(definition);
            remove_mob_applied_skill_effects(mob,skill,definition,0,services);require(services.damage==1);
        }
        // Compose real expiration, effect changes and the native attack rebuild.
        MobRuntimeTemplate base;base.attribute=7;base.level=10;
        MobRuntime mob;mob.definition=&base;mob.stun=1;mob.attribute=99;mob.wisdom=10;mob.dexterity=20;
        Composed services(mob);MobAppliedSkillPool pool(services);MobAppliedSkills owner(pool,services);
        std::array<SkillDefinition,3> definitions{};
        definitions[0].detail=0x40;definitions[1].detail=0x43;definitions[1].abilities[0]={7,5};definitions[2].detail=0x44;
        for(auto& definition:definitions) {
            auto* skill=owner.append_from_pool();require(skill!=nullptr);skill->definition=&definition;skill->end_tick=5;
        }
        owner.expire(5,{true,false,false},services);
        require(owner.records().empty() && pool.available().size()==3);
        require(mob.stun==0 && mob.wisdom==15 && mob.attribute==7 && mob.attack[2].attack==25);
        require(services.trace==std::vector<char>({'L','R','S','L','R','A','L','R'}));
        std::cout<<cases<<" original effects cases and native expiration/attack composition matched\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
