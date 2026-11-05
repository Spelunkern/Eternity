#pragma once
#include "eternity/game/world_admission.hpp"
#include "eternity/game/world_shutdown_coordinator.hpp"
#include "eternity/game/world_primary_mob_phase.hpp"
#include "eternity/game/world_primary_user_attack_phase.hpp"
#include "eternity/game/world_primary_item_phase.hpp"
#include "eternity/game/world_primary_quest_phase.hpp"
#include "eternity/game/world_primary_weather_phase.hpp"
#include "eternity/game/world_primary_logout_phase.hpp"
#include "eternity/game/world_primary_npc_idle_phase.hpp"
#include "eternity/game/world_primary_frame_prefix.hpp"
#include "eternity/game/world_thread_zones.hpp"
#include "eternity/game/world_instance_cleanup_phase.hpp"
#include "eternity/game/world_active_packet_phase.hpp"
#include "eternity/game/world_db_driver.hpp"
#include "eternity/game/world_obelisk_reset_phase.hpp"
#include "eternity/game/world_kill_count_phase.hpp"
#include "eternity/game/world_kill_count_timer.hpp"
#include "eternity/game/world_market_phase.hpp"
#include "eternity/game/world_rank_phase.hpp"
#include "eternity/game/world_send_phase.hpp"
#include "eternity/game/world_send_update.hpp"
#include "eternity/game/world_send_retained_update.hpp"
#include "eternity/game/world_send_driver.hpp"
#include "eternity/game/world_session_lost_phase.hpp"

namespace eternity::game {
struct ActiveWorldPrefixResult {
    bool completed=false;
    bool user_attack=false,effects=false,mob_npc_attack=false,mob_idle=false;
    bool npc_idle=false,items=false,quests=false,weather=false;
};
struct ActiveWorldThroughKillCountResult {
    bool completed=false;
    ActiveWorldPrefixResult prefix;
    // net_send_due is usable only when the partial frame completed.
    bool rank=false,market=false,obelisk_reset=false,net_send_due=false;
};
struct ActiveWorldFrameResult {
    bool completed=false;
    ActiveWorldThroughKillCountResult through_kill_count;
    InstanceCleanupResult cleanup=InstanceCleanupResult::skipped;
};
// CWorldThread::Update 004073a8..00407976. Weather's inner temporary
// overwrites are followed by an unconditional dirty set at its gate.
inline bool active_world_net_send_due(bool promoted,const ActiveWorldPrefixResult& prefix,bool rank) noexcept {
    return promoted || prefix.user_attack || prefix.effects || prefix.mob_npc_attack || prefix.mob_idle ||
        prefix.npc_idle || prefix.items || prefix.quests || prefix.weather || rank;
}
// Owns one existing world authority, admission list, session registry and frame
// driver on the same strand. Session/service/phase owners and the root must be
// retained through transport close, DB drain, session release and zone drain.
// The supplied update must implement real phases; no gameplay defaults exist.
template<class Session> class WorldRoot final : public std::enable_shared_from_this<WorldRoot<Session>> {
public:
    using Strand=WorldFrameDriver::Strand;
    using Tracker=VerticalTrackedDbJobs<Session>;
    using Shutdown=WorldShutdownCoordinator<Session>;
    using Update=std::function<bool(WorldRoot&,std::uint32_t)>;
    using DbUpdate=WorldDbUpdate<WorldRoot>;
    using DbDriver=WorldDbDriver<DbUpdate>;
    using AdmissionFactory=std::function<std::shared_ptr<WorldAdmissionServices>(VerticalSessionRegistry<Session>&)>;
    static std::shared_ptr<WorldRoot> create(Strand strand,std::shared_ptr<ServerMaps> maps,
        MobZoneServicesInputs inputs,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport,
        VerticalWorldPorts ports,std::shared_ptr<WorldAdmissionServices> admission_services,
        WorldFrameDriver::OverrunCounter overruns,Update update,WorldFrameDriver::Failure failure) {
        if(!pool || &inputs.pool!=pool.get()) throw std::invalid_argument("World root requires the bound mob pool");
        auto zones=MobZoneServices::create(std::move(inputs));
        return create(std::move(strand),std::move(maps),std::move(zones),std::move(pool),std::move(transport),
            std::move(ports),std::move(admission_services),std::move(overruns),std::move(update),std::move(failure));
    }
    static std::shared_ptr<WorldRoot> create(Strand strand,std::shared_ptr<ServerMaps> maps,
        std::shared_ptr<ZoneServices> zones,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport,
        VerticalWorldPorts ports,std::shared_ptr<WorldAdmissionServices> admission_services,
        WorldFrameDriver::OverrunCounter overruns,Update update,WorldFrameDriver::Failure failure) {
        return create(std::move(strand),std::move(maps),std::move(zones),std::move(pool),std::move(transport),std::move(ports),
            [services=std::move(admission_services)](auto&){return services;},std::move(overruns),std::move(update),std::move(failure));
    }
    static std::shared_ptr<WorldRoot> create(Strand strand,std::shared_ptr<ServerMaps> maps,
        MobZoneServicesInputs inputs,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport,
        VerticalWorldPorts ports,AdmissionFactory admission_factory,
        WorldFrameDriver::OverrunCounter overruns,Update update,WorldFrameDriver::Failure failure) {
        if(!pool || &inputs.pool!=pool.get()) throw std::invalid_argument("World root requires the bound mob pool");
        auto zones=MobZoneServices::create(std::move(inputs));
        return create(std::move(strand),std::move(maps),std::move(zones),std::move(pool),std::move(transport),
            std::move(ports),std::move(admission_factory),std::move(overruns),std::move(update),std::move(failure));
    }
    static std::shared_ptr<WorldRoot> create(Strand strand,std::shared_ptr<ServerMaps> maps,
        std::shared_ptr<ZoneServices> zones,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport,
        VerticalWorldPorts ports,AdmissionFactory admission_factory,
        WorldFrameDriver::OverrunCounter overruns,Update update,WorldFrameDriver::Failure failure) {
        if(!admission_factory || !overruns || !update || !failure)
            throw std::invalid_argument("World root requires real admission, update, failure and overrun owners");
        auto world=VerticalWorld::create(std::move(maps),std::move(zones),std::move(pool),std::move(transport),std::move(ports));
        auto result=std::shared_ptr<WorldRoot>(new WorldRoot(std::move(strand),std::move(admission_factory),std::move(world),std::move(update)));
        result->install_driver(std::move(overruns),std::move(failure));
        return result;
    }
    // The live session owner supplies aliases to its own registry and admission.
    // Both aliases must retain the same owner, which outlives the root's drivers.
    static std::shared_ptr<WorldRoot> create_with_session_owner(Strand strand,std::shared_ptr<ServerMaps> maps,
        MobZoneServicesInputs inputs,std::unique_ptr<MobPool> pool,MobPrimaryZoneTransport transport,
        VerticalWorldPorts ports,std::shared_ptr<VerticalSessionRegistry<Session>> sessions,
        std::shared_ptr<WorldAdmission> admission,std::function<void()> update_admission,
        std::function<void(RuntimeZone&)> update_zone_wait,WorldFrameDriver::OverrunCounter overruns,
        Update update,WorldFrameDriver::Failure failure) {
        if(!pool || &inputs.pool!=pool.get()) throw std::invalid_argument("World root requires the bound mob pool");
        if(!sessions || !admission || sessions.owner_before(admission) || admission.owner_before(sessions) || !update_admission || !update_zone_wait || !overruns || !update || !failure)
            throw std::invalid_argument("World root requires one retained session owner");
        auto zones=MobZoneServices::create(std::move(inputs));
        auto world=VerticalWorld::create(std::move(maps),std::move(zones),std::move(pool),std::move(transport),std::move(ports));
        auto result=std::shared_ptr<WorldRoot>(new WorldRoot(std::move(strand),std::move(sessions),std::move(admission),std::move(update_admission),std::move(update_zone_wait),std::move(world),std::move(update)));
        result->install_driver(std::move(overruns),std::move(failure));return result;
    }
    // Start on the world strand before opening admission. A failed/interrupted
    // start must still be followed by the normal shutdown/retry procedure.
    bool start() {
        require_strand();if(startup_attempted_ || shutdown_) throw std::logic_error("World root startup already attempted or stopped");
        startup_attempted_=true;if(!world_->start()) return false;
        try {world_->visit_primary_zones([&](RuntimeZone& zone){thread_zones_.add_active(zone);});if(send_update_services_)send_update_=std::make_unique<WorldSendUpdate<WorldRoot>>(this->shared_from_this(),send_update_services_,send_update_tick_);driver_->start();if(db_driver_)db_driver_->start();if(send_driver_)send_driver_->start();} catch(...) {
            auto failure=std::current_exception();driver_->stop();if(db_driver_)db_driver_->stop();if(send_driver_)send_driver_->stop();try {(void)world_->shutdown();} catch(...) {}
            std::rethrow_exception(failure);
        }
        return true;
    }
    VerticalWorld& world() {require_strand();return *world_;}
    WorldAdmission& admission() {require_strand();return admission_;}
    WorldThreadZones& thread_zones() {require_strand();return thread_zones_;}
    // Holds the existing authority across a live active-list traversal and
    // pins each actual primary or instance through its callback. False means
    // shutdown was requested during this pass; the caller must stop the frame.
    bool visit_active_zones(const std::function<void(RuntimeZone&)>& visit) {
        require_strand();if(!visit) throw std::invalid_argument("Active zone visitor is required");
        if(!world_->running()) throw std::logic_error("World is not running");
        bool complete=true;
        world_->serialized_update([&](VerticalWorld& current){
            auto authority=current.zones();
            thread_zones_.visit_active([&](RuntimeZone& zone){
                if(!current.running()){complete=false;return;}
                if(!zone.map)throw std::logic_error("Active world zone has no map");
                if(zone.map->settings.create_type==0) {
                    auto lease=authority->lease_primary(zone);if(!lease)throw std::logic_error("Active primary is closing");
                    lease.visit([&](RuntimeZone& pinned){visit(pinned);});
                } else {
                    auto lease=authority->lease_instance(zone);if(!lease)throw std::logic_error("Active instance is closing");
                    lease.visit([&](RuntimeZone& pinned){visit(pinned);});
                }
                if(!current.running())complete=false;
            });
        });
        return complete;
    }
    // ZoneServices::enqueue_wait must route new instances to thread_zones().
    // Hold this lease through admission's installation of zone membership.
    MobInstanceZoneLease select_instance(std::uint16_t map,const ZoneUserAccess& user) {
        require_strand();if(!world_->running() || shutdown_) throw std::logic_error("Instance selection requires an accepting world");
        return world_->zones()->select_instance(map,user);
    }
    // Caller visits the real active thread list at the original cleanup gate.
    MobInstanceExpiry expire_instance(RuntimeZone& zone,std::uint32_t now) {
        require_strand();if(!thread_zones_.contains_active(zone)) throw std::logic_error("Instance is not on the active world thread");
        return world_->zones()->expire_instance(zone,admission_,now,thread_zones_);
    }
    void visit_retiring_instance(const std::function<void(RuntimeZone&)>& visit) {
        require_strand();world_->zones()->visit_retiring_instance(visit);
    }
    MobInstanceExpiry retry_instance_retirement(std::uint32_t now) {
        require_strand();return world_->zones()->retry_instance_retirement(admission_,now,thread_zones_);
    }

    // Called after the actual world-send passes; not part of the primary-only
    // prefix. Deferred leases keep this cleanup pass due for the next frame.
    InstanceCleanupResult update_instance_cleanup(std::uint32_t now) {
        require_strand();return instance_cleanup_.update(*this,now);
    }
    std::uint32_t next_instance_cleanup()const {require_strand();return instance_cleanup_.next_deadline();}
    void bind_send_phase(std::shared_ptr<WorldSendServices> services) {
        require_strand();if(send_phase_ || startup_attempted_ || shutdown_)throw std::logic_error("World send phase must be bound once before startup");
        send_phase_=std::make_unique<WorldSendPhase>(std::move(services));
    }
    // World-wide messages enter each primary queue and the separate instance queue.
    void enqueue_world_broadcast(WorldZoneSendMessage message) {
        require_strand();validate_world_zone_send(message);
        if(!world_->running() || shutdown_)throw std::logic_error("World broadcast requires a running world");
        world_->serialized_update([&](VerticalWorld& current){
            current.visit_primary_zones([&](RuntimeZone& zone){zone.world_sends.push(message);});
            instance_broadcasts_.push(std::move(message));
        });
    }
    void distribute_instance_broadcasts() {
        require_strand();if(!world_->running() || shutdown_)throw std::logic_error("Instance broadcasts require a running world");
        world_->serialized_update([&](VerticalWorld& current){
            instance_broadcasts_.drain([&](const WorldZoneSendMessage& message){
                current.zones()->visit_logout_zones([&](RuntimeZone& zone){
                    if(zone.map->settings.create_type!=0)zone.world_sends.push(message);
                });
            });
        });
    }
    void bind_world_send_update(std::shared_ptr<WorldSendUpdateServices> services,std::function<std::uint32_t()> tick) {
        require_strand();if(startup_attempted_ || shutdown_ || send_update_services_)throw std::logic_error("World send update must be bound once before startup");
        if(!services || !tick)throw std::invalid_argument("World send update requires actual services and clock");
        send_update_services_=std::move(services);send_update_tick_=std::move(tick);
    }
    void bind_retained_world_send_update(std::shared_ptr<WorldGlobalSendServices> send,
        std::shared_ptr<WorldSendOtherServices> other,std::function<std::uint32_t()> tick) {
        require_strand();
        bind_world_send_update(std::make_shared<WorldSendRetainedUpdate<WorldRoot>>(this->shared_from_this(),std::move(send),std::move(other)),std::move(tick));
    }
    bool update_world_send(std::uint32_t now) {
        require_strand();if(!send_update_ || shutdown_)throw std::logic_error("World send update is not available");
        return send_update_->update(now);
    }
    bool world_send_running() const {require_strand();return world_->running() && !shutdown_;}
    // DB bootstrap can arrive before startup, but not after a failed start or
    // after shutdown begins. Call only on the shared world strand.
    bool world_accepts_initialization() const {require_strand();return !shutdown_ && (!startup_attempted_ || world_->running());}
    void bind_session_lost_phase(std::shared_ptr<WorldSessionLostServices> services) {
        require_strand();if(session_lost_ || startup_attempted_ || shutdown_)throw std::logic_error("Session-lost phase must be bound once before startup");
        session_lost_=std::make_unique<WorldSessionLostPhase>(std::move(services));
    }
    bool update_session_lost(std::uint32_t now) {
        require_strand();if(!session_lost_)throw std::logic_error("Session-lost services are not bound");return session_lost_->update(*this,now);
    }
    // The caller supplies the original configured send-thread term and actual
    // shared counter/failure handler; no timer starts before world startup.
    void bind_world_send_driver(std::uint32_t period,WorldFrameDriver::OverrunCounter overruns,WorldFrameDriver::Failure failure) {
        require_strand();if(startup_attempted_ || shutdown_ || send_driver_ || !send_update_services_)
            throw std::logic_error("World send driver requires its update binding before startup");
        send_driver_=WorldSendDriver<WorldRoot>::create(strand_,this->shared_from_this(),period,send_update_tick_,std::move(overruns),std::move(failure));
    }
    bool update_send_phase(bool net_send_due) {
        require_strand();if(!send_phase_)throw std::logic_error("World send services are not bound");return send_phase_->update(*this,net_send_due);
    }
    void bind_rank_phase(std::shared_ptr<WorldRankServices> services) {
        require_strand();if(rank_ || startup_attempted_ || shutdown_)throw std::logic_error("Rank phase must be bound once before startup");
        rank_=std::make_unique<WorldRankPhase>(std::move(services));
    }
    WorldRankResult update_rank_phase(std::uint32_t now) {
        require_strand();if(!rank_)throw std::logic_error("Rank services are not bound");return rank_->update(*this,now);
    }
    void bind_market_phase(std::shared_ptr<WorldMarketServices> services) {
        require_strand();if(market_ || startup_attempted_ || shutdown_)throw std::logic_error("Market phase must be bound once before startup");
        market_=std::make_unique<WorldMarketPhase>(std::move(services));
    }
    WorldMarketResult update_market_phase(std::uint32_t now) {
        require_strand();if(!market_)throw std::logic_error("Market services are not bound");return market_->update(*this,now);
    }
    void bind_kill_count_phase(std::shared_ptr<WorldKillCountServices> services) {
        require_strand();if(kill_count_ || startup_attempted_ || shutdown_)throw std::logic_error("Kill count phase must be bound once before startup");
        kill_count_=std::make_unique<WorldKillCountPhase>(std::move(services));
    }
    void request_kill_count_apply(std::uint8_t country) {require_strand();if(!kill_count_)throw std::logic_error("Kill count services are not bound");kill_count_->request_apply(country);}
    void request_kill_count_send(std::uint8_t country) {require_strand();if(!kill_count_)throw std::logic_error("Kill count services are not bound");kill_count_->request_send(country);}
    WorldKillCountResult update_kill_count() {require_strand();if(!kill_count_)throw std::logic_error("Kill count services are not bound");return kill_count_->update(*this);}
    void bind_world_kill_count_timer(std::shared_ptr<WorldKillCountTimerServices> services) {
        require_strand();if(kill_timer_ || startup_attempted_ || shutdown_ || !kill_count_)throw std::logic_error("World kill timer requires the bound zone phase before startup");
        kill_timer_=std::make_unique<WorldKillCountTimer>(std::move(services));
    }
    bool update_world_kill_count_timer(std::uint32_t now) {
        require_strand();if(!kill_timer_)throw std::logic_error("World kill timer services are not bound");return kill_timer_->update(*this,now);
    }
    void request_obelisk_reset() {require_strand();obelisk_reset_.request();}
    WorldObeliskResetResult update_obelisk_reset(const std::function<std::uint32_t()>& tick) {
        require_strand();return obelisk_reset_.update(*this,tick);
    }
    VerticalSessionRegistry<Session>& sessions() {require_strand();return sessions_;}
    void bind_primary_user_attack_phase(std::shared_ptr<PrimaryUserAttackServices> services) {
        require_strand();if(user_attack_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary user attack phase must be bound once before startup");
        user_attack_phase_=std::make_unique<WorldPrimaryUserAttackPhase>(std::move(services));
    }
    PrimaryUserAttackResult update_primary_user_attack_phase(std::uint32_t now) {
        require_strand();if(!user_attack_phase_) throw std::logic_error("Primary user attack services are not bound");
        return user_attack_phase_->update(*world_,now);
    }
    // Full active-list alternative; shares the primary phase deadline/provider.
    PrimaryUserAttackResult update_active_user_attack_phase(std::uint32_t now) {
        require_strand();if(!user_attack_phase_) throw std::logic_error("User attack services are not bound");
        return user_attack_phase_->update_active(*this,now);
    }
    // Bind the actual per-zone mob and trap services before startup. This is
    // one primary-only phase, not a complete world update.
    void bind_primary_mob_phase(std::shared_ptr<PrimaryMobPhaseServices> services) {
        require_strand();if(mob_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary mob phase must be bound once before startup");
        mob_phase_=std::make_unique<WorldPrimaryMobPhase>(std::move(services));
    }
    PrimaryMobPhaseResult update_primary_mob_phase(std::uint32_t now) {
        require_strand();if(!mob_phase_) throw std::logic_error("Primary mob phase services are not bound");
        return mob_phase_->update(*world_,now);
    }
    // Full active-list alternative after mob/NPC attack. Shares the retained
    // primary mob phase's 1000 ms deadline and real services.
    PrimaryMobPhaseResult update_active_mob_phase(std::uint32_t now) {
        require_strand();if(!mob_phase_) throw std::logic_error("Mob phase services are not bound");
        return mob_phase_->update_active(*this,now);
    }
    void bind_primary_quest_phase(std::shared_ptr<PrimaryQuestPhaseServices> services) {
        require_strand();if(quest_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary quest phase must be bound once before startup");
        quest_phase_=std::make_unique<WorldPrimaryQuestPhase>(std::move(services));
    }
    PrimaryQuestPhaseResult update_primary_quest_phase(std::uint32_t now) {
        require_strand();if(!quest_phase_) throw std::logic_error("Primary quest services are not bound");
        return quest_phase_->update(*world_,now);
    }
    PrimaryQuestPhaseResult update_active_quest_phase(std::uint32_t now) {
        require_strand();if(!quest_phase_) throw std::logic_error("Quest services are not bound");
        return quest_phase_->update_active(*this,now);
    }
    void bind_primary_weather_phase(std::shared_ptr<PrimaryWeatherServices> services) {
        require_strand();if(weather_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary weather phase must be bound once before startup");
        weather_phase_=std::make_unique<WorldPrimaryWeatherPhase>(std::move(services));
    }
    PrimaryWeatherResult update_primary_weather_phase(std::uint32_t now) {
        require_strand();if(!weather_phase_) throw std::logic_error("Primary weather services are not bound");
        return weather_phase_->update(*world_,now);
    }
    // Active-list alternative after quests; shares the primary deadline.
    PrimaryWeatherResult update_active_weather_phase(std::uint32_t now) {
        require_strand();if(!weather_phase_) throw std::logic_error("Weather services are not bound");
        return weather_phase_->update_active(*this,now);
    }
    void bind_primary_logout_phase(std::shared_ptr<PrimaryLogoutServices> services) {
        require_strand();if(logout_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary logout phase must be bound once before startup");
        logout_phase_=std::make_unique<WorldPrimaryLogoutPhase>(std::move(services));
    }
    // Called by the DB-thread scheduler on this strand, not by the world frame.
    PrimaryLogoutResult update_primary_logout_phase(std::uint32_t now) {
        require_strand();if(!logout_phase_) throw std::logic_error("Primary logout services are not bound");
        return logout_phase_->update(*world_,admission_,now);
    }
    PrimaryLogoutResult update_registry_logout_phase(std::uint32_t now) {
        require_strand();if(!logout_phase_) throw std::logic_error("Logout services are not bound");
        return logout_phase_->update_registry(*this,now);
    }
    // Bind real DB packet/event owners and counters before startup. The DB
    // driver remains active after transport closure until tracked jobs drain.
    void bind_world_db_driver(std::shared_ptr<WorldDbUpdateServices> services,
        std::shared_ptr<std::atomic<std::uint32_t>> heartbeat,std::shared_ptr<std::atomic<std::uint32_t>> overruns,
        std::function<std::uint32_t()> tick,typename DbDriver::Failure failure) {
        require_strand();if(db_driver_ || startup_attempted_ || shutdown_ || !logout_phase_) throw std::logic_error("World DB driver requires bound logout before startup");
        auto update=std::make_shared<DbUpdate>(this->shared_from_this(),std::move(services));
        db_driver_=DbDriver::create(strand_,std::move(update),std::move(heartbeat),std::move(overruns),std::move(tick),std::move(failure));
    }
    void bind_primary_npc_idle_phase(std::shared_ptr<PrimaryNpcIdleServices> services) {
        require_strand();if(npc_idle_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary NPC idle phase must be bound once before startup");
        npc_idle_phase_=std::make_unique<WorldPrimaryNpcIdlePhase>(std::move(services));
    }
    PrimaryNpcIdleResult update_primary_npc_idle_phase(std::uint32_t now) {
        require_strand();if(!npc_idle_phase_) throw std::logic_error("Primary NPC idle services are not bound");
        return npc_idle_phase_->update(*world_,now);
    }
    // Full active-list alternative after mob idle/traps. Shares the existing
    // NPC idle provider and deadline with the primary-only entry point.
    PrimaryNpcIdleResult update_active_npc_idle_phase(std::uint32_t now) {
        require_strand();if(!npc_idle_phase_) throw std::logic_error("NPC idle services are not bound");
        return npc_idle_phase_->update_active(*this,now);
    }
    // The caller supplies one retained primary from its serialized attack pass.
    bool update_primary_npc_attack(RuntimeZone& zone,std::uint32_t now) {
        require_strand();if(!npc_idle_phase_) throw std::logic_error("Primary NPC guard services are not bound");
        return npc_idle_phase_->update_attack(*world_,zone,now);
    }
    // Active-list packet pass is an alternative to the primary-only prefix's
    // packet pass; callers must execute it once at the start of a full frame.
    void bind_active_packet_phase(std::shared_ptr<PrimaryZonePacketServices> services) {
        require_strand();if(active_packet_phase_ || startup_attempted_ || shutdown_) throw std::logic_error("Active packet phase must be bound once before startup");
        active_packet_phase_=std::make_unique<WorldActivePacketPhase>(std::move(services));
    }
    bool update_active_packet_phase() {
        require_strand();if(!active_packet_phase_) throw std::logic_error("Active packet services are not bound");
        return active_packet_phase_->update(*this);
    }
    // Ordered active-list prefix through weather; the caller still owns the
    // earlier promotion and later rank, market, send and cleanup passes.
    ActiveWorldPrefixResult update_active_world_prefix(std::uint32_t now) {
        require_strand();
        if(!active_packet_phase_ || !user_attack_phase_ || !frame_prefix_ || !mob_phase_ || !npc_idle_phase_ || !quest_phase_ || !weather_phase_)
            throw std::logic_error("Active world prefix requires all phase services");
        ActiveWorldPrefixResult result;
        world_->serialized_update([&](VerticalWorld&){
            if(!update_active_packet_phase()) return;
            const auto user=update_active_user_attack_phase(now);
            if(user==PrimaryUserAttackResult::interrupted) return;
            result.user_attack=user==PrimaryUserAttackResult::completed;
            const auto effects=update_active_effect_phases(now);
            result.effects=effects.ran;
            if(!effects.completed) return;
            const auto attacks=update_active_mob_npc_attack(now);
            if(!attacks.completed) return;
            result.mob_npc_attack=attacks.ran;
            const auto mob=update_active_mob_phase(now);
            if(mob==PrimaryMobPhaseResult::interrupted) return;
            result.mob_idle=mob==PrimaryMobPhaseResult::completed;
            const auto npc=update_active_npc_idle_phase(now);
            if(npc==PrimaryNpcIdleResult::interrupted) return;
            result.npc_idle=npc==PrimaryNpcIdleResult::completed;
            const auto item=update_active_item_phase(now);
            if(item==PrimaryItemPhaseResult::interrupted) return;
            result.items=item==PrimaryItemPhaseResult::completed;
            const auto quest=update_active_quest_phase(now);
            if(quest==PrimaryQuestPhaseResult::interrupted) return;
            result.quests=quest==PrimaryQuestPhaseResult::completed;
            const auto weather=update_active_weather_phase(now);
            if(weather==PrimaryWeatherResult::interrupted) return;
            result.weather=weather==PrimaryWeatherResult::completed;
            result.completed=true;
        });
        return result;
    }
    void bind_primary_frame_prefix(std::shared_ptr<PrimaryFramePrefixServices> services) {
        require_strand();if(frame_prefix_ || startup_attempted_ || shutdown_) throw std::logic_error("Primary frame prefix must be bound once before startup");
        frame_prefix_=std::make_unique<WorldPrimaryFramePrefix>(std::move(services));
    }
    // After active user attack and before mob/NPC attack. Uses the same
    // provider and timers as the alternative primary-only frame prefix.
    ActiveEffectPhasesResult update_active_effect_phases(std::uint32_t now) {
        require_strand();if(!frame_prefix_) throw std::logic_error("Frame effect services are not bound");
        return frame_prefix_->update_active_effects(*this,now);
    }
    // After active effects and before mob idle/traps; shares the primary
    // prefix's timer and its actual mob and NPC guard providers.
    ActiveAttackPhaseResult update_active_mob_npc_attack(std::uint32_t now) {
        require_strand();if(!frame_prefix_) throw std::logic_error("Frame attack services are not bound");
        return frame_prefix_->update_active_attacks(*this,now);
    }
    bool update_primary_frame_prefix(std::uint32_t now) {
        require_strand();if(!frame_prefix_) throw std::logic_error("Primary frame prefix services are not bound");
        if(thread_zones_.has_active_instances()) throw std::logic_error("Primary-only frame cannot process active instances");
        return frame_prefix_->update(*this,now);
    }
    // Partial active frame through UpdateKillCount. Pending-zone promotion is
    // done by the frame driver; actual world sends and cleanup still follow.
    ActiveWorldThroughKillCountResult update_active_through_kill_count(std::uint32_t now,
        const std::function<std::uint32_t()>& tick) {
        require_strand();
        if(!tick || !rank_ || !market_ || !kill_count_ || !active_packet_phase_ || !user_attack_phase_ || !frame_prefix_ || !mob_phase_ || !npc_idle_phase_ || !quest_phase_ || !weather_phase_)
            throw std::logic_error("Active world frame requires actual phase services and clock");
        ActiveWorldThroughKillCountResult result;
        world_->serialized_update([&](VerticalWorld&){
            result.prefix=update_active_world_prefix(now);if(!result.prefix.completed)return;
            const auto rank=update_rank_phase(now);if(rank==WorldRankResult::interrupted)return;
            result.rank=rank==WorldRankResult::completed;
            result.net_send_due=active_world_net_send_due(promoted_this_frame_,result.prefix,result.rank);
            const auto market=update_market_phase(now);if(market==WorldMarketResult::interrupted)return;
            result.market=market==WorldMarketResult::completed;
            if(update_kill_count()==WorldKillCountResult::interrupted)return;
            const auto reset=update_obelisk_reset(tick);if(reset==WorldObeliskResetResult::interrupted)return;
            result.obelisk_reset=reset==WorldObeliskResetResult::completed;result.completed=true;
        });
        return result;
    }
    // The frame driver promotes pending zones immediately before this call.
    // The caller binds every actual phase and retains the real send owners.
    ActiveWorldFrameResult update_active_frame(std::uint32_t now,const std::function<std::uint32_t()>& tick) {
        require_strand();if(!send_phase_)throw std::logic_error("Active world frame requires actual send services");
        ActiveWorldFrameResult result;
        world_->serialized_update([&](VerticalWorld&){
            result.through_kill_count=update_active_through_kill_count(now,tick);
            if(!result.through_kill_count.completed)return;
            if(!update_send_phase(result.through_kill_count.net_send_due))return;
            result.cleanup=update_instance_cleanup(now);
            if(result.cleanup==InstanceCleanupResult::interrupted)return;
            result.completed=true;
        });
        return result;
    }
    // Uses the existing ZoneItems and its actual pool/transport services.
    PrimaryItemPhaseResult update_primary_item_phase(std::uint32_t now) {
        require_strand();return item_phase_.update(*world_,now);
    }
    PrimaryItemPhaseResult update_active_item_phase(std::uint32_t now) {
        require_strand();return item_phase_.update_active(*this,now);
    }
    // CWorld::UpdatePackets 0040bf90; the owner supplies its original cadence.
    void update_admission() {require_strand();if(admission_update_)admission_update_();else admission_.update_packets();}
    void update_zone_wait_packets(RuntimeZone& zone) {require_strand();if(zone_wait_update_)zone_wait_update_(zone);else admission_.update_zone_wait_packets(zone);}
    // Only after external transport close completion, session close submission,
    // other-producer stop, and untracked traffic fencing. Never fabricates those
    // prerequisites. The trackers must belong to this root's session registry.
    void begin_shutdown_after_transport_closed(std::vector<std::shared_ptr<Tracker>> trackers,std::function<void()> db_drained) {
        require_strand();if(shutdown_) throw std::logic_error("World root shutdown already started");
        if(!db_drained) throw std::invalid_argument("World shutdown requires DB drain notification");
        for(const auto& tracker:trackers)if(!tracker)throw std::invalid_argument("World shutdown requires actual DB trackers");
        if(send_driver_)send_driver_->stop();
        std::weak_ptr<WorldRoot> weak=this->shared_from_this();
        shutdown_=Shutdown::start(strand_,driver_,sessions_,std::move(trackers),[weak,done=std::move(db_drained)]{
            auto self=weak.lock();if(!self) throw std::logic_error("World root lost during DB drain");
            if(self->db_driver_)self->db_driver_->stop();self->db_driver_drained_=true;done();
        });
        promote_pending_for_shutdown();
    }
    // Retry after an allocation failure while transferring the pending list.
    // Normal instance expiry, admission and lease checks still apply.
    std::size_t promote_pending_for_shutdown() {
        require_strand();if(!shutdown_) throw std::logic_error("World root shutdown not started");
        if(thread_zones_.pending_count()==0) return 0;
        std::size_t count=0;
        world_->serialized_update([&](VerticalWorld&){count=thread_zones_.promote_pending_for_shutdown();});
        return count;
    }
    // Poll on the world strand after transport closure. The caller supplies
    // the real world clock and continues polling while external work drains.
    // Cleanup retains its normal deadlines, occupancy and lease checks.
    bool poll_shutdown(std::uint32_t now) {
        require_strand();if(!shutdown_) throw std::logic_error("World root shutdown not started");
        (void)promote_pending_for_shutdown();
        if(world_->running() && world_->zones()->instance_count()!=0)
            (void)update_instance_cleanup(now);
        return try_finish_shutdown();
    }
    bool try_finish_shutdown() {
        require_strand();if(!shutdown_) throw std::logic_error("World root shutdown not started");
        // Session removal must also unlink every global and zone-wait user.
        if(!admission_.empty() || !thread_zones_.primary_shutdown_ready() || (db_driver_ && !db_driver_drained_)) return false;
        // A failed release unlinks an instance before its authority can finish
        // retirement. Keep the world running so its retry can still execute.
        if(world_->zones()->instance_count()!=0) return false;
        if(!shutdown_->try_finish()) return false;
        thread_zones_.clear_drained_primaries();return true;
    }
private:
    static std::shared_ptr<WorldAdmissionServices> make_admission(AdmissionFactory& factory,VerticalSessionRegistry<Session>& registry) {
        auto services=factory(registry);if(!services) throw std::invalid_argument("World root admission factory returned null");return services;
    }
    WorldRoot(Strand strand,AdmissionFactory factory,std::unique_ptr<VerticalWorld> world,Update update)
        :strand_(std::move(strand)),owned_sessions_(std::make_unique<VerticalSessionRegistry<Session>>()),sessions_(*owned_sessions_),
         admission_services_(make_admission(factory,sessions_)),owned_admission_(std::make_unique<WorldAdmission>(*admission_services_)),admission_(*owned_admission_),world_(std::move(world)),update_(std::move(update)) {}
    WorldRoot(Strand strand,std::shared_ptr<VerticalSessionRegistry<Session>> sessions,std::shared_ptr<WorldAdmission> admission,
        std::function<void()> admission_update,std::function<void(RuntimeZone&)> zone_wait_update,std::unique_ptr<VerticalWorld> world,Update update)
        :strand_(std::move(strand)),retained_sessions_(std::move(sessions)),retained_admission_(std::move(admission)),sessions_(*retained_sessions_),admission_(*retained_admission_),admission_update_(std::move(admission_update)),zone_wait_update_(std::move(zone_wait_update)),world_(std::move(world)),update_(std::move(update)) {}
    void install_driver(WorldFrameDriver::OverrunCounter overruns,WorldFrameDriver::Failure failure) {
        std::weak_ptr<WorldRoot> weak=this->shared_from_this();
        driver_=WorldFrameDriver::create(strand_,world_,std::move(overruns),
            [weak](VerticalWorld& world,std::uint32_t now){
                auto self=weak.lock();if(!self || self->world_.get()!=&world) throw std::logic_error("World root lost its frame owner");
                self->promoted_this_frame_=self->thread_zones_.promote(now)!=0;
                struct ClearPromotion {WorldRoot& root;~ClearPromotion(){root.promoted_this_frame_=false;}} clear{*self};
                return self->update_(*self,now);
            },std::move(failure));
    }
    void require_strand() const {if(!strand_.running_in_this_thread()) throw std::logic_error("World root requires its world strand");}
    Strand strand_;std::shared_ptr<VerticalSessionRegistry<Session>> retained_sessions_;std::shared_ptr<WorldAdmission> retained_admission_;
    std::unique_ptr<VerticalSessionRegistry<Session>> owned_sessions_;VerticalSessionRegistry<Session>& sessions_;
    std::shared_ptr<WorldAdmissionServices> admission_services_;std::unique_ptr<WorldAdmission> owned_admission_;WorldAdmission& admission_;std::function<void()> admission_update_;std::function<void(RuntimeZone&)> zone_wait_update_;
    std::shared_ptr<VerticalWorld> world_;Update update_;
    WorldThreadZones thread_zones_;WorldInstanceCleanupPhase instance_cleanup_;WorldObeliskResetPhase obelisk_reset_;WorldZoneSendQueue instance_broadcasts_;std::shared_ptr<WorldSendUpdateServices> send_update_services_;std::function<std::uint32_t()> send_update_tick_;std::unique_ptr<WorldSendUpdate<WorldRoot>> send_update_;std::unique_ptr<WorldSessionLostPhase> session_lost_;std::unique_ptr<WorldSendPhase> send_phase_;std::unique_ptr<WorldRankPhase> rank_;std::unique_ptr<WorldMarketPhase> market_;std::unique_ptr<WorldKillCountPhase> kill_count_;std::unique_ptr<WorldKillCountTimer> kill_timer_;std::unique_ptr<WorldActivePacketPhase> active_packet_phase_;std::unique_ptr<WorldPrimaryFramePrefix> frame_prefix_;
    bool startup_attempted_=false,db_driver_drained_=false,promoted_this_frame_=false;WorldPrimaryItemPhase item_phase_;std::unique_ptr<WorldPrimaryNpcIdlePhase> npc_idle_phase_;std::unique_ptr<WorldPrimaryLogoutPhase> logout_phase_;std::unique_ptr<WorldPrimaryWeatherPhase> weather_phase_;std::unique_ptr<WorldPrimaryQuestPhase> quest_phase_;std::unique_ptr<WorldPrimaryUserAttackPhase> user_attack_phase_;std::unique_ptr<WorldPrimaryMobPhase> mob_phase_;std::shared_ptr<WorldFrameDriver> driver_;std::shared_ptr<DbDriver> db_driver_;std::shared_ptr<WorldSendDriver<WorldRoot>> send_driver_;std::shared_ptr<Shutdown> shutdown_;
};
}
