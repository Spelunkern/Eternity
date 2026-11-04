#include "eternity/game/mob_targeting.hpp"
namespace eternity::game {
namespace {
void timers(MobTargetingState& m, bool special, MobTargetingServices& s) {
    for(std::size_t i=0;i<m.attacks.size();++i) {
        if(!m.attacks[i].enabled || !m.attacks[i].has_skill) continue;
        if(m.attacks[i].cooldown_seconds==0 || (!special && i==0)) m.attack_times[i]=0;
        else {
            const auto random=s.random();
            const auto delay=random%(static_cast<std::int32_t>(m.attacks[i].cooldown_seconds)*1000);
            const auto tick=s.tick_ms();
            m.attack_times[i]=static_cast<std::uint32_t>(delay)+tick;
        }
    }
}
void initialize(MobTargetingState& m,const MobTargetingUser& u,MobTargetingServices& s) {
    if(m.ai==2 || m.ai==3 || m.ai==5) s.help(m,u,m.ai);
    timers(m,false,s);
}
void target(MobTargetingState& m,const MobTargetingUser& u) {m.target_type=1;m.target_id=u.id;}
}
void mob_set_target_ex(MobTargetingState& m,const MobTargetingUser& u,MobTargetingServices& s) {
    if(m.vision_fixed_user!=0) return;
    target(m,u);m.combat_timeout=0;
    if(m.effective_status()==0) initialize(m,u,s);
}
void mob_set_target(MobTargetingState& m,const MobTargetingUser& u,bool init,MobTargetingServices& s) {
    const auto status=m.effective_status();
    if(status==100) {
        if(m.vision_fixed_user==0) {target(m,u);timers(m,true,s);}
        return;
    }
    if((status!=0 && (status!=4 || m.move_mode==1)) || u.life_status==1) return;
    if(m.fsm_enabled) {mob_set_target_ex(m,u,s);s.dispatch(m,1,m.target_id);return;}
    if(m.vision_fixed_user!=0) return;
    if(m.effective_status()!=2) {m.status=1;m.next_move_time=0;}
    target(m,u);
    if(init) initialize(m,u,s);
}
}
