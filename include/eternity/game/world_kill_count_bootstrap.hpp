#pragma once
#include "eternity/net/framing.hpp"
#include "eternity/storage/sql_server.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace eternity::game {
// ps_dbAgent.exe CWorld::DBKillCountLoad 0040ecd0 and
// CConnection::PacketSystemConnect 0040b1f0, case 0201. The caller owns
// the actual SQL connection and routes the resulting packet on the world strand.
template<class Database> net::Packet load_world_kill_count_bootstrap(Database& database) {
    constexpr std::array columns{storage::SqlColumnType::long_integer,storage::SqlColumnType::long_integer};
    auto cursor=database.cursor("EXEC usp_Read_GodBless_R",{},columns);
    storage::TypedSqlRow row;
    const auto result=cursor.next(row);
    if(result==storage::SqlCursorRead::error) {
        if(const auto* error=cursor.error())throw *error;
        throw std::runtime_error("Kill-count SQL cursor failed without a diagnostic");
    }
    std::array<std::uint32_t,2> counts{};
    if(result==storage::SqlCursorRead::row) {
        if(row.size()!=counts.size())throw std::runtime_error("Invalid kill-count SQL result");
        for(std::size_t i=0;i<counts.size();++i) {
            if(!row[i])continue; // Original initializes both SQL destinations to zero.
            if(*row[i]<std::numeric_limits<std::int32_t>::min() || *row[i]>std::numeric_limits<std::int32_t>::max())
                throw std::runtime_error("Kill-count SQL value outside SQL_C_LONG range");
            counts[i]=static_cast<std::uint32_t>(*row[i]);
        }
    } else if(result!=storage::SqlCursorRead::end)throw std::runtime_error("Invalid kill-count SQL cursor result");
    net::Packet packet{0x05,0x02};
    for(auto count:counts)for(unsigned byte=0;byte<4;++byte)packet.push_back(static_cast<std::uint8_t>(count>>(byte*8)));
    return packet;
}
inline net::Packet load_world_kill_count_bootstrap(storage::SqlServer& database) {
    return load_world_kill_count_bootstrap<storage::SqlServer>(database);
}
// DBKillCountSave 0040ee00 saves the current pair, retrying a failed call at
// most three times. This is an explicit save operation, not the 0205 receiver.
template<class Database> bool save_world_kill_count(Database& database,const std::array<std::uint32_t,2>& counts) {
    const std::array<storage::Parameter,2> parameters{
        std::int64_t{std::bit_cast<std::int32_t>(counts[0])},std::int64_t{std::bit_cast<std::int32_t>(counts[1])}};
    for(unsigned attempt=0;attempt<3;++attempt) {
        try {
            const auto sets=database.query("DECLARE @Saved int; EXEC @Saved=usp_Save_GodBless_E ?,?; SELECT @Saved;",parameters);
            if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0])
                throw std::runtime_error("Invalid kill-count save result");
            const auto& value=*sets[0].rows[0][0];std::int32_t result=0;
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size())throw std::runtime_error("Invalid kill-count save return code");
            if(result>=0)return true;
        } catch(const storage::SqlError&) {
            if(attempt==2)throw;
        }
    }
    return false;
}
inline bool save_world_kill_count(storage::SqlServer& database,const std::array<std::uint32_t,2>& counts) {
    return save_world_kill_count<storage::SqlServer>(database,counts);
}
}
