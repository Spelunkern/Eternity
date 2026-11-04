#include "eternity/game/mob_targeting_actor.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/mob_ai_states.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
namespace eternity::game {
std::int32_t MobTargetingActorModeLife::life_status(const Actor& a) const {return a.attack_state().mode;}
namespace {const MobTargetingActorModeLife shared_mode_projection;}
MobTargetingActorUser::MobTargetingActorUser(const Actor& a):MobTargetingActorUser(a,shared_mode_projection) {}
MobTargetingActorUser::MobTargetingActorUser(const Actor& a,const MobTargetingActorLife& l):actor_(a),life_(l) {}
MobTargetingUser MobTargetingActorUser::target() const {return {actor_.id(),life_.life_status(actor_)};}
MobTargetingHelpUser MobTargetingActorUser::help() const {
    const auto& a=actor_.appearance();
    // Original +1366 is effect-89 selection immunity, NOT the unrelated
    // SkillStatus::hp_ability_applied trigger state. Country is live +12d.
    return {a.identity.country,static_cast<std::uint8_t>(actor_.status().no_mob_targeting),
            life_.life_status(actor_),a.attackable,a.visible};
}
MobTargetingActorWorld::MobTargetingActorWorld(ZoneMobs& m,const ZoneUsers& u,MicrosoftRandom& r,MobAiStateServices& f):mobs_(m),users_(u),random_(r),fsm_(f) {}
std::int32_t MobTargetingActorWorld::user_count(const ZoneMobs& m) const {
    if(&m!=&mobs_) throw std::invalid_argument("Targeting world queried a different mob zone");
    const auto n=users_.size();
    if(n>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::overflow_error("Targeting user count exceeds original signed count");
    return static_cast<std::int32_t>(n);
}
std::int32_t MobTargetingActorWorld::random(MobRuntime&) {return random_.next();}
std::uint32_t MobTargetingActorWorld::tick_ms(MobRuntime&) {return fsm_.tick();}
void MobTargetingActorWorld::dispatch(MobRuntime& m,std::uint32_t e,std::uint32_t a) {
    dispatch_mob_ai_event(m,std::bit_cast<std::int32_t>(e),a,fsm_);
}
}
