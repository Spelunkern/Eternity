#pragma once
#include "eternity/game/vertical_quests_world_session.hpp"
#include "eternity/game/vertical_world_session_preparation.hpp"

namespace eternity::game {
// Compose the quest callback from real world owners. The registry and all
// factory dependencies must outlive preparation and quest teardown.
inline std::function<VerticalWorldSessionSqlPreparation::AfterBind(VerticalWorldSession::Strand,VerticalWorldSession::Registry&,VerticalWorldSession&,std::shared_ptr<std::recursive_mutex>)> vertical_world_sql_quest_after_bind_provider(
    std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions,
    std::function<std::uint32_t(VerticalWorldSession&)> tick,
    std::function<VerticalWorldQuestInstall(VerticalWorldSession&,const VerticalQuestSessionRestoreResult&)> factory,
    std::function<void(VerticalWorldSession&,const VerticalWorldQuestPrepareResult&)> restore_policy) {
    if(!pool || !definitions || !tick || !factory || !restore_policy)
        throw std::invalid_argument("Incomplete SQL quest preparation owners");
    return [pool=std::move(pool),definitions=std::move(definitions),tick=std::move(tick),factory=std::move(factory),restore_policy=std::move(restore_policy)]
        (VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,VerticalWorldSession& owner,std::shared_ptr<std::recursive_mutex> mutex) {
        if(!strand.running_in_this_thread() || registry.find_account(owner.account_id()).get()!=&owner)
            throw std::invalid_argument("SQL quest preparation lost retained session");
        return vertical_world_quest_after_bind<VerticalWorldSessionSqlSave::Binding>(std::move(strand),registry,std::move(mutex),pool,definitions,tick,factory,restore_policy);
    };
}
}
