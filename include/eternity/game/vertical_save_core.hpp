#pragma once
#include "eternity/game/zone_entry.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
class VerticalSaveCoreServices {
public:
    virtual ~VerticalSaveCoreServices()=default;
    // Canonical CUser position, including users without current zone membership.
    virtual UserPosition position(Actor&)=0;
    // Ordered DB-agent delivery; caller owns failure policy and cache locking.
    virtual void send_database(const net::Packet&)=0;
};
// Prefix of ps_game.exe SendDBAgentCharGetInfo 0046c980..0046cab9. Fields are
// read in their original phases, so callbacks may update later fields. Actor,
// entry and inventory mirror survive all callbacks. Retained item mirrors must
// already be synchronized to actor.inventory(). Does not include later skill,
// effect, quest and other phases and must precede the DB exit request.
inline void vertical_save_core(Actor& actor,const ZoneEntryState& entry,VerticalSaveCoreServices& services) {
    const auto append=[](net::Packet& p,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet resource{6,6};append(resource,actor.account_id(),4);const auto current=actor.resources();
    append(resource,current.hp,2);append(resource,current.mp,2);append(resource,current.sp,2);services.send_database(resource);
    net::Packet position{7,6};append(position,actor.account_id(),4);append(position,entry.map,2);append(position,entry.direction,2);
    const auto live=services.position(actor);for(float value:{live.x,live.y,live.z})append(position,std::bit_cast<std::uint32_t>(value),4);services.send_database(position);
    net::Packet quality{0x0d,7};append(quality,actor.account_id(),4);quality.push_back(0);
    for(std::uint16_t slot=0;slot<13;++slot) {
        const auto& inventory=actor.inventory();const auto match=[&](const PlayerItem& item){return item.bag==0 && item.slot==slot;};
        const auto it=std::find_if(inventory.begin(),inventory.end(),match);if(it==inventory.end())continue;
        if(std::find_if(std::next(it),inventory.end(),match)!=inventory.end())throw std::logic_error("Duplicate equipment slot during save");
        if(it->quality!=actor.equipment_database_quality()[slot]){quality.push_back(static_cast<std::uint8_t>(slot));append(quality,it->quality,2);++quality[6];}
    }
    if(quality[6])services.send_database(quality);
}
}
