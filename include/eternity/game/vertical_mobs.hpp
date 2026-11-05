#pragma once
#include "eternity/game/mob_primary_zones.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include "eternity/game/mob_ai_states_configured.hpp"
#include "eternity/game/mob_targeting_find.hpp"
#include "eternity/game/mob_ai_states_idle.hpp"
#include "eternity/game/mob_ai_states_combat.hpp"
#include "eternity/game/mob_combat.hpp"
#include "eternity/game/user_priority_shared_cell.hpp"
#include "eternity/game/user_skill_runtime.hpp"

namespace eternity::game {
class ZoneUsers;
class MobTargetingZone;
class MobTargetingBindingCatalog;
class MobAppliedSkillsRuntimeResetServices;
class Actor;
class ActorScene;
class ActorActionWorld;
class DuelWorld;
struct MobDeathExperience;
class MobDeathWorld;
struct MobLogContext;
class MobLogTransport;
class UserPriorityAreaServices;
class MobTargetingOrdinaryDeathWorld;
struct UserSkillDefinition;
enum class VerticalMobReturn {waiting,completed,needs_path,in_progress};
// CMob::ReturnHome 004a7670 for UpdateIdle's respawn-position destination:
// deadline and near-home completion only. A
// needs_path result leaves the subject untouched for the real path provider.
// Configured state, skill reset, damage heap and callbacks are retained owners.
VerticalMobReturn vertical_mobs_return_near(MobRuntime&,std::uint32_t,
    MobAiConfiguredFsm&,const MobAiVariableCatalog&,MobAiConfiguredServices&,
    MobAppliedSkillsRuntimeResetServices&);
// Extends the near-home branch with the original stopped and direct-line
// stepping and final-waypoint cases, including retained paths, chase trails,
// A* search and blocked-home recovery. Returns in_progress where the original
// ReturnHome returns false after processing a distant subject.
VerticalMobReturn vertical_mobs_return_step(MobRuntime&,std::uint32_t,
    MobAiConfiguredFsm&,const MobAiVariableCatalog&,MobAiConfiguredServices&,
    MobAppliedSkillsRuntimeResetServices&,const MobTargetingBindingCatalog&,const MapDefinition&);
// Read UpdateIdle's live definition fields and ChasePathList head from the
// existing catalog and subject. Both owners must remain stable for the call.
MobAiIdleProjection vertical_mobs_idle_projection(const MobRuntime&,const MobTargetingBindingCatalog&);
// Required per-subject providers; all actor storage must survive its dispatch.
class VerticalMobsWorld {
public:
    virtual ~VerticalMobsWorld() = default;
    virtual MobAiConfiguredFsm& configured_fsm(MobRuntime&) = 0;
    virtual MobAiConfiguredServices& configured_services(MobRuntime&) = 0;
    virtual MobAiStateServices& stack_services(MobRuntime&) = 0;
    virtual void release_failed_stack(MobRuntime&) = 0; // ReleaseFSM and original error log
    virtual void return_home(MobRuntime&,std::uint32_t) = 0;
    // Remaining ordinary-idle targeting/movement body of CMob::UpdateIdle.
    virtual void ordinary_idle(MobRuntime&,std::uint32_t) = 0;
};
// Required existing per-subject AI and Lua owners. No operation has a default.
class VerticalMobsAiStateOwners {
public:
    virtual ~VerticalMobsAiStateOwners()=default;
    virtual MobAiConfiguredFsm& configured_fsm(MobRuntime&)=0;
    virtual MobAiConfiguredServices& configured_services(MobRuntime&)=0;
    virtual MobAiStateServices& stack_services(MobRuntime&)=0;
    virtual void release_failed_stack(MobRuntime&)=0;
};
class VerticalMobsAiOwners:public VerticalMobsAiStateOwners {
public:
    ~VerticalMobsAiOwners() override=default;
    virtual void return_home(MobRuntime&,std::uint32_t)=0;
};
// Retained per-subject owners needed by the actual configured reset and skill
// reset. These lookups must not alter gameplay state and may not be absent.
class VerticalMobsReturnDependencies {
public:
    virtual ~VerticalMobsReturnDependencies()=default;
    virtual const MobAiVariableCatalog& variables(MobRuntime&)=0;
    virtual MobAppliedSkillsRuntimeResetServices& reset_services(MobRuntime&)=0;
};
// Adapts the recovered ReturnHome body to the existing per-subject AI owners.
// All dependencies and the paired zone/map remain retained and serialized.
class VerticalMobsNativeAiOwners final:public VerticalMobsAiOwners {
public:
    VerticalMobsNativeAiOwners(VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&,
        ZoneMobs&,const MobTargetingBindingCatalog&,const MapDefinition&);
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override;
    MobAiConfiguredServices& configured_services(MobRuntime&) override;
    MobAiStateServices& stack_services(MobRuntime&) override;
    void release_failed_stack(MobRuntime&) override;
    void return_home(MobRuntime&,std::uint32_t) override;
private:
    VerticalMobsAiStateOwners& ai_;VerticalMobsReturnDependencies& dependencies_;
    ZoneMobs& zone_;const MobTargetingBindingCatalog& catalog_;const MapDefinition& map_;
};
// Borrows the actual paired zone, user grid, targeting owner, catalog, map,
// executor random stream and AI owners. The caller serializes their lifetime
// with the subject for every dispatch, including synchronous callbacks.
class VerticalMobsActorWorld final:public VerticalMobsWorld {
public:
    VerticalMobsActorWorld(ZoneMobs&,ZoneUsers&,MobTargetingZone&,
        const MobTargetingBindingCatalog&,const MapDefinition&,RandomSource&,VerticalMobsAiOwners&);
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override;
    MobAiConfiguredServices& configured_services(MobRuntime&) override;
    MobAiStateServices& stack_services(MobRuntime&) override;
    void release_failed_stack(MobRuntime&) override;
    void return_home(MobRuntime&,std::uint32_t) override;
    void ordinary_idle(MobRuntime&,std::uint32_t) override;
private:
    ZoneMobs& zone_;ZoneUsers& users_;MobTargetingZone& targeting_;
    const MobTargetingBindingCatalog& catalog_;const MapDefinition& map_;
    RandomSource& random_;VerticalMobsAiOwners& ai_;
};
// Stable per-zone idle composition for a host-owned mob phase. The host must
// pin each subject and retain/serialize the zone and all referenced owners.
class VerticalMobsNativeZoneIdle final:public VerticalMobsWorld {
public:
    VerticalMobsNativeZoneIdle(RuntimeZone&,MicrosoftRandom&,MobAiStateServices&,
        const MobTargetingBindingCatalog&,VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
    ~VerticalMobsNativeZoneIdle() override;
    VerticalMobsNativeZoneIdle(const VerticalMobsNativeZoneIdle&)=delete;
    VerticalMobsNativeZoneIdle& operator=(const VerticalMobsNativeZoneIdle&)=delete;
    MobAiConfiguredFsm& configured_fsm(MobRuntime&) override;
    MobAiConfiguredServices& configured_services(MobRuntime&) override;
    MobAiStateServices& stack_services(MobRuntime&) override;
    void release_failed_stack(MobRuntime&) override;
    void return_home(MobRuntime&,std::uint32_t) override;
    void ordinary_idle(MobRuntime&,std::uint32_t) override;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// ps_game.exe CMob::UpdateIdle 004a7e60: configured AI, Lua stack and
// ordinary idle routing. Services must reference the subject's retained owners.
void vertical_mobs_idle(MobRuntime&,std::uint32_t,VerticalMobsWorld&);
// CZone::UpdateMobAttack 00421990 traverses the live global mob list and calls
// Compose the recovered Chase owners against the actual paired map/grids and
// live FindTarget projection. All owners remain retained and serialized.
bool vertical_mobs_native_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t,ZoneMobs&,ZoneUsers&,
    const MapDefinition&,const MobTargetingBindingCatalog&,MobTargetingFindSource&,MobTargetingFindServices&,
    VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
// CMob::UpdateAttack 004a7db0 with the supplied tick. The provider must execute
// the real attack update and retain the subject through synchronous callbacks.
class VerticalMobsAttackServices {
public:
    virtual ~VerticalMobsAttackServices()=default;
    virtual void update_attack(MobRuntime&,std::uint32_t)=0;
};
// Compose the recovered Chase owners against the actual paired map/grids and
// live FindTarget projection. All owners remain retained and serialized.
bool vertical_mobs_native_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t,ZoneMobs&,ZoneUsers&,
    const MapDefinition&,const MobTargetingBindingCatalog&,MobTargetingFindSource&,MobTargetingFindServices&,
    VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
// CMob::UpdateAttack 004a7db0. Combat uses the retained per-subject combat
// service; return-home uses the same native AI owner as the idle pass.
void vertical_mobs_attack(MobRuntime&,std::uint32_t,MobAiCombatServices&,VerticalMobsAiOwners&);
// CMob::EnableAttackRange 004a19c0: strict planar range using the live mob
// size and x87-compatible intermediate rounding. The Actor overload requires
// both subjects in the same retained paired zone grids.
bool vertical_mobs_attack_range(const MobRuntime&,UserPosition,std::int32_t,const MobTargetingBindingCatalog&);
bool vertical_mobs_attack_range(const MobRuntime&,const Actor&,std::int32_t,ZoneMobs&,ZoneUsers&,
    const MobTargetingBindingCatalog&);
enum class VerticalMobChasePrefix {waiting,reached,stopped,needs_movement};
// CMob::Chase 004a6de0 through the deadline, near-target path reset and
// immobilization gates. needs_movement must invoke the real remaining body.
VerticalMobChasePrefix vertical_mobs_chase_prefix(MobRuntime&,std::uint32_t,UserPosition,std::int32_t,
    const MobTargetingBindingCatalog&);
class VerticalMobsChaseMovement {
public:
    virtual ~VerticalMobsChaseMovement()=default;
    virtual bool continue_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t range)=0;
};
enum class VerticalMobChaseLeash {completed,needs_movement};
// Chase's post-immobilization 45-unit leash; the caller has already applied
// the prefix gates and retains the target snapshot and real AI owners.
VerticalMobChaseLeash vertical_mobs_chase_leash(MobRuntime&,UserPosition,VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
// Leash continuation following the Chase prefix. Uses the existing configured
// FSM/reset owners and requires the actual remaining movement provider.
class VerticalMobsChaseLeash final:public VerticalMobsChaseMovement {
public:
    VerticalMobsChaseLeash(ZoneMobs&,ZoneUsers&,VerticalMobsAiStateOwners&,
        VerticalMobsReturnDependencies&,VerticalMobsChaseMovement&);
    bool continue_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t) override;
private:
    ZoneMobs& mobs_;ZoneUsers& users_;VerticalMobsAiStateOwners& ai_;
    VerticalMobsReturnDependencies& dependencies_;VerticalMobsChaseMovement& movement_;
};
class VerticalMobsChasePathWorld {
public:
    virtual ~VerticalMobsChasePathWorld()=default;
    // Must execute the real waypoint movement using the subject's actual path.
    virtual bool waypoint(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile)=0;
    // Must execute the real blocked-line recovery with the actual map and path.
    virtual bool blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile from,MapTile target,MapTile previous)=0;
};
// Direct-line path creation and retained A* cursor selection. The map must be
// the exact map paired with these grids and survive all callbacks.
class VerticalMobsChasePath final:public VerticalMobsChaseMovement {
public:
    VerticalMobsChasePath(ZoneMobs&,ZoneUsers&,const MapDefinition&,VerticalMobsChasePathWorld&);
    bool continue_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t) override;
private:
    ZoneMobs& mobs_;ZoneUsers& users_;const MapDefinition& map_;VerticalMobsChasePathWorld& world_;
};
// Blocked target-tile recovery within Chase. Delegates clear-target A* search
// and all waypoint movement to the required real path world.
class VerticalMobsChaseBlockedTarget final:public VerticalMobsChasePathWorld {
public:
    VerticalMobsChaseBlockedTarget(const MapDefinition&,ZoneUsers&,const MobTargetingBindingCatalog&,VerticalMobsChasePathWorld&);
    bool waypoint(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile) override;
    bool blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile,MapTile,MapTile) override;
private:
    const MapDefinition& map_;ZoneUsers& users_;const MobTargetingBindingCatalog& catalog_;VerticalMobsChasePathWorld& world_;
};
// Search failure must invoke the real CMob target-reset owner (00490980).
class VerticalMobsChaseSearchWorld {
public:
    virtual ~VerticalMobsChaseSearchWorld()=default;
    virtual void reset_target(MobRuntime&)=0;
};
// Uses the real FindTarget, vision-skill and configured FSM owners.
class VerticalMobsChaseTargetReset final:public VerticalMobsChaseSearchWorld {
public:
    VerticalMobsChaseTargetReset(MobTargetingFindSource&,MobTargetingFindServices&,
        VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
    void reset_target(MobRuntime&) override;
private:
    MobTargetingFindSource& source_;MobTargetingFindServices& targeting_;
    VerticalMobsAiStateOwners& ai_;VerticalMobsReturnDependencies& dependencies_;
};
class VerticalMobsChaseSearch final:public VerticalMobsChasePathWorld {
public:
    VerticalMobsChaseSearch(const MapDefinition&,VerticalMobsChaseSearchWorld&,VerticalMobsChasePathWorld&);
    bool waypoint(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile) override;
    bool blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile,MapTile,MapTile) override;
private:
    const MapDefinition& map_;VerticalMobsChaseSearchWorld& reset_;VerticalMobsChasePathWorld& world_;
};
// Intermediate path nodes use the actual zone movement and chase trail owners.
// Final-node behavior remains a required real continuation.
class VerticalMobsChaseIntermediate final:public VerticalMobsChasePathWorld {
public:
    VerticalMobsChaseIntermediate(ZoneMobs&,const MapDefinition&,const MobTargetingBindingCatalog&,VerticalMobsChasePathWorld&);
    bool waypoint(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile) override;
    bool blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile,MapTile,MapTile) override;
private:
    ZoneMobs& mobs_;const MapDefinition& map_;const MobTargetingBindingCatalog& catalog_;VerticalMobsChasePathWorld& world_;
};
// Final chase cursor: movement uses the retained zone and actual path owner.
class VerticalMobsChaseFinal final:public VerticalMobsChasePathWorld {
public:
    VerticalMobsChaseFinal(ZoneMobs&,const MapDefinition&,const MobTargetingBindingCatalog&);
    bool waypoint(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile) override;
    bool blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile,MapTile,MapTile) override;
private:
    ZoneMobs& mobs_;const MapDefinition& map_;const MobTargetingBindingCatalog& catalog_;
};
// The movement owner must use the live target position after any callbacks;
// the subject, target, paired grids and all owners remain retained/serialized.
bool vertical_mobs_chase(MobRuntime&,std::uint32_t,Actor&,std::int32_t,ZoneMobs&,ZoneUsers&,
    const MobTargetingBindingCatalog&,VerticalMobsChaseMovement&);
class VerticalMobsAttackDependencies {
public:
    virtual ~VerticalMobsAttackDependencies()=default;
    virtual MobAiCombatServices& combat_services(MobRuntime&)=0;
};
// Actual attack/Lua/retarget owners; actor retention is a pure membership-safe
// lookup and must keep the actor and its scene alive throughout callbacks.
class VerticalMobsActorCombatWorld {
public:
    virtual ~VerticalMobsActorCombatWorld()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual std::uint32_t tick()=0;
    virtual bool attack_ai(MobRuntime&,std::uint32_t,Actor&)=0;
    virtual void retarget(MobRuntime&,std::uint32_t)=0;
    virtual void lua_combat(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)=0;
};
struct VerticalMobsRetainedActor {Actor* actor=nullptr;std::shared_ptr<void> lease;};
// CMob::FindAttackUserMinHP 004a0810: scan the subject's actual damage
// ledger and the paired zone's users. retain() must not mutate either owner.
VerticalMobsRetainedActor vertical_mobs_find_attack_user_min_hp(MobRuntime&,ZoneMobs&,ZoneUsers&,
    const MobTargetingBindingCatalog&,VerticalMobsActorCombatWorld&);
// Required remainder of AttackAI after target eligibility and in-battle write.
// It must perform the real attack selection, movement and damage operations.
class VerticalMobsAttackAiContinuation {
public:
    virtual ~VerticalMobsAttackAiContinuation()=default;
    virtual void continue_attack_ai(MobRuntime&,std::uint32_t,Actor&)=0;
};
// First due slot passing the recovered HP gates, or type 6 requiring target
// selection. Type 6 may fall through to earlier slots if no target is found.
class VerticalMobsAttackAiReady {
public:
    virtual ~VerticalMobsAttackAiReady()=default;
    virtual void first_ready(MobRuntime&,std::uint32_t,Actor&,std::size_t slot)=0;
};
class VerticalMobsAttackAiScan final:public VerticalMobsAttackAiContinuation {
public:
    VerticalMobsAttackAiScan(const MobTargetingBindingCatalog& catalog,VerticalMobsAttackAiReady& ready):catalog_(catalog),ready_(ready){}
    void continue_attack_ai(MobRuntime&,std::uint32_t,Actor&) override;
    // Resume immediately below the rejected slot, preserving the original scan.
    void continue_before(MobRuntime&,std::uint32_t,Actor&,std::size_t exclusive_slot);
private:
    const MobTargetingBindingCatalog& catalog_;VerticalMobsAttackAiReady& ready_;
};
class VerticalMobsAttackAiAfterTarget {
public:
    virtual ~VerticalMobsAttackAiAfterTarget()=default;
    // Original user argument remains the original Actor, even after SetTarget.
    virtual void after_target_set(MobRuntime&,std::uint32_t,Actor&,std::size_t slot)=0;
};
class VerticalMobsAttackAiAfterAggro {
public:
    virtual ~VerticalMobsAttackAiAfterAggro()=default;
    virtual void after_aggro(MobRuntime&,std::uint32_t,Actor&,std::size_t slot)=0;
};
struct MobTargetingBindingSkill;
class VerticalMobsAttackAiSkillWorld {
public:
    virtual ~VerticalMobsAttackAiSkillWorld()=default;
    virtual void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&)=0;
    // Actual non-self attack/skill and movement body; never a default success.
    virtual void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t)=0;
};
// Common post-aggro/post-SetTarget branch. Self skills call the real owner and
// reload the live cooldown; other branches remain the mandatory world owner.
class VerticalMobsAttackAiSelfSkill final:public VerticalMobsAttackAiAfterTarget,public VerticalMobsAttackAiAfterAggro {
public:
    VerticalMobsAttackAiSelfSkill(const MobTargetingBindingCatalog& catalog,VerticalMobsAttackAiSkillWorld& world):catalog_(catalog),world_(world){}
    void after_target_set(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
    void after_aggro(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    void dispatch(MobRuntime&,std::uint32_t,Actor&,std::size_t);
    const MobTargetingBindingCatalog& catalog_;VerticalMobsAttackAiSkillWorld& world_;
};
class VerticalMobsStationaryAttackWorld:public VerticalMobsAttackAiSkillWorld {
public:
    // Actual CMob::Attack owner; it may synchronously change current_attack,
    // speed and the retained catalog definition before returning.
    virtual void attack(MobRuntime&,std::uint32_t,Actor&)=0;
};
// CMob::UseSkill 004a48e0 dispatches to the actual skill effect owners.
// These operations must perform their complete effects, not acknowledge a hit.
class VerticalMobsSkillEffects {
public:
    virtual ~VerticalMobsSkillEffects()=default;
    virtual void attack(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
    virtual void heal(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
    virtual void debuff(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
};
void vertical_mobs_use_skill(MobRuntime&,std::uint32_t,Actor&,
    const MobTargetingBindingSkill&,VerticalMobsSkillEffects&);
// CMob::SkillAttack 004a4990 and selected-target 004a49d0. All callbacks
// execute actual combat and transport owners; the death coordinator is real.
class VerticalMobsSkillAttackWorld {
public:
    virtual ~VerticalMobsSkillAttackWorld()=default;
    virtual void send_skill(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
    virtual void attack_user(MobRuntime&,Actor&,const MobTargetingBindingSkill&,bool send_skill)=0;
    virtual bool self_damage(MobRuntime&,std::uint32_t hp,const MobCombatCredit&)=0;
    virtual void range(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
    virtual void range_select(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
};
void vertical_mobs_skill_attack(MobRuntime&,Actor&,const MobTargetingBindingSkill&,
    VerticalMobsSkillAttackWorld&,const MobDeathExperience&,MobDeathWorld&);
// Real non-attack skill paths; no default success or suppressed missing work.
class VerticalMobsSkillOtherEffects {
public:
    virtual ~VerticalMobsSkillOtherEffects()=default;
    virtual void heal(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
    virtual void debuff(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
};
class VerticalMobsBoundSkillEffects final:public VerticalMobsSkillEffects {
public:
    VerticalMobsBoundSkillEffects(VerticalMobsSkillAttackWorld&,const MobDeathExperience&,MobDeathWorld&,VerticalMobsSkillOtherEffects&);
    void attack(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override;
    void heal(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override;
    void debuff(MobRuntime&,Actor&,const MobTargetingBindingSkill&) override;
private:VerticalMobsSkillAttackWorld& attack_;const MobDeathExperience& rules_;MobDeathWorld& death_;VerticalMobsSkillOtherEffects& other_;
};
// Compose the real ordinary attack owner with UseSkill dispatch. All nested
// skill services remain mandatory and must retain mob, actor and definition.
class VerticalMobsSkillDispatch final:public VerticalMobsStationaryAttackWorld {
public:
    VerticalMobsSkillDispatch(VerticalMobsStationaryAttackWorld&,VerticalMobsSkillEffects&);
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:VerticalMobsStationaryAttackWorld& attacks_;VerticalMobsSkillEffects& skills_;
};
// Common CMob::Attack post-hit/miss deadline. Call only after the real target
// work has reached the original tail; reads the live subject/catalog fields.
void vertical_mobs_attack_finish(MobRuntime&,std::uint32_t,const MobTargetingBindingCatalog&);
// Actual CUser::IsAttackBlock 0044c5a0 owner; it may consume a block or RNG.
class VerticalMobsAttackBlock {
public:
    virtual ~VerticalMobsAttackBlock()=default;
    virtual bool blocks(Actor&,std::uint16_t attack_type)=0;
};
// Required live user skill-definition owner for original stSkillInfo+54/+55.
// Lookup must return the referenced record's raw mode and parameter, and fail
// explicitly if the original skill ID/level cannot be resolved.
class VerticalMobsMirrorSkillDefinitions {
public:
    virtual ~VerticalMobsMirrorSkillDefinitions()=default;
    virtual std::array<std::uint8_t,2> mirror_parameters(std::int32_t skill,std::int32_t level)=0;
};
// Compose the real user-skill definition lookup with its exact 160-byte
// retained stSkillInfo source. Both owners and records outlive every call.
// The source has record(const UserSkillDefinition&) -> span<const byte>.
template<class DefinitionOwner,class RecordSource>
class VerticalMobsUserMirrorDefinitions final:public VerticalMobsMirrorSkillDefinitions {
public:
    VerticalMobsUserMirrorDefinitions(DefinitionOwner& definitions,const RecordSource& records)
        :definitions_(definitions),records_(records) {}
    std::array<std::uint8_t,2> mirror_parameters(std::int32_t skill,std::int32_t level) override {
        if(skill<=0 || skill>65535 || level<=0 || level>255)
            throw std::out_of_range("Mob reflection requires a valid user skill ID and level");
        const auto* definition=definitions_.definition(static_cast<std::uint16_t>(skill),static_cast<std::uint8_t>(level));
        if(!definition)throw std::logic_error("Mob reflection requires the live user skill definition");
        const auto record=records_.record(*definition);
        if(record.size()!=160)throw std::logic_error("Mob reflection requires the exact user skill record");
        return {std::to_integer<std::uint8_t>(record[0x54]),std::to_integer<std::uint8_t>(record[0x55])};
    }
private:
    DefinitionOwner& definitions_;const RecordSource& records_;
};
// Attack's reflection check at 004a20f1..004a2140 and CUser helper 0044e080.
bool vertical_mobs_attack_reflected(const MobRuntime&,const Actor&,
    const MobTargetingBindingCatalog&,RandomSource&,VerticalMobsMirrorSkillDefinitions&);
// Required real removal of the actor's exhausted detail-85 effects (00487b20).
class VerticalMobsBlockEffects {
public:
    virtual ~VerticalMobsBlockEffects()=default;
    virtual void remove_attack_blocks(Actor&)=0;
};
class VerticalMobsActorAttackBlock final:public VerticalMobsAttackBlock {
public:
    VerticalMobsActorAttackBlock(RandomSource& random,VerticalMobsBlockEffects& effects):random_(random),effects_(effects){}
    bool blocks(Actor&,std::uint16_t attack_type) override;
private:
    RandomSource& random_;VerticalMobsBlockEffects& effects_;
};
// Null-skill CMob::AttackSuccess 004a23f0. The supplied RNG is the shared
// executor stream and the block owner must execute the real target operation.
bool vertical_mobs_attack_success(MobRuntime&,Actor&,std::uint16_t,RandomSource&,VerticalMobsAttackBlock&);
// CMob critical helper 004a28c0 and its AttackDamage caller's 5..99
// clamp/roll. Uses the actual actor wisdom, mob attack stats and executor RNG.
std::int32_t vertical_mobs_attack_critical_chance(const MobRuntime&,const Actor&,std::size_t kind);
bool vertical_mobs_attack_critical(const MobRuntime&,const Actor&,std::size_t kind,RandomSource&);
// Normal-type AttackDamage 004a2fe9..004a3026: cap the three damage
// components and replace primary damage <= 10 using the shared RNG.
void vertical_mobs_attack_damage_limits(std::array<std::int32_t,3>&,RandomSource&);
// Normal CMob::AttackDamage through its bounds, before conditional mob logging.
// The host must execute the real subsequent mob logging operation when applicable.
bool vertical_mobs_normal_attack_damage(const MobRuntime&,const Actor&,
    const MobTargetingBindingCatalog&,RandomSource&,std::array<std::int32_t,3>&);
// Complete AttackDamage calculation including the conditional SendLogBothMob.
// Context and transport are the retained real map/definition and game-log owners.
bool vertical_mobs_logged_normal_attack_damage(const MobRuntime&,const Actor&,
    const MobTargetingBindingCatalog&,RandomSource&,std::array<std::int32_t,3>&,
    MobLogContext,MobLogTransport&);
// Required actual target-side operations of CMob::Attack 004a2000. Hit-test
// returns its real result; hit performs the real hit/damage/death body.
class VerticalMobsAttackEffects {
public:
    virtual ~VerticalMobsAttackEffects()=default;
    virtual void status_stand(Actor&)=0;
    // Original CZone::PSendRange 00421220: radius 60, target ID is immediate, priority 6.
    virtual void send_miss(MobRuntime&,Actor&,const net::Packet&)=0;
    virtual void hit(MobRuntime&,std::uint32_t,Actor&)=0;
};
// Executes the real target preparation at CMob::Attack 004a20dc..004a20f1.
// The required delegate's hit starts at 004a20f1 (absorption onward); its
// status_stand and send_miss retain their ordinary target-side meanings.
class VerticalMobsPreparedAttackEffects final:public VerticalMobsAttackEffects {
public:
    VerticalMobsPreparedAttackEffects(ActorActionWorld&,DuelWorld&,VerticalMobsAttackEffects&);
    void status_stand(Actor&) override;
    void send_miss(MobRuntime&,Actor&,const net::Packet&) override;
    void hit(MobRuntime&,std::uint32_t,Actor&) override;
private:
    ActorActionWorld& actions_;DuelWorld& duel_;VerticalMobsAttackEffects& continuation_;
};
// Required continuation of CMob::Attack at 004a2164. It must execute the
// actual target Damage overload, death/reflection, view delivery and final
// callbacks. The mutable damage buffer and all referenced owners survive it.
class VerticalMobsCalculatedHitServices {
public:
    virtual ~VerticalMobsCalculatedHitServices()=default;
    virtual void status_stand(Actor&)=0;
    virtual void send_miss(MobRuntime&,Actor&,const net::Packet&)=0;
    virtual void finish_hit(MobRuntime&,std::uint32_t,Actor&,bool reflected,bool critical,
        std::array<std::int32_t,3>& damage)=0;
};
// Required live Actor services for CUser::Damage 0044c1e0. record must
// update the same Actor damage ledger read by CUser::Death; transports must
// execute the existing self and group send owners, including their gates.
class VerticalMobsUserDamageWorld {
public:
    virtual ~VerticalMobsUserDamageWorld()=default;
    virtual void record(Actor&,std::int32_t kind,std::uint32_t source,std::int32_t amount)=0;
    virtual void send_reduction(Actor&,std::uint16_t amount)=0;
    virtual void send_energy(Actor&)=0;
};
bool vertical_mobs_user_damage(MobRuntime&,Actor&,std::array<std::int32_t,3>&,
    const SkillDefinitions&,VerticalMobsUserDamageWorld&);
// The ledger provider must mutate the bound Actor's actual CDamage owner,
// not a separate ledger. Actor and scene must be the same retained pair.
class VerticalMobsUserDamageLedger {
public:
    virtual ~VerticalMobsUserDamageLedger()=default;
    virtual void add(Actor&,std::int32_t,std::uint32_t,std::int32_t)=0;
};
class VerticalMobsUserDamageSceneWorld final:public VerticalMobsUserDamageWorld {
public:
    VerticalMobsUserDamageSceneWorld(Actor&,ActorScene&,VerticalMobsUserDamageLedger&);
    void record(Actor&,std::int32_t,std::uint32_t,std::int32_t) override;
    void send_reduction(Actor&,std::uint16_t) override;
    void send_energy(Actor&) override;
private:
    void require_actor(const Actor&) const;
    Actor& actor_;ActorScene& scene_;VerticalMobsUserDamageLedger& ledger_;
};

// Real owners for CMob::Attack 004a2164..004a223f. Damage is the mutable
// three-component CUser::Damage overload 0044c1e0, not scalar Actor::damage.
class VerticalMobsHitDeliveryWorld {
public:
    virtual ~VerticalMobsHitDeliveryWorld()=default;
    virtual void status_stand(Actor&)=0;
    virtual void send_miss(MobRuntime&,Actor&,const net::Packet&)=0;
    virtual bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&)=0;
    // Original 004a2188..004a21cb mob/target death transition and CUser::Death.
    virtual void user_killed(MobRuntime&,Actor&)=0;
    // Actual PSendRange 00421220: live mob position, radius 60, target ID
    // captured in packet is an immediate recipient, source ID 0, priority 6.
    virtual void send_hit(MobRuntime&,std::uint32_t target,const net::Packet&)=0;
    // Original 004a223f..004a235f, including reflection and final operation.
    virtual void finish_after_hit(MobRuntime&,std::uint32_t,Actor&,bool reflected,bool critical,
        bool killed,std::array<std::int32_t,3>&)=0;
};
class VerticalMobsHitDelivery final:public VerticalMobsCalculatedHitServices {
public:
    explicit VerticalMobsHitDelivery(VerticalMobsHitDeliveryWorld& world):world_(world){}
    void status_stand(Actor&) override;
    void send_miss(MobRuntime&,Actor&,const net::Packet&) override;
    void finish_hit(MobRuntime&,std::uint32_t,Actor&,bool,bool,std::array<std::int32_t,3>&) override;
private:
    VerticalMobsHitDeliveryWorld& world_;
};
class VerticalMobsUserDeath {
public:
    virtual ~VerticalMobsUserDeath()=default;
    // Execute the real CUser::Death 004586a0 with type 2 and the live mob ID.
    virtual void death(Actor&,std::uint32_t mob_id)=0;
};
// Existing ordinary/fallback death coordinator; explicitly propagates its
// unrecovered player-credit failure rather than treating that case as success.
class VerticalMobsOrdinaryUserDeath final:public VerticalMobsUserDeath {
public:
    VerticalMobsOrdinaryUserDeath(const MobTargetingBindingCatalog&,MobTargetingOrdinaryDeathWorld&);
    void death(Actor&,std::uint32_t) override;
private:
    const MobTargetingBindingCatalog& catalog_;MobTargetingOrdinaryDeathWorld& world_;
};
// CMob::Attack 004a2188..004a21d0 after the actual user damage returns true.
// Mob, Actor, both zone grids, targeting/FSM and death owners remain retained.
void vertical_mobs_user_killed(MobRuntime&,Actor&,ZoneMobs&,ZoneUsers&,
    const MobTargetingBindingCatalog&,MobTargetingZone&,VerticalMobsAiStateOwners&,
    VerticalMobsReturnDependencies&,VerticalMobsUserDeath&);
// Defensive equipment options, ps_game.exe 0044cae0. The guard and catalog
// belong to the retained actor and actual item/level-100 skill owners.
class VerticalMobsDefenseOptions {
public:
    virtual ~VerticalMobsDefenseOptions()=default;
    virtual std::uint8_t& active(Actor&)=0;
    virtual std::span<const std::byte> jewel(std::uint8_t)=0;
    virtual const UserSkillDefinition* mob_skill(std::uint8_t)=0;
    // Must execute 0044c940, including effect-1 selection or effect-3 hit.
    virtual void activate(Actor&,MobRuntime&,const UserSkillDefinition&)=0;
};
void vertical_mobs_defense_options(MobRuntime&,Actor&,VerticalMobsDefenseOptions&);
// Bind the coordinator's actual VerticalCombatItemOptionWorld. Its selected
// skill and hit worlds own all nontrivial targeting, effect and packet work.
// The two free functions are found through their dependent world arguments.
template<class ItemOptionWorld>
class VerticalMobsBoundDefenseOptions final:public VerticalMobsDefenseOptions {
public:
    explicit VerticalMobsBoundDefenseOptions(ItemOptionWorld& owner):owner_(owner) {}
    std::uint8_t& active(Actor& actor) override {return owner_.active(actor);}
    std::span<const std::byte> jewel(std::uint8_t id) override {return owner_.jewel(id);}
    const UserSkillDefinition* mob_skill(std::uint8_t id) override {return owner_.mob_skill(id);}
    void activate(Actor& actor,MobRuntime& mob,const UserSkillDefinition& skill) override {
        if(skill.effect.effect==1)vertical_combat_skill_select(actor,mob,skill,owner_.select_world());
        else if(skill.effect.effect==3)vertical_combat_skill_hit_prefix(actor,mob,skill,true,true,owner_.hit_world());
    }
private:ItemOptionWorld& owner_;
};

// Required real owners surrounding reflected CMob::Damage 00490ee0.
class VerticalMobsHitTailWorld {
public:
    virtual ~VerticalMobsHitTailWorld()=default;
    virtual void status_stand(Actor&)=0;
    virtual void send_miss(MobRuntime&,Actor&,const net::Packet&)=0;
    virtual bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&)=0;
    virtual void user_killed(MobRuntime&,Actor&)=0;
    virtual void send_hit(MobRuntime&,std::uint32_t target,const net::Packet&)=0;
    // Real CMob::Damage triple overload, credit type 1, actor ID, zero party,
    // bonus and base aggro; retain its live damage array through callbacks.
    virtual bool damage_mob(MobRuntime&,Actor&,std::array<std::int32_t,3>&)=0;
    // PSendRange: radius 60, immediate target ID, source ID 0, priority 6.
    virtual void send_reflection(MobRuntime&,std::uint32_t target,const net::Packet&)=0;
    virtual void final_attack_effects(MobRuntime&,Actor&)=0; // 0044cae0
};
// Actual retained actor/session and per-connection priority queue owners.
// Lookups must not change membership; the pin covers synchronous delivery.
class VerticalMobsPriorityConnections {
public:
    virtual ~VerticalMobsPriorityConnections()=default;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual PartyMembershipUser& connection(Actor&)=0;
    virtual UserPriorityQueue& queue(Actor&)=0;
};
// Uses the existing shared packet pool and connection priority queues. The mob,
// pool, queues, connections and pending buffer references must remain retained.
class VerticalMobsSharedPriorityCells final:public UserPrioritySharedCellServices {
public:
    VerticalMobsSharedPriorityCells(UserViewSharedBuffers&,MobRuntime&,VerticalMobsPriorityConnections&);
    std::shared_ptr<void> retain(Actor&) override;
    UserPosition origin() override;
    PartyMembershipUser& connection(Actor&) override;
    UserPriorityQueue& connection_queue(Actor&) override;
private:
    MobRuntime& mob_;VerticalMobsPriorityConnections& connections_;
};
// Binds mob miss, hit and reflected-hit views to the actual retained zone user
// grid and shared priority-area owner. The remaining hit operations are pure
// virtual and must be supplied by the host's actual runtime services.
class VerticalMobsPriorityHitTailWorld:public VerticalMobsHitTailWorld {
public:
    VerticalMobsPriorityHitTailWorld(ZoneMobs&,ZoneUsers&,UserPriorityAreaServices&);
    void send_miss(MobRuntime&,Actor&,const net::Packet&) final;
    void send_hit(MobRuntime&,std::uint32_t,const net::Packet&) final;
    void send_reflection(MobRuntime&,std::uint32_t,const net::Packet&) final;
private:
    void send(MobRuntime&,std::uint32_t,const net::Packet&);
    ZoneMobs& mobs_;ZoneUsers& users_;UserPriorityAreaServices& area_;
};
// Bind CMob::SendUseSkill 004a3f50 to the actual retained paired-zone
// priority view owner. Remaining skill attack operations are still required.
class VerticalMobsSkillPriorityWorld:public VerticalMobsSkillAttackWorld {
public:
    explicit VerticalMobsSkillPriorityWorld(VerticalMobsPriorityHitTailWorld&);
    void send_skill(MobRuntime&,Actor&,const MobTargetingBindingSkill&) final;
private:VerticalMobsPriorityHitTailWorld& views_;
};
// CMob::Heal 004909e0 uses the retained raw mob definition, actual damage
// ledger reset and actual linked-mob log owner; amounts are HP, MP, SP.
void vertical_mobs_heal(MobRuntime&,std::array<std::int32_t,3>&,
    const MobTargetingBindingCatalog&,const MobLogContext&,MobLogTransport&);
class VerticalMobsSkillHealWorld {
public:
    virtual ~VerticalMobsSkillHealWorld()=default;
    virtual void remove_state(MobRuntime&,const MobTargetingBindingSkill&)=0;
    virtual void range(MobRuntime&,Actor&,const MobTargetingBindingSkill&)=0;
};
// Binds SkillHeal 004a54e0 self-target behavior to actual mob heal, log and
// priority view owners. The host must still supply real area heal and debuff.
class VerticalMobsBoundSkillHealEffects:public VerticalMobsSkillOtherEffects {
public:
    VerticalMobsBoundSkillHealEffects(const MobTargetingBindingCatalog&,VerticalMobsPriorityHitTailWorld&,
        const MobLogContext&,MobLogTransport&,VerticalMobsSkillHealWorld&);
    void heal(MobRuntime&,Actor&,const MobTargetingBindingSkill&) final;
private:
    const MobTargetingBindingCatalog& catalog_;VerticalMobsPriorityHitTailWorld& views_;
    const MobLogContext& log_context_;MobLogTransport& log_;VerticalMobsSkillHealWorld& world_;
};
MobCombatCredit vertical_mobs_reflected_credit(const Actor&) noexcept;
// Bind existing retained stand and live mob-damage owners. The stand owner
// must expose status_stand(Actor&); the damage owner must expose
// damage(Actor&,MobRuntime&,array<int32_t,3>&,const MobCombatCredit&).
// Remaining inherited operations stay mandatory for the concrete host.
template<class StandOwner,class MobDamageOwner>
class VerticalMobsBoundHitTailWorld:public VerticalMobsPriorityHitTailWorld {
public:
    VerticalMobsBoundHitTailWorld(ZoneMobs& mobs,ZoneUsers& users,UserPriorityAreaServices& area,
        StandOwner& stand,MobDamageOwner& damage)
        :VerticalMobsPriorityHitTailWorld(mobs,users,area),stand_(stand),damage_(damage) {}
    void status_stand(Actor& actor) final {stand_.status_stand(actor);}
    bool damage_mob(MobRuntime& mob,Actor& actor,std::array<std::int32_t,3>& damage) final {
        return damage_.damage(actor,mob,damage,vertical_mobs_reflected_credit(actor));
    }
private:
    StandOwner& stand_;MobDamageOwner& damage_;
};
// Concrete mob-hit composition. All referenced services are the retained live
// owners; user damage/death and option callbacks remain required real services.
// The paired zones, actors, mob, catalogs and every provider must remain
// serialized and retained for the whole synchronous hit, including reentry.
template<class StandOwner,class MobDamageOwner>
class VerticalMobsLiveHitTailWorld final:public VerticalMobsBoundHitTailWorld<StandOwner,MobDamageOwner> {
public:
    VerticalMobsLiveHitTailWorld(ZoneMobs& mobs,ZoneUsers& users,UserPriorityAreaServices& area,
        StandOwner& stand,MobDamageOwner& mob_damage,const SkillDefinitions& skills,
        VerticalMobsUserDamageWorld& user_damage,const MobTargetingBindingCatalog& catalog,
        MobTargetingZone& targeting,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
        VerticalMobsUserDeath& user_death,VerticalMobsDefenseOptions& options)
        :VerticalMobsBoundHitTailWorld<StandOwner,MobDamageOwner>(mobs,users,area,stand,mob_damage),
         mobs_(mobs),users_(users),skills_(skills),user_damage_(user_damage),catalog_(catalog),
         targeting_(targeting),ai_(ai),dependencies_(dependencies),user_death_(user_death),options_(options) {}
    bool damage_user(MobRuntime& mob,Actor& actor,std::array<std::int32_t,3>& damage) final {
        return vertical_mobs_user_damage(mob,actor,damage,skills_,user_damage_);
    }
    void user_killed(MobRuntime& mob,Actor& actor) final {
        vertical_mobs_user_killed(mob,actor,mobs_,users_,catalog_,targeting_,ai_,dependencies_,user_death_);
    }
    void final_attack_effects(MobRuntime& mob,Actor& actor) final {
        vertical_mobs_defense_options(mob,actor,options_);
    }
private:
    ZoneMobs& mobs_;ZoneUsers& users_;const SkillDefinitions& skills_;VerticalMobsUserDamageWorld& user_damage_;
    const MobTargetingBindingCatalog& catalog_;MobTargetingZone& targeting_;VerticalMobsAiStateOwners& ai_;
    VerticalMobsReturnDependencies& dependencies_;VerticalMobsUserDeath& user_death_;VerticalMobsDefenseOptions& options_;
};
class VerticalMobsHitTail final:public VerticalMobsHitDeliveryWorld {
public:
    VerticalMobsHitTail(VerticalMobsHitTailWorld&,const MobDeathExperience&,MobDeathWorld&);
    void status_stand(Actor&) override;
    void send_miss(MobRuntime&,Actor&,const net::Packet&) override;
    bool damage_user(MobRuntime&,Actor&,std::array<std::int32_t,3>&) override;
    void user_killed(MobRuntime&,Actor&) override;
    void send_hit(MobRuntime&,std::uint32_t,const net::Packet&) override;
    void finish_after_hit(MobRuntime&,std::uint32_t,Actor&,bool,bool,bool,std::array<std::int32_t,3>&) override;
private:
    VerticalMobsHitTailWorld& world_;const MobDeathExperience& death_rules_;MobDeathWorld& death_world_;
};
class VerticalMobsCalculatedHitEffects final:public VerticalMobsAttackEffects {
public:
    VerticalMobsCalculatedHitEffects(const MobTargetingBindingCatalog&,RandomSource&,
        VerticalMobsMirrorSkillDefinitions&,const MobLogContext&,MobLogTransport&,VerticalMobsCalculatedHitServices&);
    void status_stand(Actor&) override;
    void send_miss(MobRuntime&,Actor&,const net::Packet&) override;
    void hit(MobRuntime&,std::uint32_t,Actor&) override;
private:
    const MobTargetingBindingCatalog& catalog_;RandomSource& random_;
    VerticalMobsMirrorSkillDefinitions& skills_;const MobLogContext& log_context_;
    MobLogTransport& log_transport_;VerticalMobsCalculatedHitServices& continuation_;
};
class VerticalMobsAttackTargetServices:public VerticalMobsAttackEffects {
public:
    ~VerticalMobsAttackTargetServices() override=default;
    virtual bool attack_success(MobRuntime&,Actor&,std::uint16_t attack_type)=0;
};
// Bind the recovered normal hit test to the actual executor RNG and target
// block owner, forwarding all target effects to their required real owner.
class VerticalMobsNormalAttackTarget final:public VerticalMobsAttackTargetServices {
public:
    VerticalMobsNormalAttackTarget(RandomSource&,VerticalMobsAttackBlock&,VerticalMobsAttackEffects&);
    void status_stand(Actor&) override;
    bool attack_success(MobRuntime&,Actor&,std::uint16_t) override;
    void send_miss(MobRuntime&,Actor&,const net::Packet&) override;
    void hit(MobRuntime&,std::uint32_t,Actor&) override;
private:
    RandomSource& random_;VerticalMobsAttackBlock& block_;VerticalMobsAttackEffects& effects_;
};
class VerticalMobsAttackTarget final:public VerticalMobsStationaryAttackWorld {
public:
    VerticalMobsAttackTarget(const MobTargetingBindingCatalog&,VerticalMobsAttackTargetServices&,VerticalMobsStationaryAttackWorld&);
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    const MobTargetingBindingCatalog& catalog_;VerticalMobsAttackTargetServices& target_;VerticalMobsStationaryAttackWorld& other_;
};
// Retained normal CMob::Attack composition. Every referenced service is the
// real serialized owner; all must outlive this adapter and synchronous calls.
// The log context is per subject/definition, not a shared cross-zone default.
class VerticalMobsNormalAttackComposition final:public VerticalMobsStationaryAttackWorld {
public:
    VerticalMobsNormalAttackComposition(const MobTargetingBindingCatalog&,RandomSource&,VerticalMobsBlockEffects&,
        VerticalMobsMirrorSkillDefinitions&,const MobLogContext&,MobLogTransport&,ActorActionWorld&,DuelWorld&,
        VerticalMobsHitTailWorld&,const MobDeathExperience&,MobDeathWorld&,VerticalMobsStationaryAttackWorld&);
    VerticalMobsNormalAttackComposition(const VerticalMobsNormalAttackComposition&)=delete;
    VerticalMobsNormalAttackComposition& operator=(const VerticalMobsNormalAttackComposition&)=delete;
    VerticalMobsNormalAttackComposition(VerticalMobsNormalAttackComposition&&)=delete;
    VerticalMobsNormalAttackComposition& operator=(VerticalMobsNormalAttackComposition&&)=delete;
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    VerticalMobsActorAttackBlock block_;VerticalMobsHitTail tail_;VerticalMobsHitDelivery delivery_;
    VerticalMobsCalculatedHitEffects calculation_;VerticalMobsPreparedAttackEffects preparation_;
    VerticalMobsNormalAttackTarget target_;VerticalMobsAttackTarget attack_;
};
// CMob::Attack 004a2000 gates; the required delegate must execute the real
// remaining attack, UseSkill and other attack services.
class VerticalMobsAttackGuard final:public VerticalMobsStationaryAttackWorld {
public:
    explicit VerticalMobsAttackGuard(VerticalMobsStationaryAttackWorld& world):world_(world){}
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    VerticalMobsStationaryAttackWorld& world_;
};
// Handles stationary normal and non-self skills, delegates self-skill execution
// and unrecovered chase branches to the required real world owner.
class VerticalMobsAttackAiStationary final:public VerticalMobsAttackAiSkillWorld {
public:
    VerticalMobsAttackAiStationary(ZoneMobs&,ZoneUsers&,const MobTargetingBindingCatalog&,VerticalMobsStationaryAttackWorld&);
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    ZoneMobs& mobs_;ZoneUsers& users_;const MobTargetingBindingCatalog& catalog_;VerticalMobsStationaryAttackWorld& world_;
};
class VerticalMobsChaseAttackWorld:public VerticalMobsStationaryAttackWorld {
public:
    // Actual CMob::Chase owner; returns the real result and may mutate the mob.
    virtual bool chase(MobRuntime&,std::uint32_t,Actor&,std::uint8_t range)=0;
};
// Bind the real chase implementation and paired actor targeting to the required
// Attack/UseSkill owner. The caller retains and serializes every dependency.
class VerticalMobsNativeChaseAttackWorld final:public VerticalMobsChaseAttackWorld {
public:
    VerticalMobsNativeChaseAttackWorld(ZoneMobs&,ZoneUsers&,const MapDefinition&,
        const MobTargetingBindingCatalog&,MobTargetingZone&,VerticalMobsAiStateOwners&,
        VerticalMobsReturnDependencies&,VerticalMobsStationaryAttackWorld&);
    bool chase(MobRuntime&,std::uint32_t,Actor&,std::uint8_t) override;
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    ZoneMobs& mobs_;ZoneUsers& users_;const MapDefinition& map_;const MobTargetingBindingCatalog& catalog_;
    MobTargetingZone& targeting_;VerticalMobsAiStateOwners& ai_;VerticalMobsReturnDependencies& dependencies_;
    VerticalMobsStationaryAttackWorld& attacks_;
};
// Reuses the actual Chase, Attack and UseSkill services for nonstationary
// slots. A false Chase result is gameplay state, not an error or success stub.
class VerticalMobsAttackAiChase final:public VerticalMobsStationaryAttackWorld {
public:
    VerticalMobsAttackAiChase(const MobTargetingBindingCatalog& catalog,VerticalMobsChaseAttackWorld& world):catalog_(catalog),world_(world){}
    void attack(MobRuntime&,std::uint32_t,Actor&) override;
    void use_skill(MobRuntime&,std::uint32_t,Actor&,const MobTargetingBindingSkill&) override;
    void other_attack(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    const MobTargetingBindingCatalog& catalog_;VerticalMobsChaseAttackWorld& world_;
};
// For an eligible slot, invoke the real ordinary ReTargetByAggro owner before
// the mandatory post-aggro attack continuation. That owner samples its own
// clock; the supplied tick remains the original AttackAI tick.
class VerticalMobsAttackAiAggro final:public VerticalMobsAttackAiReady {
public:
    VerticalMobsAttackAiAggro(VerticalMobsActorCombatWorld& world,VerticalMobsAttackAiAfterAggro& after):world_(world),after_(after){}
    void first_ready(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    VerticalMobsActorCombatWorld& world_;VerticalMobsAttackAiAfterAggro& after_;
};
// Mode 6 uses the actual paired grids, damage ledger and SetTarget owner.
// Other modes and the post-SetTarget attack body remain required providers.
class VerticalMobsAttackAiMode6 final:public VerticalMobsAttackAiReady {
public:
    VerticalMobsAttackAiMode6(ZoneMobs&,ZoneUsers&,const MobTargetingBindingCatalog&,
        VerticalMobsActorCombatWorld&,MobTargetingZone&,VerticalMobsAttackAiReady&,VerticalMobsAttackAiAfterTarget&);
    void first_ready(MobRuntime&,std::uint32_t,Actor&,std::size_t) override;
private:
    ZoneMobs& mobs_;ZoneUsers& users_;const MobTargetingBindingCatalog& catalog_;
    VerticalMobsActorCombatWorld& world_;MobTargetingZone& targeting_;VerticalMobsAttackAiReady& ready_;VerticalMobsAttackAiAfterTarget& after_;
};
// CMob::AttackAI 004a1ab0 target eligibility prefix; the caller retains the
// target and catalog across cleanup and the mandatory continuation.
void vertical_mobs_attack_ai_prefix(MobRuntime&,std::uint32_t,Actor&,
    const MobTargetingBindingCatalog&,VerticalMobsAttackAiContinuation&);
// Full recovered AttackAI composition. Combat world supplies real retention
// and aggro; attacks supplies real Attack and UseSkill operations.
void vertical_mobs_native_attack_ai(MobRuntime&,std::uint32_t,Actor&,ZoneMobs&,ZoneUsers&,
    const MapDefinition&,const MobTargetingBindingCatalog&,MobTargetingZone&,
    VerticalMobsActorCombatWorld&,VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&,
    VerticalMobsStationaryAttackWorld&);
// Per-dispatch bridge over the paired real grids and the subject's retained
// applied-skill owner. The subject and grids remain serialized throughout.
class VerticalMobsActorCombat final:public MobAiCombatServices {
public:
    VerticalMobsActorCombat(MobRuntime&,ZoneMobs&,ZoneUsers&,VerticalMobsActorCombatWorld&);
    std::uint32_t tick() override;
    MobAiCombatTarget* find_user(MobRuntime&,std::uint32_t) override;
    bool attack_ai(MobRuntime&,std::uint32_t,MobAiCombatTarget&) override;
    void remove_vision_fix(MobRuntime&) override;
    void retarget(MobRuntime&,std::uint32_t) override;
    void lua_combat(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t) override;
private:
    void subject(MobRuntime&) const;
    MobRuntime& mob_;ZoneMobs& mobs_;ZoneUsers& users_;VerticalMobsActorCombatWorld& world_;
    Actor* actor_=nullptr;std::shared_ptr<void> retained_;MobAiCombatTarget target_;
};
class VerticalMobsNativeAttack final:public VerticalMobsAttackServices {
public:
    VerticalMobsNativeAttack(VerticalMobsAttackDependencies& dependencies,VerticalMobsAiOwners& ai):dependencies_(dependencies),ai_(ai){}
    void update_attack(MobRuntime&,std::uint32_t) override;
private:
    VerticalMobsAttackDependencies& dependencies_;VerticalMobsAiOwners& ai_;
};
// Ordinary UpdateAttack uses a void AttackAI call; it must not reuse the
// configured-combat boolean AttackAI contract. All services are real owners.
void vertical_mobs_native_actor_attack(MobRuntime&,std::uint32_t,ZoneMobs&,ZoneUsers&,
    const MapDefinition&,const MobTargetingBindingCatalog&,MobTargetingZone&,
    VerticalMobsActorCombatWorld&,VerticalMobsAiOwners&,VerticalMobsReturnDependencies&,
    VerticalMobsStationaryAttackWorld&);
// Per-subject attack stage for a host that has already pinned the actual mob
// and retained/serialized this zone through all synchronous callbacks.
void vertical_mobs_native_zone_attack(RuntimeZone&,MobRuntime&,std::uint32_t,MicrosoftRandom&,
    MobAiStateServices&,const MobTargetingBindingCatalog&,VerticalMobsActorCombatWorld&,
    VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&,VerticalMobsStationaryAttackWorld&);
bool vertical_mobs_native_actor_attack_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    MicrosoftRandom&,MobAiStateServices&,const MobTargetingBindingCatalog&,
    VerticalMobsActorCombatWorld&,VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&,
    VerticalMobsStationaryAttackWorld&);
// Separate cadence from idle: false means no live primary-zone lease. This pass
// does not update death, respawn or any other world subsystem.
bool vertical_mobs_attack_frame(MobPrimaryZones&,std::size_t,std::uint32_t,VerticalMobsAttackServices&);
// Construct combat and return adapters from this lease's actual paired grids
// and map. The per-subject AI and combat world providers remain required.
bool vertical_mobs_actor_attack_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    const MobTargetingBindingCatalog&,VerticalMobsActorCombatWorld&,
    VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&);
// Ordinary portion of CMob::UpdateIdle 004a7e60, called only after its status
// gate. Use the real targeting search before the remaining movement body. The
// source and services describe the live subject and zone; movement runs
// synchronously on the same owners.
void vertical_mobs_ordinary_idle(MobRuntime&,std::uint32_t,bool obelisk,
    MobTargetingFindSource&,MobTargetingFindServices&,
    const std::function<void(MobRuntime&,std::uint32_t)>& movement);
// Remaining ordinary movement body at 004a7f77. Projection is the live
// definition/chase data, map is the subject zone's actual map, and random is
// the original executor's shared CRT source. The subject and its zone must
// remain retained through movement and its synchronous delivery callbacks.
void vertical_mobs_ordinary_movement(MobRuntime&,std::uint32_t,
    const MobAiIdleProjection&,const MapDefinition&,RandomSource&);
// Complete ordinary idle using the real search, map, zone and random owners.
void vertical_mobs_ordinary_idle(MobRuntime&,std::uint32_t,
    const MobAiIdleProjection&,MobTargetingFindSource&,MobTargetingFindServices&,
    const MapDefinition&,RandomSource&);
// Use the existing paired user/mob grids, Actor state and MobTargetingZone.
// The caller serializes and retains the zone, subject, users, catalog and map
// through synchronous callbacks, supplies the same zone to all owners, and
// uses the same executor random stream as the targeting owner.
void vertical_mobs_actor_idle(MobRuntime&,std::uint32_t,const MobAiIdleProjection&,
    ZoneMobs&,ZoneUsers&,MobTargetingZone&,const MobTargetingBindingCatalog&,
    const MapDefinition&,RandomSource&);
// Same live owners, deriving ordinary idle definition/chase inputs from them.
void vertical_mobs_actor_idle(MobRuntime&,std::uint32_t,ZoneMobs&,ZoneUsers&,
    MobTargetingZone&,const MobTargetingBindingCatalog&,const MapDefinition&,RandomSource&);
// CZone::UpdateMobIdle 00421a00: live traversal and death portion. The root
// must immediately follow this with its sole UpdateMobRespawn pass for this
// same zone and tick. Use the startup executor random source for corpse cleanup.
// No scheduling or attack pass is implied. False means no live primary lease.
bool vertical_mobs_idle_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    RandomSource&,VerticalMobsWorld&);
// ps_game.exe CZone::UpdateMobIdle 00421a00 calls UpdateMobRespawn
// 0041d240 after idle traversal and death cleanup. Complete that sequence in
// one retained zone dispatch using the real zone's ports and linked world.
bool vertical_mobs_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    MobInitPorts&,VerticalMobsWorld&,LinkedMobWorld&);
// Construct the targeting and idle adapters over this primary zone's actual
// owners and run its recovered idle/death/respawn pass under one lease. The stack
// router and AI owners must be real retained services for every subject.
// The executor random must be the same object used by MobInitPorts.
bool vertical_mobs_actor_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    MobInitPorts&,MicrosoftRandom&,const MobTargetingBindingCatalog&,
    MobAiStateServices&,VerticalMobsAiOwners&,LinkedMobWorld&);
// Construct the native return adapter from the same leased zone and map as
// targeting and population. The caller retains the real per-subject owners.
bool vertical_mobs_actor_frame(MobPrimaryZones&,std::size_t,std::uint32_t,
    MobInitPorts&,MicrosoftRandom&,const MobTargetingBindingCatalog&,
    MobAiStateServices&,VerticalMobsAiStateOwners&,VerticalMobsReturnDependencies&,LinkedMobWorld&);
// Attack-stage callback for the host scheduler; this does not set cadence or
// replace the complete world frame. All owners survive serialized callbacks.
class VerticalMobsWorldAttackFrame final {
public:
    VerticalMobsWorldAttackFrame(MobPrimaryZones&,const ServerMaps&,MicrosoftRandom&,MobAiStateServices&,
        const MobTargetingBindingCatalog&,VerticalMobsActorCombatWorld&,VerticalMobsAiStateOwners&,
        VerticalMobsReturnDependencies&,VerticalMobsStationaryAttackWorld&);
    void operator()(RuntimeZone&,std::uint32_t);
private:
    MobPrimaryZones& root_;const ServerMaps& maps_;MicrosoftRandom& random_;MobAiStateServices& stack_;
    const MobTargetingBindingCatalog& catalog_;VerticalMobsActorCombatWorld& combat_;VerticalMobsAiStateOwners& ai_;
    VerticalMobsReturnDependencies& dependencies_;VerticalMobsStationaryAttackWorld& attacks_;
};
// Callable for VerticalWorldPorts::frame. All references must outlive the
// callback; a dispatch must come from this authority's actual primary zone.
class VerticalMobsWorldFrame final {
public:
    VerticalMobsWorldFrame(MobPrimaryZones&,const ServerMaps&,MobInitPorts&,MicrosoftRandom&,
        const MobTargetingBindingCatalog&,MobAiStateServices&,VerticalMobsAiStateOwners&,
        VerticalMobsReturnDependencies&,LinkedMobWorld&);
    void operator()(RuntimeZone&,std::uint32_t);
private:
    MobPrimaryZones& root_;const ServerMaps& maps_;MobInitPorts& ports_;MicrosoftRandom& random_;
    const MobTargetingBindingCatalog& catalog_;MobAiStateServices& stack_;
    VerticalMobsAiStateOwners& ai_;VerticalMobsReturnDependencies& dependencies_;LinkedMobWorld& linked_;
};
// Copyable callback handle for constructing VerticalWorld before its primary
// authority exists. Bind once to that world's authority before the first frame.
// Bind and dispatch are serialized with world lifecycle and all referenced
// dependencies outlive every callback copy.
class VerticalMobsDeferredWorldFrame final {
public:
    VerticalMobsDeferredWorldFrame(const ServerMaps&,MobInitPorts&,MicrosoftRandom&,
        const MobTargetingBindingCatalog&,MobAiStateServices&,VerticalMobsAiStateOwners&,
        VerticalMobsReturnDependencies&,LinkedMobWorld&);
    void bind(const std::shared_ptr<MobPrimaryZones>&);
    void operator()(RuntimeZone&,std::uint32_t) const;
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
