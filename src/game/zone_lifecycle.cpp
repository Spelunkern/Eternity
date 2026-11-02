#include "eternity/game/zone_lifecycle.hpp"

namespace eternity::game {
bool send_nprotect_query(bool enabled,NProtectState& state,const NProtectPorts& port) {
    if(!enabled || !state.present || !state.received) return true;
    if(!port.query || !port.tick || !port.send || !port.diagnostic || !port.close) throw std::invalid_argument("Incomplete NProtect query services");
    std::array<std::uint32_t,4> query{};const auto result=port.query(query);
    if(result!=0) {port.diagnostic(result);port.close(111);return false;}
    state.received=false;state.receive_time=port.tick()+900000U;
    net::Packet packet{2,0xa3};
    for(auto value:query) for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));
    port.send(std::move(packet));return true;
}
void enter_zone_lifecycle(ZoneLifecycleState& state,const ZoneLifecyclePorts& port) {
    if(!port.tick || !port.reset_keep_tick || !port.check_map || !port.nprotect) throw std::invalid_argument("Incomplete zone lifecycle services");
    state.motion=0;state.target_kind=0;
    const auto tick=port.tick();
    state.movement.previous_time=tick;state.movement.previous=state.position;
    state.movement.back_time=tick;state.movement.total_distance=0;state.movement.maximum_speed=0;
    state.movement.check_time=tick+5000U;state.movement.back=state.position;
    port.reset_keep_tick(port.tick());port.check_map();port.nprotect();
}
}
