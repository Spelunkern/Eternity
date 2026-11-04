#include "eternity/game/mob_drops_reload.hpp"
#include <bit>
#include <charconv>

namespace eternity::game {
namespace {
void increment(std::int32_t& value) {value=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)+1U);}
class ResultCursor final:public MobDropReloadCursor {
public:
    explicit ResultCursor(const storage::ResultSet& table):table_(table) {}
    MobDropReloadRead next(MobDropRow& result) override {
        if(table_.columns.size()!=4) return MobDropReloadRead::failed;
        if(index_==table_.rows.size()) return MobDropReloadRead::end;
        const auto& row=table_.rows[index_++];if(row.size()!=4) return MobDropReloadRead::failed;
        std::array<std::int64_t,4> values{};constexpr std::array<std::int64_t,4> low{INT16_MIN,0,INT16_MIN,INT32_MIN},high{UINT16_MAX,UINT8_MAX,UINT16_MAX,UINT32_MAX};
        for(std::size_t i=0;i<4;++i) {
            if(!row[i]) return MobDropReloadRead::failed;
            const auto& text=*row[i];const auto parsed=std::from_chars(text.data(),text.data()+text.size(),values[i]);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || values[i]<low[i] || values[i]>high[i]) return MobDropReloadRead::failed;
        }
        result={static_cast<std::uint16_t>(values[0]),static_cast<std::uint8_t>(values[1]),static_cast<std::uint16_t>(values[2]),static_cast<std::uint32_t>(values[3])};
        return MobDropReloadRead::row;
    }
private:
    const storage::ResultSet& table_;std::size_t index_=0;
};
}
bool reload_mob_drop_rows(MobDropDefinitions& definitions,std::int32_t& changes,MobDropReloadCursor& source,MobDropReloadDiagnostics& diagnostics) {
    for(;;) {
        MobDropRow row;const auto status=source.next(row);
        if(status==MobDropReloadRead::end) return true;
        if(status==MobDropReloadRead::failed) {diagnostics.failed(MobDropReloadFailure::row);return false;}
        if(row.mob>=definitions.mobs.size() || row.order>=10) {diagnostics.rejected(row);continue;}
        auto& slot=definitions.mobs[row.mob][row.order];
        if(slot.grade!=row.grade) {slot.grade=row.grade;increment(changes);}
        if(slot.rate!=row.rate) {slot.rate=row.rate;increment(changes);}
    }
}
bool reload_mob_drop_result(MobDropDefinitions& definitions,const storage::ResultSet& table,std::int32_t& changes,MobDropReloadDiagnostics& diagnostics) {
    ResultCursor source(table);return reload_mob_drop_rows(definitions,changes,source,diagnostics);
}
bool reload_mob_drop_sql(MobDropDefinitions& definitions,std::uint16_t request,std::int32_t& changes,storage::SqlServer& database,MobDropReloadDiagnostics& diagnostics) {
    std::vector<storage::ResultSet> tables;
    try {const std::array<storage::Parameter,1> parameters{static_cast<std::int64_t>(request)};tables=database.query("EXEC dbo.usp_Read_MobItems_R2 ?;",parameters);}
    catch(const storage::SqlError&) {diagnostics.failed(MobDropReloadFailure::query);return false;}
    if(tables.size()!=1) {diagnostics.failed(MobDropReloadFailure::query);return false;}
    return reload_mob_drop_result(definitions,tables[0],changes,diagnostics);
}
}
