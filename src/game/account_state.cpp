#include "eternity/game/account_state.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
namespace {
std::string prefix(std::string catalog) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid account state SQL catalog identifier");
    return "["+catalog+"].dbo.";
}
std::string sql(const AccountSaveOperation& op,const std::string& name,bool returned) {
    std::string query=returned?"DECLARE @Saved int; EXEC @Saved=":"EXEC ";query+=name+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) query+=(i?",?":"?");return query+(returned?"; SELECT @Saved;":";");
}
}
bool MarketReplica::load(std::span<const std::uint32_t> ids) {if(ids.size()>ids_.size()) return false;std::copy(ids.begin(),ids.end(),ids_.begin());count_=ids.size();save_type_=0;return true;}
bool MarketReplica::load(const PlayerSnapshot& player) {return player.core.user_id==account_ && load(player.concern_market);}
InventoryReplicaResult MarketReplica::apply(std::span<const std::uint8_t> p) {
    if(p.size()<6) return InventoryReplicaResult::rejected;
    const auto dword=[&](std::size_t at){std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=static_cast<std::uint32_t>(p[at+i])<<(8*i);return value;};
    if(dword(2)!=account_) return InventoryReplicaResult::rejected;
    if(p[0]!=0x1a || p[1]!=6) return InventoryReplicaResult::unsupported;
    if(p.size()<7 || p[6]>ids_.size() || p.size()!=7+4U*p[6]) return InventoryReplicaResult::rejected;
    count_=p[6];for(std::size_t i=0;i<count_;++i) ids_[i]=dword(7+4*i);save_type_=count_?1:3;return InventoryReplicaResult::applied;
}
std::optional<net::Packet> MarketReplica::packet() const {
    if(!count_) return std::nullopt;
    net::Packet p{0x10,5};const auto put=[&](std::uint32_t value){for(unsigned int i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};put(account_);p.push_back(static_cast<std::uint8_t>(count_));for(auto id:ids()) put(id);return p;
}
std::vector<AccountSaveOperation> market_save_plan(const MarketReplica& replica,std::uint32_t character) {
    if(!character || character>INT32_MAX) throw std::invalid_argument("Invalid market save owner");
    std::vector<AccountSaveOperation> result;if(replica.save_type()!=1 && replica.save_type()!=3) return result;
    result.push_back({"usp_Save_Char_ConcernMarket_Del_E",{std::int64_t{character}}});
    if(replica.save_type()==1) for(auto id:replica.ids()) result.push_back({"usp_Save_Char_ConcernMarket_Add_E",{std::int64_t{character},std::int64_t{std::bit_cast<std::int32_t>(id)}}});
    return result;
}
MarketSaveResult execute_market_save(MarketReplica& replica,std::uint32_t character,const std::function<void(const AccountSaveOperation&)>& execute,const std::function<void(std::string_view)>& diagnostic) {
    const auto plan=market_save_plan(replica,character);MarketSaveResult result;
    try {for(const auto& operation:plan) execute(operation);if(!plan.empty()) replica.sql_saved();}
    catch(const std::runtime_error& error) {result.failure=error.what();if(diagnostic) diagnostic(error.what());}
    return result;
}
MarketStore::MarketStore(storage::SqlServer& database,std::string catalog):database_(database),prefix_(prefix(std::move(catalog))) {}
MarketSaveResult MarketStore::save_in_transaction(MarketReplica& replica,std::uint32_t character,const std::function<void(std::string_view)>& diagnostic) {
    if(!database_.character_write_context_active()) throw std::logic_error("Market save requires the complete character transaction");
    return execute_market_save(replica,character,[&](const auto& op){(void)database_.query(sql(op,prefix_,false),op.parameters);},diagnostic);
}
bool GrowthReplica::advance(std::uint8_t grow,std::uint16_t level) noexcept {
    // DBCharacter::SaveMaxGrow 004200c0 mutates CUser+66 before AllocDB/SQL.
    if(maximum_<grow) {if(grow>3) return false;maximum_=grow;return true;}
    if(grow>=3 || grow!=maximum_ || ((grow==1 || grow==2) && level<30)) return false;
    ++maximum_;return true;
}
GrowthSaveResult execute_growth_save(GrowthReplica* replica,std::uint8_t server,std::uint32_t account,std::uint8_t grow,std::uint16_t level,const std::function<std::optional<std::int64_t>(const AccountSaveOperation&)>& execute,const std::function<void(std::string_view)>& diagnostic) {
    if(!replica) return {1,false,{}};
    if(!account || account>INT32_MAX) throw std::invalid_argument("Invalid growth save owner");
    GrowthSaveResult result;if(!replica->advance(grow,level)) return result;result.advanced=true;
    const AccountSaveOperation operation{"usp_Save_Char_Maxgrow_E",{std::int64_t{server},std::int64_t{account},std::int64_t{replica->maximum()}}};
    try {const auto value=execute(operation);if(!value) {result.legacy_code=3;result.failure="Original growth DB allocation is unavailable";}else if(*value<0) {result.legacy_code=-1;result.failure="Original growth save procedure failed";}}
    catch(const std::runtime_error& error) {result.failure=error.what();} // Legacy returns 0 on API errors.
    if(result.failure && diagnostic) diagnostic(*result.failure);
    return result;
}
GrowthStore::GrowthStore(storage::SqlServer& database,std::uint8_t server,std::string catalog):database_(database),server_(server),prefix_(prefix(std::move(catalog))) {}
GrowthSaveResult GrowthStore::save(GrowthReplica& replica,std::uint32_t account,std::uint8_t grow,std::uint16_t level,const std::function<void(std::string_view)>& diagnostic) {
    if(database_.character_write_context_active()) throw std::logic_error("Growth save must precede the complete character transaction");
    return execute_growth_save(&replica,server_,account,grow,level,[&](const auto& op)->std::optional<std::int64_t>{
        const auto sets=database_.query(sql(op,prefix_,true),op.parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original growth save result changed");
        const auto& cell=*sets[0].rows[0][0];std::size_t consumed=0;const auto value=std::stoll(cell,&consumed);if(consumed!=cell.size()) throw std::runtime_error("Invalid growth procedure result");return value;
    },diagnostic);
}
}
