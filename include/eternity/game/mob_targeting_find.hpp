#pragma once
#include <cstdint>
#include <functional>
namespace eternity::game {
// ps_game.exe CMob::Targeting 0049fcf0 / FindTarget 0049fd20.
struct MobTargetingFindSource {
    std::int32_t zone_user_count=0,cells_x=0,cells_z=0,cell_x=0,cell_z=0;
    float x=0,z=0;
    std::uint8_t ai=0,radius=0;
};
struct MobTargetingFindUser {
    std::uintptr_t token=0; // Required nonzero stable actor identity, not a user ID.
    std::int32_t life_status=0;
    std::uint8_t shape_type=0,hp_ability=0;
    bool attackable=false,visible=false;
    float x=0,z=0;
};
struct MobTargetingFindServices {
    virtual ~MobTargetingFindServices()=default;
    // Required synchronous, lock-held live user-list traversal. Stop immediately
    // when visitor returns false; release the lock before this method returns.
    // Do not retain visitor or restore the shared cursor after nested calls.
    virtual void visit_cell_users(std::int32_t x,std::int32_t z,
        const std::function<bool(const MobTargetingFindUser&)>& visitor)=0;
    // SetTarget(source, selected actor, true), while its cell lock is still held.
    virtual void set_target_with_help(std::uintptr_t token)=0;
};
// Returns the selected actor even if SetTarget rejects its state/vision gate.
// Country, target distance ordering, mob status and fixed vision do not filter
// this search. Zero means no actor found; callers supply valid cell projections.
std::uintptr_t mob_find_target(MobTargetingFindSource&,MobTargetingFindServices&);
bool mob_targeting(MobTargetingFindSource&,MobTargetingFindServices&);
}
