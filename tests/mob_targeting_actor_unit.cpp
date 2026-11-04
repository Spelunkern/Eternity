#include "eternity/game/mob_targeting_actor.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include "eternity/game/mob_ai_states_configured.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
struct Scene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected damage reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected stop");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected scene zone query");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct Life final:MobTargetingActorLife {
    const Actor* actor=nullptr;std::int32_t value=0;
    std::int32_t life_status(const Actor& a) const override {check(&a==actor,"Life queried another actor");return value;}
};
struct Cells final:ZoneMobServices {
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected movement tick");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p) override {check(p.size()==17,"Unexpected entry packet");}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected movement");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Init");}
};
struct Fsm final:MobAiStateServices,MobAiConfiguredServices {
    unsigned calls=0,ticks=0;std::uint32_t argument=0,when=0;std::vector<std::int32_t> observed;
    std::function<void(MobRuntime&)> callback;
    std::uint32_t tick() override {++ticks;return 0xfffffff0;}
    void lua_call(MobRuntime& m,std::string_view name,std::uint32_t now,std::optional<std::uint32_t> arg) override {
        check(name=="OnAttacked" && arg.has_value() && m.function_type==1,"Wrong real event callback");
        ++calls;argument=*arg;when=now;observed=*m.state_stack;if(callback) callback(m);
    }
    void update(MobRuntime&,MobAiState) override {throw std::runtime_error("Unexpected polling");}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::runtime_error("Unexpected FSM error");}
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::runtime_error("Unexpected expression provider");}
};
void put(std::span<std::byte> b,unsigned at,std::uint32_t n,unsigned size=1) {
    for(unsigned i=0;i<size;++i) b[at+i]=static_cast<std::byte>((n>>(8*i))&255);
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected original actor/FSM fixture");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;
        ActorEquipmentRules equipment{items,enchant,attacks};Scene scene;Cells cells;
        auto catalog=MobTargetingBindingCatalog::load(1,1,{},{});
        auto record=catalog.mutable_mob_record(1);put(record,0,1,2);put(record,38,1);put(record,105,255);put(record,120,1,4);catalog.publish_mob_record(1);
        MobRuntimeTemplate definition;definition.id=1;
        std::ifstream input(argv[1]);check(input.good(),"Cannot read original fixture");std::string line;unsigned scenarios=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);int initial=0,move=0,life=0,fixed=0,lua=0;std::uint32_t id=0;
            row>>initial>>move>>life>>fixed>>lua>>id;char bar=0;row>>bar;check(bar=='|',"Malformed original input");
            int status=0,target_kind=0;std::uint32_t target_id=0;std::uint32_t timeout=0,nextmove=0,discriminator=0;std::array<std::uint32_t,3> times{};unsigned n=0;row>>status>>target_kind>>target_id>>timeout>>nextmove>>times[0]>>times[1]>>times[2]>>discriminator>>n;
            std::vector<std::int32_t> stack(n);for(auto& s:stack) row>>s;
            row>>bar;check(bar=='|',"Malformed original output");unsigned calls=0;std::uint32_t tick=0,argument=0;row>>calls>>tick>>argument;unsigned observed_count=0;row>>observed_count;std::vector<std::int32_t> observed(observed_count);for(auto& s:observed) row>>s;check(static_cast<bool>(row),"Missing callbacks");
            Actor actor(id,{}, {0,0,0},{},scene,equipment);Life owner;owner.actor=&actor;owner.value=life;
            MobTargetingActorUser user(actor,owner);MicrosoftRandom random(1);Fsm fsm;ZoneUsers users(1,1,0);MobRuntime m;
            ZoneMobs mobs(MapGeometry(1),cells);MobTargetingActorWorld world(mobs,users,random,fsm);MobTargetingZone zone(mobs,catalog,world);
            m.definition=&definition;m.state_stack=std::vector<std::int32_t>{initial};m.status=42;m.move_mode=move;m.vision_fixed_user=fixed;m.lua_ai=lua;m.target_kind=7;m.target_id=12345;m.combat_timeout=8765;m.next_move=777;m.attack_times={111,222,333};
            zone.set_target(m,user,false);
            check(m.status==status && m.target_kind==target_kind && m.target_id==target_id && m.state_stack==stack,"Original SetTarget/real FSM publication differs");
            check(m.combat_timeout==timeout && m.next_move==nextmove && m.attack_times==times && m.function_type==discriminator,"Original timer/discriminator publication differs");
            check(fsm.calls==calls && fsm.when==tick && fsm.argument==argument && fsm.ticks==calls && fsm.observed==observed,"Original event callback trace differs");
            ++scenarios;
        }
        check(scenarios==160,"Original coverage changed");
        Actor actor(9001,{}, {0,0,0},{},scene,equipment);Life life;life.actor=&actor;life.value=2;
        MobTargetingActorUser user(actor,life);
        check(user.target().life_status==2,"Zero HP was incorrectly treated as dead");
        actor.appearance().identity.country=255;actor.appearance().visible=false;actor.appearance().attackable=false;
        actor.status().no_mob_targeting=true;actor.status().skills.hp_ability_applied=false;
        auto projected=user.help();check(projected.country==255 && projected.hp_ability==1 && !projected.visible && !projected.attackable && projected.life_status==2,"Canonical Actor live flags differ");
        actor.status().no_mob_targeting=false;actor.status().skills.hp_ability_applied=true;
        check(user.help().hp_ability==0,"Unrelated HP trigger was used as immunity");
        life.value=0;actor.appearance().identity.country=0;actor.appearance().visible=actor.appearance().attackable=true;
        std::array<MobRuntime,3> m;ZoneMobs mobs(MapGeometry(1),cells);ZoneUsers users(1,1,0);MicrosoftRandom random(17),control(17);Fsm fsm;
        MobTargetingActorWorld world(mobs,users,random,fsm);MobTargetingZone zone(mobs,catalog,world);
        check(world.user_count(mobs)==0,"Empty real user grid differs");users.attach(actor,0,0,{});check(world.user_count(mobs)==1,"Real attached count differs");
        for(unsigned i=0;i<m.size();++i) {m[i].id=i+1;m[i].type=7;m[i].definition=&definition;check(mobs.enter(m[i]),"Entry failed");}
        m[0].status=1;m[1].state_stack=std::vector<std::int32_t>{0};m[1].lua_ai=1;
        MobAiConfiguredFsm configured(0,0);unsigned entered=0;
        configured.states[0].update=[&](MobRuntime& subject,MobAiConfiguredFsm&) {
            check(subject.target_id==actor.id() && subject.state_stack->back()==0,"Configured callback sees wrong publication/order");
            life.value=1;actor.appearance().visible=false;
        };
        configured.states[0].transitions.push_back({1,0,{}});configured.states[1].enter=[&](MobRuntime&,MobAiConfiguredFsm&) {++entered;};
        fsm.callback=[&](MobRuntime& subject) {execute_mob_ai_configured(subject,configured,fsm);evaluate_mob_ai_configured(subject,configured,fsm);};
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);
        check(m[1].target_id==actor.id() && *m[1].state_stack==std::vector<std::int32_t>({0,1}) && m[2].target_id==0 && configured.current==1 && entered==1,"Live help/fixed/configured owner composition differs");
        life.value=0;actor.appearance().visible=true;actor.appearance().identity.country=1;
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==0,"Live country mutation ignored");
        actor.appearance().identity.country=0;actor.appearance().attackable=false;
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==0,"Live attack permission ignored");
        actor.appearance().attackable=true;actor.appearance().visible=false;
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==0,"Live visibility ignored");actor.appearance().visible=true;
        SkillDefinition immunity;immunity.id=77;immunity.effect=4;immunity.detail=89;
        apply_active_effect(actor.status(),immunity,0,actor);
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==0,"Actor immunity mutation ignored");
        remove_active_effect(actor.status(),immunity,{},0,{},items,actor);zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==actor.id(),"Live Actor flags did not reopen acquisition");
        check(users.detach(actor.id()) && world.user_count(mobs)==0,"Live detachment count differs");m[2].status=0;m[2].target_id=0;
        zone.find_help(m[0],user,MobTargetingHelpKind::normal);check(m[2].target_id==0,"Detached user count was cached");
        // Shared original CRT stream and same FSM clock owner, not per-mob RNG.
        check(world.random(m[0])==control.next() && world.random(m[1])==control.next(),"Shared CRT sequencing differs");
        auto skill=catalog.mutable_skill_record(1);put(skill,0,1,2);put(skill,74,2,2);catalog.publish_skill_record(1);
        put(record,130,255);put(record,144,1,4);catalog.publish_mob_record(1);
        m[2].status=100;const auto delay=static_cast<std::uint32_t>(control.next()%2000);zone.set_target(m[2],user,false);
        check(m[2].attack_times[0]==0xfffffff0U+delay && random.state()==control.state(),"Shared CRT/clock targeting timer differs");
        bool rejected=false;ZoneMobs other(MapGeometry(1),cells);try {world.user_count(other);}catch(const std::invalid_argument&) {rejected=true;}check(rejected,"Cross-zone count owner accepted");
        std::cout<<scenarios<<" original SetTarget-to-FSM paths and live canonical Actor/configured/grid/CRT/clock composition passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
