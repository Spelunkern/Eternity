#include "eternity/game/quest_save.hpp"
#include <algorithm>

namespace eternity::game {
std::vector<QuestSaveOperation> quest_save_plan(const QuestReplica& replica,std::uint32_t character) {
    if(!character || character>INT32_MAX) throw std::invalid_argument("Invalid quest save owner");
    std::vector<QuestSaveOperation> result;
    for(auto id:replica.deleted()) result.push_back({"usp_Save_Char_Quest_Del_E",{std::int64_t{character},std::int64_t{id}}});
    for(const auto& item:replica.active()) if(item.save_type==1 || item.save_type==2) result.push_back({item.save_type==1?"usp_Save_Char_Quest_Add_E":"usp_Save_Char_Quest_Mod_E",{std::int64_t{character},std::int64_t{item.id},std::int64_t{item.delay},std::int64_t{item.counters[0]},std::int64_t{item.counters[1]},std::int64_t{item.counters[2]}}});
    for(const auto& item:replica.finished()) if(item.save_type==1 || item.save_type==2) {
        QuestSaveOperation operation{"usp_Save_Char_FinishedQuest_E",{std::int64_t{character},std::int64_t{item.id},std::int64_t{item.success}}};if(item.save_type==1) operation.parameters.emplace_back(std::int64_t{1});result.push_back(std::move(operation));
    }
    return result;
}
QuestStore::QuestStore(storage::SqlServer& database,std::string catalog):database_(database) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid quest SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
void QuestStore::save_in_transaction(const QuestReplica& replica,std::uint32_t character) {
    if(!database_.transaction_active()) throw std::logic_error("Quest save requires the complete character transaction");
    for(const auto& op:quest_save_plan(replica,character)) {
        std::string query="DECLARE @Saved int; EXEC @Saved="+prefix_+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) query+=(i?",?":"?");
        const auto sets=database_.query(query+"; SELECT @Saved;",op.parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original quest save result changed");
        std::size_t consumed=0;const auto& value=*sets[0].rows[0][0];const auto saved=std::stoll(value,&consumed);if(consumed!=value.size() || saved<0) throw std::runtime_error("Original quest save procedure failed");
    }
}
}
