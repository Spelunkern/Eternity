#include "eternity/game/obelisk_runtime_publication.hpp"

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::uint32_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}
void record(net::Packet& packet,const ObeliskRuntime& o) {
    put(packet,o.base_id,4);
    put(packet,((static_cast<std::uint32_t>(o.last_destroy_country)+1U)*16U)|static_cast<std::uint32_t>(o.country),1);
    put(packet,o.change_time,4);
}
}
net::Packet obelisk_database_change_packet(const ObeliskRuntime& o) {
    net::Packet packet;put(packet,0x207,2);record(packet,o);return packet;
}
void publish_obelisk_change(const ObeliskRuntimeMap& map,const ObeliskRuntime& o,ObeliskRuntimePublication& transport) {
    if(!map.zone_present) return;
    net::Packet packet;put(packet,0x2102,2);put(packet,o.id,4);
    put(packet,o.country==0?1U:o.country==1?2U:0U,1);transport.zone_packet(packet);
}
void publish_obelisk_database_change(const ObeliskRuntime& o,ObeliskRuntimePublication& transport) {
    transport.database_packet(obelisk_database_change_packet(o));
}
void publish_obelisk_database_list(std::span<const ObeliskRuntime> obelisks,ObeliskRuntimePublication& transport) {
    net::Packet packet{0x06,0x02,0};
    for(const auto& o:obelisks) {
        record(packet,o);++packet[2];
        if(packet[2]==100) {transport.database_packet(packet);packet.resize(3);packet[2]=0;}
    }
    if(packet[2]!=0) transport.database_packet(packet);
}
}
