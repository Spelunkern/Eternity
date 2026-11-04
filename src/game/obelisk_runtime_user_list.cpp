#include "eternity/game/obelisk_runtime_user_list.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
void put(net::Packet& p,std::uint32_t value,unsigned bytes) {for(unsigned i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
void publish_obelisk_user_list(const void* map,std::span<const ObeliskRuntime> obelisks,std::span<const ObeliskRuntimePortalScene> portals,ObeliskRuntimeUserPublication& user) {
    if(!map) throw std::invalid_argument("Obelisk user list requires actual map identity");
    net::Packet packet;put(packet,0x2101,2);put(packet,0,1);unsigned count=0;
    const auto row=[&](std::uint32_t id,std::uint32_t country,const UserPosition& p) {
        put(packet,id,4);put(packet,country,1);put(packet,std::bit_cast<std::uint32_t>(p.x),4);put(packet,std::bit_cast<std::uint32_t>(p.z),4);
        if(++count==100) {packet[2]=static_cast<std::uint8_t>(count);user.send_user(packet);packet.resize(3);count=0;}
    };
    for(const auto& o:obelisks) row(o.id,o.country==0?1U:o.country==1?2U:0U,o.center);
    for(const auto& scene:portals) {
        if(scene.map!=map) continue;
        if(!scene.link||!scene.link->country||!scene.position) throw std::invalid_argument("Matching portal user list row requires live target backing");
        const auto country=*scene.link->country;
        row(scene.link->id,country==0?103U:country==1?101U:country==2?102U:100U,*scene.position);
    }
    if(count) {packet[2]=static_cast<std::uint8_t>(count);user.send_user(packet);}
}
}
