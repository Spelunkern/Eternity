#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include <stdexcept>
namespace eternity::game {
void MobAppliedSkillsRuntime::bind(MobRuntime& m,MobPoolServices& lifetime){
    if(m.applied_skills_ || m.lifetime_services_)throw std::invalid_argument("Mob already has retained runtime services");
    auto owner=std::make_unique<MobAppliedSkillsCatalog>(catalog_,pool_,m,ids_,services_.ports(m));
    m.applied_skills_=std::move(owner);m.lifetime_services_=&lifetime;m.move_guard_.bound=true;
}
void release_mob_applied_skills_runtime_ai(MobRuntime& m,MobPoolServices& s){
    s.release_compiled_ai(m);
    if(m.state_stack){s.release_stack_ai(m);m.state_stack.reset();}
    if(m.lua_ai!=0){m.lua_ai=0;m.only_lua_attack=0;}
    if(m.lua_state){s.close_lua(m.lua_state);m.lua_state=nullptr;}
}
void reset_mob_applied_skills_runtime(MobRuntime& m,MobAppliedSkillsRuntimeResetServices& services){
    auto& owner=m.applied_skills(); // Reject an unbound legacy fixture mob.
    if(!m.definition)throw std::invalid_argument("Mob reset requires its live definition");
    const auto& d=*m.definition;
    m.attack_disabled.fill(0);m.attack.fill({});m.additions.fill({});m.move_speed=2;m.attack_speed=3;m.current_attack=0;m.attack_times.fill(0);
    m.hp=d.hp;m.sp=d.sp;m.mp=d.mp;m.dexterity=d.dexterity;m.wisdom=d.wisdom;m.luck=d.luck;m.attribute=d.attribute;m.stun=0;m.stop=0;m.sleep=0;
    owner.clear();
    if(m.zone){
        std::array<std::uint8_t,10> packet{5,3};
        for(unsigned i=0;i<4;++i){packet[2+i]=static_cast<std::uint8_t>(m.id>>(8*i));packet[6+i]=static_cast<std::uint8_t>(m.hp>>(8*i));}
        services.send_hp(m,packet);
    }
    services.send_speed(m);rebuild_mob_attack(m);
}
}
