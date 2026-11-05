#pragma once
#include "eternity/net/framing.hpp"
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace eternity::game {
struct WorldKillCountDeadlines {std::uint32_t reset=0,send=0;};
// The owner is the actual CWorld kill-count state and DB-agent transport.
class WorldKillCountTimerServices {
public:
    virtual ~WorldKillCountTimerServices()=default;
    virtual WorldKillCountDeadlines& deadlines(std::uint8_t country)=0;
    // Must implement CWorld::SetKillCount(country,0), including status/apply side effects.
    virtual void set_zero(std::uint8_t country)=0;
    virtual std::uint32_t count(std::uint8_t country)=0;
    virtual void send_database(const net::Packet& packet)=0;
};
// ps_game.exe CWorld::UpdateKillCount 00413300, disassembly 0041331a..004133f2.
class WorldKillCountTimer final {
public:
    explicit WorldKillCountTimer(std::shared_ptr<WorldKillCountTimerServices> services):services_(std::move(services)) {
        if(!services_)throw std::invalid_argument("World kill-count timer requires actual state and DB owner");
    }
    template<class Root> bool update(Root& root,std::uint32_t now) {
        if(!root.world_send_running())throw std::logic_error("World kill-count timer requires a running world");
        bool changed=false;
        for(std::uint8_t country=0;country<2;++country) {
            auto& deadline=services_->deadlines(country);
            if(!root.world_send_running())return false;
            if(deadline.reset!=0) {
                if(now<deadline.reset)continue;
                services_->set_zero(country);if(!root.world_send_running())return false;
                root.request_kill_count_send(country);deadline.send=now+600000U;deadline.reset=0;changed=true;
            } else if(deadline.send<=now) {
                root.request_kill_count_send(country);deadline.send=now+600000U;changed=true;
            }
        }
        if(!changed)return root.world_send_running();
        const auto first=services_->count(0);if(!root.world_send_running())return false;
        const auto second=services_->count(1);if(!root.world_send_running())return false;
        net::Packet packet{0x05,0x02};
        for(const auto value:{first,second})for(unsigned byte=0;byte<4;++byte)packet.push_back(static_cast<std::uint8_t>(value>>(byte*8)));
        services_->send_database(packet);return root.world_send_running();
    }
private:std::shared_ptr<WorldKillCountTimerServices> services_;
};
}
