#pragma once
#include <cstdint>
#include <functional>
namespace eternity::game {
// ps_game.exe FindHelpNormal 0049ff70 / Normal2 004a0200 / Attack 004a0470.
enum class MobTargetingHelpKind { normal=2, attack=3, normal2=5 };
struct MobTargetingHelpSource {
    std::int32_t zone_user_count=0, cells_x=0,cells_z=0,cell_x=0,cell_z=0,country=0;
    float x=0,z=0;
    std::uint16_t mob_type=0;
    std::uint8_t radius=0;
};
struct MobTargetingHelpUser {
    std::uint8_t country=0,hp_ability=0;
    std::int32_t life_status=0;
    bool attackable=false,visible=false;
};
struct MobTargetingHelpCandidate {
    std::int32_t status=0; // Effective GetStatus: top scene, empty => 0, or raw status.
    std::uint16_t mob_type=0;
    std::uint8_t ai=0;
    float x=0,z=0;
};
struct MobTargetingHelpServices {
    virtual ~MobTargetingHelpServices()=default;
    // Required world owner: hold this cell's mob lock throughout traversal and
    // callbacks; advance its live cursor after callbacks, preserving mutations.
    // Snapshot each candidate's current fields immediately before visiting it.
    // Invoke visitors synchronously; do not retain the visitor after returning.
    virtual void visit_cell(std::int32_t x,std::int32_t z,
        const std::function<void(const MobTargetingHelpCandidate&)>& visitor)=0;
    // SetTarget(candidate, original user, false), on the current live cursor.
    // The provider owns identity/user lookup and may mutate the live source.
    virtual void set_target_without_help()=0;
};
void mob_find_targeting_help(MobTargetingHelpKind,MobTargetingHelpSource&,
                            const MobTargetingHelpUser&,MobTargetingHelpServices&);
}
