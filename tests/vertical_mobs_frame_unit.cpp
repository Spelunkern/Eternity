// Reuse the existing world fixture without modifying the shared world test.
#define main vertical_world_fixture_main
#include "vertical_world_unit.cpp"
#undef main
#include "eternity/game/mob_targeting_actor.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_ai_states_combat.hpp"
#include "eternity/game/mob_log.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/user_priority_area.hpp"
#include "eternity/game/user_skill_runtime.hpp"
#include <iomanip>
struct FrameScene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::logic_error("unexpected reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 77;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("unexpected stop");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::logic_error("unexpected zone query");}
    UserShapeSocial user_shape_social() override {throw std::logic_error("unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("unexpected damage");}
};
struct FrameStack final:MobAiStateServices {
    std::uint32_t tick() override {throw std::logic_error("unexpected FSM tick");}
    void lua_call(MobRuntime&,std::string_view,std::uint32_t,std::optional<std::uint32_t>) override {throw std::logic_error("unexpected Lua");}
    void update(MobRuntime&,MobAiState) override {throw std::logic_error("unexpected FSM update");}
    void error(MobRuntime&,MobAiError,std::int32_t,std::int32_t) override {throw std::logic_error("unexpected FSM error");}
};
struct FrameAttack final:VerticalMobsAttackServices {
    std::vector<std::uint32_t> ids;std::uint32_t expected=0;ZoneMobs* zone=nullptr;
    void update_attack(MobRuntime& mob,std::uint32_t now) override {
        check(now==expected && zone && mob.zone==zone && zone->find(mob.id)==&mob);ids.push_back(mob.id);
    }
};
struct AttackOracle final:MobAiCombatServices,VerticalMobsAiOwners {
    MobAiCombatTarget target{42,0};bool found=false,mutate=false;std::string trace;
    std::uint32_t tick() override {throw std::logic_error("unexpected tick");}
    MobAiCombatTarget* find_user(MobRuntime&,std::uint32_t id) override {check(id==42);trace+='F';return found?&target:nullptr;}
    bool attack_ai(MobRuntime&,std::uint32_t now,MobAiCombatTarget& value) override {check(now==50 && &value==&target);trace+='A';return false;}
    void remove_vision_fix(MobRuntime& mob) override {trace+='V';if(mutate)mob.status=2;}
    void retarget(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected retarget");}
    void lua_combat(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t) override {throw std::logic_error("unexpected Lua");}
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override {throw std::logic_error("unexpected FSM");}
    MobAiConfiguredServices& configured_services(MobRuntime&) override {throw std::logic_error("unexpected FSM services");}
    MobAiStateServices& stack_services(MobRuntime&) override {throw std::logic_error("unexpected stack services");}
    void release_failed_stack(MobRuntime&) override {throw std::logic_error("unexpected stack release");}
    void return_home(MobRuntime&,std::uint32_t now) override {check(now==50);trace+='R';}
};
struct AttackDependencies final:VerticalMobsAttackDependencies {
    AttackOracle& services;explicit AttackDependencies(AttackOracle& value):services(value){}
    MobAiCombatServices& combat_services(MobRuntime&) override{return services;}
};
struct ActorCombatWorld final:VerticalMobsActorCombatWorld {
    std::shared_ptr<Actor> actor;MobRuntime* mob=nullptr;unsigned calls=0;
    std::shared_ptr<void> retain(Actor& value) override {check(&value==actor.get());return actor;}
    std::uint32_t tick() override {return 77;}
    bool attack_ai(MobRuntime& subject,std::uint32_t now,Actor& target) override {check(&subject==mob && &target==actor.get() && now==6000);++calls;return false;}
    void retarget(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected retarget");}
    void lua_combat(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t) override {throw std::logic_error("unexpected Lua");}
};
struct MinHpWorld:VerticalMobsActorCombatWorld {
    std::vector<std::shared_ptr<Actor>> actors;
    std::shared_ptr<void> retain(Actor& actor) override {for(auto& value:actors)if(value.get()==&actor)return value;throw std::logic_error("unknown actor");}
    std::uint32_t tick() override {throw std::logic_error("unexpected tick");}
    bool attack_ai(MobRuntime&,std::uint32_t,Actor&) override {throw std::logic_error("unexpected attack");}
    void retarget(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected retarget");}
    void lua_combat(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t) override {throw std::logic_error("unexpected Lua");}
};
struct ChasePrefixMovement final:VerticalMobsChaseMovement {
    unsigned calls=0;
    bool continue_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t) override {++calls;return false;}
};
struct ChasePathWorld final:VerticalMobsChasePathWorld {
    MobRuntime* subject=nullptr;Actor* target=nullptr;std::string route;MapTile selected{};
    bool waypoint(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,MapTile point) override {check(&mob==subject && &actor==target && now==50 && range==0 && mob.path.found());route="W";selected=point;return false;}
    bool blocked(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,MapTile,MapTile end,MapTile) override {check(&mob==subject && &actor==target && now==50 && range==0 && !mob.path.found());route="B";selected=end;return false;}
};
struct ChaseSearchReset final:VerticalMobsChaseSearchWorld {
    MobRuntime* subject=nullptr;unsigned calls=0;
    void reset_target(MobRuntime& mob) override {check(&mob==subject && !mob.path.found());++calls;}
};
void check_chase_path(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture,const std::filesystem::path& blocked_fixture,const std::filesystem::path& search_fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->definition=frame.runtime.find(1);check(mob->definition);mob->position={10,0,10};
    auto map=frame.maps->all()[0].definition;ZoneMobs mobs(map.geometry(),frame.backend);ZoneUsers users(map.geometry(),0);check(mobs.enter(*mob));check(users.attach_at_position(actor,{20,0,20}));std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t mx,mz,tx,tz,blocked,found,cx,cz,selected_x,selected_z,final_found,cursor,alloc;std::string route;
        row>>mx>>mz>>tx>>tz>>blocked>>found>>cx>>cz>>route>>selected_x>>selected_z>>final_found>>cursor>>alloc;check(bool(row));
        mob->position={std::bit_cast<float>(mx),0,std::bit_cast<float>(mz)};check(users.move_at_position(actor.id(),{std::bit_cast<float>(tx),0,std::bit_cast<float>(tz)}));
        std::fill(map.tiles.begin(),map.tiles.end(),0);if(blocked){const auto tile=15U*81U+15U;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
        mob->path.reset();if(found)mob->path.set_path({static_cast<int>(cx),static_cast<int>(cz)});
        ChasePathWorld world;world.subject=mob;world.target=&actor;VerticalMobsChasePath path(mobs,users,map,world);check(!path.continue_chase(*mob,50,actor,0));
        check(world.route==route && static_cast<std::uint32_t>(world.selected.x)==selected_x && static_cast<std::uint32_t>(world.selected.z)==selected_z && mob->path.found()==bool(final_found) && mob->path.current().has_value()==bool(cursor) && alloc==(route=="W" && !found?1U:0U));++count;
    }
    check(count==5);std::ifstream blocked_in(blocked_fixture);check(blocked_in.good());auto record=frame.full.mutable_mob_record(1);const auto saved_range=record[0x88],saved_size=record[0x3f];count=0;
    while(std::getline(blocked_in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t barrier,attack_range,status,expected_status,kind,fair,found,cx,cz;std::string route;
        row>>barrier>>attack_range>>status>>route>>expected_status>>kind>>fair>>found>>cx>>cz;check(bool(row));
        mob->position={10,0,10};check(users.move_at_position(actor.id(),{20,0,10}));std::fill(map.tiles.begin(),map.tiles.end(),0);
        for(const auto x:{barrier,20U}) {const auto tile=10U*81U+x;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
        mob->path.reset();mob->status=static_cast<int>(status);mob->target_kind=7;mob->damage->fair_combat=true;record[0x88]=static_cast<std::byte>(attack_range);record[0x3f]=std::byte{0};
        ChasePathWorld world;world.subject=mob;world.target=&actor;VerticalMobsChaseBlockedTarget blocked(map,users,frame.full,world);VerticalMobsChasePath path(mobs,users,map,blocked);check(!path.continue_chase(*mob,50,actor,0));
        const auto point=mob->path.current();check((world.route.empty()?"-":world.route)==route && mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind) && mob->damage->fair_combat==bool(fair) && mob->path.found()==bool(found) && (point?static_cast<std::uint32_t>(point->x):UINT32_MAX)==cx && (point?static_cast<std::uint32_t>(point->z):UINT32_MAX)==cz);++count;
    }
    check(count==7);record[0x88]=saved_range;record[0x3f]=saved_size;
    std::ifstream search_in(search_fixture);check(search_in.good());count=0;
    while(std::getline(search_in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned gap,resets,found,cx,cz;std::string route;row>>gap>>route>>resets>>found>>cx>>cz;check(bool(row));
        mob->position={10,0,10};check(users.move_at_position(actor.id(),{20,0,20}));std::fill(map.tiles.begin(),map.tiles.end(),0);
        for(unsigned z=0;z<81;++z)if(!gap || z!=14) {const auto tile=z*81U+15U;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
        mob->path.reset();ChasePathWorld world;world.subject=mob;world.target=&actor;ChaseSearchReset reset;reset.subject=mob;
        VerticalMobsChaseSearch search(map,reset,world);VerticalMobsChaseBlockedTarget blocked(map,users,frame.full,search);VerticalMobsChasePath path(mobs,users,map,blocked);
        check(!path.continue_chase(*mob,50,actor,0));const auto point=mob->path.current();
        check((world.route.empty()?"-":world.route)==route && reset.calls==resets && mob->path.found()==bool(found) && (point?static_cast<unsigned>(point->x):UINT32_MAX)==cx && (point?static_cast<unsigned>(point->z):UINT32_MAX)==cz);++count;
    }
    check(count==2);mob->path.reset();check(users.detach(actor.id()));check(mobs.leave(*mob));root.mob_pool().release(*mob);
}
struct AggroWorld final:MinHpWorld {
    unsigned calls=0;
    void retarget(MobRuntime& mob,std::uint32_t now) override {check(now==100);++calls;mob.target_id=777;}
};
void check_min_hp(MobPrimaryZones& root,Frame& frame,ActorScene& scene,ActorEquipmentRules equipment,const std::filesystem::path& fixture) {
    struct Row {int kind;unsigned id,hp,country,life,immune,attackable,visible;};
    const std::vector<std::vector<Row>> cases={
        {},{{1,1,10,1,0,0,1,1}},{{2,1,1,1,0,0,1,1},{1,2,30,1,0,0,1,1},{1,3,10,1,0,0,1,1}},
        {{1,1,10,1,0,0,1,1},{1,2,10,1,0,0,1,1}},
        {{1,1,1,2,0,0,1,1},{1,2,2,1,1,0,1,1},{1,3,3,1,0,1,1,1},{1,4,4,1,0,0,0,1},{1,5,5,1,0,0,1,0},{1,6,6,1,0,0,1,1}},
        {{1,1,10,1,0,0,1,1},{1,2,UINT32_MAX,1,0,0,1,1}},{{1,1,10,1,0,0,1,1},{1,9,0,0,0,0,1,1}}};
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->definition=frame.runtime.find(1);check(mob->definition);mob->position={10,0,10};
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());for(unsigned i=0;i<4;++i)record[0x78+i]=static_cast<std::byte>(i==0?2:0);
    ZoneMobs mobs(frame.maps->all()[0].definition.geometry(),frame.backend);ZoneUsers users(frame.maps->all()[0].definition.geometry(),0);check(mobs.enter(*mob));std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream input(line);unsigned index,expected;input>>index>>expected;check(bool(input) && index<cases.size());mob->damage->reset();MinHpWorld world;
        for(const auto& row:cases[index]) {
            mob->damage->add(row.kind,row.id,1,1,0);if(!row.country)continue;
            auto actor=std::make_shared<Actor>(row.id,EffectStatus{},PlayerResources{row.hp,0,0},std::vector<PersistedEffect>{},scene,equipment);
            actor->appearance().identity.country=static_cast<std::uint8_t>(row.country);actor->appearance().attackable=row.attackable;actor->appearance().visible=row.visible;actor->attack_state().mode=static_cast<int>(row.life);actor->status().no_mob_targeting=row.immune;
            check(users.attach_at_position(*actor,{10,0,10}));world.actors.push_back(std::move(actor));
        }
        auto selected=vertical_mobs_find_attack_user_min_hp(*mob,mobs,users,frame.full,world);check((selected.actor?selected.actor->id():0)==expected && bool(selected.lease)==bool(expected));
        if(!world.actors.empty()) {
            auto& actor=*world.actors.front();check(vertical_mobs_attack_range(*mob,actor,0,mobs,users,frame.full));
            mob->next_chase=0;mob->stop=mob->sleep=mob->stun=0;mob->path.reset();mob->path.set_path({1,1});ChasePrefixMovement movement;
            check(vertical_mobs_chase(*mob,50,actor,0,mobs,users,frame.full,movement) && !mob->path.found() && movement.calls==0);
            check(users.move_at_position(actor.id(),{40,0,10}));check(!vertical_mobs_chase(*mob,50,actor,0,mobs,users,frame.full,movement) && movement.calls==1);
        }
        selected={};for(auto& actor:world.actors)check(users.detach(actor->id()));++count;
    }
    check(count==7);mob->damage->reset();check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
void check_attack_range(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const auto saved=record[0x3f];std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t mx,mz,ux,uz,size,allowed;std::int32_t range;row>>mx>>mz>>ux>>uz>>size>>range>>allowed;check(bool(row));
        mob->position={std::bit_cast<float>(mx),0,std::bit_cast<float>(mz)};record[0x3f]=static_cast<std::byte>(size);
        check(vertical_mobs_attack_range(*mob,{std::bit_cast<float>(ux),0,std::bit_cast<float>(uz)},range,frame.full)==bool(allowed));++count;
    }
    check(count==17);record[0x3f]=saved;root.mob_pool().release(*mob);
}
void check_chase_prefix(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const auto saved=record[0x3f];std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,deadline,x,z,size,stopped,sleep,stun,result,reset,movement,found;std::int32_t range;
        row>>now>>deadline>>x>>z>>size>>range>>stopped>>sleep>>stun>>result>>reset>>movement>>found;check(bool(row));
        mob->position={};mob->next_chase=deadline;mob->stop=stopped;mob->sleep=sleep;mob->stun=stun;record[0x3f]=static_cast<std::byte>(size);mob->path.reset();mob->path.set_path({1,1});
        const auto actual=vertical_mobs_chase_prefix(*mob,now,{std::bit_cast<float>(x),0,std::bit_cast<float>(z)},range,frame.full);
        const auto expected=result?VerticalMobChasePrefix::reached:movement?VerticalMobChasePrefix::needs_movement:now<deadline?VerticalMobChasePrefix::waiting:VerticalMobChasePrefix::stopped;
        check(actual==expected && mob->path.found()==bool(found) && mob->path.current().has_value()==bool(found) && reset==result);++count;
    }
    check(count==12);mob->path.reset();record[0x3f]=saved;root.mob_pool().release(*mob);
}
struct AttackAiContinuation final:VerticalMobsAttackAiContinuation {
    MobRuntime* subject=nullptr;Actor* target=nullptr;unsigned calls=0;
    void continue_attack_ai(MobRuntime& mob,std::uint32_t now,Actor& actor) override {check(&mob==subject && &actor==target && now==123 && mob.in_battle);++calls;}
};
struct AttackAiReady final:VerticalMobsAttackAiReady {
    MobRuntime* subject=nullptr;Actor* target=nullptr;std::uint32_t now=0;std::uint32_t slot=UINT32_MAX;
    void first_ready(MobRuntime& mob,std::uint32_t tick,Actor& actor,std::size_t value) override {check(&mob==subject && &actor==target && tick==now && value<3);slot=static_cast<std::uint32_t>(value);}
};
struct AttackAiSkillWorld final:VerticalMobsAttackAiSkillWorld {
    MobRuntime* subject=nullptr;Actor* target=nullptr;std::span<std::byte> skill;std::uint32_t tick=0;unsigned mutation=0,calls=0,other=0;int during=-1;
    void use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& binding) override {
        check(&mob==subject && &actor==target && now==tick && binding.id==1);during=mob.current_attack;++calls;
        if(mutation){skill[0x4a]=static_cast<std::byte>(mutation&255);skill[0x4b]=static_cast<std::byte>(mutation>>8);mob.current_attack=77;}
    }
    void other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) override {check(&mob==subject && &actor==target && now==tick && slot<3);++other;}
};
struct StationaryWorld final:VerticalMobsStationaryAttackWorld {
    MobRuntime* subject=nullptr;Actor* target=nullptr;std::span<std::byte> record,skill_record;std::uint32_t now=0,mutation=0;unsigned calls=0;std::int32_t during=999;
    void attack(MobRuntime& mob,std::uint32_t tick,Actor& actor) override {
        check(&mob==subject && &actor==target && tick==now);++calls;during=mob.current_attack;
        if(mutation){mob.current_attack=static_cast<int>(mutation);mob.attack_speed=6;for(unsigned i=0;i<4;++i)record[0x84+mutation*20+i]=static_cast<std::byte>((777U>>(i*8))&255);}
    }
    void use_skill(MobRuntime& mob,std::uint32_t tick,Actor& actor,const MobTargetingBindingSkill& skill) override {
        check(&mob==subject && &actor==target && tick==now && skill.id==1);++calls;during=mob.current_attack;
        if(mutation){skill_record[0x4a]=static_cast<std::byte>(mutation&255);skill_record[0x4b]=static_cast<std::byte>(mutation>>8);mob.current_attack=77;}
    }
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override {throw std::logic_error("unexpected attack remainder");}
};
void check_attack_guard(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    StationaryWorld world;world.subject=mob;world.target=&actor;world.now=100;VerticalMobsAttackGuard guard(world);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);int sleep,stun,speed,d0,d1,d2;unsigned calls;row>>sleep>>stun>>speed>>d0>>d1>>d2>>calls;check(bool(row));
        mob->sleep=sleep;mob->stun=stun;mob->attack_speed=speed;mob->attack_disabled={d0,d1,d2};mob->current_attack=2;world.calls=0;
        guard.attack(*mob,100,actor);check(world.calls==calls && mob->current_attack==2 && (calls==0 || world.during==2));++count;
    }
    check(count==8);mob->sleep=mob->stun=0;mob->attack_disabled={};root.mob_pool().release(*mob);
}
void check_attack_finish(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,delay,multiplier,expected;int speed;row>>now>>delay>>speed>>multiplier>>expected;check(bool(row));
        for(unsigned i=0;i<4;++i)record[0x84+i]=static_cast<std::byte>((delay>>(i*8))&255);
        mob->attack_speed=speed;mob->multipliers[1]=std::bit_cast<float>(multiplier);mob->next_attack=0;
        vertical_mobs_attack_finish(*mob,now,frame.full);check(mob->next_attack==expected);++count;
    }
    check(count==10);mob->multipliers[1]=1.0F;root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
struct AttackTargetOracle final:VerticalMobsAttackTargetServices {
    MobRuntime* subject=nullptr;Actor* actor=nullptr;std::span<std::byte> record;bool success=false,mutate=false;std::string trace,packet;unsigned type=UINT32_MAX;
    void status_stand(Actor& value) override {
        check(&value==actor);trace+='S';if(mutate){subject->current_attack=2;subject->attack_speed=6;for(unsigned i=0;i<4;++i)record[0x84+i]=static_cast<std::byte>((1000U>>(i*8))&255);}
    }
    bool attack_success(MobRuntime& mob,Actor& value,std::uint16_t attack_type) override {check(&mob==subject && &value==actor);trace+='R';type=attack_type;return success;}
    void send_miss(MobRuntime& mob,Actor& value,const eternity::net::Packet& bytes) override {
        check(&mob==subject && &value==actor && mob.zone);trace+='M';std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:bytes)out<<std::setw(2)<<static_cast<unsigned>(b);packet=out.str();
    }
    void hit(MobRuntime& mob,std::uint32_t now,Actor& value) override {check(&mob==subject && &value==actor && now==100);trace+='H';}
};
void check_attack_target(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition && actor.id()==42);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());const auto old_id=mob->id;mob->id=123;mob->position={10,0,10};
    const auto& map=frame.maps->all()[0].definition;ZoneMobs zone(map.geometry(),frame.backend);AttackTargetOracle target;target.subject=mob;target.actor=&actor;target.record=record;
    StationaryWorld other;other.subject=mob;other.target=&actor;VerticalMobsAttackTarget attack(frame.full,target,other);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned sleep,disabled,slot,hit,has_zone,mutate,type,next;std::string trace,packet;
        row>>sleep>>disabled>>slot>>hit>>has_zone>>mutate>>trace>>type>>packet>>next;check(bool(row));
        if(has_zone && !mob->zone)check(zone.enter(*mob));else if(!has_zone && mob->zone)check(zone.leave(*mob));
        mob->sleep=static_cast<int>(sleep);mob->stun=0;mob->attack_disabled={static_cast<int>(disabled),0,0};mob->current_attack=static_cast<int>(slot);mob->attack_speed=3;mob->multipliers[1]=1;mob->next_attack=0;
        for(unsigned i=0;i<4;++i)record[0x84+i]=static_cast<std::byte>((2000U>>(i*8))&255);
        for(unsigned i=0;i<3;++i){record[0x80+i*20]=static_cast<std::byte>(11+i);record[0x81+i*20]=std::byte{0};}
        target.success=hit;target.mutate=mutate;target.trace.clear();target.packet="-";target.type=UINT32_MAX;
        attack.attack(*mob,100,actor);check((target.trace.empty()?"-":target.trace)==trace && target.type==type && target.packet==packet && mob->next_attack==next);++count;
    }
    check(count==7);if(mob->zone)check(zone.leave(*mob));mob->id=old_id;mob->sleep=0;mob->attack_disabled={};root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
struct MobAttackRoll final:RandomSource {int value=0;unsigned draws=0;std::int32_t next() override {++draws;return value;}};
struct MobPriorityArea final:UserPriorityAreaServices {
    struct Buffer final:UserViewBuffer {};
    std::vector<std::shared_ptr<Actor>> actors;std::vector<std::uint32_t> immediate_ids,priority_ids;std::vector<eternity::net::Packet> packets;
    std::unique_ptr<UserViewBuffer> allocate(const eternity::net::Packet& packet) override {packets.push_back(packet);return std::make_unique<Buffer>();}
    std::shared_ptr<void> retain(Actor& actor) override {for(const auto& value:actors)if(value.get()==&actor)return value;throw std::logic_error("Unknown priority actor");}
    UserPosition origin() override {throw std::logic_error("Mob priority view must use live mob origin");}
    void immediate(Actor& actor,UserViewBuffer&) override {immediate_ids.push_back(actor.id());}
    void priority(Actor& actor,UserViewBuffer&,std::uint8_t priority,float) override {check(priority==6);priority_ids.push_back(actor.id());}
};
struct MobPriorityWorld final:VerticalMobsPriorityHitTailWorld {
    using VerticalMobsPriorityHitTailWorld::VerticalMobsPriorityHitTailWorld;
    void status_stand(Actor&) override {throw std::logic_error("Unexpected stand");}
    bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override {throw std::logic_error("Unexpected user damage");}
    void user_killed(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected user death");}
    bool damage_mob(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override {throw std::logic_error("Unexpected mob damage");}
    void final_attack_effects(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected final effects");}
};
struct SkillPriorityFixture final:VerticalMobsSkillPriorityWorld {
    using VerticalMobsSkillPriorityWorld::VerticalMobsSkillPriorityWorld;
    void attack_user(MobRuntime&,Actor&,const MobTargetingBindingSkill&,bool) override {throw std::logic_error("unexpected skill hit");}
    bool self_damage(MobRuntime&,std::uint32_t,const MobCombatCredit&) override {throw std::logic_error("unexpected self damage");}
    void range(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override {throw std::logic_error("unexpected area");}
    void range_select(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override {throw std::logic_error("unexpected area select");}
};
void check_skill_priority(MobPrimaryZones& root,Frame& frame,ActorScene& scene,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->lua_ai=0;mob->position={10,0,10};
    const auto geometry=frame.maps->all()[0].definition.geometry();ZoneMobs mobs(geometry,frame.backend);ZoneUsers users(geometry,0);MobPriorityArea area;
    MobPriorityWorld views(mobs,users,area);SkillPriorityFixture world(views);MobTargetingBindingSkill skill;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;std::istringstream row(line);std::uint32_t id,target,sid,level,zone;int slot;std::string expected;
        row>>id>>target>>slot>>sid>>level>>zone>>expected;check(bool(row));mob->id=id;mob->current_attack=slot;
        skill.record[0]=static_cast<std::byte>(sid&255);skill.record[1]=static_cast<std::byte>(sid>>8);skill.record[2]=static_cast<std::byte>(level);
        auto actor=std::make_shared<Actor>(target,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);
        area.actors={actor};area.packets.clear();area.immediate_ids.clear();area.priority_ids.clear();check(users.attach_at_position(*actor,{20,0,10}));if(zone)check(mobs.enter(*mob));
        world.send_skill(*mob,*actor,skill);std::string packet;if(!area.packets.empty()){static constexpr char hex[]="0123456789abcdef";for(auto b:area.packets[0]){packet+=hex[b>>4];packet+=hex[b&15];}}
        check((packet.empty()?"-":packet)==expected && area.packets.size()==zone && area.immediate_ids.size()==zone && (!zone || area.immediate_ids[0]==target));
        if(zone)check(mobs.leave(*mob));check(users.detach(target));++count;
    }
    check(count==4);root.mob_pool().release(*mob);
}
struct MobSharedPool final:PartyMembershipSendWorld {
    PartyMembershipSendBuffer buffer;bool active=false,allow=true;unsigned allocations=0,frees=0;std::vector<std::uint32_t> immediate;std::vector<eternity::net::Packet> frames;
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {++allocations;if(!allow)return nullptr;check(!active);active=true;return &buffer;}
    void free_send_buffer(PartyMembershipSendBuffer& value) override {check(&value==&buffer && active && value.references==0);active=false;++frees;}
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& value) override {check(&value==&buffer && active);immediate.push_back(user.id);frames.push_back(value.frame);release_party_membership_buffer(value,*this);}
    std::uint32_t tick() override {throw std::logic_error("Unexpected party tick");}
    void lock(PartyNormalMembership&) override {throw std::logic_error("Unexpected party lock");}
    void unlock(PartyNormalMembership&) noexcept override {std::terminate();}
    void lock_effects(PartyMembershipUser&) override {throw std::logic_error("Unexpected effect lock");}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
    void send_user(PartyMembershipUser&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected user send");}
    void send_scene(PartyMembershipUser&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected scene send");}
    void unregister_party(PartyNormalMembership&) override {throw std::logic_error("Unexpected party unregister");}
    void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("Unexpected guild cleanup");}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {throw std::logic_error("Unexpected log");}
    void send_log(const eternity::net::Packet&) override {throw std::logic_error("Unexpected log send");}
};
struct MobSharedConnections final:VerticalMobsPriorityConnections {
    std::array<std::shared_ptr<Actor>,2> actors;std::array<PartyMembershipUser,2> users;UserPriorityQueue& priority;
    explicit MobSharedConnections(UserPriorityQueue& queue):priority(queue){for(unsigned i=0;i<2;++i){users[i].id=42+i;users[i].send_socket=1;}}
    std::shared_ptr<void> retain(Actor& actor) override {for(const auto& value:actors)if(value.get()==&actor)return value;throw std::logic_error("Unknown shared priority actor");}
    PartyMembershipUser& connection(Actor& actor) override {check(actor.id()>=42 && actor.id()<=43);return users[actor.id()-42];}
    UserPriorityQueue& queue(Actor& actor) override {check(actor.id()==43);return priority;}
};
void check_mob_shared_priority(MobPrimaryZones& root,Frame& frame){
    auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->id=77;mob->lua_ai=0;mob->position={10,0,10};
    const auto geometry=frame.maps->all()[0].definition.geometry();ZoneMobs mobs(geometry,frame.backend);ZoneUsers users(geometry,0);check(mobs.enter(*mob));
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;
    auto target=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    auto near=std::make_shared<Actor>(43,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    check(users.attach_at_position(*target,{69,0,69}) && users.attach_at_position(*near,{20,0,20}));
    MobSharedPool pool;UserViewSharedBuffers buffers(pool);UserPriorityQueue queue(pool);MobSharedConnections connections(queue);connections.actors={target,near};
    VerticalMobsSharedPriorityCells cells(buffers,*mob,connections);MobPriorityWorld world(mobs,users,cells);const eternity::net::Packet packet{5,6,1};const eternity::net::Packet framed{5,0,5,6,1};
    world.send_hit(*mob,42,packet);check(pool.immediate==std::vector<std::uint32_t>{42} && pool.frames==std::vector<eternity::net::Packet>{framed} && queue.size()==1 && pool.active && pool.buffer.references==1);
    auto* pending=queue.take_first();check(pending==&pool.buffer && pending->frame==framed);release_party_membership_buffer(*pending,pool);check(!pool.active && pool.frees==1);
    pool.allow=false;world.send_miss(*mob,*target,packet);check(!pool.active && queue.size()==0 && pool.immediate.size()==1);
    pool.allow=true;connections.users[1].send_socket=-1;world.send_reflection(*mob,42,packet);check(!pool.active && queue.size()==0 && pool.immediate.size()==2 && pool.frees==2);
    check(users.detach(42));check(users.detach(43));check(mobs.leave(*mob));root.mob_pool().release(*mob);
}
struct BoundStandOwner {
    Actor* expected=nullptr;unsigned calls=0;
    void status_stand(Actor& actor){check(&actor==expected);++calls;}
};
struct BoundMobDamageOwner final:MobCombatServices {
    unsigned calls=0;MobCombatCredit observed{};
    bool damage(Actor& actor,MobRuntime& mob,std::array<std::int32_t,3>& values,const MobCombatCredit& credit){
        check(actor.id()==42 && mob.damage);++calls;observed=credit;MobCombatState state;state.hp=mob.hp;state.mp=mob.mp;state.sp=mob.sp;
        const auto dead=mob_combat_damage(state,*mob.damage,*this,values,credit);mob.hp=state.hp;mob.mp=state.mp;mob.sp=state.sp;return dead;
    }
    std::int32_t status(const MobCombatState& state) override{return state.state;}
    void dispatch_death_event(MobCombatState&) override {throw std::logic_error("Unexpected death event");}
    std::uint32_t tick() override {throw std::logic_error("Unexpected damage tick");}
    void log_damage(MobCombatState&,std::uint32_t) override {throw std::logic_error("Unexpected damage log");}
    void retarget(MobCombatState&,bool) override {throw std::logic_error("Unexpected retarget");}
};
struct BoundHitWorld final:VerticalMobsBoundHitTailWorld<BoundStandOwner,BoundMobDamageOwner> {
    using VerticalMobsBoundHitTailWorld::VerticalMobsBoundHitTailWorld;
    bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override {throw std::logic_error("Unexpected user damage");}
    void user_killed(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected user death");}
    void final_attack_effects(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected final effects");}
};
void check_bound_hit_owners(MobPrimaryZones& root,Frame& frame){
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->damage->reset();mob->hp=100;mob->mp=100;mob->sp=100;
    const auto geometry=frame.maps->all()[0].definition.geometry();ZoneMobs mobs(geometry,frame.backend);ZoneUsers users(geometry,0);MobPriorityArea area;
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;Actor actor(42,{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    BoundStandOwner stand;stand.expected=&actor;BoundMobDamageOwner owner;BoundHitWorld world(mobs,users,area,stand,owner);
    world.status_stand(actor);std::array<std::int32_t,3> damage{25,0,0};check(!world.damage_mob(*mob,actor,damage));
    check(stand.calls==1 && owner.calls==1 && owner.observed.type==1 && owner.observed.id==42 && owner.observed.party==0 && owner.observed.bonus==0 && owner.observed.base_aggro==0 && mob->hp==75);
    const auto records=mob->damage->records();check(records.size()==1 && records[0]->type==1 && records[0]->id==42 && records[0]->damage==25 && records[0]->aggro==0 && records[0]->party==0);
    mob->damage->reset();root.mob_pool().release(*mob);
}
void check_mob_priority_views(MobPrimaryZones& root,Frame& frame) {
    auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->id=77;mob->lua_ai=0;mob->position={10,0,10};
    const auto geometry=frame.maps->all()[0].definition.geometry();ZoneMobs mobs(geometry,frame.backend),foreign(geometry,frame.backend);ZoneUsers users(geometry,0);check(mobs.enter(*mob));
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;
    auto target=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    auto near=std::make_shared<Actor>(43,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    auto far=std::make_shared<Actor>(44,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,equipment);
    check(users.attach_at_position(*target,{69,0,69}));check(users.attach_at_position(*near,{20,0,20}));check(users.attach_at_position(*far,{70,0,70}));
    MobPriorityArea area;area.actors={target,near,far};MobPriorityWorld world(mobs,users,area);const eternity::net::Packet packet{5,6,1};
    world.send_miss(*mob,*target,packet);check(area.immediate_ids==std::vector<std::uint32_t>{42} && area.priority_ids==std::vector<std::uint32_t>{43} && area.packets==std::vector<eternity::net::Packet>{packet});
    area.immediate_ids.clear();area.priority_ids.clear();mob->position={70,0,70};world.send_hit(*mob,42,packet);
    check(area.immediate_ids==std::vector<std::uint32_t>{42} && area.priority_ids==std::vector<std::uint32_t>{44} && area.packets.size()==2);
    area.immediate_ids.clear();area.priority_ids.clear();mob->position={10,0,10};world.send_reflection(*mob,42,packet);
    check(area.immediate_ids==std::vector<std::uint32_t>{42} && area.priority_ids==std::vector<std::uint32_t>{43} && area.packets.size()==3);
    MobPriorityWorld mismatched(foreign,users,area);bool rejected=false;try{mismatched.send_hit(*mob,42,packet);}catch(const std::invalid_argument&){rejected=true;}check(rejected && area.packets.size()==3);
    check(users.detach(42));check(users.detach(43));check(users.detach(44));check(mobs.leave(*mob));root.mob_pool().release(*mob);
}
struct HitTailFixture final:VerticalMobsHitTailWorld,MobDeathWorld {
    Actor* actor=nullptr;bool dead=false;std::string trace,name,packet;
    void status_stand(Actor&) override {throw std::logic_error("Unexpected stand");}
    void send_miss(MobRuntime&,Actor&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected miss");}
    bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override {throw std::logic_error("Unexpected user damage");}
    void user_killed(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected user death");}
    void send_hit(MobRuntime&,std::uint32_t,const eternity::net::Packet&) override {throw std::logic_error("Unexpected hit");}
    bool damage_mob(MobRuntime& mob,Actor& target,std::array<std::int32_t,3>& damage) override {
        check(&target==actor);trace+='D';name=reinterpret_cast<const char*>(mob.previous_attack_user.data());damage[0]+=4;mob.id=101;if(dead)mob.hp=0;return dead;
    }
    void send_reflection(MobRuntime&,std::uint32_t excluded,const eternity::net::Packet& value) override {
        check(excluded==77 && value.size()==16);trace+='P';static constexpr char digits[]="0123456789abcdef";packet.clear();for(auto byte:value){packet+=digits[byte>>4];packet+=digits[byte&15];}
    }
    void final_attack_effects(MobRuntime&,Actor&) override {trace+='F';}
    void send_death(MobRuntime&,const eternity::net::Packet&,std::uint8_t) override {}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t) override {throw std::logic_error("Unexpected credited user");}
    MobDeathParty* party(MobDeathUser&) override {throw std::logic_error("Unexpected party");}
    void lock_party(MobDeathParty&) override {throw std::logic_error("Unexpected party lock");}
    void unlock_party(MobDeathParty&) noexcept override {std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t) override {throw std::logic_error("Unexpected range");}
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t) override {throw std::logic_error("Unexpected experience");}
    void drop_items(MobRuntime&,MobDeathUser*,MobDeathParty*) override {}
    void quest_kill(MobDeathUser&,MobRuntime&) override {throw std::logic_error("Unexpected quest");}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t) override {throw std::logic_error("Unexpected obelisk");}
    void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t) override {throw std::logic_error("Unexpected obelisk destruction");}
    void destroy_linked_group(MobRuntime&,std::int32_t) override {}
    void remove_obelisk_member(MobRuntime&,void*) override {throw std::logic_error("Unexpected obelisk member");}
    void remove_linked_member(MobRuntime&,void*) override {throw std::logic_error("Unexpected linked member");}
    void log_death(MobRuntime&,MobDeathUser*,bool) override {}
    void clear_applied_skills(MobRuntime&) override {check(actor && actor->attack_state().mode==0);trace+="SK";}
};
void check_hit_tail(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->definition=frame.runtime.find(1);mob->id=100;mob->position={10,0,10};
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;Actor actor(77,{},PlayerResources{1,0,0},{},scene,equipment);actor.appearance().identity.name="Victim";
    ZoneMobs zone(frame.maps->all()[0].definition.geometry(),frame.backend);MobRespawn respawn{};MobDeathExperience rules;HitTailFixture world;world.actor=&actor;VerticalMobsHitTail tail(world,rules,world);std::string line;unsigned count=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned lua,reflected,user_dead,both,mob_dead,has_zone,id,userbyte,skillbyte;int hp;std::string trace,name,packet;
        row>>lua>>reflected>>user_dead>>both>>mob_dead>>has_zone>>trace>>name>>id>>hp>>userbyte>>skillbyte>>packet;check(bool(row));
        mob->id=100;mob->lua_ai=0;if(has_zone && !mob->zone)check(zone.enter(*mob));else if(!has_zone && mob->zone)check(zone.leave(*mob));
        mob->lua_ai=lua;respawn.both=both;respawn.current=1;respawn.next=0;mob->respawn=&respawn;mob->hp=100;mob->damage->reset();mob->damage->fair_combat=false;mob->previous_attack_user.fill(0);mob->previous_attack_skill.fill(0);mob->previous_attack_user[0]='X';mob->previous_attack_skill[0]='Y';
        actor.attack_state().mode=2;world.dead=mob_dead;world.trace.clear();world.name="-";world.packet="-";std::array<std::int32_t,3> damage{70000,-2,10};
        try {
            tail.finish_after_hit(*mob,1,actor,reflected,false,user_dead,damage);
            if(!(world.trace==trace && world.name==name && world.packet==packet && mob->id==id && damage[0]==hp && mob->previous_attack_user[0]==userbyte && mob->previous_attack_skill[0]==skillbyte))
                std::cerr<<"Hit tail row "<<count<<" actual "<<world.trace<<' '<<world.name<<' '<<mob->id<<' '<<damage[0]<<' '<<unsigned(mob->previous_attack_user[0])<<' '<<unsigned(mob->previous_attack_skill[0])<<' '<<world.packet<<" expected "<<trace<<' '<<name<<' '<<id<<' '<<hp<<' '<<userbyte<<' '<<skillbyte<<' '<<packet<<'\n';
            check(world.trace==trace && world.name==name && world.packet==packet && mob->id==id && damage[0]==hp && mob->previous_attack_user[0]==userbyte && mob->previous_attack_skill[0]==skillbyte);
        }catch(...){mob->id=100;if(mob->zone)zone.leave(*mob);mob->respawn=nullptr;mob->lua_ai=0;root.mob_pool().release(*mob);throw;}
        ++count;
    }
    check(count==64);mob->id=100;if(mob->zone)check(zone.leave(*mob));mob->respawn=nullptr;mob->lua_ai=0;root.mob_pool().release(*mob);
}
struct HitDeliveryWorld final:VerticalMobsHitDeliveryWorld {
    bool killed=false,observed_killed=false;std::string trace,packet;unsigned expected_target=77;
    void status_stand(Actor&) override {throw std::logic_error("Unexpected stand");}
    void send_miss(MobRuntime&,Actor&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected miss");}
    bool damage_user(MobRuntime& mob,Actor& actor,std::array<std::int32_t,3>& damage) override {check(actor.id()==expected_target);trace+='A';damage[0]+=4;mob.id=101;return killed;}
    void user_killed(MobRuntime& mob,Actor&) override {trace+='D';mob.id=102;}
    void send_hit(MobRuntime&,std::uint32_t excluded,const eternity::net::Packet& value) override {
        check(excluded==expected_target && value.size()==17);trace+='P';static constexpr char digits[]="0123456789abcdef";packet.clear();for(auto byte:value){packet+=digits[byte>>4];packet+=digits[byte&15];}
    }
    void finish_after_hit(MobRuntime&,std::uint32_t now,Actor&,bool,bool,bool dead,std::array<std::int32_t,3>&) override {check(now==105);trace+='T';observed_killed=dead;}
};
void check_hit_delivery(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->id=100;mob->position={10,0,10};
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules rules{items,enchant,attacks};FrameScene scene;Actor actor(77,{},PlayerResources{1,0,0},{},scene,rules);
    ZoneMobs zone(frame.maps->all()[0].definition.geometry(),frame.backend);HitDeliveryWorld world;VerticalMobsHitDelivery delivery(world);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned reflected,critical,killed,has_zone,expected_id;std::int32_t hp,sp,mp;std::string trace,packet;
        row>>reflected>>critical>>killed>>has_zone>>trace>>expected_id>>hp>>sp>>mp>>packet;check(bool(row));
        mob->id=100;if(has_zone && !mob->zone)check(zone.enter(*mob));else if(!has_zone && mob->zone)check(zone.leave(*mob));
        world.killed=killed;world.trace.clear();world.packet="-";world.observed_killed=false;std::array<std::int32_t,3> damage{70000,-2,10};
        delivery.finish_hit(*mob,105,actor,reflected,critical,damage);
        check(world.trace==trace && world.packet==packet && mob->id==expected_id && damage==std::array<std::int32_t,3>{hp,sp,mp} && world.observed_killed==(!reflected && killed));++count;
    }
    check(count==16);mob->id=100;if(mob->zone)check(zone.leave(*mob));root.mob_pool().release(*mob);
}
struct MirrorDefinitions final:VerticalMobsMirrorSkillDefinitions {
    std::int32_t expected_skill=0,expected_level=0;std::array<std::uint8_t,2> parameters{};unsigned lookups=0;
    std::array<std::uint8_t,2> mirror_parameters(std::int32_t skill,std::int32_t level) override {check(skill==expected_skill && level==expected_level);++lookups;return parameters;}
};
struct UserMirrorDefinitionOwner final:UserSkillRuntimeServices {
    SkillDefinition effect{};UserSkillDefinition info{effect,"Mirror"};unsigned calls=0;
    const UserSkillDefinition* definition(std::uint16_t id,std::uint8_t level) override {++calls;return id==42 && (level==1 || level==100)?&info:nullptr;}
    UserSkillHandle allocate() override {throw std::logic_error("Unexpected skill allocation");}
    void destroy(UserSkillHandle) noexcept override {std::terminate();}
    void strand(UserSkillHandle) noexcept override {std::terminate();}
    void invalid_definition(std::uint16_t,std::uint8_t) override {throw std::logic_error("Unexpected skill diagnostic");}
    void disconnect(std::uint32_t,std::uint32_t) override {throw std::logic_error("Unexpected disconnect");}
};
struct UserMirrorRecordSource {
    const UserSkillDefinition* expected=nullptr;std::array<std::byte,160> bytes{};std::size_t length=160;mutable unsigned calls=0;
    std::span<const std::byte> record(const UserSkillDefinition& info) const {check(&info==expected);++calls;return {bytes.data(),length};}
};
void check_user_mirror_binding(MobPrimaryZones& root,Frame& frame,Actor& actor){
    UserMirrorDefinitionOwner definitions;UserMirrorRecordSource records;records.expected=&definitions.info;records.bytes[0x54]=std::byte{1};records.bytes[0x55]=std::byte{100};
    VerticalMobsUserMirrorDefinitions<UserSkillRuntimeServices,UserMirrorRecordSource> skills(definitions,records);
    check(skills.mirror_parameters(42,1)==std::array<std::uint8_t,2>{1,100});check(skills.mirror_parameters(42,100)==std::array<std::uint8_t,2>{1,100});
    bool rejected=false;try{skills.mirror_parameters(65536,1);}catch(const std::out_of_range&){rejected=true;}check(rejected);
    rejected=false;try{skills.mirror_parameters(42,256);}catch(const std::out_of_range&){rejected=true;}check(rejected);
    rejected=false;try{skills.mirror_parameters(43,1);}catch(const std::logic_error&){rejected=true;}check(rejected);
    records.length=159;rejected=false;try{skills.mirror_parameters(42,1);}catch(const std::logic_error&){rejected=true;}check(rejected);records.length=160;
    auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->current_attack=0;auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());const auto saved_actor=actor.status();
    record[0x80]=record[0x81]=std::byte{0};for(unsigned i=0;i<6;++i)record[0x55+i]=std::byte{9};actor.status().defense[0].mirror_skill=42;actor.status().defense[0].mirror_level=1;MobAttackRoll random;
    check(vertical_mobs_attack_reflected(*mob,actor,frame.full,random,skills) && random.draws==1);
    check(definitions.calls==5 && records.calls==4);actor.status()=saved_actor;std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_attack_reflection(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);mob->current_attack=0;
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());const auto saved_actor=actor.status();
    MobAttackRoll random;MirrorDefinitions skills;std::string line;unsigned count=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,skill,level,melee,mode,parameter,roll,expected,lookups,draws;int intelligence,wisdom;std::array<unsigned,6> excluded{};
        row>>kind>>skill>>level>>melee>>mode>>parameter>>intelligence>>wisdom>>roll;for(auto& value:excluded)row>>value;row>>expected>>lookups>>draws;check(bool(row));
        record[0x80]=static_cast<std::byte>(kind);record[0x81]=std::byte{0};for(std::size_t i=0;i<excluded.size();++i)record[0x55+i]=static_cast<std::byte>(excluded[i]);
        actor.status().defense[0].mirror_skill=melee;actor.status().defense[kind].mirror_skill=skill;actor.status().defense[kind].mirror_level=level;
        actor.status().skills.equipment.base.attributes.intelligence=intelligence;actor.status().skills.equipment.base.attributes.wisdom=wisdom;
        random.value=roll;random.draws=0;skills.expected_skill=skill;skills.expected_level=level;skills.parameters={static_cast<std::uint8_t>(mode),static_cast<std::uint8_t>(parameter)};skills.lookups=0;
        check(vertical_mobs_attack_reflected(*mob,actor,frame.full,random,skills)==bool(expected) && skills.lookups==lookups && random.draws==draws);++count;
    }
    check(count==1024);actor.status()=saved_actor;std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
struct PrepareDatabase final:ActorEffectDatabase {void send_effect_database(const eternity::net::Packet&) noexcept override {std::terminate();}};
struct PrepareWorld final:ActorActionWorld,DuelWorld {
    std::string trace;std::int32_t next_duel=0;
    Actor* find_user(Actor&,std::uint32_t) override {throw std::logic_error("Unexpected vehicle lookup");}
    void send_move_cancel(Actor& actor,const eternity::net::Packet&) override {trace+='S';actor.relations().duel_state=next_duel;}
    Actor* find_duel_user(Actor&,std::uint32_t) override {trace+='D';return nullptr;}
    UserPosition duel_position(const Actor&) override {throw std::logic_error("Unexpected duel position");}
    std::int32_t drop_random() override {throw std::logic_error("Unexpected duel random");}
    DuelDropItem* allocate_drop_item() override {throw std::logic_error("Unexpected duel drop");}
    bool enter_duel_item(Actor&,DuelDropItem&,bool) override {throw std::logic_error("Unexpected duel item");}
    GameLogSubject duel_log_subject(const Actor&) override {throw std::logic_error("Unexpected duel subject");}
    std::chrono::sys_seconds local_calendar() override {throw std::logic_error("Unexpected duel calendar");}
    void send_duel_log(const eternity::net::Packet&) override {throw std::logic_error("Unexpected duel log");}
};
struct PrepareContinuation final:VerticalMobsAttackEffects {
    PrepareWorld& world;explicit PrepareContinuation(PrepareWorld& value):world(value){}
    void status_stand(Actor&) override {throw std::logic_error("Unexpected stand");}
    void send_miss(MobRuntime&,Actor&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected miss");}
    void hit(MobRuntime&,std::uint32_t now,Actor& actor) override {check(now==101 && actor.actions().pending_move==0);world.trace+='H';}
};
void check_hit_preparation(MobPrimaryZones& root,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules rules{items,enchant,attacks};FrameScene scene;PrepareDatabase database;
    Actor actor(42,{},PlayerResources{1,0,0},{},scene,rules,database);PrepareWorld world;PrepareContinuation continuation(world);VerticalMobsPreparedAttackEffects effects(world,world,continuation);
    std::string line;unsigned count=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);int initial,after;std::string trace;row>>initial>>after>>trace;check(bool(row));
        actor.relations().duel_state=initial;actor.relations().duel_opponent=77;actor.status().in_zone=true;actor.actions().move_type=2;actor.actions().pending_move=1;actor.actions().move_slot=0;
        world.next_duel=after;world.trace.clear();effects.hit(*mob,101,actor);check(world.trace==trace && actor.relations().duel_state==after);++count;
    }
    check(count==4);root.mob_pool().release(*mob);
}
struct AttackLogTransport final:MobLogTransport {
    std::vector<eternity::net::Packet> packets;
    void send_game_log(const eternity::net::Packet& packet) override {packets.push_back(packet);}
};
struct CalculatedRoll final:RandomSource {std::array<int,6> values{};unsigned draws=0;std::int32_t next() override {check(draws<values.size());return values[draws++];}};
struct CalculatedContinuation final:VerticalMobsCalculatedHitServices {
    unsigned calls=0;bool reflected=false,critical=false;std::array<std::int32_t,3> damage{};
    void status_stand(Actor&) override {throw std::logic_error("Unexpected stand");}
    void send_miss(MobRuntime&,Actor&,const eternity::net::Packet&) override {throw std::logic_error("Unexpected miss");}
    void finish_hit(MobRuntime&,std::uint32_t now,Actor&,bool reflect,bool crit,std::array<std::int32_t,3>& values) override {check(now==103);++calls;reflected=reflect;critical=crit;damage=values;}
};
void check_calculated_hit(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);auto definition=*frame.runtime.find(1);mob->definition=&definition;mob->current_attack=0;mob->lua_ai=0;mob->respawn=nullptr;mob->multipliers[0]=1.0F;
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());const auto saved_actor=actor.status();
    CalculatedRoll random;MirrorDefinitions skills;AttackLogTransport transport;CalculatedContinuation continuation;const MobLogContext context{7,"Mob"};
    VerticalMobsCalculatedHitEffects effects(frame.full,random,skills,context,transport,continuation);std::string line;unsigned count=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,mirror,mode,parameter,base,variance,mobwis,targetwis,reflected,critical,lookups,draws;int expected;
        row>>kind>>mirror>>mode>>parameter>>base>>variance>>mobwis>>targetwis;for(auto& value:random.values)row>>value;row>>reflected>>critical>>expected>>lookups>>draws;check(bool(row));
        const auto word=[&](unsigned offset,unsigned value){record[offset]=static_cast<std::byte>(value&255);record[offset+1]=static_cast<std::byte>((value>>8)&255);};
        word(0x80,kind);word(0x8a,base);word(0x8c,variance);for(unsigned i=0;i<6;++i)record[0x55+i]=std::byte{9};
        definition.level=10;definition.wisdom=mobwis;mob->attack[kind]={};actor.status().level=10;actor.status().calculated_attack[kind]={};actor.status().skills.equipment.defense_attribute=0;
        actor.status().skills.equipment.base.attributes.wisdom=targetwis;actor.status().skills.equipment.base.attributes.intelligence=20;
        actor.status().defense[0].mirror_skill=7;actor.status().defense[kind].mirror_skill=mirror;actor.status().defense[kind].mirror_level=1;
        skills.expected_skill=mirror;skills.expected_level=1;skills.parameters={static_cast<std::uint8_t>(mode),static_cast<std::uint8_t>(parameter)};skills.lookups=0;random.draws=0;continuation.calls=0;
        effects.hit(*mob,103,actor);
        if(continuation.reflected!=bool(reflected)||continuation.critical!=bool(critical)||continuation.damage[0]!=expected||skills.lookups!=lookups||random.draws!=draws)std::cerr<<"Calculated hit row "<<count<<" got "<<continuation.reflected<<' '<<continuation.critical<<' '<<continuation.damage[0]<<' '<<skills.lookups<<' '<<random.draws<<" expected "<<reflected<<' '<<critical<<' '<<expected<<' '<<lookups<<' '<<draws<<'\n';
        check(continuation.calls==1 && continuation.reflected==bool(reflected) && continuation.critical==bool(critical) && continuation.damage[0]==expected && continuation.damage[1]==0 && continuation.damage[2]==0 && skills.lookups==lookups && random.draws==draws && transport.packets.empty());++count;
    }
    check(count==256);actor.status()=saved_actor;std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_damage_log(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);auto definition=*frame.runtime.find(1);mob->definition=&definition;mob->current_attack=0;mob->id=777;
    auto record=frame.full.mutable_mob_record(1);const auto old0=record[0x80],old1=record[0x81];record[0x80]=std::byte{3};record[0x81]=std::byte{0};
    const auto old_name=actor.appearance().identity.name;actor.appearance().identity.name="Target";
    ZoneMobs zone(frame.maps->all()[0].definition.geometry(),frame.backend);check(zone.enter(*mob));MobRespawn respawn{};MobAttackRoll random;random.value=6;AttackLogTransport transport;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned lua,present,both,input,calls,subject,user,event,detail,amount;
        row>>lua>>present>>both>>input>>calls>>subject>>user>>event>>detail>>amount;check(bool(row));
        check(input==23456 && (!calls || (subject==1 && user==1 && event==1 && detail==0 && amount==input)));
        mob->lua_ai=lua;respawn.both=both;mob->respawn=present?&respawn:nullptr;transport.packets.clear();random.draws=0;
        std::array<std::int32_t,3> damage{};check(!vertical_mobs_logged_normal_attack_damage(*mob,actor,frame.full,random,damage,{7,"Mob"},transport));
        check(damage[0]==7 && random.draws==1 && transport.packets.size()==calls);
        if(calls) {const auto& p=transport.packets[0];check(p.size()==123 && p[0]==4 && p[1]==10 && p[2]==7 && p[16]==1 && p[17]=='M' && p[49]=='T' && p[81]==0 && p[113]==7 && p[114]==0);}
        ++count;
    }
    check(count==8);check(zone.leave(*mob));mob->lua_ai=1;transport.packets.clear();std::array<std::int32_t,3> damage{};
    vertical_mobs_logged_normal_attack_damage(*mob,actor,frame.full,random,damage,{7,"Mob"},transport);check(transport.packets.empty());
    mob->lua_ai=0;mob->respawn=nullptr;record[0x80]=old0;record[0x81]=old1;actor.appearance().identity.name=old_name;root.mob_pool().release(*mob);
}
struct MobDamageRoll final:RandomSource {std::array<int,5> values{};unsigned draws=0;std::int32_t next() override {check(draws<values.size());return values[draws++];}};
void check_normal_attack_damage(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);auto definition=*frame.runtime.find(1);mob->definition=&definition;mob->current_attack=0;
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());const auto saved_actor=actor.status();
    MobDamageRoll random;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,ml,ul,base,variance,attribute,wisdom,expected_critical,draws;int attack,defense,critical,userwisdom,expected;float multiplier;
        row>>kind>>ml>>ul>>base>>variance>>multiplier>>attack>>defense>>attribute>>wisdom>>critical>>userwisdom;
        for(auto& roll:random.values)row>>roll;row>>expected_critical>>expected>>draws;check(bool(row));
        const auto word=[&](unsigned offset,unsigned value){record[offset]=static_cast<std::byte>(value&255);record[offset+1]=static_cast<std::byte>((value>>8)&255);};
        word(0x80,kind);word(0x8a,base);word(0x8c,variance);
        definition.level=ml;definition.wisdom=wisdom;mob->multipliers[0]=multiplier;actor.status().level=ul;actor.status().skills.equipment.defense_attribute=attribute;actor.status().skills.equipment.base.attributes.wisdom=userwisdom;
        if(kind<3){mob->attack[kind].attack=attack;mob->attack[kind].critical_attack=critical;actor.status().calculated_attack[kind].defense=defense;}
        std::array<std::int32_t,3> damage{};random.draws=0;const auto result=vertical_mobs_normal_attack_damage(*mob,actor,frame.full,random,damage);
        if(result!=bool(expected_critical)||damage[0]!=expected||damage[1]!=0||damage[2]!=0||random.draws!=draws)std::cerr<<"Mob damage fixture row "<<count<<" actual "<<result<<' '<<damage[0]<<' '<<random.draws<<" expected "<<expected_critical<<' '<<expected<<' '<<draws<<'\n';
        check(result==bool(expected_critical) && damage[0]==expected && damage[1]==0 && damage[2]==0 && random.draws==draws);++count;
    }
    check(count==2048);actor.status()=saved_actor;std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_attack_damage_limits(const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());MobAttackRoll random;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::array<std::int32_t,3> damage{},expected{};unsigned draws;
        row>>damage[0]>>damage[1]>>damage[2]>>random.value>>expected[0]>>expected[1]>>expected[2]>>draws;check(bool(row));
        random.draws=0;vertical_mobs_attack_damage_limits(damage,random);
        check(damage==expected && random.draws==draws);++count;
    }
    check(count==256);
}
struct MobAttackBlock final:VerticalMobsAttackBlock {
    Actor* actor=nullptr;bool blocked=false;unsigned calls=0;std::uint16_t expected=0;
    bool blocks(Actor& target,std::uint16_t kind) override {check(&target==actor && kind==expected);++calls;return blocked;}
};
struct AttackBlockEffects final:VerticalMobsBlockEffects {
    Actor* actor=nullptr;unsigned kind=0,calls=0;
    void remove_attack_blocks(Actor& value) override {check(&value==actor && value.status().defense[kind].block_value==0);++calls;value.status().defense[kind].block_mode=0;}
};
struct IntegratedHitWorld final:VerticalMobsHitTailWorld {
    std::string trace;std::int32_t expected_damage=101;
    void status_stand(Actor&) override {trace+='S';}
    void send_miss(MobRuntime&,Actor&,const eternity::net::Packet& packet) override {check(packet.size()==17 && packet[0]==5 && packet[1]==6 && packet[2]==3);trace+='M';}
    bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>& damage) override {check(damage[0]==expected_damage);trace+='D';return false;}
    void user_killed(MobRuntime&,Actor&) override {throw std::logic_error("Unexpected user death");}
    void send_hit(MobRuntime&,std::uint32_t excluded,const eternity::net::Packet& packet) override {check(excluded==42 && packet.size()==17 && packet[0]==5 && packet[1]==6 && packet[2]==0 && packet[11]==101);trace+='P';}
    bool damage_mob(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override {throw std::logic_error("Unexpected reflection");}
    void send_reflection(MobRuntime&,std::uint32_t,const eternity::net::Packet&) override {throw std::logic_error("Unexpected reflected packet");}
    void final_attack_effects(MobRuntime&,Actor&) override {trace+='F';}
};
void check_native_attack_composition(MobPrimaryZones& root,Frame& frame){
    auto* mob=root.mob_pool().allocate();check(mob);auto definition=*frame.runtime.find(1);definition.level=10;definition.wisdom=0;mob->definition=&definition;mob->id=77;mob->position={10,0,10};mob->lua_ai=0;mob->respawn=nullptr;mob->sleep=mob->stun=0;mob->attack_disabled={};mob->current_attack=0;mob->attack_speed=3;mob->multipliers={1,1,1,1};mob->attack[0]={};mob->attack[0].accuracy=10;mob->attack[0].evasion=10;
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    const auto word=[&](unsigned offset,unsigned value){record[offset]=static_cast<std::byte>(value&255);record[offset+1]=static_cast<std::byte>((value>>8)&255);};
    word(0x80,0);word(0x8a,100);word(0x8c,0);for(unsigned i=0;i<4;++i)record[0x84+i]=static_cast<std::byte>((2000U>>(8*i))&255);
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;PrepareDatabase database;Actor actor(42,{},PlayerResources{1,0,0},{},scene,equipment,database);
    actor.status().level=10;actor.status().calculated_attack[0].accuracy=10;actor.status().calculated_attack[0].evasion=10;
    ZoneMobs zone(frame.maps->all()[0].definition.geometry(),frame.backend);check(zone.enter(*mob));
    CalculatedRoll random;random.values={0,99,0,0,0,0};MirrorDefinitions skills;AttackBlockEffects block;block.actor=&actor;AttackLogTransport log;const MobLogContext context{7,"Mob"};PrepareWorld actions;IntegratedHitWorld world;MobDeathExperience rules;HitTailFixture death;death.actor=&actor;StationaryWorld other;
    VerticalMobsNormalAttackComposition native(frame.full,random,block,skills,context,log,actions,actions,world,rules,death,other);
    native.attack(*mob,100,actor);check(world.trace=="SDPF" && random.draws==2 && mob->next_attack==2100 && log.packets.empty());
    world.trace.clear();random.draws=0;actor.status().block_all=1;native.attack(*mob,110,actor);check(world.trace=="SM" && random.draws==0 && mob->next_attack==2110);
    world.trace.clear();mob->sleep=1;native.attack(*mob,120,actor);check(world.trace.empty() && mob->next_attack==2110);
    mob->sleep=0;check(zone.leave(*mob));std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_attack_block(Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());const auto saved=actor.status();MobAttackRoll random;AttackBlockEffects effects;effects.actor=&actor;
    VerticalMobsActorAttackBlock block(random,effects);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,result,expected_mode,expected_value,removes,draws;int all,mode,value,roll;
        row>>kind>>all>>mode>>value>>roll>>result>>expected_mode>>expected_value>>removes>>draws;check(bool(row) && kind<3);
        actor.status().block_all=all;actor.status().defense[kind].block_mode=mode;actor.status().defense[kind].block_value=value;
        random.value=roll;random.draws=0;effects.kind=kind;effects.calls=0;
        check(block.blocks(actor,static_cast<std::uint16_t>(kind))==bool(result) && static_cast<unsigned>(actor.status().defense[kind].block_mode)==expected_mode && std::bit_cast<unsigned>(actor.status().defense[kind].block_value)==expected_value && effects.calls==removes && random.draws==draws);++count;
    }
    check(count==13);actor.status().block_all=0;actor.status().defense[0]={2,0,0,0};actor.status().level=10;actor.status().calculated_attack[0].accuracy=0;actor.status().calculated_attack[0].evasion=10;
    MobRuntimeTemplate definition{};definition.level=10;MobRuntime mob;mob.definition=&definition;mob.attack[0].accuracy=10;mob.attack[0].evasion=10;random.value=0;random.draws=0;
    check(!vertical_mobs_attack_success(mob,actor,0,random,block) && random.draws==1);
    actor.status().defense[0].block_value=-1;random.draws=0;
    check(vertical_mobs_attack_success(mob,actor,0,random,block) && random.draws==2);
    actor.status()=saved;
}
void check_attack_critical(MobPrimaryZones& root,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);const auto saved=actor.status();MobAttackRoll random;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,result,draws;int critical,wisdom,roll,raw;
        row>>kind>>critical>>wisdom>>roll>>raw>>result>>draws;check(bool(row) && kind<3);
        mob->attack[kind].critical_attack=critical;actor.status().skills.equipment.base.attributes.wisdom=wisdom;random.value=roll;random.draws=0;
        check(vertical_mobs_attack_critical_chance(*mob,actor,kind)==raw && vertical_mobs_attack_critical(*mob,actor,kind,random)==bool(result) && random.draws==draws);++count;
    }
    check(count==1024);actor.status()=saved;root.mob_pool().release(*mob);
}
void check_attack_success(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);auto definition=*frame.runtime.find(1);mob->definition=&definition;
    const auto saved=actor.status();MobAttackRoll random;MobAttackBlock block;block.actor=&actor;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned kind,blocked,ul,ml,md,expected,blocks,draws;int ua,ue,ma,me,dex,roll;
        row>>kind>>blocked>>ul>>ml>>ua>>ue>>ma>>me>>dex>>md>>roll>>expected>>blocks>>draws;check(bool(row));
        actor.status().level=static_cast<std::uint16_t>(ul);definition.level=static_cast<std::uint16_t>(ml);definition.dexterity=static_cast<std::uint16_t>(md);
        actor.status().skills.equipment.base.attributes.dexterity=dex;
        if(kind<3){actor.status().calculated_attack[kind].accuracy=ua;actor.status().calculated_attack[kind].evasion=ue;mob->attack[kind].accuracy=ma;mob->attack[kind].evasion=me;}
        block.expected=static_cast<std::uint16_t>(kind);block.blocked=blocked;block.calls=0;random.value=roll;random.draws=0;
        check(vertical_mobs_attack_success(*mob,actor,static_cast<std::uint16_t>(kind),random,block)==bool(expected) && block.calls==blocks && random.draws==draws);++count;
    }
    check(count==1024);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved_record(record.begin(),record.end());
    record[0x80]=record[0x81]=std::byte{0};for(unsigned i=0;i<4;++i)record[0x84+i]=static_cast<std::byte>((2000U>>(i*8))&255);
    actor.status().level=10;actor.status().calculated_attack[0].accuracy=0;actor.status().calculated_attack[0].evasion=10;
    definition.level=10;mob->attack[0].accuracy=10;mob->attack[0].evasion=10;mob->sleep=mob->stun=0;mob->attack_disabled={};mob->current_attack=0;mob->attack_speed=3;mob->multipliers[1]=1;mob->next_attack=0;
    block.expected=0;block.blocked=false;block.calls=0;random.value=0;random.draws=0;
    AttackTargetOracle effects;effects.subject=mob;effects.actor=&actor;effects.record=record;StationaryWorld other;other.subject=mob;other.target=&actor;
    VerticalMobsNormalAttackTarget normal(random,block,effects);VerticalMobsAttackTarget full(frame.full,normal,other);full.attack(*mob,100,actor);
    check(effects.trace=="SH" && block.calls==1 && random.draws==1 && mob->next_attack==2100);
    std::copy(saved_record.begin(),saved_record.end(),record.begin());actor.status()=saved;mob->definition=frame.runtime.find(1);root.mob_pool().release(*mob);
}
struct ChaseWorld final:VerticalMobsChaseAttackWorld {
    MobRuntime* subject=nullptr;Actor* target=nullptr;std::span<std::byte> record,skill_record;std::uint32_t now=0,mutation=0;bool reached=false,clear=false,is_skill=false;std::string trace;int during=999;
    bool chase(MobRuntime& mob,std::uint32_t tick,Actor& actor,std::uint8_t range) override {check(&mob==subject && &actor==target && tick==now && range==(is_skill?6:5));trace+='C';if(clear)mob.target_kind=0;return reached;}
    void attack(MobRuntime& mob,std::uint32_t tick,Actor& actor) override {check(&mob==subject && &actor==target && tick==now);trace+='A';during=mob.current_attack;if(mutation){mob.current_attack=2;mob.attack_speed=6;for(unsigned i=0;i<4;++i)record[0xac+i]=static_cast<std::byte>((777U>>(i*8))&255);}}
    void use_skill(MobRuntime& mob,std::uint32_t tick,Actor& actor,const MobTargetingBindingSkill& skill) override {check(&mob==subject && &actor==target && tick==now && skill.id==1);trace+='U';during=mob.current_attack;if(mutation){skill_record[0x4a]=std::byte{7};skill_record[0x4b]=std::byte{0};mob.current_attack=77;}}
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override {throw std::logic_error("unexpected chase remainder");}
};
void check_stationary_attack(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture,const std::filesystem::path& skill_fixture,const std::filesystem::path& chase_fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);mob->position={0,0,0};
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ZoneMobs mobs(frame.maps->all()[0].definition.geometry(),frame.backend);ZoneUsers users(frame.maps->all()[0].definition.geometry(),0);check(mobs.enter(*mob));check(users.attach_at_position(actor,{1,0,0}));
    std::string line;unsigned count=0;while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,slot,ai,speed,obelisk,range,distance,status,mutation,during,next,current,expected_status,kind;std::string trace;
        row>>now>>slot>>ai>>speed>>obelisk>>range>>distance>>status>>mutation>>trace>>during>>next>>current>>expected_status>>kind;check(bool(row) && slot<3);
        record[0x26]=static_cast<std::byte>(ai);record[0x68]=static_cast<std::byte>(speed);record[0x7c]=static_cast<std::byte>(obelisk);record[0x3f]=std::byte{0};const auto at=0x80+slot*20;record[at]=record[at+1]=std::byte{0};record[at+8]=static_cast<std::byte>(range);
        for(unsigned i=0;i<4;++i)record[at+4+i]=static_cast<std::byte>((2000U>>(i*8))&255);
        check(users.move_at_position(actor.id(),{std::bit_cast<float>(distance),0,0}));mob->status=static_cast<int>(status);mob->target_kind=7;mob->attack_speed=3;mob->current_attack=99;mob->attack_times={10,11,12};
        StationaryWorld world;world.subject=mob;world.target=&actor;world.now=now;world.mutation=mutation;world.record=record;VerticalMobsAttackAiStationary stationary(mobs,users,frame.full,world);stationary.other_attack(*mob,now,actor,slot);
        check(world.calls==(trace=="RA"?1U:0U) && world.during==static_cast<int>(during) && mob->attack_times[slot]==next && mob->current_attack==static_cast<int>(current) && mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind));++count;
    }
    check(count==7);auto skill=frame.full.mutable_skill_record(1);const std::vector<std::byte> saved_skill(skill.begin(),skill.end());std::ifstream skill_in(skill_fixture);check(skill_in.good());count=0;
    while(std::getline(skill_in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,slot,ai,speed,obelisk,range,distance,status,cooldown,mutation,during,next,current,expected_status,kind;std::string trace;
        row>>now>>slot>>ai>>speed>>obelisk>>range>>distance>>status>>cooldown>>mutation>>trace>>during>>next>>current>>expected_status>>kind;check(bool(row) && slot<3);
        record[0x26]=static_cast<std::byte>(ai);record[0x68]=static_cast<std::byte>(speed);record[0x7c]=static_cast<std::byte>(obelisk);record[0x3f]=std::byte{0};const auto at=0x80+slot*20;record[at]=std::byte{1};record[at+1]=std::byte{0};for(unsigned i=0;i<4;++i)record[at+16+i]=static_cast<std::byte>(i==0?1:0);frame.full.publish_mob_record(1);
        skill[0x31]=std::byte{1};skill[0x4a]=static_cast<std::byte>(cooldown&255);skill[0x4b]=static_cast<std::byte>(cooldown>>8);skill[0x4c]=static_cast<std::byte>(range);
        check(users.move_at_position(actor.id(),{std::bit_cast<float>(distance),0,0}));mob->status=static_cast<int>(status);mob->target_kind=7;mob->current_attack=99;mob->attack_times={10,11,12};
        StationaryWorld world;world.subject=mob;world.target=&actor;world.now=now;world.mutation=mutation;world.record=record;world.skill_record=skill;VerticalMobsAttackAiStationary stationary(mobs,users,frame.full,world);stationary.other_attack(*mob,now,actor,slot);
        check(world.calls==(trace=="RU"?1U:0U) && world.during==static_cast<int>(during) && mob->attack_times[slot]==next && mob->current_attack==static_cast<int>(current) && mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind));++count;
    }
    check(count==6);std::ifstream chase_in(chase_fixture);check(chase_in.good());count=0;
    while(std::getline(chase_in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,slot,is_skill,reached,kind,clear,mutation,during,next,current,expected_kind;std::string trace;
        row>>now>>slot>>is_skill>>reached>>kind>>clear>>mutation>>trace>>during>>next>>current>>expected_kind;check(bool(row) && slot<3);
        record[0x26]=std::byte{0};record[0x68]=std::byte{2};record[0x7c]=std::byte{0};const auto at=0x80+slot*20;record[at]=static_cast<std::byte>(is_skill);record[at+1]=std::byte{0};record[at+8]=std::byte{5};for(unsigned i=0;i<4;++i){record[at+4+i]=static_cast<std::byte>((2000U>>(i*8))&255);record[at+16+i]=static_cast<std::byte>(is_skill && i==0?1:0);}frame.full.publish_mob_record(1);
        skill[0x31]=std::byte{1};skill[0x4a]=std::byte{5};skill[0x4b]=std::byte{0};skill[0x4c]=std::byte{6};mob->target_kind=static_cast<int>(kind);mob->attack_speed=3;mob->current_attack=99;mob->attack_times={10,11,12};
        ChaseWorld world;world.subject=mob;world.target=&actor;world.now=now;world.mutation=mutation;world.record=record;world.skill_record=skill;world.reached=reached;world.clear=clear;world.is_skill=is_skill;
        VerticalMobsAttackAiChase chase(frame.full,world);VerticalMobsAttackAiStationary stationary(mobs,users,frame.full,chase);stationary.other_attack(*mob,now,actor,slot);
        check(trace.substr(1)==world.trace && world.during==static_cast<int>(during) && mob->attack_times[slot]==next && mob->current_attack==static_cast<int>(current) && mob->target_kind==static_cast<int>(expected_kind));++count;
    }
    check(count==9);check(users.detach(actor.id()));check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());std::copy(saved_skill.begin(),saved_skill.end(),skill.begin());frame.full.publish_skill_record(1);frame.full.publish_mob_record(1);
}
struct SkillEffectObserver final:VerticalMobsSkillEffects {
    MobRuntime* mob=nullptr;Actor* actor=nullptr;const MobTargetingBindingSkill* skill=nullptr;int mutation=999;std::string trace;
    void mark(char event,MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s){check(&m==mob && &a==actor && &s==skill);trace+=event;if(mutation!=999)m.attack_speed=mutation;}
    void attack(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {mark('A',m,a,s);}
    void heal(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {mark('H',m,a,s);}
    void debuff(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {mark('D',m,a,s);}
};
struct SkillAttackObserver final:VerticalMobsSkillAttackWorld {
    MobRuntime* mob=nullptr;Actor* actor=nullptr;MobTargetingBindingSkill* skill=nullptr;unsigned hits=0,killat=0;bool dead=false,mutate=false;std::string trace;
    void send_skill(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {check(&m==mob && &a==actor && &s==skill);trace+='S';}
    void attack_user(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s,bool send) override {check(&m==mob && &a==actor && &s==skill);trace+=send?'A':'a';++hits;if(killat && hits==killat)a.attack_state().mode=1;if(mutate && hits==1)skill->record[0x58]=std::byte{1};}
    bool self_damage(MobRuntime& m,std::uint32_t hp,const MobCombatCredit& credit) override {check(&m==mob && hp==100 && credit.type==2 && credit.id==77 && credit.party==0 && credit.bonus==0 && credit.base_aggro==0);trace+='X';return dead;}
    void range(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {check(&m==mob && &a==actor && &s==skill);trace+='R';}
    void range_select(MobRuntime& m,Actor& a,const MobTargetingBindingSkill& s) override {check(&m==mob && &a==actor && &s==skill);trace+='Q';}
};
struct SkillDeathObserver final:MobDeathWorld {
    unsigned calls=0;void send_death(MobRuntime&,const eternity::net::Packet&,std::uint8_t) override {throw std::logic_error("unexpected death view");}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected death user");}
    MobDeathParty* party(MobDeathUser&) override {throw std::logic_error("unexpected death party");}
    void lock_party(MobDeathParty&) override {throw std::logic_error("unexpected party lock");}
    void unlock_party(MobDeathParty&) noexcept override {std::terminate();}
    bool in_reward_range(MobDeathUser&,MobDeathUser&,std::int32_t) override {throw std::logic_error("unexpected range");}
    void add_experience(MobDeathUser&,std::uint32_t,std::int32_t) override {throw std::logic_error("unexpected experience");}
    void drop_items(MobRuntime&,MobDeathUser*,MobDeathParty*) override {throw std::logic_error("unexpected drop");}
    void quest_kill(MobDeathUser&,MobRuntime&) override {throw std::logic_error("unexpected quest");}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected obelisk");}
    void destroy_obelisk(MobRuntime&,std::int32_t,std::int32_t) override {throw std::logic_error("unexpected obelisk destroy");}
    void destroy_linked_group(MobRuntime&,std::int32_t) override {throw std::logic_error("unexpected linked group");}
    void remove_obelisk_member(MobRuntime&,void*) override {throw std::logic_error("unexpected obelisk remove");}
    void remove_linked_member(MobRuntime&,void*) override {throw std::logic_error("unexpected linked remove");}
    void log_death(MobRuntime&,MobDeathUser*,bool) override {throw std::logic_error("unexpected death log");}
    void clear_applied_skills(MobRuntime&) override {++calls;}
};
struct OtherSkillObserver final:VerticalMobsSkillOtherEffects {
    unsigned heals=0,debuffs=0;void heal(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override {++heals;}
    void debuff(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override {++debuffs;}
};
void check_skill_attack(MobPrimaryZones& root,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->id=77;mob->lua_ai=0;mob->respawn=nullptr;mob->hp=100;
    MobTargetingBindingSkill skill;SkillAttackObserver world;world.mob=mob;world.actor=&actor;world.skill=&skill;SkillDeathObserver death;MobDeathExperience rules;OtherSkillObserver other;
    VerticalMobsBoundSkillEffects effects(world,rules,death,other);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;std::istringstream row(line);unsigned target,detail,multi,killat,status,dead,mutate,final_mode,final_multi;std::string trace;
        row>>target>>detail>>multi>>killat>>status>>dead>>mutate>>trace>>final_mode>>final_multi;check(bool(row));
        skill.record[0x56]=static_cast<std::byte>(target);skill.record[0x32]=static_cast<std::byte>(detail);skill.record[0x33]=std::byte{0};skill.record[0x58]=static_cast<std::byte>(multi);
        mob->status=status;mob->damage->reset();mob->damage->fair_combat=false;actor.attack_state().mode=0;world.trace.clear();world.hits=0;world.killat=killat;world.dead=dead;world.mutate=mutate;death.calls=0;
        effects.attack(*mob,actor,skill);if(death.calls)world.trace+='K';
        check((world.trace.empty()?"-":world.trace)==trace && actor.attack_state().mode==static_cast<int>(final_mode) && std::to_integer<unsigned>(skill.record[0x58])==final_multi && death.calls==(trace.find('K')!=std::string::npos?1U:0U));++count;
    }
    check(count==1152);effects.heal(*mob,actor,skill);effects.debuff(*mob,actor,skill);check(other.heals==1 && other.debuffs==1);mob->damage->reset();actor.attack_state().mode=0;root.mob_pool().release(*mob);
}
void check_use_skill(MobPrimaryZones& root,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);MobTargetingBindingSkill skill;
    StationaryWorld ordinary;ordinary.subject=mob;ordinary.target=&actor;SkillEffectObserver effects;effects.mob=mob;effects.actor=&actor;effects.skill=&skill;
    VerticalMobsSkillDispatch dispatch(ordinary,effects);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;std::istringstream row(line);std::uint32_t now,sleep,stun,kind,disabled,effect,deadline,final_speed;int speed,mutate;std::string trace;
        row>>now>>sleep>>stun>>kind>>disabled>>effect>>speed>>mutate>>trace>>deadline>>final_speed;check(bool(row));
        mob->sleep=sleep;mob->stun=stun;mob->attack_speed=speed;mob->attack_disabled={};mob->attack_disabled[kind]=disabled;mob->next_attack=12345;
        skill.record[0x30]=static_cast<std::byte>(kind);skill.record[0x31]=static_cast<std::byte>(effect);effects.trace.clear();effects.mutation=mutate;
        dispatch.use_skill(*mob,now,actor,skill);check((effects.trace.empty()?"-":effects.trace)==trace && mob->next_attack==deadline && std::bit_cast<std::uint32_t>(mob->attack_speed)==final_speed);++count;
    }
    check(count==1440);mob->sleep=mob->stun=0;mob->attack_disabled={};ordinary.now=77;ordinary.calls=0;dispatch.attack(*mob,77,actor);check(ordinary.calls==1);
    skill.record[0x30]=std::byte{3};bool rejected=false;try{dispatch.use_skill(*mob,77,actor,skill);}catch(const std::out_of_range&){rejected=true;}check(rejected);
    root.mob_pool().release(*mob);
}
void check_attack_ai_self(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);auto skill=frame.full.mutable_skill_record(1);const std::vector<std::byte> saved(record.begin(),record.end()),saved_skill(skill.begin(),skill.end());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,slot,ai,speed,obelisk,cooldown,mutation,during,next,current;std::string trace;
        row>>now>>slot>>ai>>speed>>obelisk>>cooldown>>mutation>>trace>>during>>next>>current;check(bool(row) && slot<3);
        record[0x26]=static_cast<std::byte>(ai);record[0x68]=static_cast<std::byte>(speed);record[0x7c]=static_cast<std::byte>(obelisk);
        const auto at=0x80+slot*20;record[at]=std::byte{1};record[at+1]=std::byte{0};for(unsigned i=0;i<4;++i)record[at+16+i]=static_cast<std::byte>(i==0?1:0);frame.full.publish_mob_record(1);
        skill[0x31]=std::byte{2};skill[0x4a]=static_cast<std::byte>(cooldown&255);skill[0x4b]=static_cast<std::byte>(cooldown>>8);mob->current_attack=99;mob->attack_times={10,11,12};
        AttackAiSkillWorld world;world.subject=mob;world.target=&actor;world.tick=now;world.skill=skill;world.mutation=mutation;
        VerticalMobsAttackAiSelfSkill self(frame.full,world);if(count%2)self.after_target_set(*mob,now,actor,slot);else self.after_aggro(*mob,now,actor,slot);
        check(trace=="AU" && world.calls==1 && world.other==0 && world.during==static_cast<int>(during) && mob->attack_times[slot]==next && mob->current_attack==static_cast<int>(current));++count;
    }
    check(count==6);
    AttackAiSkillWorld other;other.subject=mob;other.target=&actor;other.tick=100;other.skill=skill;VerticalMobsAttackAiSelfSkill self(frame.full,other);
    record[0x80]=std::byte{0};record[0x81]=std::byte{0};mob->current_attack=99;self.after_aggro(*mob,100,actor,0);check(other.other==1 && mob->current_attack==99);
    record[0x80]=std::byte{1};record[0x81]=std::byte{0};record[0x90]=std::byte{1};record[0x91]=record[0x92]=record[0x93]=std::byte{0};frame.full.publish_mob_record(1);skill[0x31]=std::byte{1};self.after_target_set(*mob,100,actor,0);check(other.other==2 && other.calls==0 && mob->current_attack==99);
    std::copy(saved.begin(),saved.end(),record.begin());std::copy(saved_skill.begin(),saved_skill.end(),skill.begin());frame.full.publish_skill_record(1);frame.full.publish_mob_record(1);root.mob_pool().release(*mob);
}
struct AttackAiAfterTarget final:VerticalMobsAttackAiAfterTarget {
    MobRuntime* subject=nullptr;Actor* original=nullptr;std::uint32_t slot=UINT32_MAX;unsigned calls=0;
    void after_target_set(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t value) override {check(&mob==subject && &actor==original && now==100 && mob.target_id==501 && mob.target_kind==1);slot=static_cast<std::uint32_t>(value);++calls;}
};
struct AttackAiAfterAggro final:VerticalMobsAttackAiAfterAggro {
    MobRuntime* subject=nullptr;Actor* original=nullptr;unsigned calls=0;std::uint32_t slot=UINT32_MAX;
    void after_aggro(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t value) override {check(&mob==subject && &actor==original && now==100 && mob.target_id==777);slot=static_cast<std::uint32_t>(value);++calls;}
};
void check_attack_ai_mode6(MobPrimaryZones& root,Frame& frame,Actor& original,ActorScene& scene,ActorEquipmentRules equipment,const std::filesystem::path& fixture,const std::filesystem::path& aggro_fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->damage);mob->definition=frame.runtime.find(1);check(mob->definition);mob->position={10,0,10};
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());for(unsigned i=0;i<4;++i)record[0x78+i]=static_cast<std::byte>(i==0?2:0);
    ZoneMobs mobs(frame.maps->all()[0].definition.geometry(),frame.backend);ZoneUsers users(frame.maps->all()[0].definition.geometry(),0);check(mobs.enter(*mob));
    auto candidate=std::make_shared<Actor>(501,EffectStatus{},PlayerResources{10,0,0},std::vector<PersistedEffect>{},scene,equipment);candidate->appearance().identity.country=1;check(users.attach_at_position(*candidate,{10,0,10}));
    MinHpWorld world;world.actors.push_back(candidate);FrameStack stack;MobTargetingActorWorld targeting_world(mobs,users,frame.random,stack);MobTargetingZone targeting(mobs,frame.full,targeting_world);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned fixed,found,lower,expected_slot,next;std::string trace;row>>fixed>>found>>lower>>trace>>expected_slot>>next;check(bool(row));
        mob->damage->reset();if(found)mob->damage->add(1,501,1,1,0);mob->status=0;mob->state_stack.reset();mob->vision_fixed_user=fixed;mob->target_kind=0;mob->target_id=0;mob->attack_speed=3;mob->attack_times.fill(0);mob->next_attack=12345;
        record[0x82]=std::byte{0};record[0x96]=static_cast<std::byte>(lower);record[0xaa]=std::byte{6};
        AttackAiReady ready;ready.subject=mob;ready.target=&original;ready.now=100;AttackAiAfterTarget after;after.subject=mob;after.original=&original;
        VerticalMobsAttackAiMode6 mode6(mobs,users,frame.full,world,targeting,ready,after);VerticalMobsAttackAiScan scan(frame.full,mode6);scan.continue_attack_ai(*mob,100,original);
        check(mob->next_attack==next && (after.calls?after.slot:ready.slot)==expected_slot);
        check(after.calls==(trace=="FSC"?1U:0U) && (trace=="FSC"?mob->target_id==501:mob->target_id==0));++count;
    }
    check(count==6);
    std::ifstream aggro_in(aggro_fixture);check(aggro_in.good());count=0;
    while(std::getline(aggro_in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned fixed,found,lower,expected_slot,next;std::string trace;row>>fixed>>found>>lower>>trace>>expected_slot>>next;check(bool(row));
        mob->damage->reset();if(found)mob->damage->add(1,501,1,1,0);mob->status=0;mob->state_stack.reset();mob->vision_fixed_user=fixed;mob->target_kind=0;mob->target_id=0;mob->attack_speed=3;mob->attack_times.fill(0);mob->next_attack=12345;
        record[0x82]=std::byte{0};record[0x96]=static_cast<std::byte>(lower);record[0xaa]=std::byte{6};
        AggroWorld aggro_world;aggro_world.actors.push_back(candidate);AttackAiAfterAggro after_aggro;after_aggro.subject=mob;after_aggro.original=&original;
        VerticalMobsAttackAiAggro aggro(aggro_world,after_aggro);AttackAiAfterTarget after_target;after_target.subject=mob;after_target.original=&original;
        VerticalMobsAttackAiMode6 mode6(mobs,users,frame.full,aggro_world,targeting,aggro,after_target);VerticalMobsAttackAiScan scan(frame.full,mode6);scan.continue_attack_ai(*mob,100,original);
        const bool via_aggro=trace=="AC" || trace=="FAC";const bool via_target=trace=="FSC";
        check(mob->next_attack==next && (via_aggro?after_aggro.slot:after_target.slot)==expected_slot && aggro_world.calls==(via_aggro?1U:0U) && after_aggro.calls==(via_aggro?1U:0U) && after_target.calls==(via_target?1U:0U));++count;
    }
    check(count==6);mob->damage->reset();check(users.detach(candidate->id()));check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
void check_attack_ai_wait(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,enabled[3],times[3],slot,next;int speed;
        row>>now>>speed>>enabled[0]>>enabled[1]>>enabled[2]>>times[0]>>times[1]>>times[2]>>slot>>next;check(bool(row));
        for(unsigned i=0;i<3;++i){record[0x82+i*20]=static_cast<std::byte>(enabled[i]);mob->attack_times[i]=times[i];}
        mob->attack_speed=speed;mob->next_attack=12345;AttackAiReady ready;ready.subject=mob;ready.target=&actor;ready.now=now;
        VerticalMobsAttackAiScan scan(frame.full,ready);scan.continue_attack_ai(*mob,now,actor);
        check(ready.slot==slot && mob->next_attack==next);++count;
    }
    check(count==15);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_attack_ai_hp(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t maximum,hp,mode[3],slot,next;row>>maximum>>hp>>mode[0]>>mode[1]>>mode[2]>>slot>>next;check(bool(row));
        for(unsigned i=0;i<4;++i)record[0x30+i]=static_cast<std::byte>((maximum>>(8*i))&255);
        for(unsigned i=0;i<3;++i){record[0x82+i*20]=static_cast<std::byte>(mode[i]);mob->attack_times[i]=0;}
        mob->hp=hp;mob->attack_speed=3;mob->next_attack=12345;AttackAiReady ready;ready.subject=mob;ready.target=&actor;ready.now=100;
        VerticalMobsAttackAiScan scan(frame.full,ready);scan.continue_attack_ai(*mob,100,actor);
        check(ready.slot==slot && mob->next_attack==next);++count;
    }
    check(count==39);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_attack_ai_prefix(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned country,user_country,life,immune,attackable,visible,status,expected_status,kind,battle;std::string trace;
        row>>country>>user_country>>life>>immune>>attackable>>visible>>status>>trace>>expected_status>>kind>>battle;check(bool(row));
        for(unsigned i=0;i<4;++i)record[0x78+i]=static_cast<std::byte>((country>>(8*i))&255);
        actor.appearance().identity.country=static_cast<std::uint8_t>(user_country);actor.appearance().attackable=attackable;actor.appearance().visible=visible;actor.attack_state().mode=static_cast<int>(life);actor.status().no_mob_targeting=immune;
        mob->status=static_cast<int>(status);mob->target_kind=7;mob->target_id=42;mob->in_battle=false;mob->vision_fixed_user=0;
        AttackAiContinuation continuation;continuation.subject=mob;continuation.target=&actor;
        vertical_mobs_attack_ai_prefix(*mob,123,actor,frame.full,continuation);
        check(continuation.calls==(trace=="C"?1U:0U) && mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind) && mob->in_battle==bool(battle) && mob->target_id==42);++count;
    }
    check(count==7);std::copy(saved.begin(),saved.end(),record.begin());actor.appearance().identity.country=2;actor.appearance().attackable=true;actor.appearance().visible=true;actor.attack_state().mode=0;actor.status().no_mob_targeting=false;root.mob_pool().release(*mob);
}
void check_attack_oracle(const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);int status,stack,sleep,stun,found,life,mutate,expected_status,kind;std::uint32_t now,deadline;std::string expected;
        row>>status>>stack>>now>>deadline>>sleep>>stun>>found>>life>>mutate>>expected>>expected_status>>kind;check(bool(row));
        MobRuntime mob;mob.status=status;mob.target_kind=7;mob.target_id=42;mob.next_attack=deadline;mob.sleep=sleep;mob.stun=stun;if(stack)mob.state_stack=std::vector<std::int32_t>{status};
        AttackOracle services;services.found=found;services.target.life_status=life;services.mutate=mutate;
        AttackDependencies dependencies(services);VerticalMobsNativeAttack native(dependencies,services);native.update_attack(mob,now);
        check((services.trace.empty()?"-":services.trace)==expected && mob.status==expected_status && mob.target_kind==kind && mob.target_id==42);++count;
    }
    check(count==12);
}
struct FrameAi final:VerticalMobsAiOwners {
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override {throw std::logic_error("unexpected configured FSM");}
    MobAiConfiguredServices& configured_services(MobRuntime&) override {throw std::logic_error("unexpected configured services");}
    MobAiStateServices& stack_services(MobRuntime&) override {throw std::logic_error("unexpected stack services");}
    void release_failed_stack(MobRuntime&) override {throw std::logic_error("unexpected stack release");}
    void return_home(MobRuntime&,std::uint32_t) override {throw std::logic_error("unexpected return home");}
};
struct ReturnAi final:MobAiConfiguredServices {
    std::int32_t travel_condition(MobRuntime&,MobAiConfiguredFsm&,MobAiConfiguredCondition&) override {throw std::logic_error("unexpected AI condition");}
};
struct ReturnReset final:MobAppliedSkillsRuntimeResetServices {
    std::string& events;
    explicit ReturnReset(std::string& value):events(value) {}
    void send_hp(MobRuntime&,const std::array<std::uint8_t,10>&) override {throw std::logic_error("unexpected zone HP send");}
    void send_speed(MobRuntime&) override {events+='A';}
};
struct NativeState final:VerticalMobsAiStateOwners {
    explicit NativeState(FrameStack& stack):stack_(stack) {}
    FrameStack& stack_;MobRuntime* subject=nullptr;MobAiConfiguredFsm fsm{1,1};ReturnAi configured;std::string events;unsigned lookups=0;
    MobAiConfiguredFsm& configured_fsm(MobRuntime& mob) override {check(&mob==subject);++lookups;return fsm;}
    MobAiConfiguredServices& configured_services(MobRuntime& mob) override {check(&mob==subject);++lookups;return configured;}
    MobAiStateServices& stack_services(MobRuntime& mob) override {check(&mob==subject);return stack_;}
    void release_failed_stack(MobRuntime&) override {throw std::logic_error("unexpected stack release");}
};
struct NativeReset final:MobAppliedSkillsRuntimeResetServices {
    NativeState& state;explicit NativeReset(NativeState& value):state(value) {}
    void send_hp(MobRuntime& mob,const std::array<std::uint8_t,10>& packet) override {check(&mob==state.subject && packet[0]==5 && packet[1]==3 && mob.hp==mob.definition->hp);state.events+='H';}
    void send_speed(MobRuntime& mob) override {check(&mob==state.subject);state.events+='S';}
};
struct NativeDependencies final:VerticalMobsReturnDependencies {
    NativeState& state;NativeReset reset;MobAiVariableCatalog catalog;unsigned lookups=0;
    explicit NativeDependencies(NativeState& value):state(value),reset(value) {}
    const MobAiVariableCatalog& variables(MobRuntime& mob) override {check(&mob==state.subject);++lookups;return catalog;}
    MobAppliedSkillsRuntimeResetServices& reset_services(MobRuntime& mob) override {check(&mob==state.subject);++lookups;return reset;}
};
struct ChaseResetFind final:MobTargetingFindServices {
    bool found=false;unsigned visits=0,sets=0;
    void visit_cell_users(std::int32_t,std::int32_t,const std::function<bool(const MobTargetingFindUser&)>& visitor) override {
        ++visits;if(found)visitor({1,0,0,0,true,true,10,10});
    }
    void set_target_with_help(std::uintptr_t token) override {check(token==1);++sets;}
};
struct KilledUserObserver final:VerticalMobsUserDeath {
    MobRuntime& mob;Actor& actor;unsigned calls=0;int status=0,kind=0;std::uint32_t target=0;
    KilledUserObserver(MobRuntime& m,Actor& a):mob(m),actor(a) {}
    void death(Actor& subject,std::uint32_t id) override {
        check(&subject==&actor && id==mob.id && mob.vision_fixed_user==0);
        ++calls;status=mob.status;kind=mob.target_kind;target=mob.target_id;
    }
};
void check_chase_target_reset(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);unsigned enters=0;
    ai.fsm.states[1].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob);++enters;};
    MobTargetingFindSource source{};source.zone_user_count=1;source.cells_x=source.cells_z=1;source.x=source.z=10;source.radius=20;
    ChaseResetFind find;VerticalMobsChaseTargetReset reset(source,find,ai,dependencies);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned status,pais,found,expected_status,kind,id,fixed;std::string trace;
        row>>status>>pais>>found>>trace>>expected_status>>kind>>id>>fixed;check(bool(row));
        mob->status=static_cast<int>(status);mob->use_pais=pais;mob->target_kind=7;mob->target_id=42;mob->vision_fixed_user=99;
        find.found=found;find.visits=find.sets=0;enters=0;reset.reset_target(*mob);
        check(mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind) && mob->target_id==id && mob->vision_fixed_user==fixed);
        check(find.visits==(trace.find('T')!=std::string::npos?1U:0U) && find.sets==(trace.find('T')!=std::string::npos && found?1U:0U) && enters==(trace.find('F')!=std::string::npos?1U:0U));++count;
    }
    check(count==5);mob->vision_fixed_user=0;root.mob_pool().release(*mob);
}
void check_chase_leash(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);ai.fsm.states[1].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob && subject.status==100 && subject.target_kind==0 && subject.target_id==0);ai.events+='F';};
    std::string line;unsigned count=0;while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t mx,tx,hx,fixed,pais,status,result,expected_status,kind,id,found;std::string trace;
        row>>mx>>tx>>hx>>fixed>>pais>>status>>trace>>result>>expected_status>>kind>>id>>found;check(bool(row));
        mob->position={std::bit_cast<float>(mx),0,0};mob->respawn_position={std::bit_cast<float>(hx),0,0};mob->vision_fixed_user=fixed;mob->use_pais=pais;mob->status=static_cast<int>(status);mob->target_kind=7;mob->target_id=42;mob->path.reset();mob->path.set_path({1,1});ai.events.clear();
        const auto outcome=vertical_mobs_chase_leash(*mob,{std::bit_cast<float>(tx),0,0},ai,dependencies);
        check(outcome==(trace=="M"?VerticalMobChaseLeash::needs_movement:VerticalMobChaseLeash::completed) && result==0 && ai.events==(trace=="FP"?"F":"") && mob->status==static_cast<int>(expected_status) && mob->target_kind==static_cast<int>(kind) && mob->target_id==id && mob->path.found()==bool(found) && mob->path.current().has_value()==bool(found));++count;
    }
    check(count==9);mob->path.reset();root.mob_pool().release(*mob);
}
void check_near_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,deadline,result,expected_status;float x;unsigned trail,pais,status;std::string expected;
        row>>now>>deadline>>x>>trail>>pais>>status>>result>>expected_status>>expected;check(bool(row));
        mob->path.reset();mob->trail.clear();mob->damage->reset();mob->position={x,0,0};mob->respawn_position={};mob->next_chase=deadline;mob->use_pais=pais;mob->status=static_cast<std::int32_t>(status);mob->hp=0;
        mob->path.set_path({1,1});if(trail)mob->trail.add(frame.maps->all()[0].definition,{1,1});mob->damage->add(1,7,10,3,0);
        MobAiConfiguredFsm fsm{5,7};events.clear();fsm.states[7].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob && subject.status==100 && subject.hp==0);events+='F';};
        const auto outcome=vertical_mobs_return_near(*mob,now,fsm,variables,ai,reset);
        check(mob->status==static_cast<std::int32_t>(expected_status));
        if(result) {check(outcome==VerticalMobReturn::completed && expected==(pais?"FAPTD":"APTD"));check(events==(pais?"FA":"A"));check(!mob->path.current() && !mob->path.found() && mob->trail.nodes().empty() && mob->damage->records().empty() && mob->hp==mob->definition->hp);}
        else {check(outcome==(expected=="X"?VerticalMobReturn::needs_path:VerticalMobReturn::waiting));check(events.empty() && mob->path.current() && mob->damage->records().size()==1 && mob->hp==0);}
        ++cases;
    }
    check(cases==7);root.mob_pool().release(*mob);
}
struct ReturnMove final:ZoneMobServices {
    struct Broadcast final:MobBroadcast {void send_cell(CellCoordinates) override {}};
    std::span<std::byte> record;std::uint32_t after=0;unsigned moves=0;MobRuntime* mutate=nullptr;
    explicit ReturnMove(std::span<std::byte> value):record(value) {}
    std::uint32_t tick_ms() override {return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet&) override {}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet& packet,std::optional<std::uint8_t>) override {
        check(packet.size()>=7 && packet[0]==3 && packet[1]==6 && packet[6]==1);++moves;
        for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((after>>(8*i))&255);
        if(mutate)mutate->position={33,22,33};
        return std::make_unique<Broadcast>();
    }
    void lua_init(MobRuntime&) override {throw std::logic_error("unexpected Lua");}
};
void check_chase_intermediate(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ReturnMove ports(record);ports.after=500;
    auto map=frame.maps->all()[0].definition;std::fill(map.tiles.begin(),map.tiles.end(),0);
    for(unsigned z=0;z<81;++z)if(z!=14) {const auto tile=z*81U+15U;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
    ZoneMobs mobs(map.geometry(),ports);mob->position={10,3,10};check(mobs.enter(*mob));std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned step,accepted,x,y,z,deadline,cx,cz,trails,moves;
        row>>step>>accepted>>x>>y>>z>>deadline>>cx>>cz>>trails>>moves;check(bool(row));if(!accepted)continue;
        mob->position={10,3,10};mob->move_speed=2;mob->path.reset();mob->trail.clear();ports.moves=0;record[0x68]=static_cast<std::byte>(step);
        check(mob->path.find(map,{10,10},{20,20}));const auto current=mob->path.current();check(current && !mob->path.last());
        ChasePathWorld remainder;remainder.subject=mob;remainder.target=&actor;VerticalMobsChaseIntermediate intermediate(mobs,map,frame.full,remainder);
        check(!intermediate.waypoint(*mob,50,actor,0,*current));const auto after=mob->path.current();
        check(std::bit_cast<unsigned>(mob->position.x)==x && std::bit_cast<unsigned>(mob->position.y)==y && std::bit_cast<unsigned>(mob->position.z)==z && mob->next_chase==deadline && after && static_cast<unsigned>(after->x)==cx && static_cast<unsigned>(after->z)==cz && mob->trail.nodes().size()==trails && ports.moves==moves);++count;
    }
    check(count==3);mob->path.reset();mob->trail.clear();check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
struct DamageTransportScene final:ActorScene {
    std::optional<bool> party;std::vector<std::string> events;
    void capture(char route,const eternity::net::Packet& packet) noexcept {static constexpr char digits[]="0123456789abcdef";std::string text(1,route);for(auto b:packet){text+=digits[b>>4];text+=digits[b&15];}events.push_back(std::move(text));}
    void send_self(const eternity::net::Packet& packet) noexcept override {capture('S',packet);}
    void send_nearby(const eternity::net::Packet&) override {throw std::logic_error("unexpected view");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::logic_error("unexpected cell");}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const eternity::net::Packet& packet) noexcept override {capture('P',packet);}
    void reset_damage() override {throw std::logic_error("unexpected reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 77;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("unexpected target");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::logic_error("unexpected zone");}
    UserShapeSocial user_shape_social() override {throw std::logic_error("unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("unexpected periodic damage");}
};
struct DamageLedgerObserver final:VerticalMobsUserDamageLedger {
    Actor* expected=nullptr;unsigned calls=0;
    void add(Actor& actor,std::int32_t kind,std::uint32_t source,std::int32_t amount) override {check(&actor==expected && kind==2 && source==77 && amount==5);++calls;}
};
void check_damage_scene(MobPrimaryZones& root,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->id=77;SkillDefinitions skills;
    DamageTransportScene scene;Actor actor(42,EffectStatus{},PlayerResources{1,0,0},{},scene,equipment);DamageLedgerObserver ledger;ledger.expected=&actor;
    VerticalMobsUserDamageSceneWorld world(actor,scene,ledger);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);int party;unsigned hp,protection,killed,fh,fm,fs;std::string expected;row>>party>>hp>>protection>>expected>>killed>>fh>>fm>>fs;check(bool(row));
        scene.party=party<0?std::optional<bool>{}:std::optional<bool>{party==1};scene.events.clear();ledger.calls=0;
        actor.replace_resources_raw({hp,0x12345,0x23456});actor.status().skills.equipment.damage_reduction=protection;std::array<std::int32_t,3> damage{5,1,2};
        const auto dead=vertical_mobs_user_damage(*mob,actor,damage,skills,world);std::string events;
        for(const auto& event:scene.events){if(!events.empty())events+=',';events+=event;}
        check((events.empty()?"-":events)==expected && dead==bool(killed) && ledger.calls==1 && actor.resources().hp==fh && actor.resources().mp==fm && actor.resources().sp==fs);++count;
    }
    Actor wrong(43,EffectStatus{},PlayerResources{1,0,0},{},scene,equipment);bool rejected=false;try{world.send_energy(wrong);}catch(const std::invalid_argument&){rejected=true;}check(rejected && count==12);root.mob_pool().release(*mob);
}
struct UserDamageObserver final:VerticalMobsUserDamageWorld {
    Actor* actor=nullptr;std::string trace;std::uint16_t reduced=0;std::int32_t original=0;
    void record(Actor& value,std::int32_t kind,std::uint32_t source,std::int32_t amount) override {check(&value==actor && kind==2 && source==77 && amount==original);trace+='L';}
    void send_reduction(Actor& value,std::uint16_t amount) override {check(&value==actor);trace+='R';reduced=amount;}
    void send_energy(Actor& value) override {check(&value==actor);trace+='E';}
};
void check_user_damage(MobPrimaryZones& root,ActorScene& scene,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->id=77;SkillDefinitions skills;
    Actor actor(42,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);UserDamageObserver world;world.actor=&actor;
    std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t hp,mp,sp,mode,absorb,protection,dh,ds,dm,reduced,killed,fh,fm,fs,fdh,fds,fdm;std::string trace;
        row>>hp>>mp>>sp>>mode>>absorb>>protection>>dh>>ds>>dm>>trace>>reduced>>killed>>fh>>fm>>fs>>fdh>>fds>>fdm;check(bool(row));
        actor.replace_resources_raw({hp,mp,sp});actor.attack_state().mode=mode;actor.status().giveup_sp_mp=absorb;actor.status().skills.equipment.damage_reduction=protection;
        std::array<std::int32_t,3> damage{std::bit_cast<std::int32_t>(dh),std::bit_cast<std::int32_t>(ds),std::bit_cast<std::int32_t>(dm)};
        world.trace.clear();world.reduced=0;world.original=damage[0];const auto result=vertical_mobs_user_damage(*mob,actor,damage,skills,world);
        std::string expected;for(char c:trace)if(c=='L' || c=='R' || c=='E')expected+=c;
        check(world.trace==expected && world.reduced==reduced && result==bool(killed));
        check(actor.resources().hp==fh && actor.resources().mp==fm && actor.resources().sp==fs && std::bit_cast<std::uint32_t>(damage[0])==fdh && std::bit_cast<std::uint32_t>(damage[1])==fds && std::bit_cast<std::uint32_t>(damage[2])==fdm);++count;
    }
    check(count==2560);root.mob_pool().release(*mob);
}
struct DefenseOptionsFixture final:VerticalMobsDefenseOptions {
    std::uint8_t flag=0;std::array<std::array<std::byte,0x47>,20> jewels{};
    std::array<SkillDefinition,20> effects{};std::vector<UserSkillDefinition> skills;std::vector<unsigned> calls;
    DefenseOptionsFixture(){skills.reserve(20);for(unsigned i=0;i<20;++i){effects[i].id=i;skills.push_back({effects[i],"skill"});jewels[i][0x24]=std::byte{1};jewels[i][0x25]=std::byte{1};jewels[i][0x46]=static_cast<std::byte>(i);}}
    std::uint8_t& active(Actor&) override{return flag;}
    std::span<const std::byte> jewel(std::uint8_t id) override {check(id<jewels.size());return jewels[id];}
    const UserSkillDefinition* mob_skill(std::uint8_t id) override {check(id<skills.size());return &skills[id];}
    void activate(Actor&,MobRuntime&,const UserSkillDefinition& skill) override {check(flag==1);calls.push_back(skill.effect.id);}
};
struct LiveUserDeathObserver final:VerticalMobsUserDeath {
    unsigned calls=0;void death(Actor& actor,std::uint32_t id) override {check(id==77 && actor.resources().hp==0);++calls;actor.attack_state().mode=1;}
};
void check_live_hit_composition(MobPrimaryZones& root,Frame& frame,ActorScene& scene,const ActorEquipmentRules& equipment) {
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->id=77;mob->definition=frame.runtime.find(1);mob->lua_ai=0;mob->use_pais=false;mob->position={10,0,10};
    const auto geometry=frame.maps->all()[0].definition.geometry();ZoneMobs mobs(geometry,frame.backend);ZoneUsers users(geometry,0);check(mobs.enter(*mob));
    auto actor=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{100,0,0},std::vector<PersistedEffect>{},scene,equipment);actor->status().skills.benefits.no_durability_loss=1;
    GroundItemPool item_pool;ItemDefinition definition{};definition.id=1;auto* shield=item_pool.allocate();check(shield);shield->definition=&definition;shield->item.gems[0]=1;check(actor->claim_inventory_item(0,6,*shield,item_pool));
    check(users.attach_at_position(*actor,{10,0,10}));MobPriorityArea area;area.actors={actor};BoundStandOwner stand;stand.expected=actor.get();BoundMobDamageOwner mob_damage;
    FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);MobTargetingActorWorld target_world(mobs,users,frame.random,stack);MobTargetingZone targeting(mobs,frame.full,target_world);
    SkillDefinitions skills;UserDamageObserver damage_world;damage_world.actor=actor.get();damage_world.original=25;LiveUserDeathObserver death;DefenseOptionsFixture options;
    VerticalMobsLiveHitTailWorld live(mobs,users,area,stand,mob_damage,skills,damage_world,frame.full,targeting,ai,dependencies,death,options);
    MobDeathExperience rules;HitTailFixture mob_death;VerticalMobsHitTail tail(live,rules,mob_death);VerticalMobsHitDelivery delivery(tail);
    live.status_stand(*actor);check(stand.calls==1);mob->status=1;mob->target_kind=1;mob->target_id=42;mob->vision_fixed_user=0;
    std::array<std::int32_t,3> damage{25,0,0};delivery.finish_hit(*mob,10,*actor,false,false,damage);
    check(actor->resources().hp==75 && damage_world.trace=="LE" && death.calls==0 && options.calls==std::vector<unsigned>{1} && area.immediate_ids==std::vector<std::uint32_t>{42});
    actor->replace_resources_raw({20,0,0});damage_world.trace.clear();options.calls.clear();damage={25,0,0};mob->status=1;mob->target_kind=1;mob->vision_fixed_user=99;
    delivery.finish_hit(*mob,11,*actor,false,false,damage);
    check(actor->resources().hp==0 && actor->attack_state().mode==1 && damage_world.trace=="LE" && death.calls==1 && mob->status==0 && mob->target_kind==0 && mob->vision_fixed_user==0);
    check(options.calls.empty() && area.packets.size()==2 && area.immediate_ids==std::vector<std::uint32_t>({42,42}));
    check(users.detach(actor->id()) && actor->release_inventory_item(0,6) && mobs.leave(*mob));root.mob_pool().release(*mob);
}
void check_defense_options(MobPrimaryZones& root,ActorScene& scene,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned guard,status,mode,slots,final_guard;std::string expected;row>>guard>>status>>mode>>slots>>expected>>final_guard;check(bool(row));
        GroundItemPool pool;ItemDefinition definition{};definition.id=1;Actor actor(42,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);
        if(slots)for(unsigned slot=0;slot<7;++slot){auto* item=pool.allocate();check(item);item->definition=&definition;item->item.bag=0;item->item.slot=slot;item->item.gems[0]=slot+1;item->item.gems[1]=slot+8;check(actor.claim_inventory_item(0,slot,*item,pool));}
        DefenseOptionsFixture world;world.flag=guard;mob->status=status;actor.attack_state().mode=mode;
        vertical_mobs_defense_options(*mob,actor,world);
        std::ostringstream result;for(unsigned id:world.calls){if(result.tellp()>0)result<<',';result<<id;}
        check((world.calls.empty()?std::string("-"):result.str())==expected && world.flag==final_guard);
        if(slots)for(unsigned slot=0;slot<7;++slot)check(actor.release_inventory_item(0,slot));++count;
    }
    check(count==5);root.mob_pool().release(*mob);
}
namespace eternity::game {
struct DefenseSelectProbe {unsigned calls=0;};
struct DefenseHitProbe {unsigned calls=0;};
void vertical_combat_skill_select(Actor&,MobRuntime&,const UserSkillDefinition&,DefenseSelectProbe& probe){++probe.calls;}
void vertical_combat_skill_hit_prefix(Actor&,MobRuntime&,const UserSkillDefinition&,bool first,bool second,DefenseHitProbe& probe){check(first && second);++probe.calls;}
struct DefenseOptionOwnerProbe {
    std::uint8_t flag=0;std::array<std::byte,0x47> record{};SkillDefinition effect{};UserSkillDefinition skill{effect,"option"};
    DefenseSelectProbe selected;DefenseHitProbe hit;
    std::uint8_t& active(Actor&){return flag;}
    std::span<const std::byte> jewel(std::uint8_t){return record;}
    const UserSkillDefinition* mob_skill(std::uint8_t){return &skill;}
    DefenseSelectProbe& select_world(){return selected;}
    DefenseHitProbe& hit_world(){return hit;}
};
}
void check_bound_defense_options(MobPrimaryZones& root,Actor& actor){
    auto* mob=root.mob_pool().allocate();check(mob);DefenseOptionOwnerProbe owner;VerticalMobsBoundDefenseOptions bound(owner);
    check(&bound.active(actor)==&owner.flag && bound.jewel(1).data()==owner.record.data() && bound.mob_skill(1)==&owner.skill);
    owner.effect.effect=1;bound.activate(actor,*mob,owner.skill);owner.effect.effect=3;bound.activate(actor,*mob,owner.skill);
    owner.effect.effect=2;bound.activate(actor,*mob,owner.skill);check(owner.selected.calls==1 && owner.hit.calls==1);root.mob_pool().release(*mob);
}
struct DefenseNameObserver final:VerticalMobsDefenseOptions {
    std::uint8_t flag=0;std::array<std::byte,0x47> record{};SkillDefinition effect{};UserSkillDefinition skill{effect,"option"};
    std::string seen_user,seen_skill;unsigned calls=0;
    DefenseNameObserver(){record[0x24]=record[0x25]=record[0x46]=std::byte{1};}
    std::uint8_t& active(Actor&) override{return flag;}
    std::span<const std::byte> jewel(std::uint8_t id) override{check(id==1);return record;}
    const UserSkillDefinition* mob_skill(std::uint8_t id) override{check(id==1);return &skill;}
    void activate(Actor& actor,MobRuntime& mob,const UserSkillDefinition&) override {
        ++calls;check(flag==1);seen_user=reinterpret_cast<const char*>(mob.previous_attack_user.data());seen_skill=reinterpret_cast<const char*>(mob.previous_attack_skill.data());
        // Nested option entry observes the same real guard and must not dispatch.
        vertical_mobs_defense_options(mob,actor,*this);check(calls==1);
        const std::string user="changed-user",name="changed-skill";
        std::copy_n(user.c_str(),user.size()+1,mob.previous_attack_user.begin());std::copy_n(name.c_str(),name.size()+1,mob.previous_attack_skill.begin());
    }
};
void check_defense_names(MobPrimaryZones& root,ActorScene& scene,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);GroundItemPool pool;ItemDefinition definition{};definition.id=1;
    Actor actor(42,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);auto* item=pool.allocate();check(item);item->definition=&definition;item->item.gems[0]=1;check(actor.claim_inventory_item(0,6,*item,pool));
    std::string line;unsigned count=0;MobRespawn respawn{};respawn.both=true;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned lua,both,seen_guard,final_guard;std::string seen_user,seen_skill,final_user,final_skill;
        row>>lua>>both>>seen_user>>seen_skill>>seen_guard>>final_user>>final_skill>>final_guard;check(bool(row));
        mob->status=1;mob->lua_ai=lua;mob->respawn=both?&respawn:nullptr;
        mob->previous_attack_user.fill(0);mob->previous_attack_skill.fill(0);
        std::copy_n("prioruser",10,mob->previous_attack_user.begin());std::copy_n("priorskill",11,mob->previous_attack_skill.begin());
        DefenseNameObserver world;vertical_mobs_defense_options(*mob,actor,world);
        check(world.calls==1 && seen_guard==1 && world.seen_user==seen_user && world.seen_skill==seen_skill && world.flag==final_guard);
        check(reinterpret_cast<const char*>(mob->previous_attack_user.data())==final_user && reinterpret_cast<const char*>(mob->previous_attack_skill.data())==final_skill);++count;
    }
    check(count==3);mob->lua_ai=0;mob->respawn=nullptr;check(actor.release_inventory_item(0,6));root.mob_pool().release(*mob);
}
void check_user_killed(MobPrimaryZones& root,Frame& frame,Actor& actor,ActorScene& scene,const ActorEquipmentRules& equipment,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->has_applied_skills());
    mob->definition=frame.runtime.find(1);check(mob->definition);mob->id=77;
    auto record=frame.full.mutable_mob_record(1);ReturnMove ports(record);auto map=frame.maps->all()[0].definition;
    ZoneMobs mobs(map.geometry(),ports);ZoneUsers users(map.geometry(),0);mob->position={10,0,10};check(mobs.enter(*mob));
    FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);unsigned enters=0;
    ai.fsm.states[1].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob);++enters;};
    MobTargetingActorWorld target_world(mobs,users,frame.random,stack);MobTargetingZone targeting(mobs,frame.full,target_world);
    auto candidate=std::make_shared<Actor>(501,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);
    auto raw=frame.full.mutable_mob_record(1);const auto saved_radius=raw[0x69];raw[0x69]=std::byte{20};
    KilledUserObserver death(*mob,actor);std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned status,pais,expected_status,kind,id;std::string trace;
        row>>status>>pais>>trace>>expected_status>>kind>>id;check(bool(row));
        mob->status=static_cast<int>(status);mob->use_pais=pais;mob->target_kind=7;mob->target_id=42;mob->vision_fixed_user=99;
        death.calls=0;enters=0;if(status==100)check(users.attach_at_position(*candidate,{15,0,10}));
        vertical_mobs_user_killed(*mob,actor,mobs,users,frame.full,targeting,ai,dependencies,death);
        check(death.calls==1 && death.status==static_cast<int>(expected_status) && death.kind==static_cast<int>(kind) && death.target==id);
        check(enters==(trace.find('R')!=std::string::npos?1U:0U));if(status==100)check(users.detach(candidate->id()));++count;
    }
    raw[0x69]=saved_radius;check(count==5);check(mobs.leave(*mob));root.mob_pool().release(*mob);
}
void check_chase_final(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ReturnMove ports(record);ports.after=500;
    auto map=frame.maps->all()[0].definition;ZoneMobs mobs(map.geometry(),ports);mob->position={10,3,10};check(mobs.enter(*mob));std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned step,range,size,accepted,x,y,z,deadline,found,cx,cz,trails,moves;
        row>>step>>range>>size>>accepted>>x>>y>>z>>deadline>>found>>cx>>cz>>trails>>moves;check(bool(row));if(!accepted)continue;
        mob->position={10,3,10};mob->move_speed=2;mob->path.reset();mob->path.set_path({20,10});mob->trail.clear();ports.moves=0;
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);
        VerticalMobsChaseFinal final(mobs,map,frame.full);check(!final.waypoint(*mob,50,actor,static_cast<int>(range),{20,10}));const auto after=mob->path.current();
        check(std::bit_cast<unsigned>(mob->position.x)==x && std::bit_cast<unsigned>(mob->position.y)==y && std::bit_cast<unsigned>(mob->position.z)==z && mob->next_chase==deadline && mob->path.found()==bool(found) && (after?static_cast<unsigned>(after->x):UINT32_MAX)==cx && (after?static_cast<unsigned>(after->z):UINT32_MAX)==cz && mob->trail.nodes().size()==trails && ports.moves==moves);++count;
    }
    check(count==6);mob->path.reset();mob->trail.clear();check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
void check_native_chase(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ReturnMove ports(record);ports.after=500;
    auto map=frame.maps->all()[0].definition;std::fill(map.tiles.begin(),map.tiles.end(),0);ZoneMobs mobs(map.geometry(),ports);ZoneUsers users(map.geometry(),0);
    mob->position={10,3,10};check(mobs.enter(*mob));check(users.attach_at_position(actor,{20,0,10}));FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);
    MobTargetingFindSource source{};ChaseResetFind find;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned step,range,size,accepted,x,y,z,deadline,found,cx,cz,trails,moves;
        row>>step>>range>>size>>accepted>>x>>y>>z>>deadline>>found>>cx>>cz>>trails>>moves;check(bool(row));if(!accepted)continue;
        mob->position={10,3,10};mob->move_speed=2;mob->next_chase=0;mob->stop=mob->sleep=mob->stun=0;mob->vision_fixed_user=0;mob->path.reset();mob->trail.clear();ports.moves=0;
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);
        check(!vertical_mobs_native_chase(*mob,50,actor,static_cast<int>(range),mobs,users,map,frame.full,source,find,ai,dependencies));const auto after=mob->path.current();
        check(std::bit_cast<unsigned>(mob->position.x)==x && std::bit_cast<unsigned>(mob->position.y)==y && std::bit_cast<unsigned>(mob->position.z)==z && mob->next_chase==deadline && mob->path.found()==bool(found) && (after?static_cast<unsigned>(after->x):UINT32_MAX)==cx && (after?static_cast<unsigned>(after->z):UINT32_MAX)==cz && mob->trail.nodes().size()==trails && ports.moves==moves);++count;
    }
    check(count==6);mob->position={10,3,10};mob->next_chase=0;mob->path.reset();mob->path.set_path({10,10});mob->trail.clear();ports.moves=0;
    record[0x68]=std::byte{2};record[0x3f]=std::byte{0};
    check(!vertical_mobs_native_chase(*mob,50,actor,0,mobs,users,map,frame.full,source,find,ai,dependencies));
    check(mob->next_chase==50 && !mob->path.found() && mob->trail.nodes().size()==1 && ports.moves==0 && mob->position.x==10);
    mob->next_chase=0;mob->path.reset();mob->path.set_path({-1,10});mob->trail.clear();ports.moves=0;record[0x68]=std::byte{20};
    for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((500U>>(i*8))&255);
    check(!vertical_mobs_native_chase(*mob,50,actor,0,mobs,users,map,frame.full,source,find,ai,dependencies));
    check(mob->next_chase==315 && !mob->path.found() && mob->trail.nodes().size()==1 && ports.moves==0 && mob->position.x==10);
    mob->path.reset();mob->trail.clear();mob->next_chase=0;mob->position={10,3,10};mob->status=1;mob->target_kind=7;mob->target_id=42;mob->use_pais=false;
    record[0x68]=std::byte{2};record[0x3f]=std::byte{0};check(users.move_at_position(actor.id(),{20,0,20}));
    for(unsigned z=0;z<81;++z){const auto tile=z*81U+15U;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
    check(!vertical_mobs_native_chase(*mob,50,actor,0,mobs,users,map,frame.full,source,find,ai,dependencies));
    check(mob->status==0 && mob->target_kind==0 && mob->target_id==0 && !mob->path.found());
    mob->path.reset();mob->trail.clear();check(users.detach(actor.id()));check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
struct NativeAttackAggro final:MinHpWorld {
    MobRuntime* subject=nullptr;unsigned calls=0;
    void retarget(MobRuntime& mob,std::uint32_t) override {check(&mob==subject);++calls;}
};
void check_native_chase_attack(MobPrimaryZones& root,Frame& frame,const std::shared_ptr<Actor>& actor_owner) {
    Actor& actor=*actor_owner;
    auto* mob=root.mob_pool().allocate();check(mob && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ReturnMove ports(record);ports.after=500;
    auto map=frame.maps->all()[0].definition;std::fill(map.tiles.begin(),map.tiles.end(),0);ZoneMobs mobs(map.geometry(),ports);ZoneUsers users(map.geometry(),0);
    mob->position={10,0,10};check(mobs.enter(*mob));check(users.attach_at_position(actor,{20,0,10}));FrameStack stack;NativeState ai(stack);ai.subject=mob;NativeDependencies dependencies(ai);
    MobTargetingActorWorld target_world(mobs,users,frame.random,stack);MobTargetingZone targeting(mobs,frame.full,target_world);
    StationaryWorld attacks;attacks.subject=mob;attacks.target=&actor;attacks.record=record;attacks.now=50;
    VerticalMobsAttackGuard guard(attacks);VerticalMobsNativeChaseAttackWorld native(mobs,users,map,frame.full,targeting,ai,dependencies,guard);VerticalMobsAttackAiChase chase(frame.full,native);
    record[0x26]=std::byte{1};record[0x68]=std::byte{2};record[0x7c]=std::byte{0};record[0x3f]=std::byte{0};
    const unsigned at=0x80+20;record[at]=record[at+1]=std::byte{0};record[at+8]=std::byte{5};
    for(unsigned i=0;i<4;++i)record[at+4+i]=static_cast<std::byte>((2000U>>(i*8))&255);
    mob->status=1;mob->target_kind=1;mob->target_id=actor.id();mob->next_chase=0;mob->move_speed=2;mob->attack_speed=3;mob->stop=mob->sleep=mob->stun=0;mob->vision_fixed_user=0;mob->path.reset();mob->trail.clear();
    chase.other_attack(*mob,50,actor,1);check(attacks.calls==0 && mob->position.x==12.0F && mob->next_chase==550 && mob->path.found() && ports.moves==1);
    check(users.move_at_position(actor.id(),{13,0,10}));attacks.now=550;chase.other_attack(*mob,550,actor,1);
    check(attacks.calls==1 && attacks.during==1 && mob->attack_times[1]==2550 && mob->current_attack==0 && !mob->path.found());
    mob->attack_disabled[0]=1;mob->attack_times[1]=0;chase.other_attack(*mob,550,actor,1);
    check(attacks.calls==1 && mob->attack_times[1]==2550 && mob->current_attack==0);mob->attack_disabled[0]=0;
    for(unsigned z=0;z<81;++z){const auto tile=z*81U+15U;map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
    check(users.move_at_position(actor.id(),{20,0,20}));mob->position={10,0,10};mob->next_chase=0;mob->status=1;mob->target_kind=1;mob->target_id=actor.id();mob->use_pais=false;mob->path.reset();attacks.now=50;
    chase.other_attack(*mob,50,actor,1);check(attacks.calls==1 && mob->status==0 && mob->target_kind==0 && mob->target_id==0 && !mob->path.found());
    std::fill(map.tiles.begin(),map.tiles.end(),0);check(users.move_at_position(actor.id(),{20,0,10}));
    const auto country=actor.appearance().identity.country;const auto attackable=actor.appearance().attackable,visible=actor.appearance().visible;
    actor.appearance().identity.country=1;actor.appearance().attackable=true;actor.appearance().visible=true;
    for(unsigned i=0;i<4;++i)record[0x78+i]=static_cast<std::byte>(i==0?2:0);
    record[0x82]=record[0x82+40]=std::byte{0};record[0x82+20]=std::byte{1};
    mob->position={10,0,10};mob->next_chase=0;mob->status=1;mob->target_kind=1;mob->target_id=actor.id();mob->path.reset();mob->trail.clear();mob->attack_times={0,0,0};mob->in_battle=false;
    NativeAttackAggro combat;combat.subject=mob;combat.actors.push_back(actor_owner);attacks.calls=0;attacks.now=50;
    vertical_mobs_native_attack_ai(*mob,50,actor,mobs,users,map,frame.full,targeting,combat,ai,dependencies,attacks);
    check(combat.calls==1 && attacks.calls==0 && mob->in_battle && mob->position.x==12.0F && mob->next_chase==550);
    check(users.move_at_position(actor.id(),{13,0,10}));attacks.now=550;
    vertical_mobs_native_attack_ai(*mob,550,actor,mobs,users,map,frame.full,targeting,combat,ai,dependencies,attacks);
    check(combat.calls==2 && attacks.calls==1 && attacks.during==1 && mob->attack_times[1]==2550);
    actor.appearance().identity.country=2;mob->status=1;mob->target_kind=1;mob->vision_fixed_user=0;
    vertical_mobs_native_attack_ai(*mob,600,actor,mobs,users,map,frame.full,targeting,combat,ai,dependencies,attacks);
    check(combat.calls==2 && attacks.calls==1 && mob->status==0 && mob->target_kind==0);
    actor.appearance().identity.country=1;mob->status=1;mob->target_kind=1;mob->target_id=actor.id();mob->next_attack=0;mob->next_chase=0;mob->attack_times={0,0,0};mob->path.reset();attacks.now=600;
    VerticalMobsNativeAiOwners native_ai(ai,dependencies,mobs,frame.full,map);
    vertical_mobs_native_actor_attack(*mob,600,mobs,users,map,frame.full,targeting,combat,native_ai,dependencies,attacks);
    check(combat.calls==3 && attacks.calls==2 && attacks.during==1 && mob->attack_times[1]==2600);
    mob->status=1;mob->target_kind=1;mob->target_id=987654;mob->vision_fixed_user=99;
    vertical_mobs_native_actor_attack(*mob,600,mobs,users,map,frame.full,targeting,combat,native_ai,dependencies,attacks);
    check(combat.calls==3 && attacks.calls==2 && mob->status==0 && mob->target_kind==0 && mob->target_id==987654 && mob->vision_fixed_user==0);
    actor.appearance().identity.country=country;actor.appearance().attackable=attackable;actor.appearance().visible=visible;
    mob->trail.clear();check(users.detach(actor.id()));check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
void check_chase_near_final(MobPrimaryZones& root,Frame& frame,Actor& actor,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());auto* mob=root.mob_pool().allocate();check(mob);mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());ReturnMove ports(record);ports.after=500;
    auto map=frame.maps->all()[0].definition;ZoneMobs mobs(map.geometry(),ports);mob->position={10,3,10};check(mobs.enter(*mob));std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);int cursor;unsigned size,range,step,accepted,x,y,z,deadline,found,after_x,trails,moves;
        row>>cursor>>size>>range>>step>>accepted>>x>>y>>z>>deadline>>found>>after_x>>trails>>moves;check(bool(row));
        mob->position={10,3,10};mob->move_speed=2;mob->path.reset();mob->path.set_path({static_cast<int>(cursor),10});mob->trail.clear();ports.moves=0;
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((500U>>(i*8))&255);
        VerticalMobsChaseFinal final(mobs,map,frame.full);check(!final.waypoint(*mob,50,actor,static_cast<int>(range),{cursor,10}));const auto after=mob->path.current();
        check(mob->next_chase==deadline && mob->path.found()==bool(found) && (after?static_cast<unsigned>(after->x):UINT32_MAX)==after_x && mob->trail.nodes().size()==trails && ports.moves==(accepted?moves:0));
        if(moves && accepted)check(std::bit_cast<unsigned>(mob->position.x)==x && std::bit_cast<unsigned>(mob->position.y)==y && std::bit_cast<unsigned>(mob->position.z)==z);
        else check(mob->position.x==10 && mob->position.y==3 && mob->position.z==10);
        ++count;
    }
    check(count==6);mob->path.reset();mob->trail.clear();check(mobs.leave(*mob));root.mob_pool().release(*mob);std::copy(saved.begin(),saved.end(),record.begin());
}
void check_direct_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    ReturnMove ports(record);const auto& map=frame.maps->all()[0].definition;ZoneMobs zone(map.geometry(),ports);
    MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,before,after,expected_x,expected_z,expected_next;float x,z,hx,hz;int speed;unsigned step,size,stop,line_calls,moves;
        row>>now>>x>>z>>hx>>hz>>speed>>step>>size>>stop>>before>>after>>line_calls>>moves>>expected_x>>expected_z>>expected_next;check(bool(row));
        mob->position={x,0,z};check(zone.enter(*mob));mob->respawn_position={hx,0,hz};mob->next_chase=0;mob->status=3;mob->use_pais=true;mob->stop=static_cast<int>(stop);mob->sleep=mob->stun=0;mob->move_speed=speed;mob->path.reset();mob->trail.clear();
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((before>>(8*i))&255);
        ports.after=after;ports.moves=0;events.clear();MobAiConfiguredFsm fsm{5,7};
        check(vertical_mobs_return_step(*mob,now,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(events.empty() && ports.moves==moves && line_calls==moves && std::bit_cast<std::uint32_t>(mob->position.x)==expected_x && std::bit_cast<std::uint32_t>(mob->position.z)==expected_z && mob->next_chase==expected_next);
        check(zone.leave(*mob));++cases;
    }
    check(cases==6);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_finish_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    ReturnMove ports(record);const auto& map=frame.maps->all()[0].definition;ZoneMobs zone(map.geometry(),ports);
    MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,before,after,expected_x,expected_y,expected_z,expected_next;float x,z,hx,hz;int speed;unsigned step,size,mutate,moves;
        row>>now>>x>>z>>hx>>hz>>speed>>step>>size>>before>>after>>mutate>>moves>>expected_x>>expected_y>>expected_z>>expected_next;check(bool(row));
        mob->position={x,0,z};check(zone.enter(*mob));mob->respawn_position={hx,0,hz};mob->next_chase=0;mob->status=3;mob->use_pais=true;mob->stop=mob->sleep=mob->stun=0;mob->move_speed=speed;mob->path.reset();mob->trail.clear();
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((before>>(8*i))&255);
        ports.after=after;ports.moves=0;ports.mutate=mutate?mob:nullptr;events.clear();MobAiConfiguredFsm fsm{5,7};
        check(vertical_mobs_return_step(*mob,now,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(events.empty() && ports.moves==moves && std::bit_cast<std::uint32_t>(mob->position.x)==expected_x && std::bit_cast<std::uint32_t>(mob->position.y)==expected_y && std::bit_cast<std::uint32_t>(mob->position.z)==expected_z && mob->next_chase==expected_next);
        check(zone.leave(*mob));++cases;
    }
    check(cases==4);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_path_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    ReturnMove ports(record);auto map=frame.maps->all()[0].definition;const auto blocked=static_cast<std::size_t>(20*map.size+25);map.tiles[blocked/8]|=static_cast<std::uint8_t>(1U<<(blocked%8));ZoneMobs zone(map.geometry(),ports);
    MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,before,after,expected_x,expected_y,expected_z,expected_next;float x,z,hx,hz;int px,pz,speed;unsigned step,size,stop,mutate,moves,found,cursor;
        row>>now>>x>>z>>hx>>hz>>px>>pz>>speed>>step>>size>>stop>>before>>after>>mutate>>moves>>expected_x>>expected_y>>expected_z>>expected_next>>found>>cursor;check(bool(row));
        mob->position={x,0,z};check(zone.enter(*mob));mob->respawn_position={hx,0,hz};mob->next_chase=0;mob->status=3;mob->use_pais=true;mob->stop=static_cast<int>(stop);mob->sleep=mob->stun=0;mob->move_speed=speed;mob->path.reset();mob->path.set_path({px,pz});mob->trail.clear();
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((before>>(8*i))&255);
        ports.after=after;ports.moves=0;ports.mutate=mutate?mob:nullptr;events.clear();MobAiConfiguredFsm fsm{5,7};
        check(vertical_mobs_return_step(*mob,now,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(events.empty() && ports.moves==moves && std::bit_cast<std::uint32_t>(mob->position.x)==expected_x && std::bit_cast<std::uint32_t>(mob->position.y)==expected_y && std::bit_cast<std::uint32_t>(mob->position.z)==expected_z && mob->next_chase==expected_next && mob->path.found()==bool(found) && mob->path.current().has_value()==bool(cursor));
        check(zone.leave(*mob));++cases;
    }
    check(cases==5);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_trail_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    ReturnMove ports(record);auto map=frame.maps->all()[0].definition;const auto blocked=static_cast<std::size_t>(20*map.size+25);map.tiles[blocked/8]|=static_cast<std::uint8_t>(1U<<(blocked%8));ZoneMobs zone(map.geometry(),ports);
    MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);std::uint32_t now,before,after,expected_x,expected_y,expected_z,expected_next;float x,z,hx,hz;int tx,tz,speed;unsigned step,size,mutate,moves,found,cursor,trail;
        row>>now>>x>>z>>hx>>hz>>tx>>tz>>speed>>step>>size>>before>>after>>mutate>>moves>>expected_x>>expected_y>>expected_z>>expected_next>>found>>cursor>>trail;check(bool(row));
        mob->position={x,0,z};check(zone.enter(*mob));mob->respawn_position={hx,0,hz};mob->next_chase=0;mob->status=3;mob->use_pais=true;mob->stop=mob->sleep=mob->stun=0;mob->move_speed=speed;mob->path.reset();mob->trail.clear();mob->trail.add(map,{tx,tz});
        record[0x68]=static_cast<std::byte>(step);record[0x3f]=static_cast<std::byte>(size);for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((before>>(8*i))&255);
        ports.after=after;ports.moves=0;ports.mutate=mutate?mob:nullptr;events.clear();MobAiConfiguredFsm fsm{5,7};
        check(vertical_mobs_return_step(*mob,now,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(events.empty() && ports.moves==moves && std::bit_cast<std::uint32_t>(mob->position.x)==expected_x && std::bit_cast<std::uint32_t>(mob->position.y)==expected_y && std::bit_cast<std::uint32_t>(mob->position.z)==expected_z && mob->next_chase==expected_next && mob->path.found()==bool(found) && mob->path.current().has_value()==bool(cursor) && !mob->trail.nodes().empty()==bool(trail));
        check(zone.leave(*mob));++cases;
    }
    check(cases==4);std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_astar_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);const std::vector<std::byte> saved(record.begin(),record.end());
    ReturnMove ports(record);MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned gap,pais,status,moves,resets,found;std::uint32_t x,z,hx,hz,next,expected_status,cx,cz;
        row>>gap>>pais>>status>>moves>>resets>>x>>z>>hx>>hz>>next>>expected_status>>found>>cx>>cz;check(bool(row));
        MapDefinition map;map.size=32;map.tiles.resize(128);for(int zi=0;zi<32;++zi)if(!gap || zi!=10){const auto tile=static_cast<std::size_t>(zi*32+12);map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
        ZoneMobs zone(map.geometry(),ports);mob->position={25,0,25};check(zone.enter(*mob));mob->respawn_position={2,0,2};mob->next_chase=0;mob->status=static_cast<int>(status);mob->use_pais=pais;mob->stop=mob->sleep=mob->stun=0;mob->move_speed=2;mob->path.reset();mob->trail.clear();mob->damage->reset();mob->damage->add(1,7,10,3,0);mob->hp=0;
        record[0x68]=std::byte{2};record[0x3f]=std::byte{0};for(unsigned i=0;i<4;++i)record[0x64+i]=static_cast<std::byte>((500U>>(8*i))&255);
        ports.after=500;ports.moves=0;ports.mutate=nullptr;events.clear();MobAiConfiguredFsm fsm{5,7};fsm.states[7].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob && subject.status==100 && subject.respawn_position.x==2);events+='F';};
        check(vertical_mobs_return_step(*mob,100,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(ports.moves==moves && events==std::string(resets,'F') && std::bit_cast<std::uint32_t>(mob->position.x)==x && std::bit_cast<std::uint32_t>(mob->position.z)==z && std::bit_cast<std::uint32_t>(mob->respawn_position.x)==hx && std::bit_cast<std::uint32_t>(mob->respawn_position.z)==hz && mob->next_chase==next && mob->status==static_cast<int>(expected_status) && mob->path.found()==bool(found) && mob->hp==0 && mob->damage->records().size()==1);
        const auto cursor=mob->path.current();check((cursor?static_cast<std::uint32_t>(cursor->x):UINT32_MAX)==cx && (cursor?static_cast<std::uint32_t>(cursor->z):UINT32_MAX)==cz);
        check(zone.leave(*mob));++cases;
    }
    check(cases==4);mob->path.reset();mob->damage->reset();std::copy(saved.begin(),saved.end(),record.begin());root.mob_pool().release(*mob);
}
void check_blocked_return(MobPrimaryZones& root,Frame& frame,const std::filesystem::path& fixture) {
    std::ifstream in(fixture);check(in.good());std::string line;unsigned cases=0;
    auto* mob=root.mob_pool().allocate();check(mob && mob->damage && mob->has_applied_skills());mob->definition=frame.runtime.find(1);check(mob->definition);
    auto record=frame.full.mutable_mob_record(1);ReturnMove ports(record);MobAiVariableCatalog variables;ReturnAi ai;std::string events;ReturnReset reset(events);
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);unsigned immediate,pais,status,trail,mutate,expected_status,found,cursor,expected_trail;std::uint32_t x,y,z,hx,hy,hz;std::string trace;
        row>>immediate>>pais>>status>>trail>>mutate>>expected_status>>x>>y>>z>>hx>>hy>>hz>>found>>cursor>>expected_trail>>trace;check(bool(row));
        MapDefinition map;map.size=32;map.tiles.resize(128);for(int blocked:{10,immediate?24:15}){const auto tile=static_cast<std::size_t>(20*32+blocked);map.tiles[tile/8]|=static_cast<std::uint8_t>(1U<<(tile%8));}
        ZoneMobs zone(map.geometry(),ports);mob->position={25,0,20};check(zone.enter(*mob));mob->respawn_position={10,0,20};mob->next_chase=0;mob->status=static_cast<int>(status);mob->use_pais=pais;mob->stop=mob->sleep=mob->stun=0;mob->move_speed=2;mob->path.reset();mob->trail.clear();if(trail)mob->trail.add(map,{23,20});mob->damage->reset();mob->damage->add(1,7,10,3,0);mob->hp=0;
        ports.moves=0;ports.mutate=nullptr;events.clear();MobAiConfiguredFsm fsm{5,7};fsm.states[7].enter=[&](MobRuntime& subject,MobAiConfiguredFsm&){check(&subject==mob && subject.status==100 && subject.respawn_position.x==10 && subject.trail.nodes().size()==trail);events+='F';if(mutate)subject.position={26,5,20};};
        check(vertical_mobs_return_step(*mob,100,fsm,variables,ai,reset,frame.full,map)==VerticalMobReturn::in_progress);
        check(ports.moves==0 && events==(pais?"F":"") && trace==(pais?(immediate?"FPT":"FTP"):(immediate?"PT":"TP")) && mob->status==static_cast<int>(expected_status));
        check(std::bit_cast<std::uint32_t>(mob->position.x)==x && std::bit_cast<std::uint32_t>(mob->position.y)==y && std::bit_cast<std::uint32_t>(mob->position.z)==z && std::bit_cast<std::uint32_t>(mob->respawn_position.x)==hx && std::bit_cast<std::uint32_t>(mob->respawn_position.y)==hy && std::bit_cast<std::uint32_t>(mob->respawn_position.z)==hz);
        check(mob->path.found()==bool(found) && mob->path.current().has_value()==bool(cursor) && !mob->trail.nodes().empty()==bool(expected_trail) && mob->damage->records().size()==1 && mob->hp==0);
        check(zone.leave(*mob));++cases;
    }
    check(cases==6);mob->damage->reset();root.mob_pool().release(*mob);
}
int main(int argc,char** argv) {try {
    check(argc==44);check_attack_oracle(argv[9]);check_attack_damage_limits(argv[36]);write_maps(argv[1]);auto config=MapConfiguration::decode("[BASE]\nMapCount=2\n");
    auto maps=std::make_shared<ServerMaps>(ServerMaps::load(config,argv[1]));auto f=std::make_shared<Frame>(maps);auto root=root_for(f);
    check(root->initialize());attach(f,root);check(root->startup(0));check(root->startup(1));
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){
        FrameAttack attack;attack.zone=z.mobs.get();attack.expected=5077;
        const auto subjects=z.mobs->all();check(!subjects.empty());
        check(vertical_mobs_attack_frame(*root,0,5077,attack));check(attack.ids.size()==subjects.size());
        for(std::size_t i=0;i<subjects.size();++i)check(attack.ids[i]==subjects[i]->id);
    });}
    check_near_return(*root,*f,argv[2]);
    check_direct_return(*root,*f,argv[3]);
    check_finish_return(*root,*f,argv[4]);
    check_path_return(*root,*f,argv[5]);
    check_trail_return(*root,*f,argv[6]);
    check_astar_return(*root,*f,argv[7]);
    check_blocked_return(*root,*f,argv[8]);
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;
    ActorEquipmentRules equipment{items,enchant,attacks};FrameScene scene;auto player_owner=std::make_shared<Actor>(42,EffectStatus{},PlayerResources{1,0,0},std::vector<PersistedEffect>{},scene,equipment);auto& player=*player_owner;
    check_attack_ai_prefix(*root,*f,player,argv[10]);
    check_attack_ai_wait(*root,*f,player,argv[11]);
    check_attack_ai_hp(*root,*f,player,argv[12]);
    check_min_hp(*root,*f,scene,equipment,argv[13]);
    check_attack_ai_mode6(*root,*f,player,scene,equipment,argv[14],argv[15]);
    check_attack_ai_self(*root,*f,player,argv[16]);
    check_use_skill(*root,player,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-use-skill-oracle.tsv");
    check_skill_attack(*root,player,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-skill-attack-oracle.tsv");
    check_skill_priority(*root,*f,scene,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-skill-view-oracle.tsv");
    check_attack_range(*root,*f,argv[17]);
    check_stationary_attack(*root,*f,player,argv[18],argv[19],argv[20]);
    check_chase_prefix(*root,*f,argv[21]);
    check_chase_leash(*root,*f,argv[22]);
    check_chase_target_reset(*root,*f,argv[26]);
    check_user_killed(*root,*f,player,scene,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-user-killed-oracle.tsv");
    check_defense_options(*root,scene,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-defense-options-oracle.tsv");
    check_defense_names(*root,scene,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-defense-names-oracle.tsv");
    check_bound_defense_options(*root,player);
    check_user_damage(*root,scene,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-user-damage-oracle.tsv");
    check_damage_scene(*root,equipment,std::filesystem::path(argv[43]).parent_path()/"vertical-mobs-damage-transport-oracle.tsv");
    check_live_hit_composition(*root,*f,scene,equipment);
    check_chase_intermediate(*root,*f,player,argv[27]);
    check_chase_final(*root,*f,player,argv[28]);
    check_chase_near_final(*root,*f,player,argv[29]);
    check_attack_guard(*root,*f,player,argv[30]);
    check_attack_finish(*root,*f,argv[31]);
    check_attack_target(*root,*f,player,argv[32]);
    check_attack_success(*root,*f,player,argv[33]);
    check_attack_block(player,argv[34]);
    check_attack_critical(*root,player,argv[35]);
    check_normal_attack_damage(*root,*f,player,argv[37]);
    check_damage_log(*root,*f,player,argv[38]);
    check_hit_preparation(*root,argv[39]);
    check_attack_reflection(*root,*f,player,argv[40]);
    check_calculated_hit(*root,*f,player,argv[41]);
    check_hit_delivery(*root,*f,argv[42]);
    check_hit_tail(*root,*f,argv[43]);
    check_native_attack_composition(*root,*f);
    check_mob_priority_views(*root,*f);
    check_bound_hit_owners(*root,*f);
    check_user_mirror_binding(*root,*f,player);
    check_mob_shared_priority(*root,*f);
    check_native_chase(*root,*f,player,argv[28]);
    check_native_chase_attack(*root,*f,player_owner);
    check_chase_path(*root,*f,player,argv[23],argv[24],argv[25]);
    FrameStack stack;FrameAi ai;NativeState state(stack);NativeDependencies dependencies(state);LinkedFixture linked;MicrosoftRandom wrong(1);bool rejected=false;
    try {vertical_mobs_actor_frame(*root,0,5077,f->init,wrong,f->full,stack,ai,linked);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    rejected=false;try {vertical_mobs_actor_frame(*root,0,5077,f->init,f->random,f->full,stack,ai,linked);}catch(const std::logic_error&){rejected=true;}check(rejected);
    rejected=false;try {vertical_mobs_actor_frame(*root,0,5077,f->init,wrong,f->full,stack,state,dependencies,linked);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    rejected=false;try {vertical_mobs_actor_frame(*root,0,5077,f->init,f->random,f->full,stack,state,dependencies,linked);}catch(const std::logic_error&){rejected=true;}check(rejected);
    ActorCombatWorld missing_combat;rejected=false;try {vertical_mobs_actor_attack_frame(*root,0,5077,f->full,missing_combat,state,dependencies);}catch(const std::logic_error&){rejected=true;}check(rejected);
    rejected=false;try {vertical_mobs_actor_frame(*root,99,5077,f->init,f->random,f->full,stack,state,dependencies,linked);}catch(const std::out_of_range&){rejected=true;}check(rejected);
    check(state.lookups==0 && dependencies.lookups==0);
    rejected=false;try {VerticalMobsWorldFrame wrong_frame(*root,*maps,f->init,wrong,f->full,stack,state,dependencies,linked);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    VerticalMobsWorldFrame world_frame(*root,*maps,f->init,f->random,f->full,stack,state,dependencies,linked);
    std::function<void(RuntimeZone&,std::uint32_t)> frame_callback=world_frame;
    RuntimeZone foreign;foreign.map=&maps->all()[0];rejected=false;try {world_frame(foreign,5077);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    foreign.map=nullptr;rejected=false;try {world_frame(foreign,5077);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    check(state.lookups==0 && dependencies.lookups==0);
    std::uint32_t corpse=0,hunter=0,returner=0;std::int32_t before=0;
    {auto held=root->lease(0);check(bool(held));held.visit([&](RuntimeZone& z){
        z.users=std::make_unique<ZoneUsers>(z.map->definition.geometry(),0);
        auto& spawn=z.mob_population->areas()[0].spawns[0];before=spawn.current;
        for(auto* mob:z.mobs->all()) if(mob->respawn==&spawn){corpse=mob->id;mob->status=2;mob->death_time=0;break;}
        check(corpse!=0 && before>0);
        for(auto* mob:z.mobs->all()) if(mob->id!=corpse && mob->definition){
            hunter=mob->id;mob->next_new_target=0;mob->next_move=100000;
            const auto id=mob->definition->id;auto record=f->full.mutable_mob_record(id);record[0x26]=std::byte{1};record[0x69]=std::byte{20};f->full.publish_mob_record(id);
            check(z.users->attach_at_position(player,mob->position));break;
        }
        check(hunter!=0);
        for(auto* mob:z.mobs->all())if(mob->id!=corpse && mob->id!=hunter && mob->definition){
            returner=mob->id;state.subject=mob;mob->use_pais=true;mob->status=3;mob->next_chase=0;mob->hp=0;mob->damage->add(1,42,10,3,0);break;
        }
        check(returner!=0);state.fsm.states[1].enter=[&](MobRuntime& mob,MobAiConfiguredFsm&){check(&mob==state.subject && mob.status==100 && mob.hp==0 && !mob.damage->records().empty());state.events+='F';};
    });}
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){
        VerticalMobsNativeAiOwners native(state,dependencies,*z.mobs,f->full,z.map->definition);
        state.subject->next_chase=5078;native.return_home(*state.subject,5077);check(state.lookups==0 && dependencies.lookups==0 && state.events.empty());state.subject->next_chase=0;
        ZoneMobs foreign(z.map->definition.geometry(),f->backend);VerticalMobsNativeAiOwners wrong_zone(state,dependencies,foreign,f->full,z.map->definition);bool rejected_zone=false;
        try {wrong_zone.return_home(*state.subject,5077);}catch(const std::invalid_argument&){rejected_zone=true;}check(rejected_zone);
    });}
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){frame_callback(z,5077);});}
    std::uint32_t deadline=0;
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){
        auto& spawn=z.mob_population->areas()[0].spawns[0];check(z.mobs->find(corpse)==nullptr && spawn.current==before-1 && spawn.next>5077);
        deadline=spawn.next;auto* selected=z.mobs->find(hunter);check(selected && selected->current_time==5077 && selected->status==1 && selected->target_id==player.id());
        auto* returned=z.mobs->find(returner);check(returned && returned->status==100 && returned->hp==returned->definition->hp && returned->damage->records().empty() && state.events=="FHS");
        ActorCombatWorld combat_world;combat_world.actor=player_owner;combat_world.mob=selected;
        for(auto* mob:z.mobs->all())if(mob!=selected)mob->next_attack=UINT32_MAX;
        VerticalMobsActorCombat combat(*selected,*z.mobs,*z.users,combat_world);selected->next_attack=0;
        check(vertical_mobs_actor_attack_frame(*root,0,6000,f->full,combat_world,state,dependencies));check(combat_world.calls==1 && selected->status==1);
        selected->target_id=999;selected->target_kind=7;selected->vision_fixed_user=0;
        vertical_mobs_attack(*selected,6000,combat,ai);check(combat_world.calls==1 && selected->status==0 && selected->target_kind==0);
        selected->status=1;selected->target_id=player.id();
        selected->next_attack=UINT32_MAX;returned->use_pais=false;returned->status=3;returned->next_attack=0;returned->next_chase=0;
        returned->hp=0;returned->damage->add(1,42,10,3,0);state.events.clear();
        check(vertical_mobs_actor_attack_frame(*root,0,6000,f->full,combat_world,state,dependencies));
        check(returned->status==0 && returned->hp==returned->definition->hp && returned->damage->records().empty() && state.events=="HS");
        for(auto* mob:z.mobs->all())mob->next_attack=UINT32_MAX;
        selected->next_attack=0;selected->status=1;selected->target_kind=1;selected->target_id=player.id();selected->attack_times={0,0,0};selected->attack_speed=3;selected->sleep=selected->stun=0;selected->vision_fixed_user=0;selected->path.reset();
        auto record=f->full.mutable_mob_record(selected->definition->id);const std::vector<std::byte> saved(record.begin(),record.end());
        record[0x26]=std::byte{1};record[0x68]=std::byte{2};record[0x7c]=std::byte{0};record[0x3f]=std::byte{0};
        for(unsigned i=0;i<4;++i)record[0x78+i]=static_cast<std::byte>(i==0?2:0);
        record[0x82]=record[0x82+40]=std::byte{0};record[0x82+20]=std::byte{1};const auto at=0x80+20;
        record[at]=record[at+1]=std::byte{0};record[at+8]=std::byte{5};for(unsigned i=0;i<4;++i)record[at+4+i]=static_cast<std::byte>((2000U>>(i*8))&255);
        const auto country=player.appearance().identity.country;player.appearance().identity.country=1;player.appearance().attackable=true;player.appearance().visible=true;player.attack_state().mode=0;player.status().no_mob_targeting=false;
        check(z.users->move_at_position(player.id(),selected->position));NativeAttackAggro native_combat;native_combat.subject=selected;native_combat.actors.push_back(player_owner);
        StationaryWorld native_attacks;native_attacks.subject=selected;native_attacks.target=&player;native_attacks.now=6000;native_attacks.record=record;
        VerticalMobsWorldAttackFrame native_frame(*root,*maps,f->random,stack,f->full,native_combat,state,dependencies,native_attacks);
        RuntimeZone fake;fake.map=z.map;bool rejected_attack_zone=false;try{native_frame(fake,6000);}catch(const std::invalid_argument&){rejected_attack_zone=true;}check(rejected_attack_zone);
        native_frame(z,6000);check(native_combat.calls==1 && native_attacks.calls==1 && native_attacks.during==1 && selected->attack_times[1]==8000);
        selected->attack_times={0,0,0};native_attacks.now=6001;
        vertical_mobs_native_zone_attack(z,*selected,6001,f->random,stack,f->full,native_combat,state,dependencies,native_attacks);
        check(native_combat.calls==2 && native_attacks.calls==2 && native_attacks.during==1 && selected->attack_times[1]==8001);
        bool rejected_missing=false;try{vertical_mobs_native_zone_attack(fake,*selected,6001,f->random,stack,f->full,native_combat,state,dependencies,native_attacks);}catch(const std::logic_error&){rejected_missing=true;}check(rejected_missing);
        selected->target_id=999;selected->target_kind=1;selected->status=1;selected->vision_fixed_user=0;native_frame(z,6002);
        check(native_combat.calls==2 && native_attacks.calls==2 && selected->status==0 && selected->target_kind==0);
        bool rejected_idle=false;try{VerticalMobsNativeZoneIdle bad(fake,f->random,stack,f->full,state,dependencies);}catch(const std::invalid_argument&){rejected_idle=true;}check(rejected_idle);
        VerticalMobsNativeZoneIdle zone_idle(z,f->random,stack,f->full,state,dependencies);
        selected->use_pais=false;selected->status=0;selected->target_kind=0;selected->target_id=0;selected->next_new_target=0;selected->next_move=UINT32_MAX;record[0x69]=std::byte{20};
        vertical_mobs_idle(*selected,6003,zone_idle);
        check(selected->current_time==6003 && selected->status==1 && selected->target_kind==1 && selected->target_id==player.id());
        player.appearance().identity.country=country;std::copy(saved.begin(),saved.end(),record.begin());
    });}
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){frame_callback(z,deadline);});}
    {auto held=root->lease(0);held.visit([&](RuntimeZone& z){check(z.mob_population->areas()[0].spawns[0].current==before);check(z.users->detach(player.id()));});}
    check(root->drain_all([]{return 100U;}));
    {
        auto f2=std::make_shared<Frame>(maps);FrameStack stack2;NativeState state2(stack2);NativeDependencies dependencies2(state2);LinkedFixture linked2;
        VerticalMobsDeferredWorldFrame deferred(*maps,f2->init,f2->random,f2->full,stack2,state2,dependencies2,linked2);
        std::function<void(RuntimeZone&,std::uint32_t)> callback=deferred;RuntimeZone absent;bool failed=false;
        try {callback(absent,77);}catch(const std::logic_error&){failed=true;}check(failed);
        failed=false;try {deferred.bind({});}catch(const std::invalid_argument&){failed=true;}check(failed);
        std::shared_ptr<MobPrimaryZones> root2;
        MobPrimaryZoneTransport transport{[&](const eternity::net::Packet&){++f2->stats->db;},[&](const eternity::net::Packet&){++f2->stats->world;},[&](const ConfiguredMap&,ZoneMobs&,const eternity::net::Packet&){++f2->stats->map;}};
        VerticalWorldPorts ports{[&](MobPrimaryZones&){attach(f2,root2);},callback,[]{return 100U;}};
        auto world=VerticalWorld::create(maps,std::shared_ptr<ZoneServices>(f2,&f2->backend),std::move(f2->pool),transport,ports);
        root2=world->zones();deferred.bind(root2);failed=false;try {deferred.bind(root2);}catch(const std::logic_error&){failed=true;}check(failed);
        check(world->start());
        for(std::size_t i=0;i<maps->size();++i) {auto held=root2->lease(i);held.visit([&](RuntimeZone& z){
            z.users=std::make_unique<ZoneUsers>(z.map->definition.geometry(),0);
            for(auto* mob:z.mobs->all()){mob->next_move=UINT32_MAX;mob->next_new_target=UINT32_MAX;}
        });}
        world->frame(77);
        for(std::size_t i=0;i<maps->size();++i) {auto held=root2->lease(i);held.visit([&](RuntimeZone& z){check(!z.mobs->all().empty());for(auto* mob:z.mobs->all())check(mob->current_time==77);});}
        check(world->shutdown());world.reset();root2.reset();failed=false;try {callback(absent,77);}catch(const std::logic_error&){failed=true;}check(failed);
    }
    std::cout<<"Actor frame real zone, death and respawn passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
