#pragma once
#include "eternity/game/vertical_move_directory_gameplay.hpp"
#include "eternity/game/vertical_move_priority_channel.hpp"
#include <limits>
#include <map>
#include <set>

namespace eternity::game {
// Installs actual movement into a runtime whose Ports exposes gameplay and
// disconnect. The runtime must stop packet producers before disconnect and
// retain the session/registry until its completion. All calls use the strand.
// The underlying disconnect must close the canonical connection projection,
// stop other producers and fence extracted sends before its completion.
template<class Gameplay,class Strand,class Session>
class VerticalMoveRuntimeComposition final:public std::enable_shared_from_this<VerticalMoveRuntimeComposition<Gameplay,Strand,Session>> {
public:
    struct Resources {
        std::shared_ptr<void> lease;PartyMembershipSendWorld& pool;PartyMembershipUser& connection;ZoneLeaveShopState& shop;
    };
    using Directory=VerticalMoveSessionDirectory<Session>;
    using Acquire=std::function<Resources(Session&)>;
    using Services=std::function<VerticalMoveDirectoryServices<Session>(Session&,RuntimeZone&,Directory&)>;
    using Other=typename Gameplay::Other;
    static std::shared_ptr<VerticalMoveRuntimeComposition> create(Strand strand,VerticalSessionRegistry<Session>& registry,
        Acquire acquire,Services services,Other other) {
        if(!acquire || !services || !other)throw std::invalid_argument("Incomplete movement runtime composition");
        return std::shared_ptr<VerticalMoveRuntimeComposition>(new VerticalMoveRuntimeComposition(std::move(strand),registry,std::move(acquire),std::move(services),std::move(other)));
    }
    template<class Ports> void install(Ports& ports) {
        if(installed_ || !ports.disconnect)throw std::logic_error("Movement runtime requires an uninstalled disconnect owner");
        if constexpr(requires {ports.configure_admission;})
            if(!ports.configure_admission)throw std::logic_error("Movement runtime requires an admission configuration owner");
        auto self=this->shared_from_this();auto disconnect=std::move(ports.disconnect);
        if constexpr(requires {ports.configure_admission;}) {
            auto configure=std::move(ports.configure_admission);
            ports.configure_admission=[self,configure=std::move(configure)](Session& session,WorldAdmissionUser& user,const auto& snapshot) {
                self->require_strand();
                if(&session.admission()!=&user)throw std::logic_error("Movement admission lost its session user");
                configure(session,user,snapshot);
                if(!session.closing()) {
                    if(session.party_user().send_socket==-1)throw std::logic_error("Movement admission lacks a live connection projection");
                    (void)self->resources(session);
                }
            };
        }
        ports.gameplay=[self](Session& session,RuntimeZone& zone){return self->gameplay(session,zone);};
        ports.disconnect=[self,disconnect=std::move(disconnect)](Session& session,std::optional<std::uint32_t> reason,std::function<void()> done) {
            self->require_strand();if(!done)throw std::invalid_argument("Missing movement disconnect completion");
            auto owner=session.shared_from_this();auto completed=std::make_shared<bool>(false);
            auto* user=session.world_user();auto* actor=self->directory_.actor_for(session);
            if(actor && (!user || &user->actor!=actor || self->registry_.retain_actor(*actor)!=owner))
                throw std::logic_error("Movement disconnect lost its actor");
            disconnect(session,reason,[self,owner,user,actor,done=std::move(done),completed]() mutable {
                self->require_strand();if(*completed)throw std::logic_error("Movement disconnect completed twice");
                if(actor) {
                    if(owner->world_user()!=user || self->registry_.retain_actor(*actor)!=owner)
                        throw std::logic_error("Movement disconnect changed its registered actor");
                    if(self->draining_.contains(actor->id()))
                        throw std::logic_error("Movement disconnect completed during priority drain");
                    if(self->visiting_.contains(actor->id()))
                        throw std::logic_error("Movement disconnect completed during world send visit");
                    self->directory_.close(*actor);
                }
                *completed=true;done();
            });
        };
        installed_=true;
    }
    // Can be called at zone admission or when a retained recipient is first
    // visited, before that recipient has ever sent a movement packet.
    std::shared_ptr<VerticalMoveSessionResources> resources(Session& session) {
        require_strand();auto owner=session.shared_from_this();
        if(session.closing() || !session.world_user())throw std::logic_error("Movement session has no live world user");
        auto* world_user=session.world_user();auto& actor=world_user->actor;
        if(registry_.retain_actor(actor)!=owner)throw std::logic_error("Movement session does not own its actor");
        if(auto existing=directory_.find(actor))return existing;
        const auto [pending,inserted]=acquiring_.insert(actor.id());
        if(!inserted)throw std::logic_error("Movement resources are already being acquired");
        struct AcquisitionGuard {
            std::set<std::uint32_t>& pending;std::set<std::uint32_t>::iterator entry;
            ~AcquisitionGuard(){pending.erase(entry);}
        } guard{acquiring_,pending};
        auto inputs=acquire_(session);
        if(session.closing() || session.world_user()!=world_user || registry_.retain_actor(actor)!=owner)
            throw std::logic_error("Movement session changed during resource acquisition");
        return directory_.attach(owner,actor,std::move(inputs.lease),inputs.pool,inputs.connection,inputs.shop);
    }
    // World scheduler transfers at most 128 queued frames to the real channel.
    // A return value counts channel send calls, not accepted or completed writes.
    // The scheduler owns backpressure, subsequent batches and final IO fences.
    std::size_t drain(Session& session) {
        require_strand();auto owner=session.shared_from_this();
        if(session.closing() || !session.world_user())throw std::logic_error("Movement session cannot drain");
        auto& actor=session.world_user()->actor;
        if(registry_.retain_actor(actor)!=owner)throw std::logic_error("Movement session does not own its actor");
        auto current=directory_.find(actor);
        if(!current)throw std::logic_error("Movement session has no priority resources");
        if(current->connection().send_socket==-1)throw std::logic_error("Movement connection is closed");
        const auto [pending,inserted]=draining_.insert(actor.id());
        if(!inserted)throw std::logic_error("Movement priority drain is already active");
        struct DrainGuard {
            std::set<std::uint32_t>& pending;std::set<std::uint32_t>::iterator entry;
            ~DrainGuard(){pending.erase(entry);}
        } guard{draining_,pending};
        VerticalMovePriorityChannel<Session> delivery(registry_,current->pool(),
            [this](Session& recipient)->UserPriorityQueue& {
                if(!recipient.world_user())throw std::logic_error("Movement recipient has no world user");
                auto state=directory_.find(recipient.world_user()->actor);
                if(!state)throw std::logic_error("Movement recipient has no priority resources");
                return state->queue();
            },[](Session& recipient)->net::WorldChannel&{return recipient.channel();});
        return delivery.drain(actor);
    }
    // Fence the coordinator's whole priority flush, including its reset of
    // unsent queue entries. This is a synchronous world-strand fence only;
    // the transport owner separately fences posted IO and completed writes.
    void with_priority_flush(Session& session,const std::function<void()>& flush) {
        require_strand();if(!flush)throw std::invalid_argument("Missing world priority flush");
        auto owner=session.shared_from_this();auto* user=session.world_user();
        if(!user || registry_.retain_actor(user->actor)!=owner)throw std::logic_error("World priority flush lost its actor");
        auto current=directory_.find(user->actor);
        if(!current && !session.closing())current=resources(session);
        if(!current)throw std::logic_error("World priority flush has no retained resources");
        const auto [pending,inserted]=draining_.insert(user->actor.id());
        if(!inserted)throw std::logic_error("Movement priority drain is already active");
        struct DrainGuard {
            std::set<std::uint32_t>& pending;std::set<std::uint32_t>::iterator entry;
            ~DrainGuard(){pending.erase(entry);}
        } guard{draining_,pending};
        flush();
    }
    // Retain the actual session/resource pin across a synchronous world-send
    // visit. The returned lease must be released on this same world strand.
    std::shared_ptr<void> retain_send_visit(Session& session,std::shared_ptr<void> pin) {
        require_strand();if(!pin)throw std::invalid_argument("World send visit requires an owner pin");
        auto owner=session.shared_from_this();auto* user=session.world_user();
        if(!user || registry_.retain_actor(user->actor)!=owner)throw std::logic_error("World send visit lost its actor");
        auto self=this->shared_from_this();const auto id=user->actor.id();
        auto it=visiting_.try_emplace(id,0).first;
        if(it->second==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Too many world send visits");
        ++it->second;
        struct Visit {
            std::shared_ptr<VerticalMoveRuntimeComposition> self;std::uint32_t id;std::shared_ptr<void> pin;
            Visit(std::shared_ptr<VerticalMoveRuntimeComposition> owner,std::uint32_t value,std::shared_ptr<void> retained)
                :self(std::move(owner)),id(value),pin(std::move(retained)) {}
            ~Visit(){auto at=self->visiting_.find(id);if(--at->second==0)self->visiting_.erase(at);}
        };
        try{return std::make_shared<Visit>(std::move(self),id,std::move(pin));}
        catch(...){if(--it->second==0)visiting_.erase(it);throw;}
    }
    Directory& directory() noexcept {return directory_;}
private:
    // Production supplies the coordinator's asio strand. Lightweight fixture
    // executors without strand inspection keep their existing contract.
    void require_strand() const {
        if constexpr(requires(const Strand& value){value.running_in_this_thread();})
            if(!strand_.running_in_this_thread())throw std::logic_error("Movement runtime outside world strand");
    }
    VerticalMoveRuntimeComposition(Strand strand,VerticalSessionRegistry<Session>& registry,Acquire acquire,Services services,Other other)
        :strand_(std::move(strand)),registry_(registry),directory_(registry),acquire_(std::move(acquire)),services_(std::move(services)),other_(std::move(other)) {}
    std::shared_ptr<Gameplay> gameplay(Session& session,RuntimeZone& zone) {
        auto owner=session.shared_from_this();(void)resources(session);
        return make_vertical_move_directory_gameplay<Gameplay>(strand_,registry_,directory_,owner,zone,services_(session,zone,directory_),other_);
    }
    Strand strand_;VerticalSessionRegistry<Session>& registry_;Directory directory_;Acquire acquire_;Services services_;Other other_;
    std::set<std::uint32_t> acquiring_;std::set<std::uint32_t> draining_;std::map<std::uint32_t,std::size_t> visiting_;bool installed_=false;
};
}
