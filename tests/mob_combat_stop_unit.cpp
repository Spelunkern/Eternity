#include "eternity/game/mob_combat_stop.hpp"
#include "eternity/game/mob_combat_attack.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity;using namespace eternity::game;
namespace {
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
std::string state(const Actor& actor) {
    const auto& attack=actor.attack_state();std::ostringstream out;
    out<<static_cast<std::uint32_t>(attack.mode)<<','<<static_cast<std::uint32_t>(attack.recovery)<<','<<static_cast<std::uint32_t>(actor.status().current_target_type)<<','<<attack.previous_tick<<','<<attack.next_tick<<','<<actor.status().current_target;return out.str();
}
std::string record(const MobDamageRecord& r) {
    std::ostringstream out;out<<static_cast<unsigned>(r.type)<<','<<r.id<<','<<static_cast<std::uint32_t>(r.damage)<<','<<static_cast<std::uint32_t>(r.aggro)<<','<<r.party;return out.str();
}
struct Pool final:MobDamagePoolServices {
    std::function<void(MobDamageRecord*)> observe;
    std::unique_ptr<MobDamageRecord> construct() override{auto result=std::make_unique<MobDamageRecord>();observe(result.get());return result;}
};
struct Combat final:MobCombatAttackServices {
    Actor& actor;std::string events;MobDamageRecord* pending=nullptr;
    explicit Combat(Actor& subject):actor(subject) {}
    void append(const std::string& text){if(!events.empty())events+='/';events+=text;}
    void flush(){if(pending){append("A:"+record(*pending));pending=nullptr;}}
    std::int32_t status(const MobCombatState& m) override{flush();return m.state;}
    void dispatch_death_event(MobCombatState&) override{throw std::runtime_error("unexpected FSM dispatch");}
    std::uint32_t tick() override{return 123;}
    void log_damage(MobCombatState&,std::uint32_t) override{throw std::runtime_error("unexpected log");}
    void retarget(MobCombatState&,bool) override{flush();append("R");}
    void stop_attack(MobCombatAttacker&) override{mob_combat_stop(actor);}
    void death(MobCombatState&,std::int32_t type,std::uint32_t id) override{flush();append("D:"+state(actor)+','+std::to_string(type)+','+std::to_string(id));}
};
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture unavailable");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;
        const ActorEquipmentRules rules{items,enchant,mobs};
        unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            char tag;std::uint32_t mode,recovery,kind,previous,next,target,hp=0,damage=0,ohp=0;unsigned party=0,killed=0;
            std::string final,writes,events;std::istringstream row(line);
            row>>tag>>mode>>recovery>>kind>>previous>>next>>target;
            if(tag=='C')row>>hp>>damage>>party>>killed>>ohp;
            row>>final>>writes>>events;if(!row)throw std::runtime_error("invalid fixture");
            Scene scene;EffectStatus status;status.current_target_type=std::bit_cast<std::int32_t>(kind);status.current_target=target;
            Actor actor(42,status,{}, {},scene,rules);
            auto& attack=actor.attack_state();attack.mode=std::bit_cast<std::int32_t>(mode);attack.recovery=std::bit_cast<std::int32_t>(recovery);attack.previous_tick=previous;attack.next_tick=next;
            attack.motion=0xa5;attack.resume_tick=999;actor.actions().previous_target_type=7;actor.actions().previous_target=555;
            scene.calls=0;
            if(tag=='S')mob_combat_stop(actor);
            else if(tag=='C') {
                Pool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);Combat combat(actor);
                allocator.observe=[&](MobDamageRecord* r){combat.flush();combat.pending=r;};
                MobCombatState mob;mob.hp=hp;MobCombatAttacker user;user.id=42;user.aggro_bonus=3;if(party)user.party=77;
                const std::array hit={std::bit_cast<std::int32_t>(damage),0,0};
                const bool result=mob_combat_user_hit(mob,ledger,combat,user,hit,std::bit_cast<std::int32_t>(damage),0);combat.flush();
                if(events=="-")events.clear();
                if(result!=static_cast<bool>(killed)||mob.hp!=ohp||combat.events!=events)
                    throw std::runtime_error("stop composition mismatch "+std::to_string(count)+"\nactual "+combat.events+"\nexpected "+events);
            } else throw std::runtime_error("unknown fixture tag");
            const bool changed=(tag=='S'||killed)&&mode!=0&&mode!=1;
            std::string expected_writes="-";
            if(changed){expected_writes="142c:4:0/1528:4:0/143c:4:0/1440:4:0";if(std::bit_cast<std::int32_t>(recovery)<1)expected_writes+="/1274:4:1";}
            if(state(actor)!=final||scene.calls!=0||writes!=expected_writes||actor.status().current_target!=target||attack.motion!=0xa5||attack.resume_tick!=999||actor.actions().previous_target_type!=7||actor.actions().previous_target!=555)
                throw std::runtime_error("actor stop mismatch "+std::to_string(count)+" actual "+state(actor)+" expected "+final);
            ++count;
        }
        if(count!=1024)throw std::runtime_error("incomplete fixture");
        std::cout<<count<<" complete stop/Actor/caller comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
