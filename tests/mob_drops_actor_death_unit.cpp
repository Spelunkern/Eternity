#include "mob_drops_actor_rebirth_support.hpp"
#include "eternity/game/mob_targeting_lifecycle_death.hpp"
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
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected proximity delivery");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected cell delivery");}
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
    // production shadow Actor owner. Fixture-only inputs, not production metadata ownership.
    Scene* scene=nullptr;
    std::int32_t penalty=17;std::uint32_t soul=55,deadline=88,code=99;std::vector<char> steps;
    std::int32_t& death_penalty(Actor&) noexcept override {return penalty;}
    std::uint32_t& soul_state(Actor&) noexcept override {return soul;}
    std::uint32_t& rebirth_deadline(Actor&) noexcept override {return deadline;}
    std::uint32_t& rebirth_code(Actor&) noexcept override {return code;}
    PartyRegistryParty* party(const Actor&) const noexcept override {return nullptr;}
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();return s;}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_game_log(const eternity::net::Packet& p) override {check(p.size()==110 && p[0]==1 && p[1]==3 && p[74]==1 && p[75]==0 && p[78]=='G' && p[86]==0 && p[107]==0,"Actual NPC catalog/log mapping differs");steps.push_back('L');}
    void pvp_leave_by_death(Actor& a) override {check(a.attack_state().mode==1 && soul==0 && penalty==5 && a.resources().hp==120,"NPC entry publication/resource preservation differs");steps.push_back('P');}
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
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original reset fixture");std::ifstream input(argv[1]);check(input.good(),"Missing reset fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates attacks;unsigned count=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned before,after,reclaimed,repeat;std::string order,locks;row>>before>>after>>reclaimed>>order>>repeat>>locks;check(!row.fail(),"Malformed original reset fixture");
        std::string expected_order,expected_locks="LD";for(unsigned i=0;i<before;++i){if(i)expected_order+=',';expected_order+=std::to_string(i);expected_locks+=",LH,UH";}expected_locks+=",UD,LD,UD";
        check(after==0 && repeat==0 && reclaimed==before && order==(before?expected_order:"-") && locks==expected_locks,"Original unlink/reclaim/lock trace differs");
        rebirth_test::Scene scene;EffectStatus s;s.skills.equipment.base.maximum={100000,200,300};Actor a(9001,s,{100000,20,30},{},scene,{items,enchant,attacks});
        for(unsigned i=0;i<before;++i)a.damage(2,i,1,SkillDefinitions{});
        a.attack_state().mode=7;a.attack_state().motion=0xa5;a.presentation_motion(37);a.actions().pending_move=77;
        const auto hp=a.resources();const auto packets=scene.packets;const auto resets=scene.resets,ticks=scene.ticks,queries=scene.party_queries;const auto* owner=&a.damage_ledger();
        check(a.damage_ledger().entries().size()==before,"Actual damage seeding failed");a.reset_damage_ledger();a.reset_damage_ledger();
        check(a.damage_ledger().entries().size()==after && !a.damage_ledger().maximum() && owner==&a.damage_ledger(),"Same ledger reset differs");
        check(rebirth_test::same(hp,a.resources()) && a.attack_state().mode==7 && a.attack_state().motion==0xa5 && a.presentation_motion()==37 && a.actions().pending_move==77,"Reset mutated state");
        check(scene.packets==packets && scene.resets==resets && scene.ticks==ticks && scene.party_queries==queries,"Silent reset invoked scene");
        // Reuse the SAME ledger after reset, rather than replacing its owner.
        a.attack_state().mode=0;a.damage(2,999,1,SkillDefinitions{});check(a.damage_ledger().entries().size()==1 && a.damage_ledger().maximum()->source==999,"Ledger reuse retained old credit");++count;
    }
    check(count==65,"Incomplete reset coverage");auto npcs=definitions();
    for(bool fail:{false,true}) {
        Scene scene;SkillDefinition normal;normal.id=55;normal.level=1;normal.effect=4;normal.detail=82;normal.abilities[0]={1,50};SkillDefinition keep;keep.id=66;keep.level=1;keep.preserve_on_death=true;
        EffectStatus s;s.skills.equipment.base.maximum={100,200,300};Actor a(9001,s,{130,20,30},{{55,1,6},{66,1,6}},scene,{items,enchant,attacks});
        a.effects().append(33,{{55,1,6},0,6000,false},normal);a.effects().append(34,{{66,1,6},0,6000,false},keep);a.initializing(true);a.effects().initialize(a.status(),a);a.initializing(false);
        a.damage(2,99,10,SkillDefinitions{});a.actions().pending_move=77;a.status().casting=7;World w;w.scene=&scene;w.fail_corpse=fail;bool threw=false;
        try {mob_targeting_death_by_npc(a,3,1,npcs,w);}catch(const std::runtime_error& e){threw=std::string(e.what())=="Injected corpse failure";}
        check(threw==fail && w.steps==std::vector<char>({'L','P','R','D'}) && w.corpses==1,"Actual death ordering differs");
        check(a.damage_ledger().entries().empty() && a.attack_state().mode==1 && a.resources().hp==100 && a.effects().contains(34) && !a.effects().contains(33),"Actual owner effects/resources/ledger publication differs");
        check(a.actions().pending_move==(fail?77U:0U) && w.deadline==(fail?88U:200123U) && w.code==(fail?99U:0U),"Corpse failure ran later stages or deadline differs");
        check(scene.effect_database_packets.size()==1 && scene.effect_database_packets[0][0]==2 && scene.effect_database_packets[0][1]==9 && scene.released==std::vector<unsigned>{33} && a.replica().snapshot().size()==1,"Real effect removal/DB/release not composed");
        mob_targeting_death_by_npc(a,3,1,npcs,w);check(w.corpses==1,"Dead reentry repeated reset/corpse");
    }
    std::cout<<count<<" complete original reset traces matched same Actor ledger; successful NPC-death bridge and corpse-failure ordering passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
