#pragma once
#include "eternity/game/world_db_agent_save.hpp"
#include "eternity/game/world_db_obelisks.hpp"
#include "eternity/game/world_kill_count_db_owner.hpp"
#include "eternity/game/characters.hpp"
#include "eternity/game/obelisk_runtime_calendar.hpp"
#include <functional>
#include <memory>

namespace eternity::game {
// The retained world portion of ps_dbAgent.exe CConnection::PacketSystemConnect
// 0040b1f0 and CUserSaveThread::Run 0041d5b0. The real DB connection and guild
// owner must outlive this object and be confined to the same DB executor.
template<class Database,class GuildOwner> class WorldDbAgentWorld final {
public:
    WorldDbAgentWorld(Database& database,GuildOwner& guilds,std::uint32_t tick,std::uint32_t elapsed_world_seconds)
        :kills_(database,tick),obelisks_(database),saves_(kills_,obelisks_,guilds),world_tick_(tick),world_seconds_(elapsed_world_seconds) {}
    WorldDbAgentWorld(const WorldDbAgentWorld&)=delete;
    WorldDbAgentWorld& operator=(const WorldDbAgentWorld&)=delete;
    WorldDbAgentWorld(WorldDbAgentWorld&&)=delete;
    WorldDbAgentWorld& operator=(WorldDbAgentWorld&&)=delete;
    // CWorld::GetWorldDay 0040b660 uses unsigned tick subtraction.
    std::uint32_t world_day(std::uint32_t now)const noexcept{return world_seconds_+(now-world_tick_)/1000U;}
    std::vector<net::Packet> handshake_world_packets(std::uint32_t now)const {
        const auto day=world_day(now);net::Packet time{4,2};
        for(unsigned b=0;b<4;++b)time.push_back(static_cast<std::uint8_t>(day>>(8*b)));
        std::vector<net::Packet> packets;packets.push_back(std::move(time));packets.push_back(kills_.bootstrap_packet());
        auto keeps=obelisks_.packets();for(auto& packet:keeps)packets.push_back(std::move(packet));return packets;
    }
    // False indicates an unknown keep update; unsupported opcode is rejected.
    bool receive(std::span<const std::uint8_t> packet) {
        if(packet.size()<2 || packet[1]!=2)throw std::invalid_argument("Invalid DB-agent world packet");
        if(packet[0]==5) {kills_.receive(packet);return true;}
        if(packet[0]==6 || packet[0]==7)return obelisks_.receive(packet);
        throw std::invalid_argument("Unsupported DB-agent world opcode");
    }
    WorldDbAgentSaveResult periodic(std::uint32_t now){return saves_.periodic(now);}
    // Call after the actual connection-specific user SaveAll traversal has
    // posted its DB jobs; the original does not wait for those jobs here.
    WorldDbAgentSaveResult save_all(){return saves_.save_all();}
    const WorldKillCountDbOwner<Database>& kills()const noexcept{return kills_;}
    const WorldDbObelisks<Database>& obelisks()const noexcept{return obelisks_;}
private:
    WorldKillCountDbOwner<Database> kills_;WorldDbObelisks<Database> obelisks_;
    WorldDbAgentSave<WorldKillCountDbOwner<Database>,WorldDbObelisks<Database>,GuildOwner> saves_;
    std::uint32_t world_tick_,world_seconds_;
};
// SetWorldDay 0040f1c0 samples GetLocalTime, calls SubTimeSecond 0042abe0
// with base then current SYSTEMTIME, then samples GetTickCount. The existing
// calendar owner supplies OLE conversion and the original truncation behavior.
template<class Database,class GuildOwner> std::unique_ptr<WorldDbAgentWorld<Database,GuildOwner>>
make_world_db_agent_world(Database& database,GuildOwner& guilds,std::chrono::sys_seconds world_base,
    ObeliskRuntimeCalendarPlatform& calendar,const std::function<std::uint32_t()>& tick) {
    if(!tick)throw std::invalid_argument("DB-agent world requires its actual tick source");
    const auto now=calendar.local_calendar();
    if(now.year<2000 || now.year>2063)throw std::out_of_range("Current world calendar is outside packed conversion range");
    ObeliskRuntimeCalendarClock clock(calendar);
    const auto elapsed=clock.seconds_between(pack_obelisk_calendar(now),pack_world_time(world_base));
    const auto start=tick();
    return std::make_unique<WorldDbAgentWorld<Database,GuildOwner>>(database,guilds,start,elapsed);
}
template<class GuildOwner> std::unique_ptr<WorldDbAgentWorld<storage::SqlServer,GuildOwner>>
load_world_db_agent_world(storage::SqlServer& database,GuildOwner& guilds,
    ObeliskRuntimeCalendarPlatform& calendar,const std::function<std::uint32_t()>& tick) {
    const auto base=load_world_base(database);
    return make_world_db_agent_world(database,guilds,base,calendar,tick);
}
}
