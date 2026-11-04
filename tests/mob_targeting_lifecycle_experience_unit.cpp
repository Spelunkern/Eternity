#include "eternity/game/mob_targeting_lifecycle_experience.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
void check(bool b,const std::string& s) {if(!b)throw std::runtime_error(s);}
void number(Bytes& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::string hex(const Bytes& p) {constexpr char digits[]="0123456789abcdef";std::string s;for(auto n:p){s+=digits[n>>4];s+=digits[n&15];}return s;}
struct Scene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected scene reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target reset");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected user grid");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct World final:MobTargetingSoloDeathWorld,ExperienceAwardPorts {
    Actor& victim;Actor& recipient;mutable PlayerCore core;const StatusRules& rules;const Experience& curve;
    Bytes events;bool grouped=false;char failure=0;
    World(Actor& v,Actor& r,const StatusRules& s,const Experience& e):victim(v),recipient(r),rules(s),curve(e) {core.character.id=r.id();core.user_id=123;}
    bool has_party(const Actor& a) const noexcept override {if(&a!=&recipient)std::terminate();return grouped;}
    PlayerCore& player(Actor& a) const noexcept override {if(&a!=&recipient)std::terminate();return core;}
    const StatusRules& status_rules() const noexcept override {return rules;}
    const Experience& experience_curve() const noexcept override {return curve;}
    ExperienceAwardPorts& award_ports(Actor& a) const noexcept override {if(&a!=&recipient)std::terminate();return const_cast<World&>(*this);}
    void maybe_fail(char stage) {if(failure==stage)throw std::runtime_error("Injected required reward provider failure");}
    void packet(char tag,const Bytes& p) {events.push_back(static_cast<std::uint8_t>(tag));number(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());maybe_fail(tag);}
    void send_database(const Bytes& p) override {check(recipient.status().level==core.character.level,"Actor level not published before database callback");packet('B',p);}
    void send_self(const Bytes& p) override {packet('C',p);}
    void level_up(PlayerCore& p,BaseStatus&,std::uint32_t amount,std::uint32_t source) override {
        check(&p==&core&&recipient.status().level==p.character.level,"Actual retained level owner differs");
        events.push_back('L');number(events,amount);number(events,source);number(events,p.experience);number(events,p.character.level);maybe_fail('L');
    }
    void attribution(char stage,Actor& r,Actor& v) {
        check(&r==&recipient&&&v==&victim,"Wrong retained reward/drop subjects");
        events.push_back(static_cast<std::uint8_t>(stage));number(events,core.experience);number(events,core.character.level);maybe_fail(stage);
    }
    void kill_from(Actor& r,Actor& v) override {attribution('F',r,v);}
    void drop_by_user(Actor& v,Actor& r) override {attribution('I',r,v);}
};
Bytes core_state(const World& w) {
    const auto& p=w.core;const auto& c=p.character;const auto& base=w.recipient.status().skills.equipment.base;Bytes out;
    number(out,c.level,2);number(out,p.stat_points,2);number(out,p.skill_points,2);number(out,p.experience);
    for(auto n:{c.strength,c.dexterity,c.intelligence,c.wisdom,c.recovery,c.luck})number(out,n,2);
    for(auto n:{base.maximum.hp,base.maximum.mp,base.maximum.sp})number(out,n);
    const auto& a=base.attributes;
    for(auto n:{a.strength,a.dexterity,a.intelligence,a.wisdom,a.recovery,a.luck})number(out,static_cast<std::uint32_t>(n));
    return out;
}
int main(int argc,char** argv) {
    try {
        check(argc==3,"Expected helper and solo original fixtures");
        std::ifstream helper(argv[1]);check(bool(helper),"Missing GetExp fixture");std::string line;unsigned helpers=0;
        while(std::getline(helper,line)) {
            if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned level,recipient,result;row>>level>>recipient>>result;check(bool(row),"Bad GetExp fixture");
            const auto actual=mob_targeting_user_death_experience(static_cast<std::uint16_t>(level),std::bit_cast<std::int32_t>(recipient));
            check(std::bit_cast<unsigned>(actual)==result,"Original user experience differs at "+std::to_string(helpers));++helpers;
        }
        check(helpers==8192,"GetExp coverage differs");
        eternity::storage::ResultSet definitions;definitions.columns={"Level","Grow","Exp"};
        for(unsigned grow=1;grow<=4;++grow)for(unsigned level=1;level<=60;++level)definitions.rows.push_back({std::to_string(level),std::to_string(grow),"1000"});
        const auto curve=Experience::load(definitions);
        eternity::storage::ResultSet status_table;status_table.columns={"Job","Level","HP","MP","SP"};const auto rules=StatusRules::load(status_table);
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;Scene scene;
        std::ifstream solo(argv[2]);check(bool(solo),"Missing solo original fixture");unsigned solos=0;
        while(std::getline(solo,line)) {
            if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned n;char bar;std::string events,core;row>>n>>bar>>events>>bar>>core;check(bool(row)&&bar=='|',"Bad solo fixture");
            const auto grow=n%5;
            const auto vl=std::array<unsigned,7>{0,1,10,20,29,60,65535}[(n/5)%7];
            const auto level=std::array<unsigned,12>{0,1,5,19,20,25,29,30,59,60,61,65535}[(n/7)%12];
            const auto old=std::array<unsigned,4>{0,999,59000,0xfffffff0}[(n/11)%4];
            const auto doubled=(n/13)%2,half=(n/17)%2;const auto bonus=static_cast<int>((n/19)%6)-1;
            const auto source=std::array<unsigned,4>{0,777,0xffffffff,123}[(n/23)%4];
            EffectStatus vstate,rstate;vstate.level=static_cast<std::uint16_t>(vl);rstate.level=static_cast<std::uint16_t>(level);
            rstate.skills.benefits.experience_double=static_cast<int>(doubled);rstate.skills.benefits.experience_half_bonus=static_cast<int>(half);rstate.skills.benefits.amulet=bonus;
            Actor victim(9001,vstate,{0,0,0},{},scene,{items,enchant,attacks});Actor recipient(321,rstate,{11,22,33},{},scene,{items,enchant,attacks});
            victim.attack_state().mode=1;recipient.attack_state().mode=7;recipient.status().casting=6;recipient.status().current_target=77;
            World world(victim,recipient,rules,curve);world.core.character.level=static_cast<std::uint16_t>(level);world.core.character.grow=static_cast<std::uint8_t>(grow);world.core.experience=old;
            mob_targeting_solo_death_reward(victim,recipient,source,world);
            check(hex(world.events)==events,"Original solo reward events differ at "+std::to_string(n));
            check(hex(core_state(world))==core,"Original solo recipient state differs at "+std::to_string(n));
            check(victim.attack_state().mode==1&&recipient.attack_state().mode==7&&recipient.resources().hp==11&&recipient.status().casting==6&&recipient.status().current_target==77,"Solo rewards changed lifecycle/resource fields");
            ++solos;
        }
        check(solos==1024,"Solo coverage differs");
        {
            EffectStatus state;state.level=1;Actor v(9001,state,{0,0,0},{},scene,{items,enchant,attacks});Actor r(321,state,{10,20,30},{},scene,{items,enchant,attacks});
            World w(v,r,rules,curve);w.core.character.level=1;w.grouped=true;bool rejected=false;
            try{mob_targeting_solo_death_reward(v,r,777,w);}catch(const std::logic_error&){rejected=true;}
            check(rejected&&w.events.empty()&&w.core.experience==0,"Party rewards fabricated solo success");
            w.grouped=false;w.failure='B';rejected=false;
            try{mob_targeting_solo_death_reward(v,r,777,w);}catch(const std::runtime_error&){rejected=true;}
            check(rejected&&w.core.experience==72&&!w.events.empty()&&w.events[0]=='B',"Provider failure rolled back or skipped experience publication");
            const auto old=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"Cannot exercise alternate rounding");rejected=false;
            try{(void)mob_targeting_user_death_experience(1,1);}catch(const std::logic_error&){rejected=true;}
            std::fesetround(old);check(rejected,"Unsupported floating environment silently admitted");
        }
        std::cout<<helpers<<" complete original user GetExp calls and "<<solos<<" original solo Death reward regions with actual Actor/experience/level-up compositions passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
