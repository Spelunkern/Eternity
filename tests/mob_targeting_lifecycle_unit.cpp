#include "eternity/game/mob_targeting_lifecycle.hpp"
#include "eternity/game/maps.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
void append(eternity::net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i) p.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void pos(eternity::net::Packet& p,UserPosition v) {append(p,std::bit_cast<std::uint32_t>(v.x));append(p,std::bit_cast<std::uint32_t>(v.y));append(p,std::bit_cast<std::uint32_t>(v.z));}
std::string hex(const eternity::net::Packet& p) {static constexpr char h[]="0123456789abcdef";std::string s;for(auto x:p) {s+=h[x>>4];s+=h[x&15];}return s;}
std::string shot(const MobTargetingRebirthState& s) {
    eternity::net::Packet p;append(p,static_cast<std::uint32_t>(s.life));append(p,static_cast<std::uint32_t>(s.recovery));append(p,s.motion,1);
    append(p,s.current.hp);append(p,s.current.mp);append(p,s.current.sp);append(p,s.map,2);append(p,s.instance,2);pos(p,s.position);append(p,s.experience);append(p,s.in_zone,1);return hex(p);
}
struct Case final:MobTargetingRebirthServices {
    unsigned index=0,mutation=0;bool corpse=false,moveok=false,restore=false,mutated=false,reenter=false,entered=false,nested=false,fail=false;
    std::int32_t target=0,kind=0;MobTargetingRebirthState s;std::vector<std::string> events;
    UserPosition destination{101.25f,-2.5f,77.75f};
    explicit Case(unsigned n):index(n) {
        s.life=std::array<std::int32_t,8>{1,1,1,1,0,-1,2,1}[n%8];corpse=(n/8)%2;restore=(n/16)%2;target=std::array<int,4>{7,8,11,-1}[(n/32)%4];moveok=(n/128)%2;s.in_zone=(n/256)%2;mutation=(n/7)%9;
        s.recovery=std::array<int,4>{-5,0,1,7}[n%4];s.motion=9;s.current={11,22,33};s.maximum={65537,70000,0xffffffff};s.id=9001;s.experience=0x12345678;s.map=7;s.instance=3;s.position={1.25f,2.5f,3.75f};kind=static_cast<std::int32_t>(n*257)-512;
        if(n>=1536) {s.life=1;corpse=true;restore=n%2;target=n%4<2?7:8;moveok=true;s.in_zone=true;mutation=0;reenter=true;}
    }
    void event(char stage,const std::string& extra="") {events.push_back(std::string(1,stage)+extra+"/"+shot(s));}
    void mutate(char stage) {
        const auto selected=std::array<char,9>{0,'F','D','M','S','V','G','L','C'}[mutation];
        if(mutated || !(selected==stage || (mutation==3 && stage=='W'))) return;
        mutated=true;s.life=7;s.recovery=-3;s.motion=8;s.current={65537+index,7,9};s.map=8;s.instance=77;s.position={12,-3,8};s.experience=0xfedcba98;s.in_zone=!s.in_zone;
    }
    void run() {mob_targeting_rebirth(s,destination,kind,target,restore,*this);}
    std::optional<std::uint32_t> find_dead(std::uint32_t id) override {
        check(id==9001,"Wrong corpse user");event('F');
        if(reenter && !entered) {entered=true;nested=true;mob_targeting_rebirth(s,destination,kind,target,false,*this);nested=false;return 123;}
        mutate('F');return corpse && !nested?std::optional<std::uint32_t>{123}:std::nullopt;
    }
    void leave_dead(std::uint32_t id) override {check(id==123,"Wrong corpse handle");event('D');if(fail) throw std::runtime_error("Injected corpse failure");mutate('D');}
    bool move_user(std::uint16_t instance,const UserPosition& p) override {eternity::net::Packet b;pos(b,p);event('M',std::to_string(instance)+":"+hex(b));if(moveok) s.position=p;mutate('M');return moveok;}
    bool map_present(std::int32_t map) override {return map<10;}
    void zone_leave_user_move(std::int32_t map,const UserPosition& p) override {eternity::net::Packet b;pos(b,p);event('W',std::to_string(map)+":"+hex(b));s.map=static_cast<std::uint16_t>(map);s.instance=5;s.position=p;s.in_zone=false;mutate('W');}
    void send_cell_view(const eternity::net::Packet& p) override {event('V',hex(p));mutate('V');}
    void send_self(const eternity::net::Packet& p) override {event('S',hex(p));mutate('S');}
    void prepare_game_log(eternity::net::Packet& p) override {event('G');for(unsigned i=0;i<74;++i) p[i]=static_cast<std::uint8_t>(i*3);mutate('G');}
    void send_game_log(const eternity::net::Packet& p) override {event('L',hex(p));mutate('L');}
    void send_recover_change(PlayerResources r) override {eternity::net::Packet b;append(b,r.hp);append(b,r.mp);append(b,r.sp);event('C',hex(b));mutate('C');}
};
struct Scene final:ActorScene {
    unsigned released=0;
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t id) noexcept override {if(id!=33) std::terminate();++released;}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected stop");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected zone");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct World final:MobTargetingLifecycleWorld {
    ZoneUsers& users;std::uint8_t raw_presentation=9;bool heal=true,called=false;unsigned views=0,logs=0,changes=0;PlayerResources sent{};
    explicit World(ZoneUsers& u):users(u) {}
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();s.account_id=a.account_id();s.experience=12345;s.map=7;s.position=*users.position(a.id());return s;}
    std::uint16_t instance(const Actor&) const override {return 3;}
    // Explicit fixture projection for the missing distinct +1434 accessor.
    // Keep actual Actor attack motion untouched; no production shadow owner.
    std::uint8_t presentation_motion(const Actor&) const override {return raw_presentation;}
    void presentation_motion(Actor&,std::uint8_t m) override {raw_presentation=m;}
    void replace_resources(Actor&,PlayerResources) override {throw std::runtime_error("Missing exact resource owner setter");}
    std::optional<std::uint32_t> find_dead(Actor& a,std::uint32_t id) override {check(a.id()==id && a.attack_state().mode==1,"Wrong retained Actor publication");called=true;if(heal) a.heal({1,2,3});return 123;}
    void leave_dead(Actor&,std::uint32_t id) override {check(id==123,"Wrong real corpse handle");}
    bool move_user(Actor& a,std::uint16_t i,const UserPosition& p) override {check(i==3,"Wrong instance");return users.move_at_position(a.id(),p);}
    bool map_present(std::int32_t) override {throw std::runtime_error("Unexpected cross-map lookup");}
    void zone_leave_user_move(Actor&,std::int32_t,const UserPosition&) override {throw std::runtime_error("Unexpected cross-map move");}
    void send_cell_view(Actor& a,const eternity::net::Packet& p) override {check(p.size()==23 && p[0]==0x51 && p[1]==5 && a.attack_state().mode==1,"Invalid rebirth delivery/order");++views;}
    void send_recover_change(Actor&,PlayerResources r) override {sent=r;++changes;}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_game_log(const eternity::net::Packet& p) override {check(p.size()==74 && p[0]==4 && p[1]==3,"Missing actual game log header");++logs;}
};
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected rebirth oracle");std::ifstream in(argv[1]);check(in.good(),"Cannot open original fixture");std::string line;unsigned cases=0;
        while(std::getline(in,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned index=0;char separator=0;std::string final;row>>index>>separator>>final>>separator;check(separator=='|',"Malformed fixture");
            Case c(index);c.run();check(shot(c.s)==final,"Original final rebirth state differs at "+std::to_string(index));
            std::vector<std::string> events;std::string e;while(row>>e) events.push_back(e);
            if(c.events!=events) {std::ostringstream out;out<<"Original ordered trace differs at "<<index<<"\n";for(const auto& v:c.events) out<<v<<' ';throw std::runtime_error(out.str());}
            ++cases;
        }
        check(cases==1552,"Rebirth coverage changed");
        // Native provider exception: already-published writes remain; tail is not
        // run. This tests our C++ boundary, not Windows SEH equivalence.
        Case fault(24);fault.mutation=0;fault.fail=true;bool threw=false;try {fault.run();}catch(const std::runtime_error&) {threw=true;}
        check(threw && fault.s.life==1 && fault.s.current.hp==fault.s.maximum.hp,"Failure rolled back or ran success tail");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;Scene scene;
        {
            Actor actor(9001,{}, {10,20,30},{},scene,{items,enchant,attacks});actor.status().skills.equipment.base.maximum={100,200,300};actor.attack_state().mode=1;actor.attack_state().motion=0xa5;actor.attack_state().recovery=-5;
            actor.status().current_target=77;actor.status().current_target_type=2;actor.status().casting=7;actor.attack_state().previous_tick=111;actor.attack_state().next_tick=222;
            SkillDefinition effect;effect.id=55;effect.level=1;actor.effects().append(33,{{55,1,6000},0,6000,false},effect);
            ZoneUsers users(MapGeometry(120),0);users.attach_at_position(actor,{1.25f,2.5f,3.75f});World world(users);
            mob_targeting_rebirth(actor,{101.25f,-2.5f,77.75f},513,7,false,world);
            check(actor.attack_state().mode==0 && world.raw_presentation==0 && actor.attack_state().motion==0xa5 && actor.attack_state().recovery==1,"Actual shared state not published");
            check(actor.resources().hp==11 && actor.resources().mp==23 && actor.resources().sp==32 && world.sent.hp==11 && world.sent.mp==23 && world.sent.sp==32,"Actual resource owner callback lost");
            check(users.position(actor.id())->x==101.25f && world.views==1 && world.logs==1 && world.changes==1,"Actual grid/log composition missing");
            check(actor.effects().contains(33) && actor.status().current_target==77 && actor.status().casting==7 && actor.attack_state().previous_tick==111 && actor.attack_state().next_tick==222,"Rebirth invented death/target/casting cleanup");
            actor.attack_state().mode=1;world.called=false;threw=false;try {mob_targeting_rebirth(actor,{},0,7,true,world);}catch(const std::runtime_error&) {threw=true;}
            check(threw && actor.attack_state().mode==1 && !world.called && actor.resources().hp==11,"Missing resource provider was hidden");
        }
        check(scene.released==1,"Retained effect owner not released exactly once");
        std::cout<<cases<<" complete original Rebirth traces including 16 complete reentries; actual Actor/resource/grid/log/effect retention and explicit failure checks passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
