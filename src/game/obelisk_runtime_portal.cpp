#include "eternity/game/obelisk_runtime_portal.hpp"
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& p,std::uint32_t n,unsigned bytes) {for(unsigned i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
std::uint32_t translated(std::int32_t country) {return country==0?103U:country==1?101U:country==2?102U:100U;}
void changed(std::span<ObeliskRuntimePortalLink> links,std::size_t i,std::int32_t previous,ObeliskRuntimePortalWorld& world) {
    if(previous==*links[i].country) return;
    publish_obelisk_portal_change(links,i,world);
    // Publication callbacks precede these live portal reads in the executable.
    net::Packet p;put(p,0x2104,2);put(p,links[i].portal_id,2);put(p,translated(previous),1);put(p,translated(*links[i].country),1);world.broadcast(p);
}
void close(ObeliskRuntimePortalLink& link,unsigned side) {
    if(!link.open[side]) return;
    link.open[side]=false;
    const auto c=*link.country;
    *link.country=side==0?(c==0||c==2?2:3):(c>=0&&c<=1?1:3);
}
}
void publish_obelisk_portal_change(std::span<ObeliskRuntimePortalLink> links,std::size_t i,ObeliskRuntimePortalWorld& world) {
    if(i>=links.size()||!links[i].country) throw std::invalid_argument("Portal publication requires bound record");
    auto& link=links[i];net::Packet db;put(db,0x207,2);put(db,link.base_id,4);put(db,static_cast<std::uint32_t>(*link.country),1);put(db,0,4);world.database(db);
    if(link.zone_present) {net::Packet p;put(p,0x2102,2);put(p,link.id,4);put(p,translated(*link.country),1);world.map_broadcast(i,p);}
}
void update_obelisk_portal_links(std::span<ObeliskRuntimePortalLink> links,ObeliskRuntimePortalWorld& world) {
    if(links.size()>12) throw std::invalid_argument("Original portal neighbor flags support at most twelve links");
    std::array<bool,12> close_neighbor{};
    for(std::size_t i=0;i<links.size();++i) {
        auto& link=links[i];if(!link.country) continue;
        std::array<std::int32_t,2> matches{};
        for(unsigned side=0;side<2;++side) {
            if(link.counts[side]>static_cast<std::int32_t>(link.links.size()-side*4)) throw std::invalid_argument("Portal obelisk projection too short");
            for(std::int32_t n=0;n<link.counts[side];++n) {
                const auto* ob=link.links[side*4+static_cast<std::size_t>(n)];
                if(ob&&ob->last_destroy_country==static_cast<std::int32_t>(side)) ++matches[side];
            }
        }
        const auto previous=*link.country;
        const unsigned side=link.counts[0]>0?0U:1U;
        if(link.counts[side]>0) {
            if(matches[side]==link.counts[side]) {
                if(!link.open[side]) {
                    link.open[side]=true;const auto c=*link.country;
                    *link.country=side==0?(c==0||c==2?0:1):(c>=0&&c<=1?0:2);
                    if(side==0&&i+1<close_neighbor.size()) close_neighbor[i+1]=true;
                    if(side==1&&i>0) close_neighbor[i-1]=true;
                }
            } else if(matches[side]==0) close(link,side);
        }
        changed(links,i,previous,world);
    }
    for(std::size_t i=0;i<links.size();++i) {
        auto& link=links[i];if(!link.country||!close_neighbor[i]) continue;
        const auto previous=*link.country;
        if(link.counts[0]>0) close(link,0);
        else if(link.counts[1]>0) close(link,1);
        changed(links,i,previous,world);
    }
}
}
