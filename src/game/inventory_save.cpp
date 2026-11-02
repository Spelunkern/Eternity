#include "eternity/game/inventory_save.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>

namespace eternity::game {
namespace {
using Parameters=std::vector<storage::Parameter>;
std::int64_t uid(std::uint64_t value) {return std::bit_cast<std::int64_t>(value);}
std::string timestamp(std::uint32_t value) {
    // ConvTimeToString 0042ad20 formats bit fields directly, including invalid dates.
    char buffer[32];std::snprintf(buffer,sizeof(buffer),"%04u-%02u-%02u %02u:%02u:%02u",2000+(value>>26),
        (value>>22)&15,(value>>17)&31,(value>>12)&31,(value>>6)&63,value&63);return buffer;
}
}
std::vector<InventorySaveOperation> inventory_save_plan(const InventoryReplica& replica,std::uint8_t server,std::uint32_t character) {
    if(!character || character>INT32_MAX || !replica.account_id() || replica.account_id()>INT32_MAX) throw std::invalid_argument("Invalid inventory save owner");
    std::vector<InventorySaveOperation> result;
    for(const bool bank:{false,true}) {
        const std::string base=bank?"usp_Save_User_StoredItem_":"usp_Save_Char_Item_";
        const Parameters owner=bank?Parameters{std::int64_t{server},std::int64_t{replica.account_id()}}:Parameters{std::int64_t{character}};
        const auto& deleted=bank?replica.deleted_bank():replica.deleted_inventory();
        // Original counter is never reset after the first 40 UIDs: remaining
        // entries each produce a separate call (00423733 / 00423bc9).
        for(std::size_t start=0;start<deleted.size();) {
            const auto end=std::min(start+(start==0?40U:1U),deleted.size());
            std::string ids;for(auto i=start;i<end;++i) {if(i!=start) ids+=',';ids+=std::to_string(uid(deleted[i]));}
            auto parameters=owner;parameters.emplace_back(std::move(ids));result.push_back({base+"Del_E",std::move(parameters),{}, {}});
            start=end;
        }
        for(const auto& cached:bank?replica.bank():replica.inventory()) {
            if(cached.save_type!=1 && cached.save_type!=2) continue;
            const auto& item=cached.item;auto parameters=owner;parameters.emplace_back(uid(item.uid));
            if(!bank) parameters.emplace_back(std::int64_t{item.bag});
            parameters.emplace_back(std::int64_t{item.slot});
            const bool insert=cached.save_type==1;
            if(insert) {parameters.emplace_back(std::int64_t{std::bit_cast<std::int32_t>(item.id)});parameters.emplace_back(std::int64_t{item.type});parameters.emplace_back(std::int64_t{item.type_id});}
            parameters.emplace_back(std::int64_t{item.quality});
            for(auto gem:item.gems) parameters.emplace_back(std::int64_t{gem});
            if(bank && !insert) parameters.emplace_back(std::int64_t{item.count});
            parameters.emplace_back(item.craft);
            if(!bank || insert) parameters.emplace_back(std::int64_t{item.count});
            if(insert) {parameters.emplace_back(timestamp(item.made_at));parameters.emplace_back(item.make_type);}
            InventorySaveOperation operation{base+(insert?"Add_E":"Mod_E"),std::move(parameters),{}, {}};
            if(insert) {operation.retry_delete_procedure=base+"Del_E";operation.retry_delete_parameters=owner;
                operation.retry_delete_parameters.emplace_back(std::to_string(uid(item.uid)));operation.retry_delete_parameters.emplace_back(std::int64_t{0});}
            result.push_back(std::move(operation));
        }
    }
    return result;
}
InventoryStore::InventoryStore(storage::SqlServer& database,std::uint8_t server,std::string catalog):database_(database),server_id_(server) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');}))
        throw std::invalid_argument("Invalid inventory SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
void InventoryStore::save_in_transaction(const InventoryReplica& replica,std::uint32_t character) {
    if(!database_.character_write_context_active()) throw std::logic_error("Inventory save requires the complete character transaction");
    const auto plan=inventory_save_plan(replica,server_id_,character);
    const auto execute=[&](const std::string& procedure,const Parameters& parameters) {
        std::string query="DECLARE @Saved int; EXEC @Saved="+prefix_+procedure+" ";
        for(std::size_t i=0;i<parameters.size();++i) query+=(i?",?":"?");
        const auto sets=database_.query(query+"; SELECT @Saved AS EternityInventorySaveResult;",parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original inventory save result changed");
        const auto& cell=*sets[0].rows[0][0];std::size_t consumed=0;const auto value=std::stoll(cell,&consumed);
        if(consumed!=cell.size() || value<0) throw std::runtime_error("Original inventory save procedure failed");
    };
    for(const auto& operation:plan) {
        if(operation.retry_delete_procedure.empty()) execute(operation.procedure,operation.parameters);
        else {
            // SaveCharacterItemUnit 00424ac0 / BankItemUnit 00424bc0.
            try {execute(operation.procedure,operation.parameters);}
            catch(const std::runtime_error&) {execute(operation.retry_delete_procedure,operation.retry_delete_parameters);execute(operation.procedure,operation.parameters);}
        }
    }
}
}
