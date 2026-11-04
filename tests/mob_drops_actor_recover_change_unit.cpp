#include "mob_drops_actor_rebirth_support.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
bool same(PlayerResources a,PlayerResources b) {return a.hp==b.hp&&a.mp==b.mp&&a.sp==b.sp;}
struct Scene final:ActorScene {
    Actor* actor=nullptr;int group=-1,after=-1,depth=0;bool mutate=false,party_mutate=false,nested=false;
    PlayerResources changed,last,child;std::string events;
    void event(std::string value) {if(!events.empty()) events+=',';events+=value;}
    void packet(char kind,const eternity::net::Packet& p) {static constexpr char hex[]="0123456789abcdef";std::string s(1,kind);for(auto b:p){s+=hex[b>>4];s+=hex[b&15];}event(s);}
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet& p) override {
        packet('V',p);if(depth) return;
        if(mutate) {actor->replace_resources_raw(changed);group=after;event("X");}
        if(nested) {event("N");depth=1;actor->send_recover_change(child);depth=0;event("n");}
    }
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected cell send");}
    std::optional<bool> party_kind() const noexcept override {return group<0?std::nullopt:std::optional<bool>(group!=0);}
    void send_party(const eternity::net::Packet& p) noexcept override {packet('P',p);if(!depth&&party_mutate){actor->replace_resources_raw(last);group=-1;event("Y");}}
    void reset_damage() override {throw std::runtime_error("Unexpected damage reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected stop");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {return nullptr;}
    UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0;
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;
    for(std::string line;std::getline(input,line);) {
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned index,zone;std::uint32_t id;int mutate,party_mutate,nested,final_group;PlayerResources initial,view,expected;Scene scene;std::string events;
        row>>index>>id>>zone>>scene.group>>scene.after>>mutate>>party_mutate>>nested;
        for(auto* p:{&initial,&view,&scene.changed,&scene.last,&scene.child,&expected}) row>>p->hp>>p->mp>>p->sp;
        row>>final_group>>events;check(!row.fail(),"Malformed fixture");scene.mutate=mutate;scene.party_mutate=party_mutate;scene.nested=nested;
        EffectStatus status;status.in_zone=zone;status.skills.equipment.base.maximum={1,2,3};
        Actor actor(id,status,initial,{},scene,{items,enchant,mobs});scene.actor=&actor;actor.attack_state().mode=1;actor.attack_state().motion=0xa5;actor.attack_state().recovery=-7;actor.presentation_motion(37);
        const auto* owner=&actor.resources();actor.send_recover_change(view);
        check((scene.events.empty()?"-":scene.events)==events,"Original notification/callback order differs");
        check(same(actor.resources(),expected)&&scene.group==final_group&&owner==&actor.resources(),"Original live owner differs");
        check(actor.attack_state().mode==1&&actor.attack_state().motion==0xa5&&actor.attack_state().recovery==-7&&actor.presentation_motion()==37&&same(actor.status().skills.equipment.base.maximum,{1,2,3}),"Notification changed unrelated state");++cases;
    }
    check(cases==768,"Incomplete original coverage");
    rebirth_test::Scene damage_scene;EffectStatus damage_status;damage_status.skills.equipment.base.maximum={100,100,100};damage_status.in_zone=true;
    Actor damaged(9,damage_status,{100,20,30},{},damage_scene,{items,enchant,mobs});
    check(!damaged.damage(3,77,3,SkillDefinitions{}),"Damage setup failed");const auto credit=damaged.damage_ledger().maximum();check(credit && credit->amount==3,"Missing damage credit");
    damaged.replace_resources_raw({100,0x80000000U,0xffffffffU});damage_scene.packets.clear();const auto resets=damage_scene.resets;const auto ticks=damage_scene.ticks;
    damaged.send_recover_change(damaged.resources());
    check(damaged.damage_ledger().maximum()==credit && damage_scene.resets==resets && damage_scene.ticks==ticks && damage_scene.packets==std::vector<eternity::net::Packet>{{5,5,9,0,0,0,100,0,0,0,255,255}},"Pure full-HP notification altered damage or packet values");std::cout<<cases<<" original full notification traces matched actual Actor/resources, including nested callbacks and live party changes\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
