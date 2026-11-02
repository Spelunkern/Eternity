#include "eternity/game/interface_save.hpp"
#include "eternity/game/legacy_sql.hpp"
#include <algorithm>

namespace eternity::game {
std::vector<InterfaceSaveOperation> interface_save_plan(const InterfaceReplica& replica,std::uint32_t character,bool save_quick) {
    if(!character || character>INT32_MAX) throw std::invalid_argument("Invalid interface save owner");
    std::vector<InterfaceSaveOperation> result;
    if(save_quick) {result.push_back({"usp_Save_Char_QuickSlot_Del_E",{std::int64_t{character}},true});for(const auto& slot:replica.quick_slots()) result.push_back({"usp_Save_Char_QuickSlot_Add_E",{std::int64_t{character},std::int64_t{slot.bar},std::int64_t{slot.slot},std::int64_t{slot.bag},std::int64_t{slot.number}},true});}
    for(std::size_t i=0;i<replica.points().size();++i) {const auto& point=replica.points()[i];if(point.map==65535 || (point.save_type!=1 && point.save_type!=2)) continue;
        result.push_back({point.save_type==1?"usp_Create_Char_SavePoint":"usp_Save_Char_SavePoint",{std::int64_t{character},static_cast<std::int64_t>(i),std::int64_t{point.map},legacy_sql_coordinate(point.x),legacy_sql_coordinate(point.y),legacy_sql_coordinate(point.z)},false});}
    return result;
}
InterfaceSaveResult execute_interface_save_plan(std::span<const InterfaceSaveOperation> plan,const std::function<void(const InterfaceSaveOperation&)>& execute,const std::function<void(std::string_view)>& diagnostic) {
    InterfaceSaveResult result;
    for(const auto& operation:plan) {if(operation.quick && result.quick_failure) continue;
        if(operation.quick) {try{execute(operation);}catch(const std::runtime_error& error){result.quick_failure=error.what();if(diagnostic) diagnostic(error.what());}}
        else execute(operation);
    }
    return result;
}
InterfaceStore::InterfaceStore(storage::SqlServer& database,std::string catalog):database_(database) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid interface SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
InterfaceSaveResult InterfaceStore::save_in_transaction(const InterfaceReplica& replica,std::uint32_t character,bool save_quick,const std::function<void(std::string_view)>& diagnostic) {
    if(!database_.transaction_active()) throw std::logic_error("Interface save requires the complete character transaction");
    const auto plan=interface_save_plan(replica,character,save_quick);
    return execute_interface_save_plan(plan,[&](const auto& op){
        const bool point=!op.quick;
        std::string query=point?"DECLARE @X decimal(38,6),@Y decimal(38,6),@Z decimal(38,6); SELECT @X=CAST(? AS decimal(38,6)),@Y=CAST(? AS decimal(38,6)),@Z=CAST(? AS decimal(38,6)); ":"";
        query+="EXEC "+prefix_+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) {if(i) query+=',';query+=(point && i==3?"@X":point && i==4?"@Y":point && i==5?"@Z":"?");}
        auto parameters=op.parameters;if(point) {parameters.erase(parameters.begin()+3,parameters.end());parameters.insert(parameters.begin(),op.parameters.begin()+3,op.parameters.end());}
        // These original EXEC calls bind no procedure return value.
        (void)database_.query(query+";",parameters);
    },diagnostic);
}
}
