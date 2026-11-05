#pragma once
#include "eternity/game/world_send_retained_update.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include <functional>
#include <map>
#include <set>
#include <type_traits>

namespace eternity::game {
// CWorld::UpdateNetSend 0040c1b0. Session::admission() is the actual list
// member. A pre-account connection has no registry entry and must be retained
// by its real connection owner, supplied through retain_unreserved.
template<class Session> class WorldRetainedGlobalSend final:public WorldGlobalSendServices {
public:
    using RetainUnreserved=std::function<std::shared_ptr<Session>(WorldAdmissionUser&)>;
    using Flush=std::function<void(Session&)>;
    using Continue=std::function<bool()>;
    WorldRetainedGlobalSend(VerticalSessionRegistry<Session>& registry,RetainUnreserved retain_unreserved,
        Flush send_now,Flush send_priority_and_reset,Continue keep_running)
        :registry_(registry),retain_unreserved_(std::move(retain_unreserved)),send_now_(std::move(send_now)),priority_(std::move(send_priority_and_reset)),keep_running_(std::move(keep_running)) {
        if(!retain_unreserved_ || !send_now_ || !priority_ || !keep_running_)throw std::invalid_argument("Global send requires actual retention, send operations and continuation");
    }
    std::shared_ptr<void> retain(WorldAdmissionUser& user) override{return acquire(user);}
    template<class Visitor> decltype(auto) visit_session(WorldAdmissionUser& user,Visitor&& visitor) {
        static_assert(!std::is_reference_v<std::invoke_result_t<Visitor,Session&>>,"Retained session references cannot escape the visit");
        auto pin=acquire(user);return std::forward<Visitor>(visitor)(*pin->session);
    }
    void flush(WorldAdmissionUser& user) override {
        if(!keep_running_())return;
        auto pin=acquire(user);if(!keep_running_())return;
        send_now_(*pin->session);if(keep_running_())priority_(*pin->session);
    }
private:
    struct Pin {std::shared_ptr<Session> session;};
    std::shared_ptr<Pin> acquire(WorldAdmissionUser& user) {
        if(const auto found=active_.find(&user);found!=active_.end()) {
            if(auto pin=found->second.lock()) {if(&pin->session->admission()!=&user)throw std::logic_error("Global send changed pinned admission user");return pin;}
            active_.erase(found);
        }
        for(auto it=active_.begin();it!=active_.end();)if(it->second.expired())it=active_.erase(it);else ++it;
        const auto [entry,inserted]=acquiring_.insert(&user);if(!inserted)throw std::logic_error("Global send session acquisition is reentrant");
        struct Guard {std::set<WorldAdmissionUser*>& entries;std::set<WorldAdmissionUser*>::iterator entry;~Guard(){entries.erase(entry);}} guard{acquiring_,entry};
        auto session=registry_.find_account(user.user_id);
        if(!session)session=retain_unreserved_(user);
        if(!session || &session->admission()!=&user)throw std::logic_error("Global send lost its admission session");
        auto pin=std::make_shared<Pin>(Pin{std::move(session)});active_[&user]=pin;return pin;
    }
    VerticalSessionRegistry<Session>& registry_;RetainUnreserved retain_unreserved_;Flush send_now_,priority_;Continue keep_running_;
    std::map<WorldAdmissionUser*,std::weak_ptr<Pin>> active_;std::set<WorldAdmissionUser*> acquiring_;
};
}
