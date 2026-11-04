#include "eternity/game/obelisk_runtime_world_publication.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
class PortalDatabase final:public ObeliskRuntimePortalWorld {
public:
    explicit PortalDatabase(ObeliskRuntimePublication& database):database_(database) {}
    void database(const net::Packet& p) override {database_.database_packet(p);}
    // SendDBPortalLinkList requires only DB transport. Accidental expansion
    // into scene/world publication must fail instead of inventing providers.
    void map_broadcast(std::size_t,const net::Packet&) override {throw std::logic_error("World DB list requested a map service");}
    void broadcast(const net::Packet&) override {throw std::logic_error("World DB list requested a world service");}
private:
    ObeliskRuntimePublication& database_;
};
}
void publish_world_obelisk_database_lists(std::span<const ObeliskRuntimePortalLink> portals,std::span<const std::span<const ObeliskRuntime>> maps,ObeliskRuntimePublication& database) {
    PortalDatabase portal_database(database);publish_obelisk_portal_database_list(portals,portal_database);
    for(const auto records:maps) publish_obelisk_database_list(records,database);
}
}
