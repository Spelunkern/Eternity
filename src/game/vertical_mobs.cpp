#include "eternity/game/vertical_mobs.hpp"
#include "eternity/game/mob_log.hpp"
#include "eternity/game/mob_combat_stop.hpp"
#include "eternity/game/mob_death.hpp"
#include "eternity/game/user_priority_area.hpp"
#include "eternity/game/mob_targeting_lifecycle_ordinary.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/ground_items.hpp"
#include "eternity/game/user_skill_runtime.hpp"
#include "eternity/game/mob_targeting_actor.hpp"
#include "eternity/game/vertical_combat_success.hpp"
#include "eternity/game/mob_ai_states_reset.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_ai_states_combat.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
float idle_distance(UserPosition a,UserPosition b) {
    const auto x=static_cast<double>(static_cast<float>(a.x-b.x));
    const auto z=static_cast<double>(static_cast<float>(a.z-b.z));
    return static_cast<float>(std::sqrt(x*x+z*z));
}
std::int32_t idle_tile(float value) {
    const auto n=std::trunc(static_cast<double>(value));
    if(!std::isfinite(n) || n<INT32_MIN || n>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(n);
}
class ActorTargetSearch final:public MobTargetingFindServices {
public:
    ActorTargetSearch(MobRuntime& mob,ZoneUsers& users,MobTargetingZone& targeting,
        const MobTargetingBindingCatalog& catalog,MobTargetingFindSource& source)
        :mob_(mob),users_(users),targeting_(targeting),catalog_(catalog),source_(source) {}
    void visit_cell_users(std::int32_t x,std::int32_t z,
        const std::function<bool(const MobTargetingFindUser&)>& visit) override {
        users_.visit_cell(x,z,[&](Actor& actor,UserPosition position) {
            struct Restore {Actor*& slot;Actor* previous;~Restore(){slot=previous;}} restore{current_,current_};
            current_=&actor;
            if(!mob_.definition) throw std::logic_error("Target search lost its mob definition");
            const auto* definition=catalog_.mob(mob_.definition->id);
            if(!definition) throw std::logic_error("Target search lost its catalog record");
            // FindTarget 0049fd20 reads source position and chase radius after
            // locking the cell, for each eligible user. Bounds remain cached.
            source_.x=mob_.position.x;source_.z=mob_.position.z;
            source_.radius=std::to_integer<std::uint8_t>(definition->record[0x69]);
            const auto& appearance=actor.appearance();
            const auto& status=actor.status();
            const MobTargetingFindUser user{reinterpret_cast<std::uintptr_t>(&actor),actor.attack_state().mode,
                status.shape,static_cast<std::uint8_t>(status.no_mob_targeting),appearance.attackable,appearance.visible,
                position.x,position.z};
            return !visit(user);
        });
    }
    void set_target_with_help(std::uintptr_t token) override {
        if(!current_ || reinterpret_cast<std::uintptr_t>(current_)!=token)
            throw std::logic_error("Target selection requires the retained current user");
        MobTargetingActorUser user(*current_);
        targeting_.set_target(mob_,user,true);
    }
private:
    MobRuntime& mob_;ZoneUsers& users_;MobTargetingZone& targeting_;
    const MobTargetingBindingCatalog& catalog_;MobTargetingFindSource& source_;Actor* current_=nullptr;
};
}
VerticalMobReturn vertical_mobs_return_near(MobRuntime& mob,std::uint32_t now,
    MobAiConfiguredFsm& fsm,const MobAiVariableCatalog& variables,MobAiConfiguredServices& ai,
    MobAppliedSkillsRuntimeResetServices& reset) {
    if(now<mob.next_chase) return VerticalMobReturn::waiting;
    const auto distance=idle_distance(mob.respawn_position,mob.position);
    if(!((!mob.trail.nodes().empty() && distance<0.5F) || distance<10.0F))
        return VerticalMobReturn::needs_path;
    if(!mob.definition || !mob.damage || !mob.has_applied_skills())
        throw std::logic_error("Mob return requires its retained definition, damage and skill owners");
    if(mob.use_pais) reset_mob_ai_configured(mob,fsm,variables,ai);
    else if(mob_status(mob)!=2) mob.status=0;
    reset_mob_applied_skills_runtime(mob,reset);
    mob.path.reset();mob.trail.clear();mob.damage->reset();
    return VerticalMobReturn::completed;
}
VerticalMobReturn vertical_mobs_return_step(MobRuntime& mob,std::uint32_t now,
    MobAiConfiguredFsm& fsm,const MobAiVariableCatalog& variables,MobAiConfiguredServices& ai,
    MobAppliedSkillsRuntimeResetServices& reset,const MobTargetingBindingCatalog& catalog,const MapDefinition& map) {
    const auto near=vertical_mobs_return_near(mob,now,fsm,variables,ai,reset);
    if(near!=VerticalMobReturn::needs_path) return near;
    if(mob.stop!=0 || mob.sleep!=0 || mob.stun!=0) return VerticalMobReturn::in_progress;
    if(!mob.zone || !mob.definition) throw std::logic_error("Mob return step requires its live zone and definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition) throw std::logic_error("Mob return step requires its live catalog record");
    const MapTile from{idle_tile(mob.position.x),idle_tile(mob.position.z)};
    MapTile target;
    if(mob.path.found()) {
        const auto cursor=mob.path.current();
        if(!cursor) throw std::logic_error("Mob return path has no retained cursor");
        target=*cursor;
    } else {
        target={idle_tile(mob.respawn_position.x),idle_tile(mob.respawn_position.z)};
        if(from.x<0 || from.z<0 || target.x<0 || target.z<0 || from.x>=map.size || from.z>=map.size || target.x>=map.size || target.z>=map.size)
            throw std::logic_error("Mob return line endpoints are outside the recovered map contract");
        MapTile previous{};
        if(!map.enable_line(from,target,&previous)) {
            if(map.blocked(target)) {
                const bool immediate=previous.x==from.x && previous.z==from.z;
                if(mob.use_pais) reset_mob_ai_configured(mob,fsm,variables,ai);
                else if(mob_status(mob)!=2) mob.status=0;
                if(immediate) {
                    mob.respawn_position=mob.position;mob.path.reset();mob.trail.clear();
                } else {
                    mob.trail.clear();mob.path.reset();
                }
                return VerticalMobReturn::in_progress;
            }
            if(mob.trail.current()) {
                const auto waypoint=mob.trail.pop();
                if(!waypoint) throw std::logic_error("Mob return lost its retained chase trail");
                mob.path.set_path(*waypoint);target=*waypoint;
            } else {
                if(!mob.path.find(map,from,target)) {
                    const auto status=mob_status(mob);
                    if(mob.use_pais) {
                        if(status!=2) {mob.status=100;reset_mob_ai_configured(mob,fsm,variables,ai);}
                    } else if(status!=2) mob.status=0;
                    mob.respawn_position=mob.position;mob.path.reset();mob.trail.clear();
                    return VerticalMobReturn::in_progress;
                }
                const auto cursor=mob.path.current();
                if(!cursor) throw std::logic_error("Mob return search produced no path cursor");
                target=*cursor;
            }
        }
    }
    const auto difference=[](std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));};
    const auto dx=static_cast<float>(difference(target.x,from.x)),dz=static_cast<float>(difference(target.z,from.z));
    const auto squared=static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz);
    const auto length=static_cast<float>(std::sqrt(static_cast<double>(squared)));
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto byte=[&](std::size_t at){return std::to_integer<std::uint8_t>(definition->record[at]);};
    const auto step=static_cast<float>(speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+byte(0x68));
    const auto remaining=static_cast<float>(static_cast<double>(length)-(static_cast<double>(byte(0x3f))+1.0));
    const auto interval_after_move=[&] {
        if(!mob.definition) throw std::logic_error("Mob return lost its live definition during movement");
        const auto* live=catalog.mob(mob.definition->id);
        if(!live) throw std::logic_error("Mob return lost its catalog record during movement");
        std::uint32_t interval=0;for(unsigned i=0;i<4;++i) interval|=std::to_integer<std::uint32_t>(live->record[0x64+i])<<(8*i);
        return interval;
    };
    if(remaining<step) {
        const UserPosition destination{static_cast<float>(target.x),mob.position.y,static_cast<float>(target.z)};
        if(mob.zone->move(mob,destination,1)) {mob.position.x=destination.x;mob.position.z=destination.z;}
        const auto interval=interval_after_move();
        const auto value=std::trunc((static_cast<double>(remaining)/static_cast<double>(step))*static_cast<float>(interval)+static_cast<float>(now));
        const auto deadline=!std::isfinite(value) || value<INT32_MIN || value>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(value);
        mob.next_chase=std::bit_cast<std::uint32_t>(deadline);mob.path.advance();
        return VerticalMobReturn::in_progress;
    }
    const auto nx=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dx)/length):0.0F;
    const auto nz=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dz)/length):0.0F;
    const UserPosition destination{static_cast<float>(static_cast<double>(static_cast<float>(static_cast<double>(nx)*step))+mob.position.x),
        mob.position.y,static_cast<float>(static_cast<double>(static_cast<float>(static_cast<double>(nz)*step))+mob.position.z)};
    static_cast<void>(mob.zone->move(mob,destination,1));
    // MoveMob may synchronously change the definition before the deadline read.
    mob.next_chase=now+interval_after_move();
    return VerticalMobReturn::in_progress;
}
MobAiIdleProjection vertical_mobs_idle_projection(const MobRuntime& mob,const MobTargetingBindingCatalog& catalog) {
    if(!mob.definition) throw std::invalid_argument("Ordinary idle requires a mob definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition) throw std::invalid_argument("Ordinary idle requires the live catalog record");
    const auto& record=definition->record;
    const auto byte=[&](std::size_t at){return std::to_integer<std::uint8_t>(record[at]);};
    std::uint32_t normal_time=0;
    for(unsigned i=0;i<4;++i) normal_time|=static_cast<std::uint32_t>(byte(0x5c+i))<<(8*i);
    // CMob::UpdateIdle 004a7e60 reads stMobInfo and the ChasePathList
    // head at +a4; ChaseTrail owns the corresponding native list.
    return {byte(0x26),byte(0x68),byte(0x60),normal_time,byte(0x7c)!=0,
        !mob.trail.nodes().empty(),mob.zone!=nullptr};
}
namespace {
template<class Read>
void ordinary_movement(MobRuntime& mob,std::uint32_t now,Read&& read,const MapDefinition& map,RandomSource& random) {
    const auto data=read();
    const auto distance=idle_distance(mob.position,mob.respawn_position);
    if((data.chase_path_present && distance>0.5F) || distance>10.0F) {
        if(mob_status(mob)!=2) mob.status=3;
        return;
    }
    if(data.ai==4 || mob.stop!=0 || mob.sleep!=0 || mob.stun!=0 || data.move_speed==0 || data.normal_step==0) {
        mob.next_move=now+10000U+static_cast<std::uint32_t>(random.next()%30000);
        return;
    }
    const auto angle=random.next()%10000;
    const auto extra=(random.next()%500)/100;
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto length=static_cast<float>(speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+read().normal_step)+static_cast<float>(extra);
    const float radians=static_cast<float>(static_cast<double>(static_cast<float>(angle))*std::bit_cast<float>(0x3c8efa35U));
    const float sine=static_cast<float>(std::sin(static_cast<double>(radians)));
    const float cosine=static_cast<float>(std::cos(static_cast<double>(radians)));
    const UserPosition candidate{static_cast<float>(-static_cast<double>(sine)*length)+mob.position.x,
        mob.position.y,static_cast<float>(static_cast<double>(cosine)*length)+mob.position.z};
    if(mob.zone && idle_distance(candidate,mob.respawn_position)<10.0F &&
       map.enable_line({idle_tile(mob.position.x),idle_tile(mob.position.z)},
                       {idle_tile(candidate.x),idle_tile(candidate.z)}))
        static_cast<void>(mob.zone->move(mob,candidate,0));
    const auto delay=random.next()%30000;
    mob.next_move=now+10000U+read().normal_time+static_cast<std::uint32_t>(delay);
}
}
void vertical_mobs_ordinary_movement(MobRuntime& mob,std::uint32_t now,
    const MobAiIdleProjection& data,const MapDefinition& map,RandomSource& random) {
    ordinary_movement(mob,now,[&]{return data;},map,random);
}
void vertical_mobs_ordinary_idle(MobRuntime& mob,std::uint32_t now,bool obelisk,
    MobTargetingFindSource& source,MobTargetingFindServices& targeting,
    const std::function<void(MobRuntime&,std::uint32_t)>& movement) {
    if(!movement) throw std::invalid_argument("Missing ordinary mob movement owner");
    bool find=true;
    if(mob.next_new_target!=0) {
        if(now<mob.next_new_target) find=false;
        else mob.next_new_target=0;
    }
    if(find && mob_targeting(source,targeting)) return;
    if(!obelisk && mob.next_move<=now) movement(mob,now);
}
void vertical_mobs_ordinary_idle(MobRuntime& mob,std::uint32_t now,
    const MobAiIdleProjection& data,MobTargetingFindSource& source,
    MobTargetingFindServices& targeting,const MapDefinition& map,RandomSource& random) {
    vertical_mobs_ordinary_idle(mob,now,data.obelisk,source,targeting,
        [&](MobRuntime& subject,std::uint32_t tick) {
            vertical_mobs_ordinary_movement(subject,tick,data,map,random);
        });
}
namespace {
template<class Read>
void actor_idle(MobRuntime& mob,std::uint32_t now,Read&& read,
    ZoneMobs& zone,ZoneUsers& users,MobTargetingZone& targeting,const MobTargetingBindingCatalog& catalog,
    const MapDefinition& map,RandomSource& random) {
    if(mob.zone!=&zone || !mob.cell || zone.find(mob.id)!=&mob || !mob.definition)
        throw std::invalid_argument("Ordinary idle requires the retained subject and its zone");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition) throw std::invalid_argument("Ordinary idle requires the live targeting definition");
    const auto count=users.size();
    if(count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::overflow_error("Targeting user count exceeds original signed count");
    MobTargetingFindSource source{static_cast<std::int32_t>(count),zone.cell_count(),zone.cell_count(),
        mob.cell->x,mob.cell->z,mob.position.x,mob.position.z,
        std::to_integer<std::uint8_t>(definition->record[0x26]),definition->chase_range};
    ActorTargetSearch search(mob,users,targeting,catalog,source);
    vertical_mobs_ordinary_idle(mob,now,false,source,search,[&](MobRuntime& subject,std::uint32_t tick) {
        if(!read().obelisk) ordinary_movement(subject,tick,read,map,random);
    });
}
}
void vertical_mobs_actor_idle(MobRuntime& mob,std::uint32_t now,const MobAiIdleProjection& data,
    ZoneMobs& zone,ZoneUsers& users,MobTargetingZone& targeting,const MobTargetingBindingCatalog& catalog,
    const MapDefinition& map,RandomSource& random) {
    actor_idle(mob,now,[&]{return data;},zone,users,targeting,catalog,map,random);
}
void vertical_mobs_actor_idle(MobRuntime& mob,std::uint32_t now,ZoneMobs& zone,ZoneUsers& users,
    MobTargetingZone& targeting,const MobTargetingBindingCatalog& catalog,const MapDefinition& map,RandomSource& random) {
    actor_idle(mob,now,[&]{return vertical_mobs_idle_projection(mob,catalog);},zone,users,targeting,catalog,map,random);
}
VerticalMobsNativeAiOwners::VerticalMobsNativeAiOwners(VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    ZoneMobs& zone,const MobTargetingBindingCatalog& catalog,const MapDefinition& map)
    :ai_(ai),dependencies_(dependencies),zone_(zone),catalog_(catalog),map_(map) {}
MobAiConfiguredFsm& VerticalMobsNativeAiOwners::configured_fsm(MobRuntime& mob){return ai_.configured_fsm(mob);}
MobAiConfiguredServices& VerticalMobsNativeAiOwners::configured_services(MobRuntime& mob){return ai_.configured_services(mob);}
MobAiStateServices& VerticalMobsNativeAiOwners::stack_services(MobRuntime& mob){return ai_.stack_services(mob);}
void VerticalMobsNativeAiOwners::release_failed_stack(MobRuntime& mob){ai_.release_failed_stack(mob);}
void VerticalMobsNativeAiOwners::return_home(MobRuntime& mob,std::uint32_t now) {
    if(mob.zone!=&zone_ || !mob.cell || zone_.find(mob.id)!=&mob)
        throw std::invalid_argument("Mob return requires retained membership in its actual zone");
    if(now<mob.next_chase) return;
    auto& fsm=ai_.configured_fsm(mob);auto& services=ai_.configured_services(mob);
    const auto& variables=dependencies_.variables(mob);auto& reset=dependencies_.reset_services(mob);
    if(vertical_mobs_return_step(mob,now,fsm,variables,services,reset,catalog_,map_)==VerticalMobReturn::needs_path)
        throw std::logic_error("Mob return requires an unimplemented path branch");
}
VerticalMobsActorWorld::VerticalMobsActorWorld(ZoneMobs& zone,ZoneUsers& users,MobTargetingZone& targeting,
    const MobTargetingBindingCatalog& catalog,const MapDefinition& map,RandomSource& random,VerticalMobsAiOwners& ai)
    :zone_(zone),users_(users),targeting_(targeting),catalog_(catalog),map_(map),random_(random),ai_(ai) {}
MobAiConfiguredFsm& VerticalMobsActorWorld::configured_fsm(MobRuntime& mob){return ai_.configured_fsm(mob);}
MobAiConfiguredServices& VerticalMobsActorWorld::configured_services(MobRuntime& mob){return ai_.configured_services(mob);}
MobAiStateServices& VerticalMobsActorWorld::stack_services(MobRuntime& mob){return ai_.stack_services(mob);}
void VerticalMobsActorWorld::release_failed_stack(MobRuntime& mob){ai_.release_failed_stack(mob);}
void VerticalMobsActorWorld::return_home(MobRuntime& mob,std::uint32_t now){ai_.return_home(mob,now);}
void VerticalMobsActorWorld::ordinary_idle(MobRuntime& mob,std::uint32_t now){vertical_mobs_actor_idle(mob,now,zone_,users_,targeting_,catalog_,map_,random_);}
struct VerticalMobsNativeZoneIdle::Impl {
    MobTargetingActorWorld targeting_world;
    MobTargetingZone targeting;
    VerticalMobsNativeAiOwners native_ai;
    VerticalMobsActorWorld world;
    Impl(RuntimeZone& zone,MicrosoftRandom& random,MobAiStateServices& stack,
        const MobTargetingBindingCatalog& catalog,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies)
        :targeting_world(*zone.mobs,*zone.users,random,stack),targeting(*zone.mobs,catalog,targeting_world),
         native_ai(ai,dependencies,*zone.mobs,catalog,zone.map->definition),
         world(*zone.mobs,*zone.users,targeting,catalog,zone.map->definition,random,native_ai) {}
};
VerticalMobsNativeZoneIdle::VerticalMobsNativeZoneIdle(RuntimeZone& zone,MicrosoftRandom& random,MobAiStateServices& stack,
    const MobTargetingBindingCatalog& catalog,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies) {
    if(!zone.map || !zone.mobs || !zone.users)throw std::invalid_argument("Mob idle requires initialized paired zone owners");
    impl_=std::make_unique<Impl>(zone,random,stack,catalog,ai,dependencies);
}
VerticalMobsNativeZoneIdle::~VerticalMobsNativeZoneIdle()=default;
MobAiConfiguredFsm& VerticalMobsNativeZoneIdle::configured_fsm(MobRuntime& mob){return impl_->world.configured_fsm(mob);}
MobAiConfiguredServices& VerticalMobsNativeZoneIdle::configured_services(MobRuntime& mob){return impl_->world.configured_services(mob);}
MobAiStateServices& VerticalMobsNativeZoneIdle::stack_services(MobRuntime& mob){return impl_->world.stack_services(mob);}
void VerticalMobsNativeZoneIdle::release_failed_stack(MobRuntime& mob){impl_->world.release_failed_stack(mob);}
void VerticalMobsNativeZoneIdle::return_home(MobRuntime& mob,std::uint32_t now){impl_->world.return_home(mob,now);}
void VerticalMobsNativeZoneIdle::ordinary_idle(MobRuntime& mob,std::uint32_t now){impl_->world.ordinary_idle(mob,now);}
void vertical_mobs_idle(MobRuntime& mob,std::uint32_t now,VerticalMobsWorld& world) {
    mob.current_time=now;
    if(mob.use_pais) {
        const auto status=mob_status(mob);
        if(status==100) {
            auto& fsm=world.configured_fsm(mob);
            auto& services=world.configured_services(mob);
            evaluate_mob_ai_configured(mob,fsm,services);
            execute_mob_ai_configured(mob,fsm,services);
            return;
        }
        if(status==3) { world.return_home(mob,now); return; }
    }
    if(mob_status(mob)==2) return;
    if(mob.state_stack && !poll_mob_ai_stack(mob,world.stack_services(mob))) {
        world.release_failed_stack(mob);
        return;
    }
    if(mob_status(mob)==0) world.ordinary_idle(mob,now);
}
void vertical_mobs_attack(MobRuntime& mob,std::uint32_t now,MobAiCombatServices& combat,VerticalMobsAiOwners& ai) {
    if(mob_status(mob)==100 || mob.state_stack || mob_status(mob)==2 || now<mob.next_attack) return;
    if(mob.status==1) {
        if(mob.sleep!=0 || mob.stun!=0) return;
        auto* target=combat.find_user(mob,mob.target_id);
        if(target && target->life_status!=1) {static_cast<void>(combat.attack_ai(mob,now,*target));return;}
        combat.remove_vision_fix(mob);
        if(mob_status(mob)!=2) mob.status=0;
        mob.target_kind=0;
    } else if(mob_status(mob)==3) ai.return_home(mob,now);
}
bool vertical_mobs_attack_range(const MobRuntime& mob,UserPosition position,std::int32_t range,const MobTargetingBindingCatalog& catalog) {
    if(!mob.definition)throw std::invalid_argument("Mob attack range requires its live definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob attack range requires its live catalog record");
    const auto distance=idle_distance(mob.position,position);
    const auto size=std::to_integer<std::uint32_t>(definition->record[0x3f]);
    const auto sum=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(range)+size);
    const auto limit=static_cast<double>(static_cast<float>(sum))+static_cast<double>(0.3F);
    return static_cast<double>(distance)<limit;
}
bool vertical_mobs_attack_range(const MobRuntime& mob,const Actor& actor,std::int32_t range,ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog) {
    if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob || users.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Mob attack range requires retained paired zone membership");
    const auto position=users.position(actor.id());
    if(!position)throw std::logic_error("Mob attack range lost its retained user position");
    return vertical_mobs_attack_range(mob,*position,range,catalog);
}
VerticalMobChasePrefix vertical_mobs_chase_prefix(MobRuntime& mob,std::uint32_t now,UserPosition position,std::int32_t range,
    const MobTargetingBindingCatalog& catalog) {
    if(now<mob.next_chase)return VerticalMobChasePrefix::waiting;
    if(!mob.definition)throw std::invalid_argument("Mob chase requires its live definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob chase requires its live catalog record");
    const auto size=std::to_integer<std::uint8_t>(definition->record[0x3f]);
    const auto limit=static_cast<double>(static_cast<float>(size))+static_cast<double>(static_cast<float>(range))+1.5;
    if(static_cast<double>(idle_distance(position,mob.position))<limit) {mob.path.reset();return VerticalMobChasePrefix::reached;}
    if(mob.stop!=0 || mob.sleep!=0 || mob.stun!=0)return VerticalMobChasePrefix::stopped;
    return VerticalMobChasePrefix::needs_movement;
}
bool vertical_mobs_chase(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,VerticalMobsChaseMovement& movement) {
    if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob || users.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Mob chase requires retained paired zone membership");
    if(now<mob.next_chase)return false;
    const auto position=users.position(actor.id());if(!position)throw std::logic_error("Mob chase lost its retained user position");
    switch(vertical_mobs_chase_prefix(mob,now,*position,range,catalog)) {
        case VerticalMobChasePrefix::reached:return true;
        case VerticalMobChasePrefix::needs_movement:return movement.continue_chase(mob,now,actor,range);
        case VerticalMobChasePrefix::waiting:case VerticalMobChasePrefix::stopped:return false;
    }
    throw std::logic_error("Unknown mob chase prefix result");
}
VerticalMobsChaseLeash::VerticalMobsChaseLeash(ZoneMobs& mobs,ZoneUsers& users,VerticalMobsAiStateOwners& ai,
    VerticalMobsReturnDependencies& dependencies,VerticalMobsChaseMovement& movement)
    :mobs_(mobs),users_(users),ai_(ai),dependencies_(dependencies),movement_(movement) {}
VerticalMobChaseLeash vertical_mobs_chase_leash(MobRuntime& mob,UserPosition position,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies) {
    if(mob.vision_fixed_user==0) {
        if(idle_distance(position,mob.respawn_position)>45.0F) {
            if(mob_status(mob)!=2)mob.status=3;
            mob.target_kind=0;mob.path.reset();return VerticalMobChaseLeash::completed;
        }
        if(idle_distance(position,mob.position)>45.0F) {
            if(mob.use_pais)reset_mob_ai_configured(mob,ai.configured_fsm(mob),dependencies.variables(mob),ai.configured_services(mob));
            else {if(mob_status(mob)!=2)mob.status=0;mob.target_kind=0;}
            mob.path.reset();return VerticalMobChaseLeash::completed;
        }
    }
    return VerticalMobChaseLeash::needs_movement;
}
bool VerticalMobsChaseLeash::continue_chase(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range) {
    if(mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || users_.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Mob chase leash requires retained paired zone membership");
    const auto position=users_.position(actor.id());if(!position)throw std::logic_error("Mob chase leash lost its retained user position");
    if(vertical_mobs_chase_leash(mob,*position,ai_,dependencies_)==VerticalMobChaseLeash::completed)return false;
    return movement_.continue_chase(mob,now,actor,range);
}
VerticalMobsChasePath::VerticalMobsChasePath(ZoneMobs& mobs,ZoneUsers& users,const MapDefinition& map,VerticalMobsChasePathWorld& world)
    :mobs_(mobs),users_(users),map_(map),world_(world) {}
bool VerticalMobsChasePath::continue_chase(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range) {
    if(mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || users_.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Mob chase path requires retained paired zone membership");
    if(mob.path.found()) {
        const auto cursor=mob.path.current();if(!cursor)throw std::logic_error("Mob chase lost its retained path cursor");
        return world_.waypoint(mob,now,actor,range,*cursor);
    }
    const auto position=users_.position(actor.id());if(!position)throw std::logic_error("Mob chase path lost its retained user position");
    const MapTile from{idle_tile(mob.position.x),idle_tile(mob.position.z)},target{idle_tile(position->x),idle_tile(position->z)};
    if(from.x<0 || from.z<0 || target.x<0 || target.z<0 || from.x>=map_.size || from.z>=map_.size || target.x>=map_.size || target.z>=map_.size)
        throw std::logic_error("Mob chase line endpoints are outside the recovered map contract");
    MapTile previous{};
    if(!map_.enable_line(from,target,&previous))return world_.blocked(mob,now,actor,range,from,target,previous);
    mob.path.set_path(target);
    return world_.waypoint(mob,now,actor,range,target);
}
VerticalMobsChaseBlockedTarget::VerticalMobsChaseBlockedTarget(const MapDefinition& map,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,VerticalMobsChasePathWorld& world):map_(map),users_(users),catalog_(catalog),world_(world) {}
bool VerticalMobsChaseBlockedTarget::waypoint(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,MapTile point) {
    return world_.waypoint(mob,now,actor,range,point);
}
bool VerticalMobsChaseBlockedTarget::blocked(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,
    MapTile from,MapTile target,MapTile previous) {
    if(!map_.blocked(target))return world_.blocked(mob,now,actor,range,from,target,previous);
    if(!mob.definition || !mob.damage || mob.path.found() || users_.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Mob chase blocked target requires retained subject, user and path owners");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob chase blocked target requires its live catalog record");
    const auto position=users_.position(actor.id());if(!position)throw std::logic_error("Mob chase blocked target lost its user position");
    const auto dx=std::abs(static_cast<float>(mob.position.x-static_cast<float>(previous.x)));
    const auto dz=std::abs(static_cast<float>(mob.position.z-static_cast<float>(previous.z)));
    if(dx<=1.0F && dz<=1.0F) {
        mob.damage->fair_combat=false;
        if(mob_status(mob)!=2)mob.status=3;
        mob.target_kind=0;mob.path.reset();return false;
    }
    mob.path.set_path(previous);
    const auto size=std::to_integer<std::uint32_t>(definition->record[0x3f]);
    const auto attack_range=std::to_integer<std::uint32_t>(definition->record[0x88]);
    const auto limit=static_cast<double>(static_cast<float>(size+attack_range))+static_cast<double>(0.3F);
    const UserPosition waypoint{static_cast<float>(previous.x),0,static_cast<float>(previous.z)};
    if(static_cast<double>(idle_distance(*position,waypoint))>limit) {
        if(mob_status(mob)!=2)mob.status=3;
        mob.target_kind=0;mob.path.reset();return false;
    }
    return world_.waypoint(mob,now,actor,range,previous);
}
VerticalMobsChaseTargetReset::VerticalMobsChaseTargetReset(MobTargetingFindSource& source,MobTargetingFindServices& targeting,
    VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies)
    :source_(source),targeting_(targeting),ai_(ai),dependencies_(dependencies) {}
void VerticalMobsChaseTargetReset::reset_target(MobRuntime& mob) {
    if(mob_status(mob)==2)return;
    if(!mob.has_applied_skills())throw std::logic_error("Mob target reset requires its applied skills owner");
    mob.applied_skills().owner().remove_vision_fix(mob);
    if(!mob.use_pais) {
        if(mob_status(mob)!=2)mob.status=0;
        mob.target_kind=0;mob.target_id=0;return;
    }
    if(!mob_find_target(source_,targeting_))
        reset_mob_ai_configured(mob,ai_.configured_fsm(mob),dependencies_.variables(mob),ai_.configured_services(mob));
}
VerticalMobsChaseSearch::VerticalMobsChaseSearch(const MapDefinition& map,VerticalMobsChaseSearchWorld& reset,VerticalMobsChasePathWorld& world)
    :map_(map),reset_(reset),world_(world) {}
bool VerticalMobsChaseSearch::waypoint(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,MapTile point) {
    return world_.waypoint(mob,now,actor,range,point);
}
bool VerticalMobsChaseSearch::blocked(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,
    MapTile from,MapTile target,MapTile) {
    if(mob.path.found() || map_.blocked(target))
        throw std::invalid_argument("Mob chase search requires an unblocked target and an unused path");
    if(!mob.path.find(map_,from,target)) {
        reset_.reset_target(mob);
        mob.path.reset();return false;
    }
    const auto point=mob.path.current();
    if(!point)throw std::logic_error("Mob chase search returned no path cursor");
    return world_.waypoint(mob,now,actor,range,*point);
}
VerticalMobsChaseIntermediate::VerticalMobsChaseIntermediate(ZoneMobs& mobs,const MapDefinition& map,
    const MobTargetingBindingCatalog& catalog,VerticalMobsChasePathWorld& world):mobs_(mobs),map_(map),catalog_(catalog),world_(world) {}
bool VerticalMobsChaseIntermediate::blocked(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,
    MapTile from,MapTile target,MapTile previous) {return world_.blocked(mob,now,actor,range,from,target,previous);}
bool VerticalMobsChaseIntermediate::waypoint(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,MapTile point) {
    if(mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || !mob.path.found() || !mob.path.current() ||
       mob.path.current()->x!=point.x || mob.path.current()->z!=point.z || !mob.definition)
        throw std::invalid_argument("Mob chase waypoint requires retained zone, definition and path cursor");
    if(mob.path.last())return world_.waypoint(mob,now,actor,range,point);
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob chase waypoint requires its live catalog record");
    const MapTile from{idle_tile(mob.position.x),idle_tile(mob.position.z)};
    const auto diff=[](std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));};
    const auto dx=static_cast<float>(diff(point.x,from.x)),dz=static_cast<float>(diff(point.z,from.z));
    const auto squared=static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz);
    const auto length=static_cast<float>(std::sqrt(static_cast<double>(squared)));
    const auto distance=idle_tile(length);constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto step=speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+std::to_integer<std::uint8_t>(definition->record[0x68]);
    mob.trail.add(map_,from);
    if(distance<step) {
        static_cast<void>(mobs_.move(mob,{static_cast<float>(point.x),0,static_cast<float>(point.z)},1));
        if(!mob.definition)throw std::logic_error("Mob chase lost its definition during movement");
        const auto* live=catalog_.mob(mob.definition->id);if(!live)throw std::logic_error("Mob chase lost its catalog record during movement");
        std::uint32_t interval=0;for(unsigned i=0;i<4;++i)interval|=std::to_integer<std::uint32_t>(live->record[0x64+i])<<(8*i);
        mob.next_chase=now+static_cast<std::uint32_t>(distance/step)*interval;mob.path.advance();return false;
    }
    const auto nx=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dx)/length):0.0F;
    const auto nz=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dz)/length):0.0F;
    const UserPosition destination{static_cast<float>(static_cast<double>(static_cast<float>(static_cast<double>(nx)*step))+mob.position.x),mob.position.y,
        static_cast<float>(static_cast<double>(static_cast<float>(static_cast<double>(nz)*step))+mob.position.z)};
    static_cast<void>(mobs_.move(mob,destination,1));
    if(!mob.definition)throw std::logic_error("Mob chase lost its definition during movement");
    const auto* live=catalog_.mob(mob.definition->id);if(!live)throw std::logic_error("Mob chase lost its catalog record during movement");
    std::uint32_t interval=0;for(unsigned i=0;i<4;++i)interval|=std::to_integer<std::uint32_t>(live->record[0x64+i])<<(8*i);
    mob.next_chase=now+interval;return false;
}
VerticalMobsChaseFinal::VerticalMobsChaseFinal(ZoneMobs& mobs,const MapDefinition& map,const MobTargetingBindingCatalog& catalog)
    :mobs_(mobs),map_(map),catalog_(catalog) {}
bool VerticalMobsChaseFinal::blocked(MobRuntime&,std::uint32_t,Actor&,std::int32_t,MapTile,MapTile,MapTile) {
    throw std::logic_error("Final chase waypoint cannot resolve a blocked line");
}
bool VerticalMobsChaseFinal::waypoint(MobRuntime& mob,std::uint32_t now,Actor&,std::int32_t range,MapTile point) {
    const auto cursor=mob.path.current();
    if(mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || !mob.definition || !mob.path.found() || !mob.path.last() ||
       !cursor || cursor->x!=point.x || cursor->z!=point.z)
        throw std::invalid_argument("Final chase waypoint requires retained zone, definition and final path cursor");
    const auto* definition=catalog_.mob(mob.definition->id);if(!definition)throw std::invalid_argument("Final chase waypoint requires its live catalog record");
    const MapTile from{idle_tile(mob.position.x),idle_tile(mob.position.z)};
    const auto diff=[](std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));};
    const auto dx=static_cast<float>(diff(point.x,from.x)),dz=static_cast<float>(diff(point.z,from.z));
    const auto squared=static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz);
    const auto length=static_cast<float>(std::sqrt(static_cast<double>(squared)));
    const auto nx=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dx)/length):0.0F;
    const auto nz=dx!=0 || dz!=0?static_cast<float>(static_cast<double>(dz)/length):0.0F;
    constexpr std::array<std::int32_t,5> speeds{-4,-2,0,2,4};
    const auto step=speeds[static_cast<std::size_t>(std::clamp(mob.move_speed,0,4))]+std::to_integer<std::uint8_t>(definition->record[0x68]);
    const auto size=std::to_integer<std::uint8_t>(definition->record[0x3f]);
    const auto remaining=static_cast<float>(static_cast<double>(length)-(static_cast<double>(size)+0.4F)-static_cast<float>(range));
    mob.trail.add(map_,from);
    if(remaining<static_cast<float>(step)) {
        if(static_cast<double>(size)+0.6F-static_cast<float>(range)>=length) {mob.next_chase=now;mob.path.advance();return false;}
        const auto clearance=static_cast<float>(static_cast<double>(static_cast<float>(size))+static_cast<float>(range)+0.4F);
        const auto x=static_cast<float>(static_cast<double>(mob.position.x)+static_cast<float>(static_cast<double>(nx)*length));
        const auto z=static_cast<float>(static_cast<double>(mob.position.z)+static_cast<float>(static_cast<double>(nz)*length));
        const UserPosition destination{static_cast<float>(static_cast<double>(x)-static_cast<float>(static_cast<double>(nx)*clearance)),mob.position.y,
            static_cast<float>(static_cast<double>(z)-static_cast<float>(static_cast<double>(nz)*clearance))};
        static_cast<void>(mobs_.move(mob,destination,1));
        if(!mob.definition)throw std::logic_error("Mob chase lost its definition during movement");
        const auto* live=catalog_.mob(mob.definition->id);if(!live)throw std::logic_error("Mob chase lost its catalog record during movement");
        std::uint32_t interval=0;for(unsigned i=0;i<4;++i)interval|=std::to_integer<std::uint32_t>(live->record[0x64+i])<<(8*i);
        const auto value=std::trunc((static_cast<double>(remaining)/static_cast<float>(step))*static_cast<float>(interval)+static_cast<float>(now));
        const auto deadline=!std::isfinite(value) || value<INT32_MIN || value>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(value);
        mob.next_chase=std::bit_cast<std::uint32_t>(deadline);mob.path.advance();return false;
    }
    const UserPosition destination{static_cast<float>(static_cast<double>(mob.position.x)+static_cast<float>(static_cast<double>(nx)*step)),mob.position.y,
        static_cast<float>(static_cast<double>(mob.position.z)+static_cast<float>(static_cast<double>(nz)*step))};
    static_cast<void>(mobs_.move(mob,destination,1));
    if(!mob.definition)throw std::logic_error("Mob chase lost its definition during movement");
    const auto* live=catalog_.mob(mob.definition->id);if(!live)throw std::logic_error("Mob chase lost its catalog record during movement");
    std::uint32_t interval=0;for(unsigned i=0;i<4;++i)interval|=std::to_integer<std::uint32_t>(live->record[0x64+i])<<(8*i);
    mob.next_chase=now+interval;return false;
}
bool vertical_mobs_native_chase(MobRuntime& mob,std::uint32_t now,Actor& actor,std::int32_t range,
    ZoneMobs& mobs,ZoneUsers& users,const MapDefinition& map,const MobTargetingBindingCatalog& catalog,
    MobTargetingFindSource& source,MobTargetingFindServices& targeting,VerticalMobsAiStateOwners& ai,
    VerticalMobsReturnDependencies& dependencies) {
    VerticalMobsChaseFinal final(mobs,map,catalog);
    VerticalMobsChaseIntermediate intermediate(mobs,map,catalog,final);
    VerticalMobsChaseTargetReset reset(source,targeting,ai,dependencies);
    VerticalMobsChaseSearch search(map,reset,intermediate);
    VerticalMobsChaseBlockedTarget blocked(map,users,catalog,search);
    VerticalMobsChasePath path(mobs,users,map,blocked);
    VerticalMobsChaseLeash leash(mobs,users,ai,dependencies,path);
    return vertical_mobs_chase(mob,now,actor,range,mobs,users,catalog,leash);
}
void vertical_mobs_attack_ai_prefix(MobRuntime& mob,std::uint32_t now,Actor& actor,
    const MobTargetingBindingCatalog& catalog,VerticalMobsAttackAiContinuation& continuation) {
    if(!mob.definition) throw std::invalid_argument("Mob AttackAI requires its retained definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition) throw std::invalid_argument("Mob AttackAI requires its retained catalog record");
    std::uint32_t country=0;for(unsigned i=0;i<4;++i)country|=std::to_integer<std::uint32_t>(definition->record[0x78+i])<<(8*i);
    const auto& appearance=actor.appearance();
    if(country==appearance.identity.country || actor.attack_state().mode==1 || actor.status().no_mob_targeting || !appearance.attackable || !appearance.visible) {
        if(!mob.has_applied_skills()) throw std::logic_error("Mob AttackAI requires retained applied skills");
        mob.applied_skills().owner().remove_vision_fix(mob);
        if(mob_status(mob)!=2)mob.status=0;
        mob.target_kind=0;return;
    }
    mob.in_battle=true;continuation.continue_attack_ai(mob,now,actor);
}
void VerticalMobsAttackAiScan::continue_attack_ai(MobRuntime& mob,std::uint32_t now,Actor& actor) {
    continue_before(mob,now,actor,3);
}
void VerticalMobsAttackAiScan::continue_before(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t exclusive_slot) {
    if(exclusive_slot>3)throw std::invalid_argument("Mob AttackAI scan slot out of range");
    if(!mob.definition)throw std::invalid_argument("Mob AttackAI requires its retained definition");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob AttackAI requires its retained catalog record");
    for(std::size_t slot=exclusive_slot;slot--!=0;) {
        const auto mode=std::to_integer<std::uint8_t>(definition->record[0x82+slot*20]);
        if(mode==0 || now<mob.attack_times[slot])continue;
        if(mode>=3 && mode<=5) {
            std::uint32_t maximum=0;for(unsigned i=0;i<4;++i)maximum|=std::to_integer<std::uint32_t>(definition->record[0x30+i])<<(8*i);
            const auto threshold=[&](float multiplier) {
                const auto value=std::trunc(static_cast<double>(static_cast<float>(maximum))*static_cast<double>(multiplier));
                return !std::isfinite(value) || value<INT32_MIN || value>INT32_MAX?0x80000000U:std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(value));
            };
            if(mode==3 && (mob.hp>threshold(0.4F) || mob.hp<threshold(0.2F)))continue;
            if(mode==4 && mob.hp>threshold(0.05F))continue;
            if(mode==5 && mob.hp>=threshold(0.2F))continue;
        }
        ready_.first_ready(mob,now,actor,slot);return;
    }
    constexpr std::array<std::int32_t,7> speeds{1500,1000,500,0,-500,-1000,-1500};
    const auto speed=speeds[static_cast<std::size_t>(std::clamp(mob.attack_speed,0,6))];
    mob.next_attack=now+2500U+static_cast<std::uint32_t>(speed);
}
VerticalMobsAttackAiMode6::VerticalMobsAttackAiMode6(ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,VerticalMobsActorCombatWorld& world,MobTargetingZone& targeting,
    VerticalMobsAttackAiReady& ready,VerticalMobsAttackAiAfterTarget& after)
    :mobs_(mobs),users_(users),catalog_(catalog),world_(world),targeting_(targeting),ready_(ready),after_(after) {}
void vertical_mobs_use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,
    const MobTargetingBindingSkill& skill,VerticalMobsSkillEffects& effects) {
    if(mob.sleep!=0 || mob.stun!=0)return;
    const auto kind=std::to_integer<std::uint8_t>(skill.record[0x30]);
    if(kind>=mob.attack_disabled.size())throw std::out_of_range("Mob skill attack type is outside the retained attack fields");
    if(mob.attack_disabled[kind]!=0)return;
    switch(std::to_integer<std::uint8_t>(skill.record[0x31])) {
    case 1:effects.attack(mob,actor,skill);break;
    case 2:effects.heal(mob,actor,skill);break;
    case 3:effects.debuff(mob,actor,skill);break;
    default:break;
    }
    constexpr std::array<std::int32_t,7> speeds{1500,1000,500,0,-500,-1000,-1500};
    mob.next_attack=now+2500U+static_cast<std::uint32_t>(speeds[static_cast<std::size_t>(std::clamp(mob.attack_speed,0,6))]);
}
void vertical_mobs_skill_attack(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill,
    VerticalMobsSkillAttackWorld& world,const MobDeathExperience& rules,MobDeathWorld& death) {
    const auto target=std::to_integer<std::uint8_t>(skill.record[0x56]);
    if(target==6){world.range(mob,actor,skill);return;}
    if(target==7){world.range_select(mob,actor,skill);return;}
    if(target!=3)return;
    const auto detail=std::to_integer<std::uint16_t>(skill.record[0x32]) |
        (std::to_integer<std::uint16_t>(skill.record[0x33])<<8);
    if(detail==22) {
        world.send_skill(mob,actor,skill);
        unsigned hit=0;
        while(hit<std::to_integer<std::uint8_t>(skill.record[0x58])) {
            world.attack_user(mob,actor,skill,false);
            if(actor.attack_state().mode==1)return;
            ++hit;
        }
    } else if(detail==23) {
        world.attack_user(mob,actor,skill,true);
        if(mob_status(mob)!=2 && world.self_damage(mob,mob.hp,MobCombatCredit{2,mob.id,0,0,0}))
            mob_death(mob,2,mob.id,rules,death);
    } else if(detail==21 || detail==24 || detail==25 || detail==26 || detail==27) {
        world.attack_user(mob,actor,skill,true);
    }
}
VerticalMobsBoundSkillEffects::VerticalMobsBoundSkillEffects(VerticalMobsSkillAttackWorld& attack,const MobDeathExperience& rules,
    MobDeathWorld& death,VerticalMobsSkillOtherEffects& other):attack_(attack),rules_(rules),death_(death),other_(other) {}
void VerticalMobsBoundSkillEffects::attack(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill) {vertical_mobs_skill_attack(mob,actor,skill,attack_,rules_,death_);}
void VerticalMobsBoundSkillEffects::heal(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill) {other_.heal(mob,actor,skill);}
void VerticalMobsBoundSkillEffects::debuff(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill) {other_.debuff(mob,actor,skill);}
VerticalMobsSkillDispatch::VerticalMobsSkillDispatch(VerticalMobsStationaryAttackWorld& attacks,VerticalMobsSkillEffects& skills)
    :attacks_(attacks),skills_(skills) {}
void VerticalMobsSkillDispatch::attack(MobRuntime& mob,std::uint32_t now,Actor& actor) {attacks_.attack(mob,now,actor);}
void VerticalMobsSkillDispatch::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill) {
    vertical_mobs_use_skill(mob,now,actor,skill,skills_);
}
void VerticalMobsSkillDispatch::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    attacks_.other_attack(mob,now,actor,slot);
}
void VerticalMobsAttackAiAggro::first_ready(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    if(slot>=3)throw std::invalid_argument("Mob AttackAI aggro requires a valid slot");
    world_.retarget(mob,now);
    after_.after_aggro(mob,now,actor,slot);
}
void VerticalMobsAttackAiSelfSkill::after_target_set(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot){dispatch(mob,now,actor,slot);}
void VerticalMobsAttackAiSelfSkill::after_aggro(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot){dispatch(mob,now,actor,slot);}
void VerticalMobsAttackAiSelfSkill::dispatch(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    if(slot>=3 || !mob.definition)throw std::invalid_argument("Mob AttackAI skill requires its live slot and definition");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob AttackAI skill requires its live catalog record");
    const auto at=0x80+slot*20;
    const auto type=std::to_integer<std::uint16_t>(definition->record[at])|(std::to_integer<std::uint16_t>(definition->record[at+1])<<8);
    if(type==0){world_.other_attack(mob,now,actor,slot);return;}
    const auto* skill=definition->attacks[slot].skill;
    if(!skill)throw std::logic_error("Mob AttackAI skill requires its retained skill owner");
    if(std::to_integer<std::uint8_t>(skill->record[0x31])!=2){world_.other_attack(mob,now,actor,slot);return;}
    mob.current_attack=static_cast<std::int32_t>(slot);world_.use_skill(mob,now,actor,*skill);
    if(!mob.definition)throw std::logic_error("Mob AttackAI lost its definition during skill use");
    definition=catalog_.mob(mob.definition->id);
    if(!definition || !definition->attacks[slot].skill)throw std::logic_error("Mob AttackAI lost its skill during skill use");
    const auto& live=definition->attacks[slot].skill->record;
    const auto cooldown=std::to_integer<std::uint32_t>(live[0x4a])|(std::to_integer<std::uint32_t>(live[0x4b])<<8);
    mob.attack_times[slot]=now+cooldown*1000U;mob.current_attack=0;
}
void vertical_mobs_attack_finish(MobRuntime& mob,std::uint32_t now,const MobTargetingBindingCatalog& catalog) {
    if(!mob.definition)throw std::invalid_argument("Mob attack deadline requires its live definition");
    const auto* definition=catalog.mob(mob.definition->id);if(!definition)throw std::invalid_argument("Mob attack deadline requires its live catalog record");
    std::uint32_t delay=0;for(unsigned i=0;i<4;++i)delay|=std::to_integer<std::uint32_t>(definition->record[0x84+i])<<(8*i);
    constexpr std::array<std::int32_t,7> speeds{1500,1000,500,0,-500,-1000,-1500};
    const auto interval=delay+static_cast<std::uint32_t>(speeds[static_cast<std::size_t>(std::clamp(mob.attack_speed,0,6))]);
    const auto value=std::trunc(static_cast<double>(static_cast<float>(interval))/static_cast<double>(mob.multipliers[1]));
    const auto result=!std::isfinite(value) || value<INT32_MIN || value>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(value);
    mob.next_attack=now+std::bit_cast<std::uint32_t>(result);
}
bool VerticalMobsActorAttackBlock::blocks(Actor& actor,std::uint16_t kind) {
    auto& status=actor.status();if(status.block_all!=0)return true;
    if(kind>=status.defense.size())throw std::invalid_argument("Attack block requires an original defense category");
    auto& defense=status.defense[kind];
    if(defense.block_mode==1) {
        defense.block_value=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(defense.block_value)-1U);
        if(defense.block_value==0)effects_.remove_attack_blocks(actor);
        return true;
    }
    if(defense.block_mode==2) {
        if(defense.block_value==100)return true;
        return random_.next()%100<=defense.block_value;
    }
    return false;
}
bool vertical_mobs_attack_reflected(const MobRuntime& mob,const Actor& actor,
    const MobTargetingBindingCatalog& catalog,RandomSource& random,VerticalMobsMirrorSkillDefinitions& skills) {
    if(!mob.definition || mob.current_attack<0 || mob.current_attack>=3)
        throw std::invalid_argument("Mob reflection check requires its live definition and selected slot");
    auto* definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob reflection check requires its live catalog record");
    const auto offset=0x80+static_cast<std::size_t>(mob.current_attack)*20;
    const auto kind=std::to_integer<unsigned>(definition->record[offset])|(std::to_integer<unsigned>(definition->record[offset+1])<<8);
    if(kind>=3)throw std::out_of_range("Mob reflection check requires an original attack category");
    const auto skill=actor.status().defense[kind].mirror_skill;
    if(skill==0)return false;
    const auto level=actor.status().defense[kind].mirror_level;
    const auto parameters=skills.mirror_parameters(skill,level);
    const auto mode=parameters[0],value=parameters[1];bool reflected=false;
    if(mode==1)reflected=random.next()%100<=value;
    else if(mode==2 || mode==3){
        const auto& attributes=actor.status().skills.equipment.base.attributes;
        const auto stat=mode==2?attributes.wisdom:attributes.intelligence;
        const auto product=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(stat)*10U);
        const auto roll=random.next()%100;
        if(value==0)throw std::domain_error("Mob reflection skill has a zero divisor");
        reflected=roll<=product/value;
    }
    if(!reflected)return false;
    if(!mob.definition)throw std::logic_error("Mob reflection check lost its definition");
    definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::logic_error("Mob reflection check lost its catalog record");
    const auto melee_skill=actor.status().defense[0].mirror_skill;
    for(std::size_t i=0;i<6;++i)
        if(std::to_integer<std::int32_t>(definition->record[0x55+i])==melee_skill)return false;
    return true;
}
bool vertical_mobs_attack_success(MobRuntime& mob,Actor& actor,std::uint16_t kind,RandomSource& random,VerticalMobsAttackBlock& block) {
    if(block.blocks(actor,kind))return false;
    if(kind>2)return true;
    if(!mob.definition)throw std::invalid_argument("Mob AttackSuccess requires its retained definition");
    vertical_combat_require_float_environment();
    const auto spill=[](std::int32_t n)->double {volatile float result=static_cast<float>(n);return result;};
    const auto sum=[](std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));};
    const auto mul=[](double a,double b){volatile double result=a*b;return result;};
    const auto div=[](double a,double b){volatile double result=a/b;return result;};
    const auto add=[](double a,double b){volatile double result=a+b;return result;};
    const auto sub=[](double a,double b){volatile double result=a-b;return result;};
    const auto integer=[](double n){n=std::trunc(n);return !std::isfinite(n) || n<INT32_MIN || n>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(n);};
    const auto& user=actor.status().calculated_attack[kind];const auto& attacker=mob.attack[kind];
    const auto player_level=static_cast<std::int32_t>(actor.status().level),mob_level=static_cast<std::int32_t>(mob.definition->level);
    if(kind==2) {
        const auto denominator=static_cast<float>(player_level+mob_level);
        const auto level_term=static_cast<float>(add(mul(div(add(mul(spill(player_level-mob_level-2),100.0),spill(player_level)),denominator),double(1.1F)),spill(user.evasion)));
        const auto dex=actor.status().skills.equipment.base.attributes.dexterity,enemy=static_cast<std::int32_t>(mob.definition->dexterity);
        const auto numerator=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(dex)*11U-static_cast<std::uint32_t>(enemy)*10U);
        const auto dex_term=static_cast<float>(add(mul(div(spill(numerator),spill(sum(dex,enemy))),double(3.9F)),spill(attacker.accuracy)));
        const auto clamp=[](double value){return value<1.0?1.0:value>70.0?70.0:value;};
        const auto chance=std::clamp(integer(add(clamp(level_term),clamp(dex_term))),1,90);
        return random.next()%100>=chance;
    }
    const auto level=static_cast<float>(mob_level),levels=static_cast<float>(player_level+mob_level);
    const auto level_part=mul(div(level,levels),160.0);
    const auto defense=mul(div(spill(user.accuracy),spill(sum(attacker.evasion,user.accuracy))),100.0);
    const auto accuracy=mul(div(spill(attacker.accuracy),spill(sum(user.evasion,attacker.accuracy))),100.0);
    const auto chance=std::clamp(integer(sub(level_part,sub(defense,accuracy))),5,95);
    return random.next()%100<chance;
}
std::int32_t vertical_mobs_attack_critical_chance(const MobRuntime& mob,const Actor& actor,std::size_t kind) {
    if(kind>=mob.attack.size())throw std::invalid_argument("Mob critical chance requires an original attack category");
    vertical_combat_require_float_environment();
    volatile float critical=static_cast<float>(mob.attack[kind].critical_attack);
    volatile float wisdom=static_cast<float>(actor.status().skills.equipment.base.attributes.wisdom);
    volatile double scaled=static_cast<double>(critical)/100.0;
    volatile double restored=scaled*100.0;
    volatile double penalty=static_cast<double>(wisdom)*static_cast<double>(0.034F);
    const auto value=std::trunc(restored-penalty);
    return !std::isfinite(value) || value<INT32_MIN || value>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(value);
}
bool vertical_mobs_attack_critical(const MobRuntime& mob,const Actor& actor,std::size_t kind,RandomSource& random) {
    const auto chance=std::clamp(vertical_mobs_attack_critical_chance(mob,actor,kind),5,99);
    return random.next()%100<chance;
}
void vertical_mobs_attack_damage_limits(std::array<std::int32_t,3>& damage,RandomSource& random) {
    for(auto& value:damage)value=std::min(value,30000);
    if(damage[0]<=10)damage[0]=random.next()%10+1;
}
bool vertical_mobs_normal_attack_damage(const MobRuntime& mob,const Actor& actor,
    const MobTargetingBindingCatalog& catalog,RandomSource& random,std::array<std::int32_t,3>& damage) {
    if(!mob.definition || mob.current_attack<0 || mob.current_attack>=3)
        throw std::invalid_argument("Mob AttackDamage requires a retained definition and selected slot");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob AttackDamage requires its live catalog record");
    const auto at=0x80+static_cast<std::size_t>(mob.current_attack)*20;
    const auto word=[&](std::size_t offset){return std::to_integer<std::uint16_t>(definition->record[offset])|(std::to_integer<std::uint16_t>(definition->record[offset+1])<<8);};
    const auto kind=word(at),base=word(at+10),variance=word(at+12);
    damage.fill(0);
    if(kind>2){vertical_mobs_attack_damage_limits(damage,random);return false;}
    vertical_combat_require_float_environment();
    const auto spill=[](std::int32_t n)->double{volatile float v=static_cast<float>(n);return v;};
    const auto store=[](double n)->float{volatile float v=static_cast<float>(n);return v;};
    const auto add=[](double a,double b){volatile double v=a+b;return v;};
    const auto sub=[](double a,double b){volatile double v=a-b;return v;};
    const auto mul=[](double a,double b){volatile double v=a*b;return v;};
    const auto div=[](double a,double b){volatile double v=a/b;return v;};
    const auto ml=static_cast<std::int32_t>(mob.definition->level),ul=static_cast<std::int32_t>(actor.status().level);
    const auto level=std::clamp(store(mul(div(add(mul(spill(ml-ul-1),100.0),spill(ml)),100.0),10.0)),1.0F,95.0F);
    float attack=store(add(mul(spill(base),mob.multipliers[0]),spill(mob.attack[kind].attack)));
    if(variance>0)attack=store(add(attack,spill(random.next()%variance)));
    const auto attribute=std::bit_cast<std::int32_t>(actor.status().skills.equipment.defense_attribute);
    constexpr std::array<float,9> factors{1.0F,0.8F,0.8F,0.8F,0.8F,0.7F,0.7F,0.7F,0.7F};
    if(attribute<0)throw std::out_of_range("Mob AttackDamage negative defense attribute");
    const auto factor=attribute>=9?1.0F:factors[static_cast<std::size_t>(attribute)];
    const auto wisdom=mob.definition->wisdom;
    const auto wisdom_roll=[&](){return wisdom==0?0:random.next()%wisdom+1;};
    const auto first=wisdom_roll();
    const auto defense=spill(actor.status().calculated_attack[kind].defense);
    const auto attack_part=mul(add(attack,mul(div(attack,100.0),level)),factor);
    const auto defense_part=sub(defense,mul(div(defense,100.0),level));
    float value=store(add(sub(attack_part,defense_part),mul(spill(first),double(0.1F))));
    if(value<0.0F)value=0.0F;
    const bool critical=vertical_mobs_attack_critical(mob,actor,kind,random);
    if(critical){
        const auto bonus=store(mul(spill(wisdom_roll()),double(1.4F)));
        value=store(add(add(mul(value,1.5),mul(div(bonus,100.0),level)),bonus));
    }
    const auto integer=std::trunc(static_cast<double>(value));
    damage[0]=!std::isfinite(integer)||integer<INT32_MIN||integer>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(integer);
    vertical_mobs_attack_damage_limits(damage,random);
    return critical;
}
bool vertical_mobs_logged_normal_attack_damage(const MobRuntime& mob,const Actor& actor,
    const MobTargetingBindingCatalog& catalog,RandomSource& random,std::array<std::int32_t,3>& damage,
    MobLogContext context,MobLogTransport& transport) {
    const bool critical=vertical_mobs_normal_attack_damage(mob,actor,catalog,random,damage);
    if(mob.lua_ai!=0 || (mob.respawn && mob.respawn->both))
        send_mob_log(mob,context,1,actor.appearance().identity.name,std::nullopt,
            std::bit_cast<std::uint32_t>(damage[0]),transport);
    return critical;
}
VerticalMobsPreparedAttackEffects::VerticalMobsPreparedAttackEffects(ActorActionWorld& actions,DuelWorld& duel,VerticalMobsAttackEffects& continuation)
    :actions_(actions),duel_(duel),continuation_(continuation) {}
void VerticalMobsPreparedAttackEffects::status_stand(Actor& actor){continuation_.status_stand(actor);}
void VerticalMobsPreparedAttackEffects::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){continuation_.send_miss(mob,actor,packet);}
void VerticalMobsPreparedAttackEffects::hit(MobRuntime& mob,std::uint32_t now,Actor& actor){
    actor.stop_casting(actions_);
    if(actor.relations().duel_state!=0)actor.leave_duel_on_attack(duel_);
    continuation_.hit(mob,now,actor);
}
void VerticalMobsHitDelivery::status_stand(Actor& actor){world_.status_stand(actor);}
void VerticalMobsHitDelivery::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){world_.send_miss(mob,actor,packet);}
void VerticalMobsHitDelivery::finish_hit(MobRuntime& mob,std::uint32_t now,Actor& actor,bool reflected,bool critical,std::array<std::int32_t,3>& damage){
    bool killed=false;
    if(!reflected){
        killed=world_.damage_user(mob,actor,damage);
        if(killed)world_.user_killed(mob,actor);
    }
    net::Packet packet{5,6,static_cast<std::uint8_t>(critical)};
    const auto put=[&](std::uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    put(mob.id,4);const auto target=actor.id();put(target,4);
    for(const auto value:damage)put(reflected?0U:std::bit_cast<std::uint32_t>(value),2);
    if(mob.zone)world_.send_hit(mob,target,packet);
    world_.finish_after_hit(mob,now,actor,reflected,critical,killed,damage);
}
VerticalMobsUserDamageSceneWorld::VerticalMobsUserDamageSceneWorld(Actor& actor,ActorScene& scene,VerticalMobsUserDamageLedger& ledger)
    :actor_(actor),scene_(scene),ledger_(ledger) {}
void VerticalMobsUserDamageSceneWorld::require_actor(const Actor& actor) const {
    if(&actor!=&actor_)throw std::invalid_argument("Mob damage transport requires its retained Actor");
}
void VerticalMobsUserDamageSceneWorld::record(Actor& actor,std::int32_t kind,std::uint32_t source,std::int32_t amount) {
    require_actor(actor);ledger_.add(actor,kind,source,amount);
}
void VerticalMobsUserDamageSceneWorld::send_reduction(Actor& actor,std::uint16_t amount) {
    require_actor(actor);scene_.send_self(net::Packet{0x25,0x05,static_cast<std::uint8_t>(amount),static_cast<std::uint8_t>(amount>>8)});
}
void VerticalMobsUserDamageSceneWorld::send_energy(Actor& actor) {
    require_actor(actor);const auto party=scene_.party_kind();if(!party)return;
    const auto values=actor.resources();net::Packet packet(12,0);const auto put=[&](std::size_t at,std::uint32_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)packet[at+i]=static_cast<std::uint8_t>(n>>(i*8));};
    put(0,*party?0xc0c:0xc03,2);put(2,actor.id(),4);put(6,values.hp,2);put(8,values.sp,2);put(10,values.mp,2);
    scene_.send_party(packet);
}
bool vertical_mobs_user_damage(MobRuntime& mob,Actor& actor,std::array<std::int32_t,3>& damage,
    const SkillDefinitions& skills,VerticalMobsUserDamageWorld& world) {
    if(actor.attack_state().mode==1 || actor.resources().hp==0)return false;
    const auto source=mob.id;actor.wear_defense();world.record(actor,2,source,damage[0]);
    auto current=actor.resources();
    const auto signed32=[](std::uint32_t n){return std::bit_cast<std::int32_t>(n);};
    const auto subtract=[&](std::uint32_t& resource,std::int32_t amount) {
        if(signed32(resource)<amount)resource=0;
        else resource-=static_cast<std::uint32_t>(amount);
    };
    subtract(current.sp,damage[1]);subtract(current.mp,damage[2]);
    const auto redirect=[&](std::uint32_t& resource,std::int32_t& part) {
        if(signed32(resource)<damage[0]) {
            damage[0]=signed32(static_cast<std::uint32_t>(damage[0])-resource);
            part=signed32(static_cast<std::uint32_t>(part)+resource);resource=0;
        } else {
            resource-=static_cast<std::uint32_t>(damage[0]);
            part=signed32(static_cast<std::uint32_t>(part)+static_cast<std::uint32_t>(damage[0]));damage[0]=0;
        }
    };
    if(damage[0]!=0) {
        switch(actor.status().giveup_sp_mp) {
        case 1:redirect(current.sp,damage[1]);break;
        case 2:redirect(current.mp,damage[2]);break;
        case 3:redirect(current.sp,damage[1]);redirect(current.mp,damage[2]);break;
        default:break;
        }
    }
    const auto protection=actor.status().skills.equipment.damage_reduction;
    std::uint16_t reduced=0;
    if(protection!=0 && damage[0]!=0) {
        if(damage[0]>protection) {reduced=static_cast<std::uint16_t>(protection);damage[0]=signed32(static_cast<std::uint32_t>(damage[0])-static_cast<std::uint32_t>(protection));}
        else {reduced=static_cast<std::uint16_t>(static_cast<std::uint32_t>(damage[0])-1U);damage[0]=1;}
    }
    actor.replace_resources_raw(current);
    if(reduced!=0) {world.send_reduction(actor,reduced);current=actor.resources();}
    const bool killed=signed32(current.hp)<=damage[0];
    if(killed) current={0,0,0};else current.hp-=static_cast<std::uint32_t>(damage[0]);
    actor.replace_resources_raw(current);world.send_energy(actor);
    if(!killed)actor.check_hp_ability(skills);
    return killed;
}
VerticalMobsOrdinaryUserDeath::VerticalMobsOrdinaryUserDeath(const MobTargetingBindingCatalog& catalog,MobTargetingOrdinaryDeathWorld& world)
    :catalog_(catalog),world_(world) {}
void VerticalMobsOrdinaryUserDeath::death(Actor& actor,std::uint32_t mob_id){mob_targeting_death_ordinary(actor,2,mob_id,catalog_,world_);}
void vertical_mobs_user_killed(MobRuntime& mob,Actor& actor,ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,MobTargetingZone& targeting,VerticalMobsAiStateOwners& ai,
    VerticalMobsReturnDependencies& dependencies,VerticalMobsUserDeath& death){
    if(!mob.has_applied_skills())throw std::logic_error("Mob user death requires the retained applied-skill owner");
    mob.applied_skills().owner().remove_vision_fix(mob);
    if(!mob.use_pais){
        if(mob_status(mob)!=2)mob.status=0;
        mob.target_kind=0;
    }else{
        if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob || !mob.definition)
            throw std::logic_error("Mob user death targeting requires the live zone subject");
        const auto* definition=catalog.mob(mob.definition->id);
        if(!definition)throw std::logic_error("Mob user death targeting requires the live catalog record");
        const auto count=users.size();if(count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::overflow_error("Targeting user count exceeds original signed count");
        MobTargetingFindSource source{static_cast<std::int32_t>(count),mobs.cell_count(),mobs.cell_count(),mob.cell->x,mob.cell->z,
            mob.position.x,mob.position.z,std::to_integer<std::uint8_t>(definition->record[0x26]),std::to_integer<std::uint8_t>(definition->record[0x69])};
        ActorTargetSearch find(mob,users,targeting,catalog,source);
        if(!mob_find_target(source,find))reset_mob_ai_configured(mob,ai.configured_fsm(mob),dependencies.variables(mob),ai.configured_services(mob));
    }
    death.death(actor,mob.id);
}
void vertical_mobs_defense_options(MobRuntime& mob,Actor& actor,VerticalMobsDefenseOptions& world) {
    auto& active=world.active(actor);
    if(active || mob_status(mob)==2 || actor.attack_state().mode==1)return;
    active=1;struct Guard {std::uint8_t& flag;~Guard(){flag=0;}} guard{active};
    constexpr std::array<std::uint8_t,6> slots{6,0,1,3,2,4};
    for(const auto slot:slots) {
        auto held=actor.retained_inventory_item(0,slot);
        if(!held) {if(!actor.inventory_slot_empty(0,slot))throw std::logic_error("Defensive options require retained equipment");continue;}
        for(std::size_t i=0;i<6;++i) {
            held=actor.retained_inventory_item(0,slot);
            if(!held || !held->item || !held->pool || held->item->item.bag!=0 || held->item->item.slot!=slot)
                throw std::logic_error("Defensive option equipment lost its retained owner");
            const auto id=held->item->item.gems[i];if(!id)continue;
            const auto item=world.jewel(id);if(item.empty())continue;
            if(item.size()<0x47)throw std::logic_error("Defensive option requires complete jewel definition");
            if(item[0x24]==std::byte{0} || item[0x25]==std::byte{0})continue;
            const auto skill_id=std::to_integer<std::uint8_t>(item[0x46]);if(!skill_id || skill_id==16)continue;
            const auto* skill=world.mob_skill(skill_id);if(!skill)continue;
            if(mob.lua_ai!=0 || (mob.respawn && mob.respawn->both)) {
                auto& user=mob.previous_attack_user;auto& name=mob.previous_attack_skill;
                const auto user_end=std::find(user.begin(),user.end(),0),name_end=std::find(name.begin(),name.end(),0);
                if(user_end==user.end() || name_end==name.end())throw std::logic_error("Defensive option prior name exceeds buffer");
                const auto saved_user=user,saved_name=name;
                const auto length=std::min(skill->name.size(),skill->name.find('\0'));
                if(length>=name.size())throw std::invalid_argument("Defensive option skill name exceeds buffer");
                std::copy_n(skill->name.begin(),length,name.begin());name[length]=0;
                world.activate(actor,mob,*skill);
                std::copy_n(saved_user.begin(),static_cast<std::size_t>(user_end-user.begin())+1,user.begin());
                std::copy_n(saved_name.begin(),static_cast<std::size_t>(name_end-name.begin())+1,name.begin());
            } else world.activate(actor,mob,*skill);
        }
    }
}
VerticalMobsPriorityHitTailWorld::VerticalMobsPriorityHitTailWorld(ZoneMobs& mobs,ZoneUsers& users,UserPriorityAreaServices& area)
    :mobs_(mobs),users_(users),area_(area) {}
VerticalMobsSharedPriorityCells::VerticalMobsSharedPriorityCells(UserViewSharedBuffers& buffers,MobRuntime& mob,VerticalMobsPriorityConnections& connections)
    :UserPrioritySharedCellServices(buffers),mob_(mob),connections_(connections) {}
std::shared_ptr<void> VerticalMobsSharedPriorityCells::retain(Actor& actor){return connections_.retain(actor);}
UserPosition VerticalMobsSharedPriorityCells::origin(){return mob_.position;}
PartyMembershipUser& VerticalMobsSharedPriorityCells::connection(Actor& actor){return connections_.connection(actor);}
UserPriorityQueue& VerticalMobsSharedPriorityCells::connection_queue(Actor& actor){return connections_.queue(actor);}
VerticalMobsSkillPriorityWorld::VerticalMobsSkillPriorityWorld(VerticalMobsPriorityHitTailWorld& views):views_(views) {}
void vertical_mobs_skill_announcement(MobRuntime& mob,std::uint32_t target,const MobTargetingBindingSkill& skill,VerticalMobsPriorityHitTailWorld& views) {
    net::Packet packet{0x0b,0x06,4};const auto append=[&](std::uint32_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)packet.push_back(static_cast<std::uint8_t>(n>>(i*8)));};
    append(mob.id,4);append(target,4);append(static_cast<std::uint32_t>(mob.current_attack),1);
    append(std::to_integer<std::uint32_t>(skill.record[0]),1);append(std::to_integer<std::uint32_t>(skill.record[1]),1);
    append(std::to_integer<std::uint32_t>(skill.record[2]),1);packet.resize(21,0);
    if(mob.zone)views.send_hit(mob,target,packet);
}
void VerticalMobsSkillPriorityWorld::send_skill(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill) {
    vertical_mobs_skill_announcement(mob,actor.id(),skill,views_);
}
void vertical_mobs_heal(MobRuntime& mob,std::array<std::int32_t,3>& values,
    const MobTargetingBindingCatalog& catalog,const MobLogContext& context,MobLogTransport& log) {
    if(!mob.definition)throw std::logic_error("Mob heal requires its retained definition");
    const auto* def=catalog.mob(mob.definition->id);if(!def)throw std::logic_error("Mob heal requires its retained catalog record");
    const auto read=[&](std::size_t at,unsigned n){std::uint32_t out=0;for(unsigned i=0;i<n;++i)out|=std::to_integer<std::uint32_t>(def->record[at+i])<<(8*i);return out;};
    const auto max_hp=read(0x30,4),max_mp=read(0x34,2),max_sp=read(0x36,2),before=mob.hp;
    auto hp=mob.hp+static_cast<std::uint32_t>(values[0]);if(hp>max_hp){values[0]=std::bit_cast<std::int32_t>(max_hp-mob.hp);hp=max_hp;}
    const auto clamp=[&](std::uint16_t current,std::int32_t& amount,std::uint32_t maximum){
        const auto result=static_cast<std::uint32_t>(current)+static_cast<std::uint32_t>(amount);
        if(std::bit_cast<std::int32_t>(result)>static_cast<std::int32_t>(maximum))amount=std::bit_cast<std::int32_t>(maximum-static_cast<std::uint32_t>(current));
        return static_cast<std::uint16_t>(static_cast<std::uint32_t>(current)+static_cast<std::uint32_t>(amount));
    };
    const auto mp=clamp(mob.mp,values[1],max_mp),sp=clamp(mob.sp,values[2],max_sp);
    const bool reset=hp==max_hp && before!=hp;if(reset && !mob.damage)throw std::logic_error("Mob heal requires its retained damage ledger");
    mob.hp=hp;mob.mp=mp;mob.sp=sp;if(reset)mob.damage->reset();
    if(mob.lua_ai!=0 || (mob.respawn && mob.respawn->both))send_mob_log(mob,context,3,std::nullopt,std::nullopt,std::bit_cast<std::uint32_t>(values[0]),log);
}
VerticalMobsAreaSkillHealWorld::VerticalMobsAreaSkillHealWorld(ZoneMobs& mobs,const MobTargetingBindingCatalog& catalog,
    VerticalMobsPriorityHitTailWorld& views,MobLogTransport& log):mobs_(mobs),catalog_(catalog),views_(views),log_(log) {}
void VerticalMobsAreaSkillHealWorld::range(MobRuntime& caster,Actor& actor,const MobTargetingBindingSkill& skill) {
    vertical_mobs_skill_announcement(caster,0,skill,views_);
    if(caster.zone!=&mobs_ || !caster.cell || mobs_.find(caster.id)!=&caster)throw std::logic_error("Mob area heal requires its retained live zone subject");
    const auto xmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(caster.cell->x)-2);
    const auto xmax=std::min<std::int64_t>(mobs_.cell_count()-1,static_cast<std::int64_t>(caster.cell->x)+2);
    const auto zmin=std::max<std::int64_t>(0,static_cast<std::int64_t>(caster.cell->z)-2);
    const auto zmax=std::min<std::int64_t>(mobs_.cell_count()-1,static_cast<std::int64_t>(caster.cell->z)+2);
    const auto definition=[&](const MobRuntime& m)->const MobTargetingBindingDefinition& {
        if(!m.definition)throw std::logic_error("Mob area heal requires each live definition");
        const auto* def=catalog_.mob(m.definition->id);if(!def)throw std::logic_error("Mob area heal requires each live catalog record");return *def;
    };
    const auto read=[](const auto& record,std::size_t at,unsigned count){std::uint32_t n=0;for(unsigned i=0;i<count;++i)n|=std::to_integer<std::uint32_t>(record[at+i])<<(i*8);return n;};
    for(auto z=zmin;z<=zmax;++z)for(auto x=xmin;x<=xmax;++x) {
        mobs_.visit_cell({static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)},[&](MobRuntime& target){
            const auto country=read(definition(caster).record,0x78,4);
            if(country!=2 && read(definition(target).record,0x78,4)!=country)return;
            if(mob_status(target)==2)return;
            const float dx=target.position.x-caster.position.x,dz=target.position.z-caster.position.z;
            const auto distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
            if(!(distance<=static_cast<float>(read(skill.record,0x57,1))))return;
            auto pin=retain(target);if(!pin)throw std::logic_error("Mob area heal requires a target pool pin");
            const auto detail=read(skill.record,0x32,2);std::array<std::int32_t,3> values{};
            if(detail==41){
                values={static_cast<std::int32_t>(read(skill.record,0x88,2)),static_cast<std::int32_t>(read(skill.record,0x8a,2)),static_cast<std::int32_t>(read(skill.record,0x8c,2))};
                if(values[0]!=0)values[0]+=static_cast<std::int32_t>(read(definition(caster).record,0x3a,2));
                vertical_mobs_heal(target,values,catalog_,context(target),log_);
            }else if(detail==42)remove_state(target,skill);else return;
            net::Packet packet{0x0d,0x06,0};const auto append=[&](std::uint32_t n,unsigned count){for(unsigned i=0;i<count;++i)packet.push_back(static_cast<std::uint8_t>(n>>(i*8)));};
            append(target.id,4);append(0,4);append(read(skill.record,0,2),2);append(read(skill.record,2,1),1);
            for(const auto value:values)append(std::bit_cast<std::uint32_t>(value),2);
            if(target.zone)views_.send_hit(target,actor.id(),packet);
        });
    }
}
VerticalMobsBoundSkillHealEffects::VerticalMobsBoundSkillHealEffects(const MobTargetingBindingCatalog& catalog,
    VerticalMobsPriorityHitTailWorld& views,const MobLogContext& context,MobLogTransport& log,VerticalMobsSkillHealWorld& world)
    :catalog_(catalog),views_(views),log_context_(context),log_(log),world_(world) {}
void VerticalMobsBoundSkillHealEffects::heal(MobRuntime& mob,Actor& actor,const MobTargetingBindingSkill& skill) {
    if(std::to_integer<std::uint8_t>(skill.record[0x56])!=2){world_.range(mob,actor,skill);return;}
    const auto word=[&](std::size_t at){return std::to_integer<std::uint32_t>(skill.record[at])|(std::to_integer<std::uint32_t>(skill.record[at+1])<<8);};
    const auto detail=word(0x32);std::array<std::int32_t,3> values{};
    if(detail==41){
        values={static_cast<std::int32_t>(word(0x88)),static_cast<std::int32_t>(word(0x8a)),static_cast<std::int32_t>(word(0x8c))};
        if(values[0]!=0){if(!mob.definition)throw std::logic_error("Mob skill heal requires its definition");const auto* def=catalog_.mob(mob.definition->id);if(!def)throw std::logic_error("Mob skill heal requires its catalog record");values[0]+=static_cast<std::int32_t>(std::to_integer<std::uint32_t>(def->record[0x3a])|(std::to_integer<std::uint32_t>(def->record[0x3b])<<8));}
        vertical_mobs_heal(mob,values,catalog_,log_context_,log_);
    }else if(detail==42)world_.remove_state(mob,skill);else return;
    net::Packet packet{0x0b,0x06,0};const auto append=[&](std::uint32_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)packet.push_back(static_cast<std::uint8_t>(n>>(i*8)));};
    append(mob.id,4);append(0,4);append(static_cast<std::uint32_t>(mob.current_attack),1);append(word(0),2);append(std::to_integer<std::uint8_t>(skill.record[2]),1);
    for(const auto value:values)append(std::bit_cast<std::uint32_t>(value),2);
    if(mob.zone)views_.send_hit(mob,actor.id(),packet);
}
MobCombatCredit vertical_mobs_reflected_credit(const Actor& actor) noexcept{return {1,actor.id(),0,0,0};}
void VerticalMobsPriorityHitTailWorld::send(MobRuntime& mob,std::uint32_t target,const net::Packet& packet){
    if(mob.zone!=&mobs_)throw std::invalid_argument("Mob priority view requires its retained zone");
    class LiveMobArea final:public UserPriorityAreaServices {
    public:
        LiveMobArea(MobRuntime& subject,UserPriorityAreaServices& delegate):mob_(subject),delegate_(delegate){}
        std::unique_ptr<UserViewBuffer> allocate(const net::Packet& value) override{return delegate_.allocate(value);}
        std::shared_ptr<void> retain(Actor& actor) override{return delegate_.retain(actor);}
        UserPosition origin() override{return mob_.position;}
        void immediate(Actor& actor,UserViewBuffer& buffer) override{delegate_.immediate(actor,buffer);}
        void priority(Actor& actor,UserViewBuffer& buffer,std::uint8_t priority,float distance) override{delegate_.priority(actor,buffer,priority,distance);}
    private:MobRuntime& mob_;UserPriorityAreaServices& delegate_;
    } services(mob,area_);
    user_priority_area(users_,mobs_.cell_count(),mobs_.cell_count(),packet,0,60.0F,false,0,target,6,services);
}
void VerticalMobsPriorityHitTailWorld::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){send(mob,actor.id(),packet);}
void VerticalMobsPriorityHitTailWorld::send_hit(MobRuntime& mob,std::uint32_t target,const net::Packet& packet){send(mob,target,packet);}
void VerticalMobsPriorityHitTailWorld::send_reflection(MobRuntime& mob,std::uint32_t target,const net::Packet& packet){send(mob,target,packet);}
VerticalMobsHitTail::VerticalMobsHitTail(VerticalMobsHitTailWorld& world,const MobDeathExperience& rules,MobDeathWorld& death)
    :world_(world),death_rules_(rules),death_world_(death) {}
void VerticalMobsHitTail::status_stand(Actor& actor){world_.status_stand(actor);}
void VerticalMobsHitTail::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){world_.send_miss(mob,actor,packet);}
bool VerticalMobsHitTail::damage_user(MobRuntime& mob,Actor& actor,std::array<std::int32_t,3>& damage){return world_.damage_user(mob,actor,damage);}
void VerticalMobsHitTail::user_killed(MobRuntime& mob,Actor& actor){world_.user_killed(mob,actor);}
void VerticalMobsHitTail::send_hit(MobRuntime& mob,std::uint32_t excluded,const net::Packet& packet){world_.send_hit(mob,excluded,packet);}
void VerticalMobsHitTail::finish_after_hit(MobRuntime& mob,std::uint32_t,Actor& actor,bool reflected,bool,bool killed,std::array<std::int32_t,3>& damage){
    if(mob.lua_ai==0 && reflected && !killed){
        const bool named=mob.respawn && mob.respawn->both;
        if(named){
            const auto& name=actor.appearance().identity.name;const auto length=name.find('\0');
            const auto count=length==std::string::npos?name.size():length;
            if(count>=mob.previous_attack_user.size())throw std::invalid_argument("Mob reflected attack user name exceeds original buffer");
            std::copy_n(name.begin(),count,mob.previous_attack_user.begin());mob.previous_attack_user[count]=0;
        }
        if(world_.damage_mob(mob,actor,damage)){
            mob_combat_stop(actor);
            mob_death(mob,1,actor.id(),death_rules_,death_world_);
        }
        if(named){mob.previous_attack_user[0]=0;mob.previous_attack_skill[0]=0;}
        net::Packet packet{0x1b,5};
        const auto put=[&](std::uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
        const auto target=actor.id();put(target,4);put(mob.id,4);
        for(const auto value:damage)put(std::bit_cast<std::uint32_t>(value),2);
        if(mob.zone)world_.send_reflection(mob,target,packet);
    }
    world_.final_attack_effects(mob,actor);
}
VerticalMobsCalculatedHitEffects::VerticalMobsCalculatedHitEffects(const MobTargetingBindingCatalog& catalog,RandomSource& random,
    VerticalMobsMirrorSkillDefinitions& skills,const MobLogContext& context,MobLogTransport& transport,VerticalMobsCalculatedHitServices& continuation)
    :catalog_(catalog),random_(random),skills_(skills),log_context_(context),log_transport_(transport),continuation_(continuation) {}
void VerticalMobsCalculatedHitEffects::status_stand(Actor& actor){continuation_.status_stand(actor);}
void VerticalMobsCalculatedHitEffects::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){continuation_.send_miss(mob,actor,packet);}
void VerticalMobsCalculatedHitEffects::hit(MobRuntime& mob,std::uint32_t now,Actor& actor){
    const bool reflected=vertical_mobs_attack_reflected(mob,actor,catalog_,random_,skills_);
    std::array<std::int32_t,3> damage{};
    const bool critical=vertical_mobs_logged_normal_attack_damage(mob,actor,catalog_,random_,damage,log_context_,log_transport_);
    continuation_.finish_hit(mob,now,actor,reflected,critical,damage);
}
VerticalMobsNormalAttackTarget::VerticalMobsNormalAttackTarget(RandomSource& random,VerticalMobsAttackBlock& block,VerticalMobsAttackEffects& effects)
    :random_(random),block_(block),effects_(effects) {}
void VerticalMobsNormalAttackTarget::status_stand(Actor& actor){effects_.status_stand(actor);}
bool VerticalMobsNormalAttackTarget::attack_success(MobRuntime& mob,Actor& actor,std::uint16_t kind){return vertical_mobs_attack_success(mob,actor,kind,random_,block_);}
void VerticalMobsNormalAttackTarget::send_miss(MobRuntime& mob,Actor& actor,const net::Packet& packet){effects_.send_miss(mob,actor,packet);}
void VerticalMobsNormalAttackTarget::hit(MobRuntime& mob,std::uint32_t now,Actor& actor){effects_.hit(mob,now,actor);}
VerticalMobsAttackTarget::VerticalMobsAttackTarget(const MobTargetingBindingCatalog& catalog,
    VerticalMobsAttackTargetServices& target,VerticalMobsStationaryAttackWorld& other):catalog_(catalog),target_(target),other_(other) {}
void VerticalMobsAttackTarget::attack(MobRuntime& mob,std::uint32_t now,Actor& actor) {
    if(mob.sleep!=0 || mob.stun!=0 || mob.attack_disabled[0]!=0)return;
    target_.status_stand(actor);
    if(!mob.definition || mob.current_attack<0 || mob.current_attack>=3)
        throw std::logic_error("Mob Attack requires the live definition and selected slot after StatusStand");
    const auto* definition=catalog_.mob(mob.definition->id);if(!definition)throw std::logic_error("Mob Attack lost its catalog record");
    const auto at=0x80+static_cast<std::size_t>(mob.current_attack)*20;
    const auto type=std::to_integer<std::uint16_t>(definition->record[at])|(std::to_integer<std::uint16_t>(definition->record[at+1])<<8);
    if(target_.attack_success(mob,actor,static_cast<std::uint16_t>(type)))target_.hit(mob,now,actor);
    else if(mob.zone) {
        net::Packet packet{5,6,3};
        const auto append=[&](std::uint32_t value){for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
        append(mob.id);append(actor.id());packet.resize(17,0);target_.send_miss(mob,actor,packet);
    }
    vertical_mobs_attack_finish(mob,now,catalog_);
}
void VerticalMobsAttackTarget::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill){other_.use_skill(mob,now,actor,skill);}
VerticalMobsNormalAttackComposition::VerticalMobsNormalAttackComposition(const MobTargetingBindingCatalog& catalog,RandomSource& random,VerticalMobsBlockEffects& block,
    VerticalMobsMirrorSkillDefinitions& skills,const MobLogContext& context,MobLogTransport& log,ActorActionWorld& actions,DuelWorld& duel,
    VerticalMobsHitTailWorld& world,const MobDeathExperience& rules,MobDeathWorld& death,VerticalMobsStationaryAttackWorld& other)
    :block_(random,block),tail_(world,rules,death),delivery_(tail_),calculation_(catalog,random,skills,context,log,delivery_),
     preparation_(actions,duel,calculation_),target_(random,block_,preparation_),attack_(catalog,target_,other) {}
void VerticalMobsNormalAttackComposition::attack(MobRuntime& mob,std::uint32_t now,Actor& actor){attack_.attack(mob,now,actor);}
void VerticalMobsNormalAttackComposition::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill){attack_.use_skill(mob,now,actor,skill);}
void VerticalMobsNormalAttackComposition::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot){attack_.other_attack(mob,now,actor,slot);}
void VerticalMobsAttackTarget::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot){other_.other_attack(mob,now,actor,slot);}
void VerticalMobsAttackGuard::attack(MobRuntime& mob,std::uint32_t now,Actor& actor) {
    if(mob.sleep!=0 || mob.stun!=0 || mob.attack_disabled[0]!=0)return;
    world_.attack(mob,now,actor);
}
void VerticalMobsAttackGuard::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill) {
    world_.use_skill(mob,now,actor,skill);
}
void VerticalMobsAttackGuard::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    world_.other_attack(mob,now,actor,slot);
}
VerticalMobsAttackAiStationary::VerticalMobsAttackAiStationary(ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,VerticalMobsStationaryAttackWorld& world)
    :mobs_(mobs),users_(users),catalog_(catalog),world_(world) {}
void VerticalMobsAttackAiStationary::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill){world_.use_skill(mob,now,actor,skill);}
void VerticalMobsAttackAiStationary::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    if(slot>=3 || !mob.definition)throw std::invalid_argument("Mob stationary attack requires its live slot and definition");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob stationary attack requires its live catalog record");
    const auto& record=definition->record;const auto byte=[&](std::size_t at){return std::to_integer<std::uint8_t>(record[at]);};
    const auto at=0x80+slot*20;const bool stationary=byte(0x26)==4 || byte(0x68)==0 || byte(0x7c)!=0;
    if(!stationary){world_.other_attack(mob,now,actor,slot);return;}
    if(byte(at)!=0 || byte(at+1)!=0) {
        const auto* skill=definition->attacks[slot].skill;
        if(!skill)throw std::logic_error("Mob stationary skill requires its retained skill owner");
        if(std::to_integer<std::uint8_t>(skill->record[0x31])==2){world_.other_attack(mob,now,actor,slot);return;}
        const auto cooldown=[&] {
            if(!mob.definition)throw std::logic_error("Mob stationary skill lost its definition");
            const auto* live=catalog_.mob(mob.definition->id);
            if(!live || !live->attacks[slot].skill)throw std::logic_error("Mob stationary skill lost its skill owner");
            const auto& raw=live->attacks[slot].skill->record;
            return std::to_integer<std::uint32_t>(raw[0x4a])|(std::to_integer<std::uint32_t>(raw[0x4b])<<8);
        };
        if(!vertical_mobs_attack_range(mob,actor,std::to_integer<std::uint8_t>(skill->record[0x4c]),mobs_,users_,catalog_)) {
            mob.attack_times[slot]=now+cooldown()*1000U;
            if(mob_status(mob)!=2)mob.status=0;
            mob.target_kind=0;return;
        }
        mob.current_attack=static_cast<std::int32_t>(slot);world_.use_skill(mob,now,actor,*skill);
        mob.attack_times[slot]=now+cooldown()*1000U;mob.current_attack=0;return;
    }
    if(!vertical_mobs_attack_range(mob,actor,byte(at+8),mobs_,users_,catalog_)) {
        if(mob_status(mob)!=2)mob.status=0;
        mob.target_kind=0;return;
    }
    mob.current_attack=static_cast<std::int32_t>(slot);world_.attack(mob,now,actor);
    if(mob.current_attack!=0) {
        if(mob.current_attack<0 || mob.current_attack>=3 || !mob.definition)
            throw std::logic_error("Mob attack callback lost its live attack slot or definition");
        definition=catalog_.mob(mob.definition->id);if(!definition)throw std::logic_error("Mob attack callback lost its live catalog record");
        const auto offset=0x84+static_cast<std::size_t>(mob.current_attack)*20;std::uint32_t delay=0;
        for(unsigned i=0;i<4;++i)delay|=std::to_integer<std::uint32_t>(definition->record[offset+i])<<(8*i);
        constexpr std::array<std::int32_t,7> speeds{1500,1000,500,0,-500,-1000,-1500};
        mob.attack_times[slot]=now+delay+static_cast<std::uint32_t>(speeds[static_cast<std::size_t>(std::clamp(mob.attack_speed,0,6))]);
    }
    mob.current_attack=0;
}
VerticalMobsNativeChaseAttackWorld::VerticalMobsNativeChaseAttackWorld(ZoneMobs& mobs,ZoneUsers& users,const MapDefinition& map,
    const MobTargetingBindingCatalog& catalog,MobTargetingZone& targeting,VerticalMobsAiStateOwners& ai,
    VerticalMobsReturnDependencies& dependencies,VerticalMobsStationaryAttackWorld& attacks)
    :mobs_(mobs),users_(users),map_(map),catalog_(catalog),targeting_(targeting),ai_(ai),dependencies_(dependencies),attacks_(attacks) {}
bool VerticalMobsNativeChaseAttackWorld::chase(MobRuntime& mob,std::uint32_t now,Actor& actor,std::uint8_t range) {
    if(mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob || !mob.definition)
        throw std::invalid_argument("Mob chase attack requires its retained zone and definition");
    const auto* definition=catalog_.mob(mob.definition->id);if(!definition)throw std::invalid_argument("Mob chase attack requires its live catalog record");
    const auto count=users_.size();if(count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::overflow_error("Targeting user count exceeds original signed count");
    MobTargetingFindSource source{static_cast<std::int32_t>(count),mobs_.cell_count(),mobs_.cell_count(),mob.cell->x,mob.cell->z,
        mob.position.x,mob.position.z,std::to_integer<std::uint8_t>(definition->record[0x26]),definition->chase_range};
    ActorTargetSearch search(mob,users_,targeting_,catalog_,source);
    return vertical_mobs_native_chase(mob,now,actor,range,mobs_,users_,map_,catalog_,source,search,ai_,dependencies_);
}
void VerticalMobsNativeChaseAttackWorld::attack(MobRuntime& mob,std::uint32_t now,Actor& actor){attacks_.attack(mob,now,actor);}
void VerticalMobsNativeChaseAttackWorld::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill){attacks_.use_skill(mob,now,actor,skill);}
void VerticalMobsNativeChaseAttackWorld::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot){attacks_.other_attack(mob,now,actor,slot);}
void VerticalMobsAttackAiChase::attack(MobRuntime& mob,std::uint32_t now,Actor& actor){world_.attack(mob,now,actor);}
void VerticalMobsAttackAiChase::use_skill(MobRuntime& mob,std::uint32_t now,Actor& actor,const MobTargetingBindingSkill& skill){world_.use_skill(mob,now,actor,skill);}
void VerticalMobsAttackAiChase::other_attack(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    if(slot>=3 || !mob.definition)throw std::invalid_argument("Mob chase attack requires its live slot and definition");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob chase attack requires its live catalog record");
    const auto& record=definition->record;const auto byte=[&](std::size_t at){return std::to_integer<std::uint8_t>(record[at]);};
    const auto at=0x80+slot*20;
    if(byte(0x26)==4 || byte(0x68)==0 || byte(0x7c)!=0){world_.other_attack(mob,now,actor,slot);return;}
    const bool skill_slot=byte(at)!=0 || byte(at+1)!=0;
    const auto* skill=skill_slot?definition->attacks[slot].skill:nullptr;
    if(skill_slot && !skill)throw std::logic_error("Mob chase attack requires its retained skill owner");
    if(skill && std::to_integer<std::uint8_t>(skill->record[0x31])==2){world_.other_attack(mob,now,actor,slot);return;}
    const auto cooldown=[&] {
        if(!mob.definition)throw std::logic_error("Mob chase skill lost its definition");
        const auto* live=catalog_.mob(mob.definition->id);
        if(!live || !live->attacks[slot].skill)throw std::logic_error("Mob chase skill lost its skill owner");
        const auto& raw=live->attacks[slot].skill->record;
        return std::to_integer<std::uint32_t>(raw[0x4a])|(std::to_integer<std::uint32_t>(raw[0x4b])<<8);
    };
    const auto range=skill?std::to_integer<std::uint8_t>(skill->record[0x4c]):byte(at+8);
    if(!world_.chase(mob,now,actor,range)) {
        if(skill_slot && mob.target_kind==0)mob.attack_times[slot]=now+cooldown()*1000U;
        return;
    }
    if(skill_slot) {
        if(!mob.definition)throw std::logic_error("Mob chase lost its definition during movement");
        definition=catalog_.mob(mob.definition->id);
        if(!definition || !definition->attacks[slot].skill)throw std::logic_error("Mob chase lost its skill during movement");
        mob.current_attack=static_cast<std::int32_t>(slot);world_.use_skill(mob,now,actor,*definition->attacks[slot].skill);
        mob.attack_times[slot]=now+cooldown()*1000U;mob.current_attack=0;return;
    }
    mob.current_attack=static_cast<std::int32_t>(slot);world_.attack(mob,now,actor);
    if(mob.current_attack!=0) {
        if(mob.current_attack<0 || mob.current_attack>=3 || !mob.definition)throw std::logic_error("Mob chase attack lost its live slot or definition");
        definition=catalog_.mob(mob.definition->id);if(!definition)throw std::logic_error("Mob chase attack lost its live catalog record");
        const auto offset=0x84+static_cast<std::size_t>(mob.current_attack)*20;std::uint32_t delay=0;
        for(unsigned i=0;i<4;++i)delay|=std::to_integer<std::uint32_t>(definition->record[offset+i])<<(8*i);
        constexpr std::array<std::int32_t,7> speeds{1500,1000,500,0,-500,-1000,-1500};
        mob.attack_times[slot]=now+delay+static_cast<std::uint32_t>(speeds[static_cast<std::size_t>(std::clamp(mob.attack_speed,0,6))]);
    }
    mob.current_attack=0;
}
void VerticalMobsAttackAiMode6::first_ready(MobRuntime& mob,std::uint32_t now,Actor& actor,std::size_t slot) {
    if(slot>=3 || !mob.definition)throw std::invalid_argument("Mob AttackAI mode 6 requires its live slot and definition");
    const auto* definition=catalog_.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob AttackAI mode 6 requires its live catalog record");
    if(std::to_integer<std::uint8_t>(definition->record[0x82+slot*20])!=6 || mob.vision_fixed_user!=0) {
        ready_.first_ready(mob,now,actor,slot);return;
    }
    auto selected=vertical_mobs_find_attack_user_min_hp(mob,mobs_,users_,catalog_,world_);
    if(!selected.actor) {VerticalMobsAttackAiScan scan(catalog_,*this);scan.continue_before(mob,now,actor,slot);return;}
    MobTargetingActorUser target(*selected.actor);targeting_.set_target(mob,target,true);
    after_.after_target_set(mob,now,actor,slot);
}
void vertical_mobs_native_attack_ai(MobRuntime& mob,std::uint32_t now,Actor& actor,ZoneMobs& mobs,ZoneUsers& users,
    const MapDefinition& map,const MobTargetingBindingCatalog& catalog,MobTargetingZone& targeting,
    VerticalMobsActorCombatWorld& combat,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    VerticalMobsStationaryAttackWorld& attacks) {
    if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob || users.find_actor(actor.id())!=&actor)
        throw std::invalid_argument("Native mob AttackAI requires retained paired zone membership");
    VerticalMobsNativeChaseAttackWorld chase_world(mobs,users,map,catalog,targeting,ai,dependencies,attacks);
    VerticalMobsAttackAiChase chase(catalog,chase_world);
    VerticalMobsAttackAiStationary stationary(mobs,users,catalog,chase);
    VerticalMobsAttackAiSelfSkill self(catalog,stationary);
    VerticalMobsAttackAiAggro aggro(combat,self);
    VerticalMobsAttackAiMode6 mode6(mobs,users,catalog,combat,targeting,aggro,self);
    VerticalMobsAttackAiScan scan(catalog,mode6);
    vertical_mobs_attack_ai_prefix(mob,now,actor,catalog,scan);
}
VerticalMobsActorCombat::VerticalMobsActorCombat(MobRuntime& mob,ZoneMobs& mobs,ZoneUsers& users,
    VerticalMobsActorCombatWorld& world):mob_(mob),mobs_(mobs),users_(users),world_(world) {subject(mob);}
VerticalMobsRetainedActor vertical_mobs_find_attack_user_min_hp(MobRuntime& mob,ZoneMobs& mobs,ZoneUsers& users,
    const MobTargetingBindingCatalog& catalog,VerticalMobsActorCombatWorld& world) {
    if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob || !mob.damage || !mob.definition)
        throw std::invalid_argument("Mob minimum-HP search requires its retained zone, ledger and definition");
    const auto* definition=catalog.mob(mob.definition->id);
    if(!definition)throw std::invalid_argument("Mob minimum-HP search requires its retained catalog record");
    std::uint32_t country=0;for(unsigned i=0;i<4;++i)country|=std::to_integer<std::uint32_t>(definition->record[0x78+i])<<(8*i);
    VerticalMobsRetainedActor best;
    for(const auto* record:mob.damage->records()) {
        if(!record || record->type!=1)continue;
        auto* actor=users.find_actor(record->id);if(!actor)continue;
        const auto& appearance=actor->appearance();
        if(country==appearance.identity.country || actor->attack_state().mode==1 || actor->status().no_mob_targeting || !appearance.attackable || !appearance.visible)continue;
        if(!best.actor || std::bit_cast<std::int32_t>(actor->resources().hp)<std::bit_cast<std::int32_t>(best.actor->resources().hp)) {
            auto lease=world.retain(*actor);if(!lease)throw std::logic_error("Mob minimum-HP search requires retained actor ownership");
            best={actor,std::move(lease)};
        }
    }
    return best;
}
void VerticalMobsActorCombat::subject(MobRuntime& mob) const {
    if(&mob!=&mob_ || mob.zone!=&mobs_ || !mob.cell || mobs_.find(mob.id)!=&mob)
        throw std::invalid_argument("Mob combat requires its retained subject and zone");
}
std::uint32_t VerticalMobsActorCombat::tick(){return world_.tick();}
MobAiCombatTarget* VerticalMobsActorCombat::find_user(MobRuntime& mob,std::uint32_t id) {
    subject(mob);actor_=nullptr;retained_.reset();
    auto* actor=users_.find_actor(id);if(!actor) return nullptr;
    auto retained=world_.retain(*actor);if(!retained) throw std::logic_error("Mob combat requires a retained target actor");
    actor_=actor;retained_=std::move(retained);target_={actor->id(),actor->attack_state().mode};return &target_;
}
bool VerticalMobsActorCombat::attack_ai(MobRuntime& mob,std::uint32_t now,MobAiCombatTarget& target) {
    subject(mob);if(&target!=&target_ || !actor_ || !retained_ || target.id!=actor_->id())
        throw std::invalid_argument("Mob combat requires its retained target projection");
    auto retained=retained_;return world_.attack_ai(mob,now,*actor_);
}
void VerticalMobsActorCombat::remove_vision_fix(MobRuntime& mob) {
    subject(mob);if(!mob.has_applied_skills()) throw std::logic_error("Mob combat requires retained applied skills");
    mob.applied_skills().owner().remove_vision_fix(mob);
}
void VerticalMobsActorCombat::retarget(MobRuntime& mob,std::uint32_t now){subject(mob);world_.retarget(mob,now);}
void VerticalMobsActorCombat::lua_combat(MobRuntime& mob,std::uint32_t now,std::uint32_t hp,std::uint32_t count){subject(mob);world_.lua_combat(mob,now,hp,count);}
void VerticalMobsNativeAttack::update_attack(MobRuntime& mob,std::uint32_t now) {
    vertical_mobs_attack(mob,now,dependencies_.combat_services(mob),ai_);
}
void vertical_mobs_native_actor_attack(MobRuntime& mob,std::uint32_t now,ZoneMobs& mobs,ZoneUsers& users,
    const MapDefinition& map,const MobTargetingBindingCatalog& catalog,MobTargetingZone& targeting,
    VerticalMobsActorCombatWorld& combat,VerticalMobsAiOwners& ai,VerticalMobsReturnDependencies& dependencies,
    VerticalMobsStationaryAttackWorld& attacks) {
    if(mob.zone!=&mobs || !mob.cell || mobs.find(mob.id)!=&mob)
        throw std::invalid_argument("Ordinary mob attack requires retained zone membership");
    if(mob_status(mob)==100 || mob.state_stack || mob_status(mob)==2 || now<mob.next_attack)return;
    if(mob.status==1) {
        if(mob.sleep!=0 || mob.stun!=0)return;
        auto* actor=users.find_actor(mob.target_id);
        if(actor && actor->attack_state().mode!=1) {
            auto retained=combat.retain(*actor);if(!retained)throw std::logic_error("Mob attack requires retained actor ownership");
            vertical_mobs_native_attack_ai(mob,now,*actor,mobs,users,map,catalog,targeting,combat,ai,dependencies,attacks);return;
        }
        if(!mob.has_applied_skills())throw std::logic_error("Mob attack requires its applied skills owner");
        mob.applied_skills().owner().remove_vision_fix(mob);
        if(mob_status(mob)!=2)mob.status=0;
        mob.target_kind=0;return;
    }
    if(mob_status(mob)==3)ai.return_home(mob,now);
}
void vertical_mobs_native_zone_attack(RuntimeZone& zone,MobRuntime& mob,std::uint32_t now,
    MicrosoftRandom& random,MobAiStateServices& stack,const MobTargetingBindingCatalog& catalog,
    VerticalMobsActorCombatWorld& combat,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    VerticalMobsStationaryAttackWorld& attacks) {
    if(!zone.map || !zone.mobs || !zone.users)throw std::logic_error("Native mob attack requires initialized paired zone owners");
    if(mob.zone!=zone.mobs.get() || !mob.cell || zone.mobs->find(mob.id)!=&mob)
        throw std::invalid_argument("Native mob attack requires the actual zone subject");
    MobTargetingActorWorld target_world(*zone.mobs,*zone.users,random,stack);
    MobTargetingZone targeting(*zone.mobs,catalog,target_world);
    VerticalMobsNativeAiOwners native(ai,dependencies,*zone.mobs,catalog,zone.map->definition);
    vertical_mobs_native_actor_attack(mob,now,*zone.mobs,*zone.users,zone.map->definition,catalog,targeting,combat,native,dependencies,attacks);
}
bool vertical_mobs_native_actor_attack_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    MicrosoftRandom& random,MobAiStateServices& stack,const MobTargetingBindingCatalog& catalog,
    VerticalMobsActorCombatWorld& combat,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    VerticalMobsStationaryAttackWorld& attacks) {
    auto lease=root.lease(map);if(!lease)return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs || !zone.users || !zone.map)throw std::logic_error("Native mob attack frame requires initialized paired zone owners");
        MobTargetingActorWorld target_world(*zone.mobs,*zone.users,random,stack);
        MobTargetingZone targeting(*zone.mobs,catalog,target_world);
        VerticalMobsNativeAiOwners native(ai,dependencies,*zone.mobs,catalog,zone.map->definition);
        zone.mobs->visit_global([&](MobRuntime& mob) {
            vertical_mobs_native_actor_attack(mob,now,*zone.mobs,*zone.users,zone.map->definition,catalog,targeting,combat,native,dependencies,attacks);
        });
    });return true;
}
bool vertical_mobs_attack_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    VerticalMobsAttackServices& services) {
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs) throw std::logic_error("Mob attack frame requires the initialized zone owner");
        zone.mobs->visit_global([&](MobRuntime& mob){services.update_attack(mob,now);});
    });
    return true;
}
bool vertical_mobs_actor_attack_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    const MobTargetingBindingCatalog& catalog,VerticalMobsActorCombatWorld& combat_world,
    VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies) {
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs || !zone.users || !zone.map)
            throw std::logic_error("Actor mob attack frame requires the initialized primary zone owners");
        VerticalMobsNativeAiOwners native(ai,dependencies,*zone.mobs,catalog,zone.map->definition);
        zone.mobs->visit_global([&](MobRuntime& mob) {
            VerticalMobsActorCombat combat(mob,*zone.mobs,*zone.users,combat_world);
            vertical_mobs_attack(mob,now,combat,native);
        });
    });
    return true;
}
bool vertical_mobs_idle_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    RandomSource& random,VerticalMobsWorld& world) {
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs || !zone.linked_mobs || !zone.mob_population || !zone.map)
            throw std::logic_error("Mob frame requires the actual initialized zone owners");
        zone.mobs->visit_global([&](MobRuntime& mob){ vertical_mobs_idle(mob,now,world); });
        zone.mobs->update_deaths(root.mob_pool(),random,now,*zone.linked_mobs);
    });
    return true;
}
namespace {
void run_mob_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,RuntimeZone& zone,
    MobInitPorts& ports,VerticalMobsWorld& world,LinkedMobWorld& linked) {
    if(!zone.mobs || !zone.linked_mobs || !zone.mob_population || !zone.map)
        throw std::logic_error("Mob frame requires the actual initialized zone owners");
    zone.mobs->visit_global([&](MobRuntime& mob){ vertical_mobs_idle(mob,now,world); });
    zone.mobs->update_deaths(root.mob_pool(),ports.random,now,*zone.linked_mobs);
    zone.update_mob_population(root.mob_pool(),ports,now,
        [&](std::uint32_t tick) {
            if(!root.tick(map,tick)) throw std::logic_error("Missing obelisk respawn owner");
        },
        [&](std::uint32_t tick) {
            zone.linked_mobs->update(zone.map->definition,*zone.mobs,root.mob_pool(),ports,tick,linked);
        });
}
}
bool vertical_mobs_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    MobInitPorts& ports,VerticalMobsWorld& world,LinkedMobWorld& linked) {
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        run_mob_frame(root,map,now,zone,ports,world,linked);
    });
    return true;
}
namespace {
void run_actor_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,RuntimeZone& zone,
    MobInitPorts& ports,MicrosoftRandom& random,const MobTargetingBindingCatalog& catalog,
    MobAiStateServices& stack,VerticalMobsAiOwners& ai,LinkedMobWorld& linked) {
    if(!zone.mobs || !zone.users || !zone.map || !zone.linked_mobs || !zone.mob_population)
        throw std::logic_error("Actor mob frame requires the initialized primary zone owners");
    MobTargetingActorWorld targeting_world(*zone.mobs,*zone.users,random,stack);
    MobTargetingZone targeting(*zone.mobs,catalog,targeting_world);
    VerticalMobsActorWorld world(*zone.mobs,*zone.users,targeting,catalog,zone.map->definition,random,ai);
    run_mob_frame(root,map,now,zone,ports,world,linked);
}
}
bool vertical_mobs_actor_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    MobInitPorts& ports,MicrosoftRandom& random,const MobTargetingBindingCatalog& catalog,
    MobAiStateServices& stack,VerticalMobsAiOwners& ai,LinkedMobWorld& linked) {
    if(&ports.random!=&random) throw std::invalid_argument("Mob frame requires the startup executor random stream");
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        run_actor_frame(root,map,now,zone,ports,random,catalog,stack,ai,linked);
    });
    return true;
}
bool vertical_mobs_actor_frame(MobPrimaryZones& root,std::size_t map,std::uint32_t now,
    MobInitPorts& ports,MicrosoftRandom& random,const MobTargetingBindingCatalog& catalog,
    MobAiStateServices& stack,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    LinkedMobWorld& linked) {
    if(&ports.random!=&random) throw std::invalid_argument("Mob frame requires the startup executor random stream");
    auto lease=root.lease(map);
    if(!lease) return false;
    lease.visit([&](RuntimeZone& zone) {
        if(!zone.mobs || !zone.map)
            throw std::logic_error("Actor mob frame requires the initialized primary zone owners");
        VerticalMobsNativeAiOwners native(ai,dependencies,*zone.mobs,catalog,zone.map->definition);
        run_actor_frame(root,map,now,zone,ports,random,catalog,stack,native,linked);
    });
    return true;
}
VerticalMobsWorldAttackFrame::VerticalMobsWorldAttackFrame(MobPrimaryZones& root,const ServerMaps& maps,
    MicrosoftRandom& random,MobAiStateServices& stack,const MobTargetingBindingCatalog& catalog,
    VerticalMobsActorCombatWorld& combat,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    VerticalMobsStationaryAttackWorld& attacks):root_(root),maps_(maps),random_(random),stack_(stack),catalog_(catalog),
    combat_(combat),ai_(ai),dependencies_(dependencies),attacks_(attacks) {}
void VerticalMobsWorldAttackFrame::operator()(RuntimeZone& zone,std::uint32_t now) {
    const auto maps=maps_.all();std::size_t index=0;
    while(index<maps.size() && &maps[index]!=zone.map)++index;
    if(index==maps.size())throw std::invalid_argument("Mob attack frame requires its retained server map");
    auto lease=root_.lease(index);if(!lease)throw std::logic_error("Primary mob zone unavailable during attack frame");
    lease.visit([&](RuntimeZone& primary) {
        if(&primary!=&zone)throw std::invalid_argument("Mob attack frame requires its actual primary zone");
        if(!vertical_mobs_native_actor_attack_frame(root_,index,now,random_,stack_,catalog_,combat_,ai_,dependencies_,attacks_))
            throw std::logic_error("Primary mob zone unavailable during attack dispatch");
    });
}
VerticalMobsWorldFrame::VerticalMobsWorldFrame(MobPrimaryZones& root,const ServerMaps& maps,
    MobInitPorts& ports,MicrosoftRandom& random,const MobTargetingBindingCatalog& catalog,
    MobAiStateServices& stack,VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,
    LinkedMobWorld& linked):root_(root),maps_(maps),ports_(ports),random_(random),catalog_(catalog),
    stack_(stack),ai_(ai),dependencies_(dependencies),linked_(linked) {
    if(&ports_.random!=&random_) throw std::invalid_argument("Mob frame requires the startup executor random stream");
}
void VerticalMobsWorldFrame::operator()(RuntimeZone& zone,std::uint32_t now) {
    const auto maps=maps_.all();std::size_t index=0;
    while(index<maps.size() && &maps[index]!=zone.map) ++index;
    if(index==maps.size()) throw std::invalid_argument("Mob frame requires its retained server map");
    auto lease=root_.lease(index);
    if(!lease) throw std::logic_error("Primary mob zone unavailable during world frame");
    lease.visit([&](RuntimeZone& primary) {
        if(&primary!=&zone) throw std::invalid_argument("Mob frame requires its actual primary zone");
        if(!primary.mobs || !primary.map) throw std::logic_error("Actor mob frame requires the initialized primary zone owners");
        VerticalMobsNativeAiOwners native(ai_,dependencies_,*primary.mobs,catalog_,primary.map->definition);
        run_actor_frame(root_,index,now,primary,ports_,random_,catalog_,stack_,native,linked_);
    });
}
struct VerticalMobsDeferredWorldFrame::State {
    const ServerMaps& maps;MobInitPorts& ports;MicrosoftRandom& random;const MobTargetingBindingCatalog& catalog;
    MobAiStateServices& stack;VerticalMobsAiStateOwners& ai;VerticalMobsReturnDependencies& dependencies;
    LinkedMobWorld& linked;std::weak_ptr<MobPrimaryZones> root;bool bound=false;
};
VerticalMobsDeferredWorldFrame::VerticalMobsDeferredWorldFrame(const ServerMaps& maps,MobInitPorts& ports,
    MicrosoftRandom& random,const MobTargetingBindingCatalog& catalog,MobAiStateServices& stack,
    VerticalMobsAiStateOwners& ai,VerticalMobsReturnDependencies& dependencies,LinkedMobWorld& linked)
    :state_(std::make_shared<State>(State{maps,ports,random,catalog,stack,ai,dependencies,linked,{}})) {
    if(&ports.random!=&random) throw std::invalid_argument("Mob frame requires the startup executor random stream");
}
void VerticalMobsDeferredWorldFrame::bind(const std::shared_ptr<MobPrimaryZones>& root) {
    if(!root) throw std::invalid_argument("Mob frame requires a primary authority");
    if(state_->bound) throw std::logic_error("Mob frame primary authority already bound");
    state_->root=root;state_->bound=true;
}
void VerticalMobsDeferredWorldFrame::operator()(RuntimeZone& zone,std::uint32_t now) const {
    auto root=state_->root.lock();
    if(!root) throw std::logic_error("Mob frame primary authority is not live");
    VerticalMobsWorldFrame frame(*root,state_->maps,state_->ports,state_->random,state_->catalog,
        state_->stack,state_->ai,state_->dependencies,state_->linked);
    frame(zone,now);
}
}
