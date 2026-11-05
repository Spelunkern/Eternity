#pragma once
#include "eternity/game/vertical_world_session_save_binding.hpp"
#include <atomic>

namespace eternity::game {
// Owns the dedicated SQL connections and recovery writer borrowed by the real
// CharacterSaveStore. open() performs blocking ODBC connection setup and must
// run on a database worker, never on the world strand. No database lifecycle or
// schema operation is performed. The recovery path is supplied by the runtime.
class VerticalWorldSessionSqlSave final:public std::enable_shared_from_this<VerticalWorldSessionSqlSave> {
public:
    using Binding=VerticalWorldSessionSaveBinding<CharacterSaveStore>;
    static std::shared_ptr<VerticalWorldSessionSqlSave> open(const storage::SqlServerConfig& config,std::uint8_t server,
        std::filesystem::path recovery_path,std::string catalog="PS_GameData",std::shared_ptr<std::recursive_mutex> mutex={}) {
        if(recovery_path.empty() || catalog.empty())throw std::invalid_argument("Missing save recovery path or catalog");
        if(!mutex)mutex=std::make_shared<std::recursive_mutex>();
        return std::shared_ptr<VerticalWorldSessionSqlSave>(new VerticalWorldSessionSqlSave(config,server,std::move(recovery_path),std::move(catalog),std::move(mutex)));
    }
    // Aliases retain both connections, the recovery file and the cache mutex.
    std::shared_ptr<CharacterSaveStore> store() {return {shared_from_this(),&store_};}
    std::shared_ptr<std::recursive_mutex> mutex() {return {shared_from_this(),mutex_.get()};}
    // Only inspect after all database work using this bundle has drained.
    bool connections_clean() const noexcept {return !main_.character_write_context_active() && !auxiliary_.character_write_context_active();}
    // One bundle is dedicated to one session; callers retain the resulting
    // binding through drain. Failed construction leaves it available for retry.
    std::shared_ptr<Binding> bind(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,UserLifecycleState state,std::string username,
        CharacterSaveLifecyclePorts ports,std::shared_ptr<VerticalWorldSessionAckRouter> router,Binding::Other other,Binding::Failure failure) {
        if(!strand.running_in_this_thread())throw std::logic_error("SQL save binding outside world strand");
        bool expected=false;if(!bound_.compare_exchange_strong(expected,true))throw std::logic_error("SQL save bundle already bound");
        try {return Binding::create(strand,registry,std::move(session),dispatcher,store(),mutex(),state,std::move(username),
            std::move(ports),std::move(router),std::move(other),std::move(failure));}
        catch(...) {bound_=false;throw;}
    }
    // The social owner and jobs must already be built with this bundle's mutex.
    // One binding retains their callback target until the save workers drain.
    template<class Database> std::shared_ptr<Binding> bind_social(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,DbJobDispatcher& dispatcher,
        UserLifecycleState state,std::string username,CharacterSaveLifecyclePorts ports,
        std::shared_ptr<VerticalWorldSessionAckRouter> router,Binding::Other other,Binding::Failure failure,
        std::shared_ptr<VerticalSocialSession<VerticalWorldSession,Database>> social,
        std::shared_ptr<VerticalSocialPacketJobs<VerticalWorldSession,Database>> packets) {
        if(!strand.running_in_this_thread())throw std::logic_error("SQL social binding outside world strand");
        bool expected=false;if(!bound_.compare_exchange_strong(expected,true))throw std::logic_error("SQL save bundle already bound");
        try {return Binding::create_social(strand,registry,std::move(session),dispatcher,store(),mutex(),state,std::move(username),
            std::move(ports),std::move(router),std::move(other),std::move(failure),std::move(social),std::move(packets));}
        catch(...) {bound_=false;throw;}
    }
private:
    VerticalWorldSessionSqlSave(const storage::SqlServerConfig& config,std::uint8_t server,std::filesystem::path path,std::string catalog,std::shared_ptr<std::recursive_mutex> mutex)
        :main_(config),auxiliary_(config),mutex_(std::move(mutex)),recovery_(std::move(path)),store_(main_,auxiliary_,server,*mutex_,recovery_,std::move(catalog)){}
    storage::SqlServer main_,auxiliary_;std::shared_ptr<std::recursive_mutex> mutex_;CharacterRecoveryFile recovery_;CharacterSaveStore store_;std::atomic<bool> bound_{false};
};
}
