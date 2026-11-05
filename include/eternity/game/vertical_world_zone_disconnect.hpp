#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include <map>

namespace eternity::game {
// Owns the delayed location-4 LeaveUser sequence. The caller owns this object,
// its real services and world indexes until all pending operations have drained.
// Each returned service remains the same object for the complete operation.
// Wire begin to the session disconnect service, resume to the zone logout
// scheduler and acknowledge to the DB 0302 router's leave continuation.
// ps_game.exe CWorld::LeaveUser 0040ca20 and DB 0302 00408d30..00408dfd.
class VerticalWorldZoneDisconnect final {
public:
    using Service=std::function<VerticalDisconnectServices&(VerticalWorldSession&)>;
    VerticalWorldZoneDisconnect(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,WorldUsers& users,Service service)
        :strand_(std::move(strand)),registry_(registry),users_(users),service_(std::move(service)) {
        if(!service_)throw std::invalid_argument("Missing zone disconnect services");
    }
    VerticalWorldZoneDisconnect(const VerticalWorldZoneDisconnect&)=delete;
    VerticalWorldZoneDisconnect& operator=(const VerticalWorldZoneDisconnect&)=delete;
    ~VerticalWorldZoneDisconnect(){if(!pending_.empty())std::terminate();}
    void begin(VerticalWorldSession& session,std::function<void()> done) {
        require_strand();auto owner=retain(session);if(!done || session.admission().location!=4)throw std::logic_error("Invalid zone disconnect start");
        auto operation=std::make_shared<Operation>();operation->owner=std::move(owner);operation->done=std::move(done);
        if(!pending_.emplace(session.account_id(),operation).second)throw std::logic_error("Zone disconnect already started");
        operation->services=&service_(*operation->owner);advance(operation);
    }
    void resume(VerticalWorldSession& session) {
        require_strand();auto operation=find(session);if(operation->phase!=Phase::waiting)throw std::logic_error("Zone disconnect is not waiting");
        advance(operation);
    }
    void acknowledge(VerticalWorldSession& session) {
        require_strand();auto operation=find(session);
        if(operation->phase==Phase::requesting) {
            if(!operation->request_started)throw std::logic_error("Zone disconnect acknowledgment before save request acceptance");
            if(operation->ack)throw std::logic_error("Duplicate zone disconnect acknowledgment");operation->ack=true;return;
        }
        if(operation->phase!=Phase::saving)throw std::logic_error("Zone disconnect is not saving");
        finalize(operation);
    }
    bool contains(VerticalWorldSession& session) const {require_strand();retain(session);auto it=pending_.find(session.account_id());return it!=pending_.end() && it->second->owner.get()==&session;}
    std::size_t pending() const {require_strand();return pending_.size();}
private:
    enum class Phase {initializing,waiting,requesting,saving,finalizing,completing};
    struct Operation {std::shared_ptr<VerticalWorldSession> owner;std::function<void()> done;VerticalDisconnectServices* services=nullptr;Phase phase=Phase::initializing;bool request_started=false,ack=false;};
    class RequestFence final:public VerticalDisconnectServices {
    public:
        RequestFence(Operation& operation,VerticalDisconnectServices& inner):operation_(operation),inner_(inner){}
        std::recursive_mutex& user_mutex(WorldUser& user) override{return inner_.user_mutex(user);}
        std::recursive_mutex& zone_mutex(RuntimeZone& zone) override{return inner_.zone_mutex(zone);}
        void dead_disconnect(WorldUser& user) override{inner_.dead_disconnect(user);}
        std::uint32_t tick_ms() override{return inner_.tick_ms();}
        void prepare_save(WorldUser& user) override{inner_.prepare_save(user);}
        void request_save(WorldUser& user) override{inner_.request_save(user);operation_.request_started=true;}
        void leave_zone(WorldUser& user,RuntimeZone& zone) override{inner_.leave_zone(user,zone);}
        void finish_final(WorldUser& user) override{inner_.finish_final(user);}
    private:
        Operation& operation_;VerticalDisconnectServices& inner_;
    };
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Zone disconnect outside world strand");}
    std::shared_ptr<VerticalWorldSession> retain(VerticalWorldSession& session) const {
        auto owner=registry_.find_account(session.account_id());if(owner.get()!=&session)throw std::logic_error("Zone disconnect lost session owner");return owner;
    }
    std::shared_ptr<Operation> find(VerticalWorldSession& session) const {
        retain(session);auto it=pending_.find(session.account_id());if(it==pending_.end() || it->second->owner.get()!=&session)throw std::logic_error("Zone disconnect operation not found");return it->second;
    }
    void advance(const std::shared_ptr<Operation>& operation) {
        operation->phase=Phase::requesting;operation->request_started=false;
        RequestFence services(*operation,*operation->services);
        auto result=operation->owner->disconnect_in_zone(users_,false,services);
        if(result==VerticalDisconnectResult::deferred) {if(operation->ack)throw std::logic_error("Unexpected acknowledgment before save");operation->phase=Phase::waiting;return;}
        if(result!=VerticalDisconnectResult::save_requested)throw std::logic_error("Unexpected zone disconnect result");
        operation->phase=Phase::saving;if(operation->ack)finalize(operation);
    }
    void finalize(const std::shared_ptr<Operation>& operation) {
        operation->phase=Phase::finalizing;
        if(operation->owner->disconnect_in_zone(users_,true,*operation->services)!=VerticalDisconnectResult::finalized)
            throw std::logic_error("Zone disconnect did not finalize");
        operation->phase=Phase::completing;
        operation->done();
        auto it=pending_.find(operation->owner->account_id());if(it==pending_.end() || it->second!=operation)throw std::logic_error("Zone disconnect lost operation");
        pending_.erase(it);
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;WorldUsers& users_;Service service_;
    std::map<std::uint32_t,std::shared_ptr<Operation>> pending_;
};
}
