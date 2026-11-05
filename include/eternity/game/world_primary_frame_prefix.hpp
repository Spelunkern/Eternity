#pragma once
#include "eternity/game/world_primary_user_attack_phase.hpp"
#include "eternity/game/world_primary_mob_phase.hpp"
#include "eternity/game/world_primary_npc_idle_phase.hpp"
#include "eternity/game/world_primary_item_phase.hpp"
#include "eternity/game/world_primary_quest_phase.hpp"
#include "eternity/game/world_primary_zone_packets.hpp"
#include "eternity/game/world_primary_applied_skills.hpp"
#include "eternity/game/world_primary_effect_ticks.hpp"
#include "eternity/game/world_primary_effects.hpp"
#include "eternity/game/world_zone_mob_attack.hpp"

namespace eternity::game {
// Implementations must perform these operations on the actual retained zone.
// A provider must not report success without executing the corresponding work.
class PrimaryFramePrefixServices : public PrimaryZonePacketServices, public PrimaryAppliedSkillServices, public PrimaryEffectTickServices, public PrimaryEffectServices, public ZoneMobAttackServices {
public:
    virtual ~PrimaryFramePrefixServices()=default;
};
struct ActiveEffectPhasesResult {bool completed=true,ran=false;};
struct ActiveAttackPhaseResult {bool completed=true,ran=false;};
// Prefix of ps_game.exe CWorldThread::Update 00407390, through quest updates.
// The caller holds the world serialization lock throughout this operation.
// This is intentionally primary-only and does not process the earlier pending
// zone queue or the subsequent weather, rank, market, send and instance passes.
class WorldPrimaryFramePrefix final {
public:
    explicit WorldPrimaryFramePrefix(std::shared_ptr<PrimaryFramePrefixServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary frame prefix requires real phase services");
    }
    template<class Root> bool update(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            auto pass=[&](auto&& operation) {
                current.visit_primary_zones([&](RuntimeZone& zone){
                    if(!current.running()) {complete=false;return;}
                    operation(zone);
                    if(!current.running()) complete=false;
                });
                return complete;
            };
            auto timed=[&](std::uint32_t& deadline,std::uint32_t interval,auto&& operation) {
                if(now<deadline) return true;
                if(!pass(operation)) return false;
                deadline=now+interval;return true;
            };
            if(!pass([&](RuntimeZone& zone){
                if(!update_primary_zone_packets(current,root.admission(),zone,*services_,[&](RuntimeZone& waiting){root.update_zone_wait_packets(waiting);})) complete=false;
            })) return;
            if(root.update_primary_user_attack_phase(now)==PrimaryUserAttackResult::interrupted) {complete=false;return;}
            if(!timed(skills_,1000U,[&](RuntimeZone& zone){
                if(!update_primary_applied_skills(current,zone,*services_,now)) complete=false;
            })) return;
            if(!timed(effect_ticks_,3000U,[&](RuntimeZone& zone){
                if(!update_primary_effect_ticks(current,zone,*services_,now)) complete=false;
            })) return;
            if(!timed(effects_,5000U,[&](RuntimeZone& zone){
                if(!update_primary_effects(current,zone,*services_,now)) complete=false;
            })) return;
            if(!timed(attack_,500U,[&](RuntimeZone& zone){
                if(!update_zone_mob_attacks(current,zone,*services_,now)) complete=false;
                if(current.running() && !root.update_primary_npc_attack(zone,now)) complete=false;
            })) return;
            if(root.update_primary_mob_phase(now)==PrimaryMobPhaseResult::interrupted) {complete=false;return;}
            if(root.update_primary_npc_idle_phase(now)==PrimaryNpcIdleResult::interrupted) {complete=false;return;}
            if(root.update_primary_item_phase(now)==PrimaryItemPhaseResult::interrupted) {complete=false;return;}
            if(root.update_primary_quest_phase(now)==PrimaryQuestPhaseResult::interrupted) complete=false;
        });
        return complete;
    }
    // Active-list alternatives for the three consecutive original passes.
    // They share deadlines with update(); callers choose one path per frame.
    template<class Root> ActiveEffectPhasesResult update_active_effects(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        ActiveEffectPhasesResult result;
        world.serialized_update([&](VerticalWorld& current){
            auto timed=[&](std::uint32_t& deadline,std::uint32_t interval,auto&& operation) {
                if(now<deadline) return true;
                bool complete=true;
                const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
                    if(!current.running() || !operation(current,zone)) complete=false;
                });
                if(!visited || !complete) {result.completed=false;return false;}
                deadline=now+interval;result.ran=true;return true;
            };
            if(!timed(skills_,1000U,[&](VerticalWorld& current,RuntimeZone& zone){return update_primary_applied_skills(current,zone,*services_,now);})) return;
            if(!timed(effect_ticks_,3000U,[&](VerticalWorld& current,RuntimeZone& zone){return update_primary_effect_ticks(current,zone,*services_,now);})) return;
            (void)timed(effects_,5000U,[&](VerticalWorld& current,RuntimeZone& zone){return update_primary_effects(current,zone,*services_,now);});
        });
        return result;
    }
    // Full active-list alternative for CWorldThread's paired mob/NPC pass.
    // It shares the primary prefix's 500 ms deadline and real providers.
    template<class Root> ActiveAttackPhaseResult update_active_attacks(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        ActiveAttackPhaseResult result;
        if(now<attack_) return result;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){
            if(!world.running()) {complete=false;return;}
            if(!update_zone_mob_attacks(world,zone,*services_,now)) complete=false;
            if(!world.running() || !root.update_primary_npc_attack(zone,now)) complete=false;
        });
        if(!visited || !complete) {result.completed=false;return result;}
        attack_=now+500U;result.ran=true;return result;
    }
private:
    std::shared_ptr<PrimaryFramePrefixServices> services_;
    std::uint32_t skills_=0,effect_ticks_=0,effects_=0,attack_=0;
};
}
