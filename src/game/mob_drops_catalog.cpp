#include "eternity/game/mob_drops_catalog.hpp"
#include <stdexcept>

namespace eternity::game {
void initialize_mob_drop_catalog(MobDropCatalog& catalog,std::span<const MobDropCatalogRecord> records) {
    std::array<const MobDropCatalogRecord*,65536> table{};
    for(const auto& record:records) table[static_cast<std::size_t>(record.table_type)*256+record.table_id]=&record;
    for(auto& grade:catalog.grades) grade.clear();
    catalog.unrestricted_counts.fill(0);
    for(unsigned pass=0;pass<2;++pass) {
        for(std::size_t type=0;type<256;++type) for(std::size_t id=1;id<256;++id) {
            const auto* record=table[type*256+id];if(!record || !record->entry) continue;
            auto& entry=*record->entry;
            if(record->grade>1024 || entry.item.type==0 || entry.item.type_id==0 || entry.item.real_type==27) continue;
            const bool restricted=record->server!=0 || entry.limit.maximum!=0;
            if(restricted!=(pass!=0)) continue;
            catalog.grades[record->grade].push_back(&entry);
        }
        if(pass==0) for(std::size_t grade=0;grade<catalog.grades.size();++grade) catalog.unrestricted_counts[grade]=catalog.grades[grade].size();
    }
    // The original limit pass starts at table type one, unlike the grade pass.
    constexpr std::array<std::uint32_t,7> intervals{0,1440000,14400000,28800000,43200000,86400000,259200000};
    for(std::size_t type=1;type<256;++type) for(std::size_t id=1;id<256;++id) {
        const auto* record=table[type*256+id];if(!record || !record->entry) continue;
        auto& entry=*record->entry;auto& limit=entry.limit;
        if(entry.item.type==0 || entry.item.type_id==0 || limit.maximum==0) continue;
        // Larger selectors read beyond the recovered stack table in the EXE.
        if(limit.maximum>=intervals.size()) throw std::invalid_argument("Drop interval selector exceeds original table");
        const auto interval=intervals[limit.maximum];
        limit.count=0;limit.next_tick=0;limit.random_delay=interval/10;limit.delay=interval-limit.random_delay;
    }
}
}

#include "eternity/game/equipment.hpp"
#include <algorithm>
#include <charconv>
namespace eternity::game {
MobDropCatalogDefinitions MobDropCatalogDefinitions::load_sql(const storage::ResultSet& table) {
    const auto definitions=ItemDefinitions::load(table);
    const auto index=[&](std::string_view name) {
        const auto at=std::find(table.columns.begin(),table.columns.end(),name);
        if(at==table.columns.end()) throw std::invalid_argument("Missing drop catalog SQL column: "+std::string(name));
        return static_cast<std::size_t>(at-table.columns.begin());
    };
    const std::array<std::size_t,8> columns{index("Type"),index("TypeID"),index("ItemID"),index("Grade"),index("Drop"),index("Server"),index("Quality"),index("ReqDex")};
    MobDropCatalogDefinitions result;result.entries.resize(table.rows.size());result.records.reserve(table.rows.size());
    for(std::size_t i=0;i<table.rows.size();++i) {
        const auto& row=table.rows[i];
        const auto number=[&](std::size_t field,std::int64_t low,std::int64_t high) {
            const auto at=columns[field];if(at>=row.size() || !row[at]) throw std::invalid_argument("Missing drop catalog SQL value");
            const auto& text=*row[at];std::int64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<low || value>high) throw std::invalid_argument("Invalid drop catalog SQL value");
            return value;
        };
        auto& entry=result.entries[i];entry.item.type=static_cast<std::uint8_t>(number(0,0,255));entry.item.type_id=static_cast<std::uint8_t>(number(1,0,255));entry.item.id=static_cast<std::uint32_t>(number(2,0,INT32_MAX));
        const auto grade=static_cast<std::uint16_t>(number(3,INT16_MIN,UINT16_MAX));entry.limit.maximum=static_cast<std::uint16_t>(number(4,INT16_MIN,UINT16_MAX));
        auto server=static_cast<std::uint8_t>(number(5,0,255));if(server>=10) server=0;
        entry.item.quality=static_cast<std::uint16_t>(number(6,0,UINT16_MAX));entry.item.dex=static_cast<std::uint16_t>(number(7,0,UINT16_MAX));
        if(const auto* definition=definitions.find(entry.item.type,entry.item.type_id)) entry.item.real_type=definition->real_type;
        // Original freshly loaded definition bytes start zero, including limit
        // state; InitLimit fills only nonzero selectors in type tables 1..255.
        entry.limit.random_delay=0;
        result.records.push_back({entry.item.type,entry.item.type_id,&entry,grade,server});
    }
    return result;
}
}
