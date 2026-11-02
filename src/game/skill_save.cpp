#include "eternity/game/skill_save.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
std::vector<SkillSaveOperation> skill_save_plan(const SkillReplica& replica,std::uint32_t character) {
    if(!character || character>INT32_MAX) throw std::invalid_argument("Invalid skill save owner");
    const auto learned=replica.learned();std::vector<SkillSaveOperation> result;std::size_t index=0;
    if(replica.reset_pending()) result.push_back({"usp_Save_Char_Skill_Del_E",{std::int64_t{character},std::int64_t{0},std::int64_t{1}}});
    else for(;index<replica.saved_count();++index) {const auto& item=learned.at(index);result.push_back({"usp_Save_Char_Skill_Mod_E",{std::int64_t{character},std::int64_t{item.id},std::int64_t{item.level},std::int64_t{std::bit_cast<std::int32_t>(item.delay)}}});}
    for(;index<learned.size();++index) {const auto& item=learned[index];result.push_back({"usp_Save_Char_Skill_Add_E",{std::int64_t{character},std::int64_t{item.id},std::int64_t{item.level},std::int64_t{item.number},std::int64_t{std::bit_cast<std::int32_t>(item.delay)}}});}
    result.push_back({"usp_Save_Char_ApplySkill_Del_E",{std::int64_t{character},std::int64_t{0},std::int64_t{0},std::int64_t{1}}});
    for(const auto& item:replica.effects()) if(item.delay) result.push_back({"usp_Save_Char_ApplySkill_Add_E2",{std::int64_t{character},std::int64_t{item.id},std::int64_t{item.level},std::int64_t{std::bit_cast<std::int32_t>(item.delay)}}});
    return result;
}
SkillStore::SkillStore(storage::SqlServer& database,std::string catalog):database_(database) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid skill SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
void SkillStore::save_in_transaction(const SkillReplica& replica,std::uint32_t character) {
    if(!database_.transaction_active()) throw std::logic_error("Skill save requires the complete character transaction");
    for(const auto& op:skill_save_plan(replica,character)) {
        std::string query="DECLARE @Saved int; EXEC @Saved="+prefix_+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) query+=(i?",?":"?");
        const auto sets=database_.query(query+"; SELECT @Saved;",op.parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original skill save result changed");
        std::size_t consumed=0;const auto& value=*sets[0].rows[0][0];const auto saved=std::stoll(value,&consumed);if(consumed!=value.size() || saved<0) throw std::runtime_error("Original skill save procedure failed");
    }
}
}
