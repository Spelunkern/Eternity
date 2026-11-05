#pragma once
#include "eternity/game/vertical_world_session_save_jobs.hpp"
#include "eternity/game/vertical_social_save_fence.hpp"
#include "eternity/game/user_skill_runtime.hpp"

namespace eternity::game {
// Owns the per-session DB lifecycle, store and shared cache mutex. The store
// must be configured to use this same mutex and must retain its dedicated SQL
// connections and recovery writer. The owner retains this binding until drain.
// All methods and lifecycle post calls run on the world strand. The DB worker
// retains the resource bundle, session and bridge through accepted work.
template<class Store=CharacterSaveStore> class VerticalWorldSessionSaveBinding final {
public:
    using Jobs=VerticalWorldSessionSaveJobs<Store>;
    using Bridge=typename Jobs::Bridge;
    using Other=typename Jobs::Other;
    using Failure=typename Jobs::Failure;
    static std::shared_ptr<VerticalWorldSessionSaveBinding> create(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,
        std::shared_ptr<Store> store,std::shared_ptr<std::recursive_mutex> mutex,UserLifecycleState state,std::string username,
        CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,Other other,Failure failure) {
        if(!strand.running_in_this_thread() || !session || !session->character() || !store || !mutex || state.user_id!=session->account_id())
            throw std::invalid_argument("Invalid session save binding");
        auto resources=std::make_shared<Resources>(strand,registry,session,std::move(store),std::move(mutex),state);
        auto destination=std::make_shared<Destination>();
        ports.post=[destination](std::uint32_t job) {
            if(!destination->submit(job))throw std::runtime_error("Session save job rejected");
        };
        resources->bridge=std::make_unique<Bridge>(resources->state,session->character()->replica(),*resources->store,
            *resources->mutex,std::move(username),std::move(ports));
        // The alias keeps the store, mutex and state alive through every DB job.
        std::shared_ptr<Bridge> bridge(resources,resources->bridge.get());
        auto jobs=Jobs::create(strand,registry,std::move(session),dispatcher,std::move(bridge),std::move(router),std::move(other),std::move(failure));
        destination->jobs=jobs;
        return std::shared_ptr<VerticalWorldSessionSaveBinding>(new VerticalWorldSessionSaveBinding(std::move(resources),std::move(jobs),std::move(destination)));
    }
    // Bind social persistence and the ordering fence as one operation. The
    // resource bundle pins the social callback target through worker drain.
    template<class Database> static std::shared_ptr<VerticalWorldSessionSaveBinding> create_social(
        VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,
        DbJobDispatcher& dispatcher,std::shared_ptr<Store> store,std::shared_ptr<std::recursive_mutex> mutex,
        UserLifecycleState state,std::string username,CharacterSaveLifecyclePorts ports,
        std::shared_ptr<VerticalWorldSessionAckRouter> router,Other other,Failure failure,
        std::shared_ptr<VerticalSocialSession<VerticalWorldSession,Database>> social,
        std::shared_ptr<VerticalSocialPacketJobs<VerticalWorldSession,Database>> packets) {
        if(!session || !mutex || !social || !packets || !social->owns_session(*session) ||
           !packets->owns_session(*session) || !packets->owns_social(*social) || !social->uses_save_mutex(*mutex))
            throw std::invalid_argument("Mismatched social save owners");
        ports=social->bind_save(std::move(ports));
        auto binding=create(strand,registry,std::move(session),dispatcher,std::move(store),std::move(mutex),state,
            std::move(username),std::move(ports),std::move(router),std::move(other),failure);
        binding->attach_social(registry,std::move(social),std::move(packets),std::move(failure));return binding;
    }
    bool matches(const VerticalWorldSession& session) const noexcept {return resources_->session.get()==&session;}
    bool matches(const VerticalWorldSession::Strand& strand,const VerticalWorldSession::Registry& registry,const VerticalWorldSession& session) const noexcept {
        return resources_->strand==strand && resources_->registry==&registry && matches(session);
    }
    // 0804 addresses only a DB slot; verify its live skill identity before
    // applying any part of a zone save to the retained replica.
    bool matches_zone_skills(const UserSkillRuntime& skills) const {
        require_strand();std::lock_guard lock(*resources_->mutex);auto* user=resources_->session->world_user();
        if(!resources_->accepting || !user || &skills.actor()!=&user->actor)return false;
        const auto count=skills.learned_count();if(count>256)return false;
        const auto rows=resources_->session->character()->replica().skills().learned();
        for(std::int32_t i=0;i<count;++i)if(const auto skill=skills.slot(static_cast<std::uint32_t>(i))) {
            const auto slot=static_cast<std::uint8_t>(skill->number);
            if(slot>=rows.size() || rows[slot].number!=slot || rows[slot].id!=static_cast<std::uint16_t>(skill->skill_id) ||
               rows[slot].level!=static_cast<std::uint8_t>(skill->level))return false;
        }
        return true;
    }
    bool disconnect(std::span<const std::uint8_t> packet) {require_strand();std::lock_guard lock(*resources_->mutex);return resources_->accepting && jobs_->bridge().disconnect(packet);}
    InventoryReplicaResult apply(std::span<const std::uint8_t> packet) {require_strand();std::lock_guard lock(*resources_->mutex);return resources_->accepting?jobs_->bridge().apply(packet):InventoryReplicaResult::rejected;}
    // Bulk skill, effect and quest updates silently ignore absent rows. A zone
    // exit must not claim its live state was saved when its replica is stale.
    // Validate and apply under the same recursive mutex as the DB worker.
    InventoryReplicaResult apply_zone_save(std::span<const std::uint8_t> packet,const UserSkillRuntime* skills=nullptr) {
        require_strand();std::lock_guard lock(*resources_->mutex);if(!resources_->accepting)return InventoryReplicaResult::rejected;
        if(packet.size()>=7 && packet[0]==4 && packet[1]==8 && packet.size()==7+3U*packet[6]) {
            if(skills && !matches_zone_skills(*skills))return InventoryReplicaResult::rejected;
            const auto rows=resources_->session->character()->replica().skills().learned();
            for(std::size_t at=7;at<packet.size();at+=3) {
                const auto slot=packet[at];
                if(slot>=rows.size() || rows[slot].number!=slot)return InventoryReplicaResult::rejected;
            }
        }
        if(packet.size()>=7 && packet[0]==4 && packet[1]==9 && packet.size()==7+7U*packet[6]) {
            const auto rows=resources_->session->character()->replica().skills().effects();std::size_t current=0;
            for(std::size_t at=7;at<packet.size();at+=7) {
                const auto id=static_cast<std::uint16_t>(packet[at]|(std::uint16_t(packet[at+1])<<8));
                while(current<rows.size() && (rows[current].id!=id || rows[current].level!=packet[at+2]))++current;
                if(current==rows.size())return InventoryReplicaResult::rejected;
                // PacketUserSkillApply does not advance after a match.
            }
        }
        if(packet.size()>=7 && packet[0]==4 && packet[1]==10 && packet.size()==7+7U*packet[6]) {
            const auto rows=resources_->session->character()->replica().quests().active();std::size_t current=0;
            for(std::size_t at=7;at<packet.size();at+=7) {
                const auto id=static_cast<std::uint16_t>(packet[at]|(std::uint16_t(packet[at+1])<<8));
                while(current<rows.size() && rows[current].id!=id)++current;
                if(current==rows.size())return InventoryReplicaResult::rejected;
                ++current;
            }
        }
        return jobs_->bridge().apply(packet);
    }
    bool owns_session(const VerticalWorldSession& session) const noexcept {return resources_->session.get()==&session;}
    std::recursive_mutex& save_mutex() noexcept {return *resources_->mutex;}
    bool submit(std::uint32_t job) {require_strand();std::lock_guard lock(*resources_->mutex);return resources_->accepting && destination_->submit(job);}
    template<class Database> void attach_social(VerticalWorldSession::Registry& registry,
        std::shared_ptr<VerticalSocialSession<VerticalWorldSession,Database>> social,
        std::shared_ptr<VerticalSocialPacketJobs<VerticalWorldSession,Database>> packets,Failure failure) {
        if(destination_->started || social_owner_)throw std::logic_error("Social save fence installed too late");
        if(!social || !packets || !social->owns_session(*resources_->session) || !packets->owns_social(*social) || !packets->uses_save_mutex(*resources_->mutex))throw std::invalid_argument("Social save mutex mismatch");
        using Fence=VerticalSocialSaveFence<VerticalWorldSession,Database,Jobs>;
        auto expected=resources_->session;std::weak_ptr<Jobs> weak_jobs=jobs_;
        auto fence=std::make_shared<Fence>(expected,std::move(packets),registry,
            [expected,weak_jobs](VerticalWorldSession& session) {return &session==expected.get()?weak_jobs.lock():std::shared_ptr<Jobs>{};},std::move(failure));
        std::weak_ptr<Fence> weak=fence;
        destination_->gate=[weak](std::uint32_t job){auto owner=weak.lock();return owner && owner->submit(job);};
        drain_social_=[weak](std::function<void()> done){auto owner=weak.lock();if(!owner)throw std::logic_error("Social save fence lost");owner->stop_and_drain(std::move(done));};
        resources_->social=std::move(social);social_owner_=std::move(fence);
    }
    template<class Database> bool binds_social(const VerticalSocialSession<VerticalWorldSession,Database>& social) const noexcept {return resources_->social.get()==&social && static_cast<bool>(social_owner_);}
    void stop_and_drain(std::function<void()> done) {
        require_strand();if(!done)throw std::invalid_argument("Missing save binding drain completion");
        {std::lock_guard lock(*resources_->mutex);resources_->accepting=false;}
        if(drain_social_)drain_social_(std::move(done));else {auto jobs=jobs_;jobs->stop_and_drain(std::move(done));}
    }
private:
    void require_strand() const {if(!resources_->strand.running_in_this_thread())throw std::logic_error("Save binding outside world strand");}
    struct Destination {
        std::weak_ptr<Jobs> jobs;std::function<bool(std::uint32_t)> gate;bool started=false;
        bool submit(std::uint32_t job) {started=true;if(gate)return gate(job);auto owner=jobs.lock();return owner && owner->submit(job);}
    };
    struct Resources {
        Resources(VerticalWorldSession::Strand executor,VerticalWorldSession::Registry& sessions,std::shared_ptr<VerticalWorldSession> owner,std::shared_ptr<Store> s,std::shared_ptr<std::recursive_mutex> m,UserLifecycleState v)
            :strand(std::move(executor)),registry(&sessions),session(std::move(owner)),mutex(std::move(m)),store(std::move(s)),state(v){}
        VerticalWorldSession::Strand strand;VerticalWorldSession::Registry* registry;std::shared_ptr<VerticalWorldSession> session;std::shared_ptr<std::recursive_mutex> mutex;std::shared_ptr<Store> store;
        UserLifecycleState state;std::shared_ptr<void> social;std::unique_ptr<Bridge> bridge;bool accepting=true;
    };
    VerticalWorldSessionSaveBinding(std::shared_ptr<Resources> resources,std::shared_ptr<Jobs> jobs,std::shared_ptr<Destination> destination)
        :resources_(std::move(resources)),jobs_(std::move(jobs)),destination_(std::move(destination)){}
    std::shared_ptr<Resources> resources_;std::shared_ptr<Jobs> jobs_;std::shared_ptr<Destination> destination_;
    std::shared_ptr<void> social_owner_;std::function<void(std::function<void()>)> drain_social_;
};
}
