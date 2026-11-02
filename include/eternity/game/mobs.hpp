#pragma once
#include "eternity/game/stats.hpp"
#include "eternity/game/pathfinding.hpp"
#include "eternity/game/random.hpp"
#include <functional>

namespace eternity::game {
class ZoneMobs;
// Projection consumed by CMob::Init/SetAttack and respawn timers. Mob skills, loot and complete AI
// definitions remain separate owners; this does not substitute their loaders.
struct MobRuntimeTemplate {
    std::uint16_t id=0,level=0,sp=0,mp=0,dexterity=0,wisdom=0,luck=0,physical_defense=0,magic_defense=0;
    std::uint32_t hp=0,shield_defense=0,respawn_time=5000;
    std::uint8_t ai=0,attribute=0,respawn_selector=0;
};
class MobRuntimeTemplates {
public:
    static MobRuntimeTemplates load(const storage::ResultSet&);
    const MobRuntimeTemplate* find(std::uint32_t id) const noexcept;
private:
    std::map<std::uint16_t,MobRuntimeTemplate> records_;
};
struct MobRespawn {
    std::int32_t type=0,count=0,current=0;std::uint32_t next=0;
    const MobRuntimeTemplate* definition=nullptr;
    bool obelisk=false,both=false;
    void* obelisk_source=nullptr;void* both_source=nullptr;
};
struct MobRuntime {
    std::uint32_t id=0;const MobRuntimeTemplate* definition=nullptr;std::uint16_t type=0;
    ZoneMobs* zone=nullptr;std::optional<CellCoordinates> cell;UserPosition position,respawn_position,exit_position,selected_position;
    std::uint32_t hp=0;std::uint16_t sp=0,mp=0;std::int32_t dexterity=0,wisdom=0,luck=0,attribute=0;
    // CObject stack FSM: inactive uses status; active/empty means zero.
    // State handlers are owned by the actual AI provider. Init retains this stack.
    std::optional<std::vector<std::int32_t>> state_stack;
    std::int32_t status=0,target_kind=0;std::uint32_t target_id=0;
    std::array<std::int32_t,3> attack_disabled{};std::array<AttackValues,3> attack{};std::array<AttackAdd,3> additions{};
    std::int32_t move_speed=2,attack_speed=3,current_attack=0;std::array<std::uint32_t,3> attack_times{};
    bool use_pais=false,attacked=false,in_battle=false,attack_at_moment=false,check_attack_count=false,recover_enabled=false;
    std::int32_t battle_type=0,attacked_count=0;std::uint32_t recover_time=5000;std::array<float,4> multipliers{1,1,1,1};std::array<bool,4> attack_enabled{};
    std::uint32_t next_recover=0,next_help=0,next_attack=0,next_chase=0,next_move=0,next_target=0,next_new_target=0;
    bool death_remove=false;std::uint32_t death_time=0;UserPosition previous_position,move_direction;std::uint32_t previous_move_time=0;float move_range=0;
    MobRespawn* respawn=nullptr;std::int32_t stun=0,stop=0,sleep=0;AStarPath path;ChaseTrail trail;
    std::array<std::uint8_t,32> previous_attack_user{},previous_attack_skill{};
    std::uint32_t lock_on_tick=0,vision_fixed_user=0;std::int32_t unbeatable=0;
    // Init deliberately retains these fields. The actual AI/Lua providers own
    // their state/resources and may update this live object during callbacks.
    std::int32_t lua_ai=0,only_lua_attack=0,hold_position=0,move_mode=0;
    std::uint32_t combat_timeout=0,retarget_time=0,lua_attacked_count=0,lua_order_time=0,function_type=0,current_time=0;
    UserPosition move_target;
    ServerRandom drop_random;void* lua_state=nullptr;
private:
    friend class ZoneMobs;friend class MobPool;
    // Host bookkeeping for intrusive membership. MapReleaseObject retains the
    // public zone/cell fields after unlinking, so those fields are not ownership.
    ZoneMobs* membership_owner_=nullptr;
};
struct MobInitPorts {
    RandomSource& random;
    std::function<std::uint32_t()> tick;
    std::function<bool(MobRuntime&,std::uint8_t ai)> compiled_ai;
    std::function<void(MobRuntime&)> lua_ai;
};
// Regular (non-obelisk/non-principal) corpse accounting, UpdateMobDeath 00421d02.
void account_regular_mob_corpse(MobRespawn&,const MobRuntimeTemplate&,std::uint32_t now,RandomSource&);
// ps_game.exe CMob::SetAttack 004902f0, InitAttack 00490290, Init 00490040.
void rebuild_mob_attack(MobRuntime&);
void reset_mob_attack(MobRuntime&);
void initialize_mob(MobRuntime&,std::uint32_t type,MobRespawn*,MobInitPorts&);
}
