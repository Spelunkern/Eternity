#include "eternity/game/mob_targeting_find.hpp"
#include <algorithm>
#include <cmath>
namespace eternity::game {
std::uintptr_t mob_find_target(MobTargetingFindSource& m,MobTargetingFindServices& s) {
    if(m.zone_user_count<=0) return 0;
    const auto xmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(m.cell_x)-1);
    const auto xmax=std::min<std::int64_t>(static_cast<std::int64_t>(m.cells_x)-1,static_cast<std::int64_t>(m.cell_x)+1);
    const auto zmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(m.cell_z)-1);
    const auto zmax=std::min<std::int64_t>(static_cast<std::int64_t>(m.cells_z)-1,static_cast<std::int64_t>(m.cell_z)+1);
    for(auto z=zmin;z<=zmax;++z) for(auto x=xmin;x<=xmax;++x) {
        std::uintptr_t found=0;
        s.visit_cell_users(static_cast<std::int32_t>(x),static_cast<std::int32_t>(z),[&](const MobTargetingFindUser& u) {
            if(u.life_status==1 || u.shape_type!=0 || u.hp_ability!=0 || !u.attackable || !u.visible) return true;
            const float dx=u.x-m.x,dz=u.z-m.z;
            const auto distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
            if(!(distance<=static_cast<float>(m.radius))) return true;
            found=u.token;s.set_target_with_help(found);return false;
        });
        if(found) return found;
    }
    return 0;
}
bool mob_targeting(MobTargetingFindSource& m,MobTargetingFindServices& s) {
    return (m.ai==1 || m.ai==3) && mob_find_target(m,s)!=0;
}
}
