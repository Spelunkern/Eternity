#pragma once
#include "eternity/game/vertical_world_session_owner.hpp"

namespace eternity::game {
// Required live services for a per-session SQL save preparation. Providers run
// on the world strand and return actual state and retained owners; SQL opens on
// the DB executor. All referenced dispatcher/router/services outlive drains.
struct VerticalWorldSessionSqlPreparationServices {
    std::function<UserLifecycleState(VerticalWorldSession&)> state;
    std::function<std::filesystem::path(VerticalWorldSession&)> recovery_path;
    std::function<CharacterSaveLifecyclePorts(VerticalWorldSession&)> lifecycle;
    std::function<std::shared_ptr<std::recursive_mutex>(VerticalWorldSession&)> cache_mutex;
    std::function<std::shared_ptr<VerticalWorldSessionAckRouter>(VerticalWorldSession::Registry&)> ack_router;
    VerticalWorldSessionSqlSave::Binding::Other other;
    VerticalWorldSessionSqlSave::Binding::Failure failure;
};
// This binds the existing real SQL opener, save store and session save jobs;
// it performs no connection or SQL work during provider construction. Social
// saves, disconnect and failure callbacks remain supplied by their live owners.
// ps_game.exe PacketUserDBChar 0046cf60 / ps_dbAgent.exe OnDispatchDB 00403dd0.
inline VerticalWorldSessionSqlOwner::PreparationProvider vertical_world_session_sql_preparation_provider(
    DbJobDispatcher& dispatcher,storage::SqlServerConfig config,std::uint8_t server,std::string catalog,
    VerticalWorldSessionSqlPreparationServices services) {
    if(catalog.empty() || !services.state || !services.recovery_path || !services.lifecycle || !services.cache_mutex ||
       !services.ack_router || !services.other || !services.failure)
        throw std::invalid_argument("Incomplete SQL preparation services");
    return [&dispatcher,config=std::move(config),server,catalog=std::move(catalog),services=std::move(services)]
        (VerticalWorldSession::Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session) {
        if(!strand.running_in_this_thread() || !session || registry.find_account(session->account_id())!=session || session->username().empty())
            throw std::invalid_argument("SQL preparation requires the retained account");
        auto state=services.state(*session);if(state.user_id!=session->account_id())throw std::invalid_argument("SQL preparation account mismatch");
        auto path=services.recovery_path(*session);auto ports=services.lifecycle(*session);
        auto mutex=services.cache_mutex(*session);auto router=services.ack_router(registry);
        if(path.empty() || !mutex || !router || !router->matches(strand,registry) || !ports.send || !ports.leave ||
           !ports.reset || !ports.roster || !ports.save_friends || !ports.save_blocks)
            throw std::invalid_argument("Incomplete retained SQL save owners");
        auto username=session->username();
        return make_vertical_world_session_sql_preparation(std::move(strand),std::move(database),registry,std::move(session),dispatcher,
            config,server,std::move(path),catalog,state,std::move(username),std::move(ports),std::move(router),services.other,services.failure,std::move(mutex));
    };
}

// Use the exact retained social account mutex rather than a separately
// supplied cache lock. Social job ownership and save callbacks are still
// required from their real provider; this does not install a social session.
inline VerticalWorldSessionSqlOwner::RuntimePreparationProvider vertical_world_session_sql_account_preparation_provider(
    DbJobDispatcher& dispatcher,storage::SqlServerConfig config,std::uint8_t server,std::string catalog,
    VerticalWorldSessionSqlPreparationServices services) {
    if(services.cache_mutex || catalog.empty() || !services.state || !services.recovery_path || !services.lifecycle ||
       !services.ack_router || !services.other || !services.failure)
        throw std::invalid_argument("Incomplete account SQL preparation services");
    return [&dispatcher,config=std::move(config),server,catalog=std::move(catalog),services=std::move(services)]
        (VerticalWorldSession::Strand strand,asio::any_io_executor database,VerticalWorldSession::Registry& registry,
         VerticalWorldSessionSqlOwner::Runtime& runtime,std::shared_ptr<VerticalWorldSession> session) {
        auto bound=services;bound.cache_mutex=[&runtime](VerticalWorldSession& owner) {
            return runtime.social_session(owner).account()->shared_mutex();
        };
        auto provider=vertical_world_session_sql_preparation_provider(dispatcher,config,server,catalog,std::move(bound));
        return provider(std::move(strand),std::move(database),registry,std::move(session));
    };
}
}
