#include "eternity/game/mob_combat_scheduler_users.hpp"
#include <bit>
#include <fstream>
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


std::string hex(const net::Packet& bytes){const char* digits="0123456789abcdef";std::string out;for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
struct World final:MobCombatSchedulerUserWorld,ActorActionWorld {
    Actor& actor;Actor& peer;unsigned found=0,range=0,mutation=0;std::int32_t team_mode=0;mutable std::string events;
    World(Actor& a,Actor& p):actor(a),peer(p){}
    std::string snapshot() const {
        const auto& a=actor.attack_state();const auto& s=actor.status();std::ostringstream out;
        out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<a.previous_tick<<','<<a.next_tick<<','<<a.resume_tick<<','<<static_cast<unsigned>(a.motion)<<','<<static_cast<std::uint32_t>(s.skills.equipment.attack_speed)<<','<<peer.id()<<','<<static_cast<std::uint32_t>(peer.attack_state().mode)<<','<<peer.appearance().visible<<','<<peer.appearance().attackable<<','<<static_cast<unsigned>(peer.status().shape);return out.str();
    }
    void event(char tag,const std::string& extra="") const {if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot();if(!extra.empty())events+=','+extra;}
    std::int32_t attack_speed(Actor&) override{event('Q');return 4;}
    void remove_mob_transform(Actor&) override{event('T');}
    Actor* find_attack_user(Actor&,std::uint32_t id) override {
        event('F',std::to_string(id));if(mutation&1){actor.status().current_target=42;actor.status().skills.equipment.attack_speed=9;}
        if(mutation&32){peer.attack_state().mode=0;peer.appearance().visible=true;peer.appearance().attackable=true;peer.status().shape=0;}
        return found?&peer:nullptr;
    }
    std::int32_t map_team_mode(const Actor&) const override{event('D');return team_mode;}
    bool in_user_range(Actor&,Actor&) override{event('R');if(mutation&2){peer.attack_state().mode=1;actor.status().skills.equipment.attack_speed=0;}return range!=0;}
    void attack_user(Actor&,Actor&) override{event('A');if(mutation&4){peer.attack_state().mode=1;actor.attack_state().mode=0;actor.status().skills.equipment.attack_speed=9;}}
    void cancel_move(Actor& a) override{event('C');if(mutation&8){a.attack_state().resume_tick=1234;a.attack_state().motion=7;}a.cancel_move(*this);}
    void send_self(Actor& a,const net::Packet& packet) override{event('P',hex(packet));if(mutation&16){a.attack_state().resume_tick=9;a.attack_state().motion=8;}}
    [[noreturn]] static void unused(){throw std::runtime_error("unexpected mob/skill/movement service");}
    const MobCombatSchedulerSkillBinding* selected_skill(const Actor&) const override{unused();}
    Actor* enable_skill_user(Actor&,std::uint32_t,std::uint16_t) override{unused();}
    void use_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void attack_skill_mob(Actor&,MobRuntime*,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void use_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    void attack_skill_user(Actor&,Actor&,const MobCombatSchedulerSkillBinding&,std::uint32_t) override{unused();}
    MobRuntime* enable_mob(Actor&,std::uint32_t) override{unused();}
    bool in_range(Actor&,MobRuntime&) override{unused();}
    bool is_both_mob(MobRuntime&) override{unused();}
    void attack_mob(Actor&,MobRuntime&) override{unused();}
    Actor* find_user(Actor&,std::uint32_t) override{unused();}
    void send_move_cancel(Actor&,const net::Packet&) override{unused();}
};
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::uint32_t requested,identity,speed,now,resume,recovery;unsigned found,target_mode,visible,attackable,shape,country,peer_country,map_mode,relation,range,mutation,result;
            std::string final,events;std::istringstream row(line);row>>requested>>identity>>found>>target_mode>>visible>>attackable>>shape>>country>>peer_country>>map_mode>>relation>>range>>mutation>>speed>>now>>resume>>recovery>>result>>final>>events;
            if(!row)throw std::runtime_error("invalid normal-user fixture");
            Scene scene;EffectStatus status;status.current_target_type=1;status.current_target=requested;status.skills.equipment.attack_speed=std::bit_cast<std::int32_t>(speed);
            Actor actor(42,status,{}, {},scene,rules);Actor peer(identity,{}, {},{},scene,rules);scene.calls=0;
            actor.appearance().identity.country=static_cast<std::uint8_t>(country);peer.appearance().identity.country=static_cast<std::uint8_t>(peer_country);
            peer.appearance().visible=visible!=0;peer.appearance().attackable=attackable!=0;peer.status().shape=static_cast<std::uint8_t>(shape);peer.attack_state().mode=static_cast<std::int32_t>(target_mode);
            auto& attack=actor.attack_state();attack.mode=2;attack.recovery=std::bit_cast<std::int32_t>(recovery);attack.previous_tick=55;attack.next_tick=1;attack.resume_tick=resume;attack.motion=0xa5;
            if(relation==1){actor.relations().duel_state=4;actor.relations().duel_opponent=identity;}
            else if(relation==2||relation==4){actor.relations().guild_id=1;peer.relations().guild_id=2;actor.relations().opposing_guild_id=2;peer.relations().opposing_guild_id=relation==2?1:9;actor.relations().guild_duel_ready=1;peer.relations().guild_duel_ready=1;}
            else if(relation==3){peer.relations().duel_state=4;peer.relations().duel_opponent=42;}
            World world(actor,peer);world.found=found;world.range=range;world.mutation=mutation;world.team_mode=static_cast<std::int32_t>(map_mode);
            MobCombatSchedulerBindings bindings{true};const bool actual=mob_combat_scheduler(actor,bindings,now,world);if(events=="-")events.clear();
            if(actual!=static_cast<bool>(result)||world.snapshot()!=final||world.events!=events||scene.calls!=0)
                throw std::runtime_error("normal-user scheduler mismatch "+std::to_string(count)+" actual "+world.snapshot()+" expected "+final+"\nactual events "+world.events+"\nexpected events "+events);
            ++count;
        }
        if(count!=2048)throw std::runtime_error("incomplete normal-user fixture");std::cout<<count<<" complete normal-user scheduler comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
