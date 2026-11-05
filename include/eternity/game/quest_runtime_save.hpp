#pragma once
#include "eternity/game/quest_runtime.hpp"
#include <algorithm>
#include <functional>

namespace eternity::game {
// ps_game.exe SendDBAgentCharGetInfo quest phase 0046ccd4..0046ce4a. The
// services supply the actual recursive quest lock. Full batches are sent under
// that lock, the final partial batch after unlock. Until nested cursor mutation
// is recovered, send callbacks must not mutate the active quest list.
struct QuestRuntimeSavePorts {
    std::function<void()> lock,unlock;
    std::function<void(const net::Packet&)> send;
};
inline void quest_runtime_save(QuestRuntime& quests,std::uint32_t account,std::uint32_t tick,
    const QuestRuntimeSavePorts& ports) {
    if(!ports.lock || !ports.unlock || !ports.send)throw std::invalid_argument("Quest save requires lock and delivery");
    net::Packet packet{4,0x0a};for(unsigned i=0;i<4;++i)packet.push_back(static_cast<std::uint8_t>(account>>(8*i)));packet.push_back(0);
    ports.lock();
    struct Lock {const QuestRuntimeSavePorts& ports;bool held=true;~Lock(){if(held)ports.unlock();}} lock{ports};
    auto* quest=quests.first_active();
    while(quest) {
        if(!quest->definition)throw std::logic_error("Active quest lacks definition");
        const auto maximum=quest->definition->maximum_minutes();std::uint32_t delay;
        if(maximum)delay=quest->packet.end_tick<=tick?1U:std::min((quest->packet.end_tick-tick)/60000U,maximum);
        else delay=tick<=quest->start_tick?0U:std::min((tick-quest->start_tick)/60000U,quest->definition->minimum_minutes());
        packet.push_back(static_cast<std::uint8_t>(quest->packet.id));packet.push_back(static_cast<std::uint8_t>(quest->packet.id>>8));
        packet.push_back(static_cast<std::uint8_t>(delay));packet.push_back(static_cast<std::uint8_t>(delay>>8));
        packet.insert(packet.end(),quest->packet.counters.begin(),quest->packet.counters.end());
        quest=quests.next_active();
        if(++packet[6]==201){ports.send(packet);packet.resize(7);packet[6]=0;}
    }
    lock.held=false;ports.unlock();
    if(packet[6])ports.send(packet);
}
}
