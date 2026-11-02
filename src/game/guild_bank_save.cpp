#include "eternity/game/guild_bank_save.hpp"
#include "eternity/game/guild_bank.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>

namespace eternity::game {
namespace {
std::int64_t signed_uid(std::uint64_t value) {return std::bit_cast<std::int64_t>(value);}
std::string timestamp(std::uint32_t value) {
    // ConvTimeToString 0042ad20 uses raw packed fields, including invalid dates.
    char text[32];std::snprintf(text,sizeof(text),"%04u-%02u-%02u %02u:%02u:%02u",2000+(value>>26),(value>>22)&15,(value>>17)&31,(value>>12)&31,(value>>6)&63,value&63);return text;
}
}
std::uint32_t save_guild_bank(std::uint32_t guild,std::span<CachedInventoryItem* const,240> slots,std::vector<std::uint64_t>& deleted,GuildBankSavePorts& ports) {
    if(!ports.acquire || !ports.release || !ports.prepare || !ports.execute || !ports.diagnostic) throw std::invalid_argument("Incomplete guild bank persistence ports");
    const auto owner=std::int64_t{std::bit_cast<std::int32_t>(guild)};bool acquired=false;std::uint32_t errors=0;
    struct Lease {GuildBankSavePorts& ports;bool& active;~Lease(){if(active) ports.release();}} lease{ports,acquired};
    const auto acquire=[&] {if(acquired) {ports.prepare();return true;}acquired=ports.acquire();if(!acquired) ports.diagnostic(GuildBankSqlResult::lease_error);return acquired;};
    bool delete_success=true;
    for(std::size_t begin=0;begin<deleted.size();) {
        if(!acquire()) return 1;
        // The original never resets its counter after the first 40 UIDs.
        const auto end=std::min(begin+(begin==0?40U:1U),deleted.size());std::string ids;
        for(auto i=begin;i<end;++i) {if(i!=begin) ids+=',';ids+=std::to_string(signed_uid(deleted[i]));}
        const auto status=ports.execute({"usp_save_guild_storeditem_del_e",{owner,std::move(ids)}});
        if(status!=GuildBankSqlResult::success) {++errors;ports.diagnostic(status);delete_success=false;break;}begin=end;
    }
    if(delete_success) deleted.clear();
    for(std::size_t slot=0;slot<slots.size();++slot) {
        auto* cached=slots[slot];if(!cached) continue;
        const bool insert=cached->save_type==1;
        if(insert || cached->save_type==2) {
            if(!acquire()) return 1;
            const auto& item=cached->item;GuildBankSaveOperation operation{insert?"usp_save_guild_storeditem_add_e":"usp_save_guild_storeditem_mod_e",{owner,signed_uid(item.uid),static_cast<std::int64_t>(slot)}};auto& p=operation.parameters;
            if(insert) {p.emplace_back(std::int64_t{std::bit_cast<std::int32_t>(item.id)});p.emplace_back(std::int64_t{item.type});p.emplace_back(std::int64_t{item.type_id});}
            p.emplace_back(std::int64_t{item.quality});for(auto gem:item.gems) p.emplace_back(std::int64_t{gem});
            if(!insert) p.emplace_back(std::int64_t{item.count});p.emplace_back(item.craft);if(insert) {p.emplace_back(std::int64_t{item.count});p.emplace_back(timestamp(item.made_at));p.emplace_back(item.make_type);}
            const auto status=ports.execute(operation);
            if(status!=GuildBankSqlResult::success) {
                // Only failure of Query itself schedules insert cleanup. Bind,
                // MoreResults and negative procedure results leave no new UID.
                if(insert && status==GuildBankSqlResult::query_error) deleted.push_back(item.uid);
                ++errors;ports.diagnostic(status);break;
            }
        }
        // The original also clears unrecognized save states without issuing SQL.
        cached->save_type=0;
    }
    return errors;
}
GuildBankStore::GuildBankStore(storage::SqlServer& database,std::function<void(const std::string&)> diagnostic,std::string catalog):database_(database),diagnostic_(std::move(diagnostic)) {
    if(!diagnostic_) throw std::invalid_argument("Missing guild bank save diagnostic");
    if(catalog.empty() || catalog.size()>128 || catalog.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos) throw std::invalid_argument("Invalid guild bank SQL catalog");prefix_="["+catalog+"].dbo.";
}
std::uint32_t GuildBankStore::save(std::uint32_t guild,GuildBankReplica& bank) {
    if(database_.transaction_active()) throw std::logic_error("Guild bank saving requires independent autocommit operations");
    std::string diagnostic;
    GuildBankSavePorts ports{[]{return true;},[]{},[]{},[&](const GuildBankSaveOperation& operation) {
        try {
            std::string query="DECLARE @Saved int; EXEC @Saved="+prefix_+operation.procedure+" ";
            for(std::size_t i=0;i<operation.parameters.size();++i) query+=(i?",?":"?");
            const auto sets=database_.query(query+"; SELECT @Saved AS EternityGuildBankSaveResult;",operation.parameters);
            if(sets.size()!=1 || sets.front().rows.size()!=1 || sets.front().rows.front().size()!=1 || !sets.front().rows.front().front()) throw std::runtime_error("Guild bank save result contract changed");
            const auto& text=*sets.front().rows.front().front();std::size_t consumed=0;const auto result=std::stoll(text,&consumed);
            if(consumed!=text.size()) throw std::runtime_error("Invalid guild bank save result");
            if(result<0) {diagnostic="Guild bank save procedure returned a negative result";return GuildBankSqlResult::negative_result;}return GuildBankSqlResult::success;
        } catch(const std::exception& error) {
            diagnostic=error.what();if(diagnostic.starts_with("bind query parameter")) return GuildBankSqlResult::bind_error;
            if(diagnostic.starts_with("execute query") || diagnostic.starts_with("prepare query") || diagnostic.starts_with("allocate statement")) return GuildBankSqlResult::query_error;
            return GuildBankSqlResult::results_error;
        }
    },[&](GuildBankSqlResult){diagnostic_(diagnostic);}};
    return bank.save(guild,ports);
}

}
