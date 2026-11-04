#include "mob_drops_actor_rebirth_support.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity::game;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original raw-store fixture");std::ifstream input(argv[1]);check(input.good(),"Missing original raw-store fixture");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,attack,motion,final_motion,final_attack;std::uint32_t hp,mp,sp,life,final_life;PlayerResources expected;
        row>>scenario>>hp>>mp>>sp>>life>>attack>>motion>>expected.hp>>expected.mp>>expected.sp>>final_life>>final_motion>>final_attack;check(!row.fail(),"Malformed original store fixture");
        rebirth_test::Scene scene;EffectStatus status;status.skills.equipment.base.maximum={1,2,3};status.in_zone=true;
        Actor actor(9,status,{0xaabbccdd,0x11223344,0x55667788},{},scene,{items,enchant,mobs});actor.attack_state().mode=std::bit_cast<std::int32_t>(life);actor.attack_state().motion=static_cast<std::uint8_t>(attack);actor.attack_state().recovery=-7;actor.presentation_motion(static_cast<std::uint8_t>(motion));
        const auto* owned=&actor.resources();actor.replace_resources_raw({hp,mp,sp});actor.presentation_motion(0);
        check(rebirth_test::same(actor.resources(),expected) && &actor.resources()==owned,"Exact single-owner raw resource store differs");
        check(std::bit_cast<std::uint32_t>(actor.attack_state().mode)==final_life && actor.attack_state().motion==final_attack && actor.presentation_motion()==final_motion && actor.attack_state().recovery==-7,"Presentation store aliased life/attack/recovery");
        check(scene.packets.empty() && scene.resets==0 && scene.ticks==0 && scene.party_queries==0 && rebirth_test::same(actor.status().skills.equipment.base.maximum,{1,2,3}),"Raw setters clamped or invoked sinks");++cases;
    }
    check(cases==512,"Incomplete raw value coverage");
    rebirth_test::Scene scene;EffectStatus status;status.skills.equipment.base.maximum={100,100,100};
    Actor actor(9,status,{100,20,30},{},scene,{items,enchant,mobs});check(!actor.damage(3,77,3,SkillDefinitions{}),"Damage setup failed");
    const auto damage=actor.damage_ledger().maximum();check(damage && damage->amount==3,"Damage setup did not record credit");
    scene.packets.clear();const auto resets=scene.resets;const auto queries=scene.party_queries;actor.replace_resources_raw({100,0x80000000U,0xffffffffU});
    check(actor.damage_ledger().maximum()==damage && scene.packets.empty() && scene.resets==resets && scene.party_queries==queries,"Raw maximum HP reset damage or emitted healing");
    actor.attack_state().motion=0xa5;actor.presentation_motion(37);actor.attack_state().mode=2;actor.status().current_target_type=3;
    scene.on_tick=[&] {check(actor.presentation_motion()==0 && actor.attack_state().motion==0xa5 && actor.attack_state().mode==2 && actor.status().current_target_type==0,"EnterZone presentation publication before clock differs");};
    unsigned auth=0;actor.enter_zone_lifecycle({1,2,3},7,[&]{++auth;});
    check(actor.presentation_motion()==0 && actor.attack_state().motion==0xa5 && actor.attack_state().mode==2 && scene.ticks==2 && auth==1,"EnterZone changed attack motion/life");
    check(scene.packets.empty() && actor.damage_ledger().maximum()==damage && actor.resources().mp==0x80000000U,"Zone initialization invoked resource replacement side effects");
    std::cout<<cases<<" original Rebirth raw-store/presentation slices matched actual Actor owner; damage/reference preservation and EnterZone byte separation passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
