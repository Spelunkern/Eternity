#include "eternity/game/mob_drops_effect_transport.hpp"
#include "eternity/game/skill_replica.hpp"
namespace eternity::game {
void ActorEffectReplicaDelivery::send_effect_database(const net::Packet& packet) noexcept {
    try {
        std::lock_guard lock(mutex_);
        if(packet.size()!=9 || packet[0]!=2 || packet[1]!=9) {diagnostics_.rejected_effect_database(packet,InventoryReplicaResult::rejected);return;}
        const auto result=replica_.apply(packet);
        if(result!=InventoryReplicaResult::applied) diagnostics_.rejected_effect_database(packet,result);
    }catch(...) {diagnostics_.failed_effect_database(packet);}
}
net::Packet effect_database_removed_packet(std::uint32_t account,std::uint16_t skill,std::uint8_t level) {
    net::Packet packet{2,9};
    for(unsigned shift=0;shift<32;shift+=8) packet.push_back(static_cast<std::uint8_t>(account>>shift));
    packet.push_back(static_cast<std::uint8_t>(skill));packet.push_back(static_cast<std::uint8_t>(skill>>8));packet.push_back(level);return packet;
}
}
