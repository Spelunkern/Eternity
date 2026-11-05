#pragma once
#include "eternity/game/vertical_world_session_preparation.hpp"
#include "eternity/game/vertical_social_selection.hpp"
#include "eternity/game/vertical_world_session_social.hpp"

namespace eternity::game {
// Retain this owner through packet/save drain and world release. The replica is
// account-owned and must also survive character selection cycles. Services and
// WorldUsers/registry are the real world owners and outlive this bundle.
template<class Database> class VerticalSocialWorldOwner final {
public:
    using Social=VerticalSocialSession<VerticalWorldSession,Database>;
    using Jobs=VerticalSocialPacketJobs<VerticalWorldSession,Database>;
    static std::shared_ptr<VerticalSocialWorldOwner> create(VerticalWorldSession::Strand strand,
        std::shared_ptr<VerticalWorldSession> session,std::shared_ptr<VerticalSocialReplica> replica,
        std::shared_ptr<Database> database,std::shared_ptr<std::recursive_mutex> mutex,
        std::shared_ptr<ZoneLeaveRequestState> requests,VerticalWorldSession::Registry& registry,WorldUsers& world,
        typename Social::Services services,DbJobDispatcher& dispatcher,typename Jobs::Failure failure,
        std::string prefix="PS_GameData.dbo.",std::optional<VerticalSocialSelection> selected={}) {
        if(!strand.running_in_this_thread() || !session || !session->character() || !session->world_user() ||
           !replica || !database || !mutex || !requests || !failure)
            throw std::invalid_argument("Incomplete retained social world owner");
        auto owner=std::shared_ptr<VerticalSocialWorldOwner>(new VerticalSocialWorldOwner(std::move(session),std::move(replica),
            std::move(database),std::move(mutex),std::move(requests)));
        auto selection=selected?std::move(*selected):[&]{std::lock_guard lock(*owner->mutex_);return vertical_load_social_selection(owner->session_->character()->snapshot(),*owner->replica_);}();
        if(selection.catalog.account_id()!=owner->replica_->account_id() || selection.catalog.character_id()!=owner->session_->character()->snapshot().core.character.id)throw std::invalid_argument("Mismatched social selection");
        owner->friends_loaded_=selection.friends_loaded;owner->blocks_loaded_=selection.blocks_loaded;
        owner->social_=std::make_shared<Social>(*owner->session_,*owner->session_->world_user(),std::move(selection.catalog),
            *owner->requests_,*owner->replica_,owner->session_->character()->replica(),*owner->database_,*owner->mutex_,
            registry,world,std::move(services),std::move(prefix));
        owner->jobs_=Jobs::create(strand,registry,owner->session_,*owner->social_,dispatcher,std::move(failure));
        return owner;
    }
    // Transfer the already-loaded catalog once, then bind entry delivery to the
    // exact live social owner. No second load may reset account replica state.
    static std::shared_ptr<VerticalSocialWorldOwner> create_for_selection(VerticalWorldSession::Strand strand,
        std::shared_ptr<VerticalWorldSession> session,VerticalWorldSessionSocial& selection,
        std::shared_ptr<Database> database,std::shared_ptr<ZoneLeaveRequestState> requests,
        VerticalWorldSession::Registry& registry,WorldUsers& world,typename Social::Services services,
        DbJobDispatcher& dispatcher,typename Jobs::Failure failure,std::string prefix="PS_GameData.dbo.") {
        auto account=selection.account();if(!account)throw std::invalid_argument("Missing social account");
        auto replica=std::shared_ptr<VerticalSocialReplica>(account,&account->replica());
        VerticalSocialSelection loaded{selection.take_catalog(),selection.friends_loaded(),selection.blocks_loaded()};
        auto owner=create(strand,std::move(session),std::move(replica),std::move(database),account->shared_mutex(),std::move(requests),
            registry,world,std::move(services),dispatcher,std::move(failure),std::move(prefix),std::move(loaded));
        selection.bind_catalog(owner->social_);return owner;
    }
    // The session owns the invitation state used by both social packets and
    // zone leave; retain that same state for the complete social lifetime.
    static std::shared_ptr<VerticalSocialWorldOwner> create_for_session(VerticalWorldSession::Strand strand,
        std::shared_ptr<VerticalWorldSession> session,VerticalWorldSessionSocial& selection,
        std::shared_ptr<Database> database,VerticalWorldSession::Registry& registry,WorldUsers& world,
        typename Social::Services services,DbJobDispatcher& dispatcher,typename Jobs::Failure failure,
        std::string prefix="PS_GameData.dbo.") {
        if(!session)throw std::invalid_argument("Missing social session");
        auto requests=std::shared_ptr<ZoneLeaveRequestState>(session,&session->zone_requests());
        return create_for_selection(strand,std::move(session),selection,std::move(database),std::move(requests),
            registry,world,std::move(services),dispatcher,std::move(failure),std::move(prefix));
    }
    VerticalPreparedSocialOwners<Database> prepared() const {return {social_,jobs_};}
    Social& social() const noexcept {return *social_;}
    Jobs& jobs() const noexcept {return *jobs_;}
    bool friends_loaded() const noexcept {return friends_loaded_;}
    bool blocks_loaded() const noexcept {return blocks_loaded_;}
private:
    VerticalSocialWorldOwner(std::shared_ptr<VerticalWorldSession> session,std::shared_ptr<VerticalSocialReplica> replica,
        std::shared_ptr<Database> database,std::shared_ptr<std::recursive_mutex> mutex,std::shared_ptr<ZoneLeaveRequestState> requests)
        :session_(std::move(session)),replica_(std::move(replica)),database_(std::move(database)),mutex_(std::move(mutex)),requests_(std::move(requests)){}
    std::shared_ptr<VerticalWorldSession> session_;std::shared_ptr<VerticalSocialReplica> replica_;std::shared_ptr<Database> database_;
    std::shared_ptr<std::recursive_mutex> mutex_;std::shared_ptr<ZoneLeaveRequestState> requests_;
    std::shared_ptr<Social> social_;std::shared_ptr<Jobs> jobs_;bool friends_loaded_=false,blocks_loaded_=false;
};
}
