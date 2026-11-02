#include "eternity/game/guild_sql.hpp"
#include <bit>
#include <charconv>
#include <limits>

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t value,unsigned int n=4) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void fixed(net::Packet& p,const std::string& value,std::size_t n) {p.insert(p.end(),value.begin(),value.end());p.insert(p.end(),n-value.size(),0);}
void integer(const storage::Row& row,std::size_t at,std::int64_t& value,std::int64_t low,std::int64_t high) {
    if(at>=row.size()) throw std::runtime_error("Missing guild SQL field");if(!row[at]) return;
    const auto& raw=*row[at];std::int64_t parsed=0;const auto result=std::from_chars(raw.data(),raw.data()+raw.size(),parsed);
    if(result.ec!=std::errc{} || result.ptr!=raw.data()+raw.size() || parsed<low || parsed>high) throw std::runtime_error("Guild SQL numeric conversion failed");value=parsed;
}
void string(const storage::Row& row,std::size_t at,std::string& value,std::size_t maximum) {
    if(at>=row.size()) throw std::runtime_error("Missing guild SQL text");if(!row[at]) return;auto raw=row[at]->substr(0,row[at]->find('\0'));if(raw.size()>maximum) throw std::runtime_error("Guild SQL string truncation");value=std::move(raw);
}
const storage::ResultSet& result(const std::vector<storage::ResultSet>& rows) {if(rows.size()!=1) throw std::runtime_error("Unexpected guild SQL result count");return rows.front();}
net::Packet packet(const GuildMetadata& data,const GuildMetadata& cached) {
    net::Packet p{2,0x0b,1};const auto& info=data.listing;const auto& keep=cached.listing;
    append(p,info.id);fixed(p,info.name,25);fixed(p,info.master,21);p.push_back(static_cast<std::uint8_t>(info.country));append(p,keep.points);p.push_back(keep.rank);append(p,cached.etin);p.push_back(cached.use_house);fixed(p,keep.notice,65);p.push_back(cached.buy_house);
    // LoadGuildNUserList never writes this DWORD. Normalize undefined stack data
    // to zero; do not invent a copy of the SQL KeepEtin value here.
    append(p,0);append(p,cached.return_count);return p;
}
}
bool GuildDbMetadata::upgrade_npc(std::uint8_t slot,std::uint16_t type,std::uint8_t level,std::uint32_t etin,std::uint32_t keep) {
    if(!npcs.upgrade(slot,type,level)) return false;metadata.etin=etin;metadata.keep_etin=keep;save_type=2;return true;
}
void GuildDbMetadata::reset_npcs(std::uint32_t keep) {npcs.reset();metadata.keep_etin=keep;save_type=2;}
GuildSql::GuildSql(storage::SqlServer& main,storage::SqlServer& auxiliary,InventoryReplicaPool& pool,GuildSqlPorts ports,std::string catalog):main_(main),auxiliary_(auxiliary),pool_(pool),bank_store_(auxiliary,ports.diagnostic,catalog),npc_store_(auxiliary,ports.diagnostic,catalog),ports_(std::move(ports)) {
    if(&main==&auxiliary || !ports_.send || !ports_.allocate || !ports_.diagnostic) throw std::invalid_argument("Guild loading requires two SQL connections and replica services");
    if(catalog.empty() || catalog.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos) throw std::invalid_argument("Invalid guild SQL catalog");prefix_="["+catalog+"].dbo.";
}
bool GuildSql::save() {
    if(main_.transaction_active() || auxiliary_.transaction_active()) throw std::logic_error("Guild saving requires independent connections in autocommit mode");
    std::vector<GuildDbMetadata*> ordered;for(auto& [id,guild]:replicas_) ordered.push_back(&guild);std::string diagnostic;
    GuildSavePorts ports{[]{return true;},[]{},[]{},[&](const GuildSaveOperation& operation) {
        try {(void)main_.query("EXEC "+prefix_+operation.procedure+" ?,?,?,?,?,?,?,?,?;",operation.parameters);return GuildSaveResult{};}
        catch(const storage::SqlError& error) {diagnostic=error.what();return GuildSaveResult{false,error.sqlstate,error.native_error};}
        catch(const std::exception& error) {diagnostic=error.what();return GuildSaveResult{false,{},0};}
    },[&](GuildDbMetadata& guild){return static_cast<std::int32_t>(bank_store_.save(guild.metadata.listing.id,*guild.bank));},[&](GuildDbMetadata& guild){return static_cast<std::int32_t>(npc_store_.save(guild.metadata.listing.id,guild.npcs));},[&](const GuildSaveResult&){ports_.diagnostic(diagnostic);}};
    return save_guilds(ordered,ports);
}
bool GuildSql::load_maximum() {
    try {const auto rows=main_.query("EXEC "+prefix_+"usp_Read_MaxGuildID_R;");const auto& data=result(rows);std::int64_t value=0;if(!data.rows.empty()) integer(data.rows.front(),0,value,INT32_MIN,INT32_MAX);net::Packet p{1,0x0b};append(p,static_cast<std::uint32_t>(value));ports_.send(std::move(p));return true;}
    catch(const std::exception& error) {ports_.diagnostic(error.what());return false;}
}
bool GuildSql::members(std::uint32_t id) {
    try {
        const std::array<storage::Parameter,1> parameters={std::int64_t{std::bit_cast<std::int32_t>(id)}};
        const auto rows=auxiliary_.query("EXEC "+prefix_+"usp_Read_GuildChar_R ?;",parameters);const auto& data=result(rows);std::array<std::int64_t,5> values{};std::string name;
        net::Packet p{3,0x0b};append(p,id);p.push_back(0);
        const auto flush=[&]{if(p[6]) ports_.send(std::move(p));p={3,0x0b};append(p,id);p.push_back(0);};
        for(const auto& row:data.rows) {
            integer(row,0,values[0],INT32_MIN,INT32_MAX);string(row,1,name,20);integer(row,2,values[1],INT16_MIN,INT16_MAX);integer(row,3,values[2],0,255);integer(row,4,values[3],INT32_MIN,INT32_MAX);integer(row,5,values[4],0,255);
            append(p,static_cast<std::uint32_t>(values[0]));append(p,static_cast<std::uint32_t>(values[1]),2);p.push_back(static_cast<std::uint8_t>(values[2]));p.push_back(static_cast<std::uint8_t>(values[4]));fixed(p,name,21);if(++p[6]==60) flush();
        }
        flush();return true;
    } catch(const std::exception& error) {ports_.diagnostic(error.what());return false;}
}
bool GuildSql::load() {
    try {
        const auto rows=main_.query("EXEC "+prefix_+"usp_Read_Guild2_R;");const auto& data=result(rows);std::array<std::int64_t,9> values{};std::string name,master,notice;
        for(const auto& row:data.rows) {
            integer(row,0,values[0],INT32_MIN,INT32_MAX);string(row,1,name,24);string(row,2,master,20);integer(row,3,values[1],0,255);integer(row,4,values[2],INT32_MIN,INT32_MAX);integer(row,5,values[3],0,255);integer(row,6,values[4],INT32_MIN,INT32_MAX);integer(row,7,values[5],0,255);string(row,8,notice,63);integer(row,9,values[6],0,255);integer(row,10,values[7],INT32_MIN,INT32_MAX);integer(row,11,values[8],INT32_MIN,INT32_MAX);
            GuildMetadata metadata;metadata.listing={static_cast<std::uint32_t>(values[0]),static_cast<std::uint32_t>(values[1]),static_cast<std::uint32_t>(values[2]),static_cast<std::uint8_t>(values[3]),name,master,notice};metadata.etin=static_cast<std::uint32_t>(values[4]);metadata.use_house=static_cast<std::uint8_t>(values[5]);metadata.buy_house=static_cast<std::uint8_t>(values[6]);metadata.return_count=static_cast<std::uint32_t>(values[7]);metadata.keep_etin=static_cast<std::uint32_t>(values[8]);
            auto found=replicas_.find(metadata.listing.id);
            if(found==replicas_.end()) {
                if(!ports_.allocate()) continue;
                found=replicas_.emplace(metadata.listing.id,GuildDbMetadata{metadata,{},std::make_unique<GuildBankReplica>(pool_)}).first;
                (void)load_bank(metadata.listing.id,*found->second.bank);(void)load_npcs(metadata.listing.id,found->second.npcs);
            }
            ports_.send(packet(metadata,found->second.metadata));
            // Original caller ignores the member loader's result and continues.
            (void)members(metadata.listing.id);
        }
        return true;
    } catch(const std::exception& error) {ports_.diagnostic(error.what());return false;}
}
bool GuildSql::load_npcs(std::uint32_t id,GuildNpcReplica& npcs) {
    try {
        const std::array<storage::Parameter,1> parameters={std::int64_t{std::bit_cast<std::int32_t>(id)}};
        const auto rows=auxiliary_.query("EXEC "+prefix_+"usp_Read_GuildNpcLv_R ?;",parameters);const auto& data=result(rows);std::array<std::int64_t,3> values{};
        for(const auto& row:data.rows) {integer(row,0,values[0],INT16_MIN,INT16_MAX);integer(row,1,values[1],0,255);integer(row,2,values[2],0,255);npcs.load(static_cast<std::uint8_t>(values[2]),static_cast<std::uint16_t>(values[0]),static_cast<std::uint8_t>(values[1]));}
        return true;
    } catch(const std::exception& error) {ports_.diagnostic(error.what());return false;}
}
void GuildSql::send_npcs() {for(const auto& [id,replica]:replicas_) if(auto p=replica.npcs.packet(id)) ports_.send(std::move(*p));}
bool GuildSql::load_bank(std::uint32_t id,GuildBankReplica& bank) {
    bank.begin_load();
    try {
        const std::array<storage::Parameter,1> parameters={std::int64_t{std::bit_cast<std::int32_t>(id)}};
        const auto rows=auxiliary_.query("EXEC "+prefix_+"usp_Read_Guild_StoredItems_Detail_R ?;",parameters);const auto& data=result(rows);std::array<std::int64_t,21> v{};std::uint64_t uid=0;std::string craft,make;
        for(const auto& row:data.rows) {
            integer(row,0,v[0],INT32_MIN,INT32_MAX);integer(row,1,v[1],0,255);integer(row,2,v[2],0,255);
            if(row.size()<21) throw std::runtime_error("Missing guild bank SQL fields");
            if(row[3]) {const auto& text=*row[3];const auto parsed=std::from_chars(text.data(),text.data()+text.size(),uid);if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::runtime_error("Guild bank UID conversion failed");}
            integer(row,4,v[4],INT32_MIN,INT32_MAX);integer(row,5,v[5],INT16_MIN,INT16_MAX);
            for(std::size_t i=6;i<12;++i) integer(row,i,v[i],0,255);
            string(row,12,craft,20);integer(row,13,v[13],0,255);string(row,14,make,1);for(std::size_t i=15;i<21;++i) integer(row,i,v[i],INT32_MIN,INT32_MAX);
            PlayerItem item;item.uid=uid;item.id=static_cast<std::uint32_t>(v[0]);item.type=static_cast<std::uint8_t>(v[1]);item.type_id=static_cast<std::uint8_t>(v[2]);item.quality=static_cast<std::uint16_t>(v[5]);for(std::size_t i=0;i<6;++i) item.gems[i]=static_cast<std::uint8_t>(v[6+i]);item.craft=craft;item.count=static_cast<std::uint8_t>(v[13]);item.make_type=make;
            std::uint32_t stamp=static_cast<std::uint16_t>(v[15])-16U;for(const auto [i,bits]:std::array<std::pair<std::size_t,unsigned int>,5>{{{16,4},{17,5},{18,5},{19,6},{20,6}}}) stamp=(stamp<<bits)+static_cast<std::uint16_t>(v[i]);item.made_at=stamp;
            bank.load(std::move(item),static_cast<std::int32_t>(v[4]));
        }
        bank.finish_load(true);return true;
    } catch(const std::exception& error) {bank.finish_load(false);ports_.diagnostic(error.what());return false;}
}
InventoryReplicaResult GuildSql::apply(std::span<const std::uint8_t> p) {
    if(p.size()<6) return InventoryReplicaResult::rejected;
    const auto op=std::uint32_t{p[0]}|(std::uint32_t{p[1]}<<8);
    if(op==0xb0f || op==0xb17) {
        if(p.size()!=(op==0xb0f?18U:10U) || (op==0xb0f && p[9]>=8)) return InventoryReplicaResult::rejected;
        const auto dword=[&](std::size_t at){std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=std::uint32_t{p[at+i]}<<(8*i);return value;};
        const auto found=replicas_.find(dword(2));if(found==replicas_.end()) return InventoryReplicaResult::applied;
        if(op==0xb0f) found->second.upgrade_npc(p[9],static_cast<std::uint16_t>(p[6]|(std::uint16_t{p[7]}<<8)),p[8],dword(10),dword(14));
        else found->second.reset_npcs(dword(6));return InventoryReplicaResult::applied;
    }
    const std::size_t size=op==0xb11?10:op==0xb14?8:op==0xb15?7:0;
    if(size==0) return InventoryReplicaResult::unsupported;
    if(p.size()!=size || p[6]>=240 || (op==0xb11 && p[8]>=240)) return InventoryReplicaResult::rejected;
    std::uint32_t id=0;for(unsigned int i=0;i<4;++i) id|=std::uint32_t{p[2+i]}<<(8*i);
    const auto found=replicas_.find(id);
    if(found==replicas_.end()) {ports_.diagnostic("Guild bank command references unknown guild "+std::to_string(id));return InventoryReplicaResult::applied;}
    return found->second.bank->apply(p);
}
std::optional<std::uint32_t> GuildSql::save_bank(std::uint32_t guild) {
    const auto found=replicas_.find(guild);if(found==replicas_.end()) return std::nullopt;
    return bank_store_.save(guild,*found->second.bank);
}
std::optional<std::uint32_t> GuildSql::save_npcs(std::uint32_t guild) {
    const auto found=replicas_.find(guild);if(found==replicas_.end()) return std::nullopt;
    return npc_store_.save(guild,found->second.npcs);
}
void GuildSql::send_banks() {for(const auto& [id,replica]:replicas_) for(auto& p:replica.bank->packets(id)) ports_.send(std::move(p));}

}
