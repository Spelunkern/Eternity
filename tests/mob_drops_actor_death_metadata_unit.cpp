#include "mob_drops_actor_rebirth_support.hpp"
#include "eternity/game/mob_targeting_lifecycle_death.hpp"
#include "eternity/game/mob_targeting_lifecycle.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
void put(eternity::net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
struct Scene final:ActorScene,ActorEffectDatabase {
    std::vector<eternity::net::Packet> effect_database_packets;
    void send_effect_database(const eternity::net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    std::vector<unsigned> released;
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    std::vector<eternity::net::Packet> notices;
    void send_nearby(const eternity::net::Packet& p) override {notices.push_back(p);}
    void send_cell_view(const eternity::net::Packet& p) override {notices.push_back(p);}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected scene reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t id) noexcept override {released.push_back(id);}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target stop");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected grid");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct World:MobTargetingNpcDeathWorld {
    // Explicit projections for currently unexposed lifecycle fields, NOT a
    // production shadow Actor owner. All four references bind actual Actor metadata.
    Scene* scene=nullptr;
    std::vector<char> steps;
    std::int32_t& death_penalty(Actor& a) noexcept override {return a.death_metadata().penalty;}
    std::uint32_t& soul_state(Actor& a) noexcept override {return a.death_metadata().soul;}
    std::uint32_t& rebirth_deadline(Actor& a) noexcept override {return a.death_metadata().deadline;}
    std::uint32_t& rebirth_code(Actor& a) noexcept override {return a.death_metadata().code;}
    PartyRegistryParty* party(const Actor&) const noexcept override {return nullptr;}
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();return s;}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_game_log(const eternity::net::Packet& p) override {check(p.size()==110 && p[0]==1 && p[1]==3 && p[74]==1 && p[75]==0 && p[78]=='G' && p[86]==0 && p[107]==0,"Actual NPC catalog/log mapping differs");steps.push_back('L');}
    void pvp_leave_by_death(Actor& a) override {check(a.attack_state().mode==1 && a.death_metadata().soul==0 && a.death_metadata().penalty==5 && a.resources().hp==120,"NPC entry publication/resource preservation differs");steps.push_back('P');}
    void reset_damage(Actor& a) override {
        check(!a.effects().contains(33) && a.effects().contains(34) && a.status().casting==0 && a.resources().hp==100,"Actual effect/resource cleanup not composed");
        check(a.damage_ledger().entries().size()==1 && a.damage_ledger().entries()[0].amount==10,"Actual ledger not retained until reset boundary");
        check(scene && scene->effect_database_packets.size()==1 && scene->released==std::vector<unsigned>{33} && a.replica().snapshot().size()==1,"DB/replica/release must precede ledger reset");
        const auto* owner=&a.damage_ledger();const auto hp=a.resources();const auto life=a.attack_state().mode;
        a.reset_damage_ledger();check(&a.damage_ledger()==owner && a.damage_ledger().entries().empty() && !a.damage_ledger().maximum(),"Same ledger reset failed");
        check(a.resources().hp==hp.hp && a.resources().mp==hp.mp && a.resources().sp==hp.sp && a.attack_state().mode==life && a.effects().contains(34),"Silent reset changed owners");steps.push_back('R');
    }
    bool fail_corpse=false;unsigned corpses=0;
    void enter_dead(Actor& a) override {check(a.damage_ledger().entries().empty() && a.actions().pending_move==77,"Reset must precede corpse and pending-move clear");steps.push_back('D');++corpses;if(fail_corpse) throw std::runtime_error("Injected corpse failure");}
    void send_death_view(Actor&,const eternity::net::Packet&,std::uint32_t) override {throw std::runtime_error("Death view must not run after reset failure");}
    void lock_party(PartyRegistryParty*) override {throw std::runtime_error("Unexpected party lock");}
    void unlock_party(PartyRegistryParty*) noexcept override {std::terminate();}
    void send_party(PartyRegistryParty*,Actor&,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected party delivery");}
};
NpcDefinitions definitions() {
    eternity::net::Packet b;
    for(unsigned group=0;group<13;++group) {
        put(b,group==2?1:0);if(group!=2) continue;
        put(b,3,1);put(b,1,2);for(unsigned i=0;i<4;++i) put(b,0);
        const std::string name="Guardian";put(b,9);b.insert(b.end(),name.begin(),name.end());b.push_back(0);
        put(b,0);put(b,0);put(b,0);
    }
    return NpcDefinitions::decode(b);
}
struct RebirthWorld final:MobTargetingLifecycleWorld {
    Scene& scene;explicit RebirthWorld(Scene& s):scene(s){}unsigned moved=0,logs=0;
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();s.map=7;return s;}
    std::uint16_t instance(const Actor&) const override {return 4;}
    std::uint8_t presentation_motion(const Actor& a) const override {return a.presentation_motion();}
    void presentation_motion(Actor& a,std::uint8_t value) override {a.presentation_motion(value);}
    void replace_resources(Actor& a,PlayerResources p) override {a.replace_resources_raw(p);}
    std::optional<std::uint32_t> find_dead(Actor& a,std::uint32_t id) override {check(id==a.id() && a.resources().hp==100 && a.death_metadata().deadline==200123,"Rebirth lost actual metadata/resources");return 17;}
    void leave_dead(Actor&,std::uint32_t id) override {check(id==17,"Wrong corpse identity");}
    bool move_user(Actor&,std::uint16_t instance,const UserPosition&) override {check(instance==4,"Wrong instance");++moved;return true;}
    bool map_present(std::int32_t) override {throw std::runtime_error("Unexpected cross-map lookup");}
    void zone_leave_user_move(Actor&,std::int32_t,const UserPosition&) override {throw std::runtime_error("Unexpected cross-map move");}
    void send_cell_view(Actor&,const eternity::net::Packet& p) override {scene.send_cell_view(p);}
    void send_recover_change(Actor& a,PlayerResources p) override {a.send_recover_change(p);}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_game_log(const eternity::net::Packet& p) override {check(p[0]==4&&p[1]==3,"Wrong Rebirth log");++logs;}
};
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original metadata fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned count=0;
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates attacks;
    for(std::string line;std::getline(input,line);) {
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned index;std::uint32_t raw;ActorDeathMetadata inherited,ctor,reset;
        auto read=[&](ActorDeathMetadata& m){row>>raw;m.penalty=std::bit_cast<std::int32_t>(raw);row>>m.soul>>m.deadline>>m.code;};row>>index;read(inherited);read(ctor);read(reset);check(!row.fail(),"Malformed original metadata");
        rebirth_test::Scene scene;Actor a(9,{}, {77,88,99},{},scene,{items,enchant,attacks});
        check(!a.death_metadata_bound(),"Unspecified inherited bytes silently invented");bool rejected=false;try{a.death_metadata();}catch(const std::logic_error&){rejected=true;}check(rejected,"Missing metadata provider hidden");
        a.attack_state().mode=7;a.attack_state().motion=0xa5;a.presentation_motion(37);a.initialize_death_metadata(inherited);
        auto& m=a.death_metadata();check(m.penalty==ctor.penalty&&m.soul==ctor.soul&&m.deadline==ctor.deadline&&m.code==ctor.code,"Constructor projection differs");
        auto* owner=&m;m=inherited;a.reset_death_metadata();check(&a.death_metadata()==owner&&m.penalty==reset.penalty&&m.soul==reset.soul&&m.deadline==reset.deadline&&m.code==reset.code,"ResetCharacter metadata projection differs");
        rejected=false;try{a.initialize_death_metadata(inherited);}catch(const std::logic_error&){rejected=true;}check(rejected,"Repeated construction silently overwrote live state");
        check(a.attack_state().mode==7&&a.attack_state().motion==0xa5&&a.presentation_motion()==37&&a.resources().hp==77&&scene.packets.empty()&&scene.resets==0&&scene.ticks==0,"Metadata API mutated other owners or called ports");++count;
    }
    check(count==216,"Incomplete original metadata coverage");auto npcs=definitions();
    for(bool fail:{false,true}) {
        Scene scene;SkillDefinition normal;normal.id=55;normal.level=1;normal.effect=4;normal.detail=82;normal.abilities[0]={1,50};SkillDefinition keep;keep.id=66;keep.level=1;keep.preserve_on_death=true;
        EffectStatus s;s.skills.equipment.base.maximum={100,200,300};Actor a(9001,s,{130,20,30},{{55,1,6},{66,1,6}},scene,{items,enchant,attacks});
        a.effects().append(33,{{55,1,6},0,6000,false},normal);a.effects().append(34,{{66,1,6},0,6000,false},keep);a.initializing(true);a.effects().initialize(a.status(),a);a.initializing(false);
        a.damage(2,99,10,SkillDefinitions{});a.actions().pending_move=77;a.status().casting=7;a.initialize_death_metadata({17,55,999,99});a.death_metadata().deadline=88;auto* metadata=&a.death_metadata();World w;w.scene=&scene;w.fail_corpse=fail;bool threw=false;
        try {mob_targeting_death_by_npc(a,3,1,npcs,w);}catch(const std::runtime_error& e){threw=std::string(e.what())=="Injected corpse failure";}
        check(threw==fail && w.steps==std::vector<char>({'L','P','R','D'}) && w.corpses==1,"Actual death ordering differs");
        check(a.damage_ledger().entries().empty() && a.attack_state().mode==1 && a.resources().hp==100 && a.effects().contains(34) && !a.effects().contains(33),"Actual owner effects/resources/ledger publication differs");
        check(a.actions().pending_move==(fail?77U:0U) && a.death_metadata().deadline==(fail?88U:200123U) && a.death_metadata().code==(fail?99U:0U),"Corpse failure ran later stages or deadline differs");
        check(scene.effect_database_packets.size()==1 && scene.effect_database_packets[0][0]==2 && scene.effect_database_packets[0][1]==9 && scene.released==std::vector<unsigned>{33} && a.replica().snapshot().size()==1,"Real effect removal/DB/release not composed");
        mob_targeting_death_by_npc(a,3,1,npcs,w);check(w.corpses==1,"Dead reentry repeated reset/corpse");
        check(&a.death_metadata()==metadata,"Bridge replaced actual metadata owner");
        if(!fail) {a.status().in_zone=true;a.attack_state().recovery=-7;a.attack_state().motion=0xa5;a.presentation_motion(37);RebirthWorld rebirth(scene);
            mob_targeting_rebirth(a,{4,5,6},0,7,true,rebirth);
            check(a.attack_state().mode==0&&a.attack_state().recovery==1&&a.attack_state().motion==0xa5&&a.presentation_motion()==0&&rebirth.moved==1&&rebirth.logs==1,"Actual Rebirth tail differs");
            check(metadata->penalty==5&&metadata->soul==0&&metadata->deadline==200123&&metadata->code==0,"Rebirth cleared preserved original metadata");
            check(scene.notices.size()==2&&scene.notices[0][0]==0x51&&scene.notices[1]==eternity::net::Packet({5,5,0x29,0x23,0,0,100,0,200,0,44,1}),"Actual cell/RecoverChange owner composition differs");
        }
    }
    std::cout<<count<<" original constructor/reset metadata projections matched; actual metadata NPCdeath/Rebirth composition passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
