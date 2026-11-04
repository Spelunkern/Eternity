#include "eternity/game/mob_combat_scheduler_actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity;using namespace eternity::game;
namespace {
std::string hex(const net::Packet& bytes){const char* d="0123456789abcdef";std::string out;for(auto b:bytes){out+=d[b>>4];out+=d[b&15];}return out;}
struct Scene final:ActorScene {
    Actor* actor=nullptr;MobCombatSchedulerBindings* bindings=nullptr;
    unsigned mutation=0,released=0;bool observing=false,error=false,saw_removal=false;std::string events;
    std::string snapshot() const {
        const auto& a=actor->attack_state();const auto& s=actor->status();const auto& m=actor->actions();std::ostringstream out;
        out<<static_cast<std::uint32_t>(a.mode)<<','<<static_cast<std::uint32_t>(a.recovery)<<','<<static_cast<std::uint32_t>(s.current_target_type)<<','<<s.current_target<<','<<a.previous_tick<<','<<a.next_tick<<','<<a.resume_tick<<','<<static_cast<unsigned>(a.motion)<<','<<static_cast<unsigned>(s.shape)<<','<<s.shape_template<<','<<static_cast<std::uint32_t>(m.move_type)<<','<<m.pending_move<<','<<m.move_slot;
        for(auto value:m.move_slots)out<<','<<value;
        out<<','<<actor->effects().size()<<','<<released<<','<<bindings->weapon_present;return out.str();
    }
    void event(char tag,const std::string& extra=""){if(!events.empty())events+='|';events+=std::string(1,tag)+':'+snapshot();if(!extra.empty())events+=','+extra;}
    void send_self(const net::Packet& packet) noexcept override {
        event('S',hex(packet));
        if(packet.size()>=2&&packet[0]==0x0e&&packet[1]==5) {
            saw_removal=true;if(actor->effects().size()!=0||!actor->replica().snapshot().empty())error=true;
            if(mutation&1){actor->actions().move_type=2;actor->actions().pending_move=1;actor->actions().move_slot=10;}
            if(mutation&2)bindings->weapon_present=false;
        }
    }
    void send_cell_view(const net::Packet& packet) override{event('V',hex(packet));}
    void release_effect_id(std::uint32_t id) noexcept override{if(observing){++released;event('F',std::to_string(id));}}
    std::optional<bool> party_kind() const noexcept override{return {};}
    void send_nearby(const net::Packet&) override{error=true;}
    void send_party(const net::Packet&) noexcept override{error=true;}
    void reset_damage() override{error=true;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{error=true;return {};}
    std::uint32_t tick_ms() const noexcept override{return 0;}
    void stop_target_mob(std::uint32_t) override{error=true;}
    void taunt(std::uint16_t,float) override{error=true;}
    ZoneUsers* user_zone() override{error=true;return nullptr;}
    UserShapeSocial user_shape_social() override{error=true;return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{error=true;}
};
struct World final:MobCombatSchedulerActorWorld {
    Scene& scene;MobRuntime mob;explicit World(Scene& s):scene(s){}
    MobRuntime* enable_mob(Actor&,std::uint32_t) override{scene.event('E');return &mob;}
    bool in_range(Actor&,MobRuntime&) override{scene.event('R');return true;}
    bool is_both_mob(MobRuntime&) override{scene.event('B');return false;}
    void attack_mob(Actor&,MobRuntime& m) override{scene.event('A');m.status=scene.mutation&8?2:0;}
    void send_move_cancel(Actor& actor,const net::Packet& packet) override {
        scene.event('M',hex(packet));if(scene.mutation&4){actor.attack_state().resume_tick=1234;actor.attack_state().motion=7;}
    }
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
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing fixture");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;ActorEquipmentRules rules{items,enchant,mobs};unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned variant,shape,zone,mutation,result;std::uint32_t move_type,pending,slot,now;std::string final,events,backend;
            std::istringstream row(line);row>>variant>>shape>>zone>>move_type>>pending>>slot>>mutation>>now>>result>>final>>events>>backend;if(!row)throw std::runtime_error("invalid actor fixture");
            SkillDefinition skill;skill.id=variant==2?127:200;skill.level=1;skill.detail=variant==2?90:(variant==1?88:82);skill.change_type=variant==2?100:0;
            std::vector<PersistedEffect> saved;if(variant)saved.push_back({skill.id,1,60});
            MobCombatSchedulerBindings bindings{true};Scene scene;EffectStatus status;status.in_zone=zone!=0;status.shape=static_cast<std::uint8_t>(shape);status.shape_template=321;status.current_target_type=2;status.current_target=77;status.skills.equipment.attack_speed=4;
            Actor actor(42,status,{},saved,scene,rules);scene.actor=&actor;scene.bindings=&bindings;scene.mutation=mutation;
            if(variant)actor.effects().append(1000,{{skill.id,1,60},0,60000,false},skill);
            auto& a=actor.attack_state();a.mode=2;a.recovery=0;a.previous_tick=55;a.next_tick=1;a.motion=0xa5;
            auto& movement=actor.actions();movement.move_type=std::bit_cast<std::int32_t>(move_type);movement.pending_move=pending;movement.move_slot=slot;for(unsigned i=0;i<11;++i)movement.move_slots[i]=0x11110000u+i;
            scene.observing=true;World world(scene);const bool actual=mob_combat_scheduler(actor,bindings,now,world);if(events=="-")events.clear();
            // Canonical Actor owns a DB-agent replica, not an immediate 0902
            // transport. Audit equivalent erase state before 050e; retain the
            // original wire request separately as evidence rather than fake it.
            const bool removed=backend!="-";
            if(removed&&backend!="02092a000000c80001")throw std::runtime_error("unexpected original backend contract");
            if(actual!=static_cast<bool>(result)||scene.snapshot()!=final||scene.events!=events||scene.error||scene.saw_removal!=removed||actor.replica().snapshot().size()!=(variant&&!removed?1u:0u))
                throw std::runtime_error("actor-adapter mismatch "+std::to_string(count)+" actual "+scene.snapshot()+" expected "+final+"\nactual events "+scene.events+"\nexpected events "+events);
            scene.observing=false;++count;
        }
        if(count!=768)throw std::runtime_error("incomplete actor fixture");std::cout<<count<<" complete actor-owner polling comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
