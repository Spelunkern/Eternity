#include "eternity/game/mob_targeting_help.hpp"
#include <algorithm>
#include <cmath>
namespace eternity::game {
void mob_find_targeting_help(MobTargetingHelpKind kind,MobTargetingHelpSource& m,
                            const MobTargetingHelpUser& u,MobTargetingHelpServices& s) {
    if(m.zone_user_count<=0 || m.country==u.country || u.life_status==1 || u.hp_ability!=0 || !u.attackable || !u.visible) return;
    // Original valid cell coordinates are nonnegative and below dimensions.
    // Use wide bounds to avoid signed overflow on malformed external projections.
    const auto xmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(m.cell_x)-1);
    const auto xmax=std::min<std::int64_t>(static_cast<std::int64_t>(m.cells_x)-1,static_cast<std::int64_t>(m.cell_x)+1);
    const auto zmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(m.cell_z)-1);
    const auto zmax=std::min<std::int64_t>(static_cast<std::int64_t>(m.cells_z)-1,static_cast<std::int64_t>(m.cell_z)+1);
    const auto radius=static_cast<float>(m.radius); // Cached once before traversal.
    for(auto z=zmin;z<=zmax;++z) for(auto x=xmin;x<=xmax;++x) {
        s.visit_cell(static_cast<std::int32_t>(x),static_cast<std::int32_t>(z),[&](const MobTargetingHelpCandidate& c) {
            if(c.status!=0) return;
            if(kind==MobTargetingHelpKind::normal && c.mob_type!=m.mob_type) return;
            if(kind==MobTargetingHelpKind::attack && c.ai!=1 && c.ai!=3) return;
            const float dx=c.x-m.x,dz=c.z-m.z;
            // x87 stores the deltas as floats, keeps squared terms in double
            // precision, calls sqrt(double), then stores the distance as float.
            const auto distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
            if(distance<=radius) s.set_target_without_help();
        });
    }
}
}
