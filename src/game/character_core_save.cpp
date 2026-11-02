#include "eternity/game/character_core_save.hpp"
#include <algorithm>
#include <bit>
#include "eternity/game/legacy_sql.hpp"

namespace eternity::game {
namespace {
std::int64_t signed_word(std::uint16_t value) {return std::bit_cast<std::int16_t>(value);}
std::int64_t signed_dword(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}
std::int64_t kill_used(std::uint32_t value) {return static_cast<std::uint32_t>(static_cast<std::int32_t>(signed_word(static_cast<std::uint16_t>(value))));}

}
std::vector<CharacterCoreSaveOperation> character_core_save_plan(const CharacterReplica& replica) {
    const auto p=replica.core();const auto& c=p.character;
    if(!c.id || c.id>INT32_MAX || !p.user_id || p.user_id>INT32_MAX) throw std::invalid_argument("Invalid character save owner");
    std::vector<CharacterCoreSaveOperation> result;
    result.push_back({"usp_Save_Char_Info_E",{std::int64_t{c.id},std::int64_t{c.level},signed_word(p.stat_points),signed_word(p.skill_points),
        signed_word(c.strength),signed_word(c.dexterity),signed_word(c.recovery),signed_word(c.intelligence),signed_word(c.wisdom),signed_word(c.luck),
        signed_word(c.hp),signed_word(c.mp),signed_word(c.sp),signed_word(c.map),signed_word(p.direction),signed_dword(p.experience),signed_dword(p.money),
        legacy_sql_coordinate(p.x),legacy_sql_coordinate(p.y),legacy_sql_coordinate(p.z),std::int64_t{p.hunger},std::int64_t{p.vigor},std::int64_t{p.cg},std::int64_t{p.og},std::int64_t{p.ig},
        signed_dword(p.kills[0]),signed_dword(p.kills[1]),signed_dword(p.kills[2]),signed_dword(p.kills[3]),kill_used(p.kill_level),kill_used(p.dead_level)}});
    if(replica.body_changed()) result.push_back({"usp_Save_Char_BodyChg_E",{std::int64_t{c.id},std::int64_t{c.hair},std::int64_t{c.face},std::int64_t{c.size},std::int64_t{c.sex}}});
    return result;
}
CharacterCoreStore::CharacterCoreStore(storage::SqlServer& database,std::string catalog):database_(database) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid character SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
void CharacterCoreStore::save_in_transaction(const CharacterReplica& replica) {
    if(!database_.character_write_context_active()) throw std::logic_error("Core save requires the complete character transaction");
    for(const auto& op:character_core_save_plan(replica)) {
        const bool info=op.procedure=="usp_Save_Char_Info_E";
        // SQL EXEC accepts variables, not CAST expressions. Decimal locals match
        // the original literals, including positive zero and the 38-digit limit.
        std::string query=info?"DECLARE @Saved int,@X decimal(38,6),@Y decimal(38,6),@Z decimal(38,6); SELECT @X=CAST(? AS decimal(38,6)),@Y=CAST(? AS decimal(38,6)),@Z=CAST(? AS decimal(38,6)); ":"DECLARE @Saved int; ";
        query+="EXEC @Saved="+prefix_+op.procedure+" ";
        for(std::size_t i=0;i<op.parameters.size();++i) {if(i) query+=',';query+=(info && i==17?"@X":info && i==18?"@Y":info && i==19?"@Z":"?");}
        auto parameters=op.parameters;
        if(info) {parameters.erase(parameters.begin()+17,parameters.begin()+20);parameters.insert(parameters.begin(),op.parameters.begin()+17,op.parameters.begin()+20);}
        const auto sets=database_.query(query+"; SELECT @Saved;",parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original character core save result changed");
        std::size_t consumed=0;const auto& value=*sets[0].rows[0][0];const auto saved=std::stoll(value,&consumed);
        if(consumed!=value.size() || saved<0) throw std::runtime_error("Original character core save procedure failed");
    }
}
}
