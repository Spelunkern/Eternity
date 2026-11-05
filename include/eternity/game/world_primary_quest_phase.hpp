#pragma once
#include "eternity/game/vertical_world.hpp"
#include "eternity/game/vertical_quests.hpp"

namespace eternity::game {
class PrimaryQuestPhaseServices {
public:
    virtual ~PrimaryQuestPhaseServices()=default;
    // The token pins the actual actor, session, quest runtime, definition and
    // transport owners until its visitor returns. It must not change membership.
    virtual std::shared_ptr<void> retain(Actor&)=0;
    // Retention pins every zone member; eligibility may decline an update
    // while an already retained actor is closing or failed.
    virtual bool eligible(Actor&)=0;
    virtual VerticalQuests& quests(Actor&)=0;
    virtual void failure(Actor&,std::exception_ptr)=0;
};
inline void vertical_primary_quest_update_actor(PrimaryQuestPhaseServices& services,Actor& actor,std::uint32_t now,const std::function<bool()>& running) {
    try {if(!running() || !services.eligible(actor))return;auto& quests=services.quests(actor);if(running()) quests.update_timers_for(actor,now);}
    catch(...) {services.failure(actor,std::current_exception());}
}
enum class PrimaryQuestPhaseResult {skipped,completed,interrupted};
// CWorldThread::Update 0040771d..00407760 / CZone::UpdateQuest 00422320.
// The active-list entry point also visits instances retained by their owner.
class WorldPrimaryQuestPhase final {
public:
    explicit WorldPrimaryQuestPhase(std::shared_ptr<PrimaryQuestPhaseServices> services):services_(std::move(services)) {
        if(!services_) throw std::invalid_argument("Primary quest phase requires real quest owners");
    }
    PrimaryQuestPhaseResult update(VerticalWorld& world,std::uint32_t now) {
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryQuestPhaseResult::skipped;
        auto result=PrimaryQuestPhaseResult::completed;
        world.serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){
                if(!dispatch(current,zone,now)) result=PrimaryQuestPhaseResult::interrupted;
            });
        });
        if(result==PrimaryQuestPhaseResult::completed) next_=now+60000U;
        return result;
    }
    // Shares the primary deadline and visits only zones retained by the root.
    template<class Root> PrimaryQuestPhaseResult update_active(Root& root,std::uint32_t now) {
        auto& world=root.world();
        if(!world.running()) throw std::logic_error("World is not running");
        if(now<next_) return PrimaryQuestPhaseResult::skipped;
        bool complete=true;
        const bool visited=root.visit_active_zones([&](RuntimeZone& zone){if(!dispatch(world,zone,now)) complete=false;});
        if(!visited || !complete) return PrimaryQuestPhaseResult::interrupted;
        next_=now+60000U;return PrimaryQuestPhaseResult::completed;
    }
    std::uint32_t next_deadline() const noexcept {return next_;}
private:
    bool dispatch(VerticalWorld& world,RuntimeZone& zone,std::uint32_t now) {
        if(!world.running()) return false;
        if(!zone.users) throw std::logic_error("Active zone has no retained user owner");
        zone.users->visit_all([&](Actor& actor){return services_->retain(actor);},[&](Actor& actor){
            if(world.running()) vertical_primary_quest_update_actor(*services_,actor,now,[&]{return world.running();});
        });
        return world.running();
    }
    std::shared_ptr<PrimaryQuestPhaseServices> services_;std::uint32_t next_=0;
};
}
