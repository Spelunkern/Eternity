#include "eternity/game/mob_combat_scheduler_zone.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
struct Scene final:ActorScene {
    mutable unsigned calls=0;
    void send_self(const net::Packet&) noexcept override{++calls;}
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


struct World final:MobCombatSchedulerActorWorld {
    std::function<void(Actor&)> hit;MobRuntime mob;
    MobRuntime* enable_mob(Actor&,std::uint32_t) override{return &mob;}
    bool in_range(Actor&,MobRuntime&) override{return true;}
    bool is_both_mob(MobRuntime&) override{return false;}
    void attack_mob(Actor& actor,MobRuntime&) override{hit(actor);}
    void send_move_cancel(Actor&,const net::Packet&) override{unused();}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected actor-adapter world service");}
    Actor* find_user(Actor&,std::uint32_t) override{unused();}
    Actor* find_attack_user(Actor&,std::uint32_t) override{unused();}
    std::int32_t map_team_mode(const Actor&) const override{unused();}
    bool in_user_range(Actor&,Actor&) override{unused();}
    void attack_user(Actor&,Actor&) override{unused();}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&) const override{unused();}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t) override{unused();}
    void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
};

using Owners=std::map<std::uint32_t,std::shared_ptr<Actor>>;
constexpr std::array<std::uint32_t,6> ids={90,10,70,20,60,30};
std::shared_ptr<void> lease(Owners& owners,Actor& actor){return owners.at(actor.id());}
void composition(const ActorEquipmentRules& rules) {
    unsigned retired=0;std::uint32_t last_deadline=0;unsigned last_motion=99;Scene scene;Owners owners;ZoneUsers zone(2,2,0);
    auto* first=new Actor(90,{}, {},{},scene,rules);
    owners.emplace(90,std::shared_ptr<Actor>(first,[&](Actor* actor){++retired;last_deadline=actor->attack_state().resume_tick;last_motion=actor->attack_state().motion;delete actor;}));
    owners.emplace(10,std::make_shared<Actor>(10,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules));
    owners.emplace(70,std::make_shared<Actor>(70,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules));
    first->attack_state().mode=2;first->attack_state().next_tick=1;first->attack_state().resume_tick=100;first->status().current_target_type=2;first->status().current_target=77;first->status().skills.equipment.attack_speed=4;
    owners.at(10)->attack_state().resume_tick=101; // false poll must not stop traversal
    zone.attach(*first,0,0,{});zone.attach(*owners.at(10),0,0,{});
    const std::weak_ptr<Actor> weak=owners.at(90);World world;
    world.hit=[&](Actor& actor){check(actor.id()==90,"wrong active actor");check(zone.detach(90),"cannot detach current actor");owners.erase(90);check(!weak.expired(),"actor was not retained through attack");check(zone.find_actor(10)!=nullptr,"nested ID lookup failed");zone.attach(*owners.at(70),1,1,{});};
    struct Services final:MobCombatSchedulerZoneServices {
        Owners& owners;World& world_;MobCombatSchedulerBindings binding{true};std::vector<std::uint32_t> seen;
        Services(Owners& o,World& w):owners(o),world_(w){}
        std::shared_ptr<void> retain(Actor& a) override{return lease(owners,a);}
        MobCombatSchedulerBindings& bindings(Actor& a) override{seen.push_back(a.id());return binding;}
        MobCombatSchedulerActorWorld& world(Actor&) override{return world_;}
    } services(owners,world);
    mob_combat_scheduler_zone(zone,100,services);
    check(services.seen==std::vector<std::uint32_t>{90,10,70},"scheduler skipped false poll or live append");
    check(weak.expired()&&retired==1&&last_deadline==2350&&last_motion==0,"lease did not span post-attack scheduler writes");
    check(owners.at(10)->attack_state().resume_tick==101,"poll sampled another tick");
}
void lock_and_invalid_lease(const ActorEquipmentRules& rules) {
    Scene scene;Owners owners;ZoneUsers zone(1,1,0);
    owners.emplace(90,std::make_shared<Actor>(90,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules));zone.attach(*owners.at(90),0,0,{});
    bool rejected=false;try{zone.visit_all([](Actor&){return std::shared_ptr<void>{};},[](Actor&){throw std::runtime_error("unretained callback ran");});}catch(const std::logic_error&){rejected=true;}
    check(rejected,"empty retention token accepted");
    std::promise<void> started;auto ready=started.get_future();std::future<Actor*> pending;
    zone.visit_all([&](Actor& a){return lease(owners,a);},[&](Actor&){
        pending=std::async(std::launch::async,[&]{started.set_value();return zone.find_actor(90);});ready.wait();
        check(pending.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout,"whole-zone membership lock not held through callback");
    });
    check(pending.get()==owners.at(90).get(),"membership lock not released");
}
}
int main(int argc,char**argv) {
    try {
        check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"missing fixture");ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned scenario,pattern,n;std::uint32_t tick;std::string expected,members;std::istringstream row(line);row>>scenario>>pattern>>n>>tick>>expected>>members;check(static_cast<bool>(row),"invalid fixture");
            Scene scene;Owners owners;ZoneUsers zone(2,2,0);
            for(auto id:ids)owners.emplace(id,std::make_shared<Actor>(id,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,rules));
            for(unsigned i=0;i<n;++i)zone.attach(*owners.at(ids[i]),0,0,{});
            bool mutated=false;std::string events;
            const auto event=[&](const std::string& text){if(!events.empty())events+='|';events+=text;};
            std::function<void(std::uint32_t,unsigned)> run;
            run=[&](std::uint32_t now,unsigned depth){zone.visit_all([&](Actor& a){return lease(owners,a);},[&](Actor& actor){
                const auto id=actor.id();event("P,"+std::to_string(id)+','+std::to_string(now)+','+std::to_string(depth));
                if(!mutated) {
                    mutated=true;
                    const auto remove=[&](std::uint32_t target){const auto weak=std::weak_ptr<Actor>(owners.at(target));check(zone.detach(target),"detach failed");owners.erase(target);check(target==id?!weak.expired():weak.expired(),"retention differs from current/future actor");event("R,"+std::to_string(target));};
                    const auto append=[&]{zone.attach(*owners.at(ids[n]),1,1,{});event("A,"+std::to_string(ids[n]));};
                    const auto find=[&](std::uint32_t target){auto* found=zone.find_actor(target);event("Q,"+std::to_string(target)+','+std::to_string(found?found->id():0));};
                    if(pattern==0)find(ids[n-1]);else if(pattern==1)remove(id);else if(pattern==2)remove(ids[1]);else if(pattern==3)append();
                    else if(pattern==4){zone.move(id,1,1,{});event("M,"+std::to_string(id));}
                    else if(pattern==5)run(now+7,depth+1);
                    else if(pattern==6){remove(id);append();}
                    else {remove(ids[1]);find(ids[1]);append();}
                }
                return false; // void visitor deliberately discards this result
            });};
            run(tick,1);
            std::string actual;zone.visit_all([&](Actor& a){return lease(owners,a);},[&](Actor& actor){if(!actual.empty())actual+=',';actual+=std::to_string(actor.id());});
            check(events==expected&&actual==members,"original zone traversal mismatch scenario "+std::to_string(scenario)+"\nactual "+events+"\nexpected "+expected+"\nmembers "+actual+" expected "+members);
            ++count;
        }
        check(count==256,"incomplete zone loop fixture");composition(rules);lock_and_invalid_lease(rules);
        std::cout<<count<<" whole original zone loops and native retained scheduler composition passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
