#pragma once
#include "eternity/game/world_kill_count_bootstrap.hpp"
#include <span>

namespace eternity::game {
// Retained portion of ps_dbAgent.exe's CWorld. SQL use and packet dispatch
// must be serialized by the caller; the supplied connection must outlive it.
template<class Database> class WorldKillCountDbOwner final {
public:
    explicit WorldKillCountDbOwner(Database& database,std::uint32_t constructor_tick)
        :database_(database),last_world_save_(constructor_tick+900000U) {
        receive(load_world_kill_count_bootstrap(database_));
    }
    // CConnection::PacketSystemConnect 0040b1f0 case 0205 updates memory only.
    void receive(std::span<const std::uint8_t> packet) {
        if(packet.size()!=10 || packet[0]!=0x05 || packet[1]!=0x02)
            throw std::invalid_argument("Invalid DB-agent kill-count packet");
        std::array<std::uint32_t,2> next{};
        for(std::size_t c=0;c<next.size();++c)
            for(unsigned b=0;b<4;++b)next[c]|=static_cast<std::uint32_t>(packet[2+4*c+b])<<(8*b);
        counts_=next;
    }
    // Same packet sent to a connecting game server by case 0201.
    net::Packet bootstrap_packet()const {
        net::Packet packet{0x05,0x02};
        for(auto count:counts_)for(unsigned b=0;b<4;++b)packet.push_back(static_cast<std::uint8_t>(count>>(8*b)));
        return packet;
    }
    const std::array<std::uint32_t,2>& counts()const noexcept{return counts_;}
    // CUserSaveThread::Run 0041d5b0: unsigned elapsed comparison and a
    // timestamp update even if this save fails. Other save phases are separate.
    bool periodic_due(std::uint32_t now)const noexcept{return now-last_world_save_>=900000U;}
    bool save_periodic(std::uint32_t now) {
        if(!periodic_due(now))throw std::logic_error("World kill-count save is not due");
        last_world_save_=now;return save_world_kill_count(database_,counts_);
    }
    // CUserMgr::SaveAll 0041d2f0 invokes this independently of the timer.
    bool save_all(){return save_world_kill_count(database_,counts_);}
private:
    Database& database_;std::array<std::uint32_t,2> counts_{};std::uint32_t last_world_save_;
};
}
