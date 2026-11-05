#pragma once
#include "eternity/net/framing.hpp"
#include "eternity/storage/sql_server.hpp"
#include <array>
#include <bit>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace eternity::game {
struct WorldDbObelisk {std::uint32_t id;std::uint8_t country;std::uint32_t changed;std::uint8_t dirty;};
// ps_dbAgent.exe CKeepObeliskMgr DBLoad 00412160, DBSave 004122c0,
// AddFromDB 004128a0, AddFromGame 00412940, Update 00412a10, SendList 00412a80.
// Connection and owner are confined to one DB executor.
template<class Database> class WorldDbObelisks final {
public:
    explicit WorldDbObelisks(Database& database):database_(database) {
        constexpr std::array columns{storage::SqlColumnType::long_integer,storage::SqlColumnType::tiny_integer,storage::SqlColumnType::long_integer};
        auto cursor=database_.cursor("EXEC usp_Read_Keep_R",{},columns);storage::TypedSqlRow row;
        for(;;) {
            const auto result=cursor.next(row);if(result==storage::SqlCursorRead::end)break;
            if(result==storage::SqlCursorRead::error) {if(const auto* error=cursor.error())throw *error;throw std::runtime_error("Keep SQL cursor failed");}
            if(result!=storage::SqlCursorRead::row || row.size()!=3 || !row[0] || !row[1] || !row[2] || *row[0]<INT32_MIN || *row[0]>INT32_MAX || *row[1]<0 || *row[1]>255 || *row[2]<INT32_MIN || *row[2]>INT32_MAX)
                throw std::runtime_error("Invalid keep SQL row");
            const auto id=static_cast<std::uint32_t>(*row[0]);
            records_.try_emplace(id,WorldDbObelisk{id,static_cast<std::uint8_t>(*row[1]),static_cast<std::uint32_t>(*row[2]),0});
        }
    }
    bool receive(std::span<const std::uint8_t> packet) {
        if(packet.size()<2 || packet[1]!=2)throw std::invalid_argument("Invalid keep DB packet");
        if(packet[0]==6) {
            if(packet.size()<3 || packet.size()!=3+static_cast<std::size_t>(packet[2])*9)throw std::invalid_argument("Invalid keep DB list");
            for(std::size_t i=0;i<packet[2];++i) {const auto offset=3+i*9;const auto id=read(packet,offset);records_.try_emplace(id,WorldDbObelisk{id,packet[offset+4],read(packet,offset+5),1});}
            return true;
        }
        if(packet[0]==7) {
            if(packet.size()!=11)throw std::invalid_argument("Invalid keep DB update");
            const auto it=records_.find(read(packet,2));if(it==records_.end())return false;
            it->second.country=packet[6];it->second.changed=read(packet,7);if(it->second.dirty!=1)it->second.dirty=2;return true;
        }
        throw std::invalid_argument("Unknown keep DB packet");
    }
    std::vector<net::Packet> packets()const {
        std::vector<net::Packet> result;net::Packet packet{6,2,0};
        for(const auto& [id,record]:records_) {
            put(packet,id);packet.push_back(record.country);put(packet,record.changed);++packet[2];
            if(packet[2]==100) {result.push_back(std::move(packet));packet={6,2,0};}
        }
        if(packet[2])result.push_back(std::move(packet));return result;
    }
    bool save() {
        for(auto& [id,record]:records_) {
            if(!record.dirty)continue;
            const std::array<storage::Parameter,3> parameters{std::int64_t{std::bit_cast<std::int32_t>(id)},std::int64_t{record.country},std::int64_t{std::bit_cast<std::int32_t>(record.changed)}};
            database_.query(record.dirty==1?"EXEC usp_Create_Keep_E ?,?,?":"EXEC usp_Save_Keep_E ?,?,?",parameters);
            record.dirty=0;
        }
        return true;
    }
    const std::map<std::uint32_t,WorldDbObelisk>& records()const noexcept{return records_;}
private:
    static std::uint32_t read(std::span<const std::uint8_t> p,std::size_t offset) {std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=static_cast<std::uint32_t>(p[offset+i])<<(8*i);return value;}
    static void put(net::Packet& packet,std::uint32_t value) {for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    Database& database_;std::map<std::uint32_t,WorldDbObelisk> records_;
};
}
