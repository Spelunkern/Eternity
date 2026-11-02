#include "eternity/game/guild_save.hpp"
#include "eternity/game/guild_sql.hpp"
#include <bit>

namespace eternity::game {
std::string guild_save_sql(const GuildSaveOperation& operation,std::string_view prefix) {
    std::string query="EXEC "+std::string(prefix)+operation.procedure+' ';
    for(std::size_t i=0;i<operation.parameters.size();++i) {
        if(i) query+=',';
        if(i==3) {const auto& value=std::get<std::string>(operation.parameters[i]);query+='\'';query+=value.substr(0,value.find('\0'));query+='\'';}
        else query+=std::to_string(std::get<std::int64_t>(operation.parameters[i]));
    }
    query+=' ';return query;
}
bool save_guilds(std::span<GuildDbMetadata* const> ordered,GuildSavePorts& ports) {
    if(!ports.acquire || !ports.release || !ports.prepare || !ports.execute || !ports.bank || !ports.npcs || !ports.diagnostic) throw std::invalid_argument("Incomplete guild save services");
    if(!ports.acquire()) {ports.diagnostic({false,{},0});return false;}
    struct Lease {GuildSavePorts& ports;~Lease(){ports.release();}} lease{ports};
    const auto signed_value=[](std::uint32_t v)->std::int64_t{return std::bit_cast<std::int32_t>(v);};
    for(auto* guild:ordered) {
        if(!guild) throw std::invalid_argument("Null guild save record");
        unsigned int timeouts=0;
        while(guild->save_type==2) {
            const auto& m=guild->metadata;const auto& g=m.listing;
            const GuildSaveOperation operation{"usp_Save_GuildInfo_E2",{signed_value(g.id),signed_value(g.points),signed_value(m.etin),g.notice,std::int64_t{m.buy_house},signed_value(m.return_count),signed_value(m.keep_etin),std::int64_t{g.rank},std::int64_t{m.use_house}}};
            const auto result=ports.execute(operation);
            if(result.success) {guild->save_type=0;ports.prepare();break;}
            if(result.sqlstate=="HYT00") {
                if(++timeouts==3) {ports.diagnostic(result);return false;}
                ports.prepare();continue;
            }
            if(result.native_error==105 && guild->metadata.listing.notice.size()>64) guild->metadata.listing.notice.resize(64);
            ports.diagnostic(result);ports.prepare();break;
        }
        if(ports.bank(*guild)>0) (void)ports.bank(*guild);
        if(ports.npcs(*guild)>0) (void)ports.npcs(*guild);
    }
    return true;
}
}
