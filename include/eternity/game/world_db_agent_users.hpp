#pragma once
#include "eternity/game/user_lifecycle.hpp"
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// ps_dbAgent.exe CUserMgr::UpdateUserSave 0041d230 and SaveAll 0041d2f0.
// User is the retained CharacterSaveLifecycle (or its real save-job bridge).
// All state, connection and user operations are serialized on the DB owner.
template<class Connection,class User> class WorldDbAgentUsers final {
public:
    using Post=std::function<void(std::uint32_t)>;
    using Retire=std::function<void()>;
    void retain(std::uint32_t account,Connection& connection,std::shared_ptr<User> user,UserLifecycleState& state,Post post,Retire retire) {
        require_idle();if(!account || state.user_id!=account || !user || !post || !retire)throw std::invalid_argument("DB-agent user requires account, retained lifecycle, connection, post and retire owners");
        if(!users_.emplace(account,Entry{&connection,std::move(user),&state,std::move(post),std::move(retire)}).second)throw std::logic_error("Duplicate DB-agent user");
    }
    bool release(std::uint32_t account,const User& expected) {
        require_idle();const auto found=users_.find(account);if(found==users_.end() || found->second.user.get()!=&expected)return false;
        users_.erase(found);return true;
    }
    void update_user_save(std::uint32_t now) {
        Guard guard(*this);
        for(auto& [account,entry]:users_) {static_cast<void>(account);entry.user->update_save(now,last_save_tick_);}
    }
    // Match the removed game connection. Active users post shutdown-save job
    // 22 and stay retained; other users leave the registry before retirement.
    void connection_removed(Connection& connection) {
        Guard guard(*this);
        for(auto it=users_.begin();it!=users_.end();) {
            auto& entry=it->second;if(entry.connection!=&connection){++it;continue;}
            if(entry.state->status==2) {entry.post(22);++it;continue;}
            auto retired=std::move(entry);it=users_.erase(it);retired.retire();
        }
    }
    std::size_t size()const noexcept{return users_.size();}
    std::uint32_t last_save_tick()const noexcept{return last_save_tick_;}
private:
    struct Entry {Connection* connection;std::shared_ptr<User> user;UserLifecycleState* state;Post post;Retire retire;};
    void require_idle()const {if(traversing_)throw std::logic_error("Reentrant DB-agent user registry mutation");}
    struct Guard {WorldDbAgentUsers& owner;explicit Guard(WorldDbAgentUsers& value):owner(value){owner.require_idle();owner.traversing_=true;}~Guard(){owner.traversing_=false;}};
    std::map<std::uint32_t,Entry> users_;std::uint32_t last_save_tick_=0;bool traversing_=false;
};
}
