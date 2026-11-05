#pragma once
#include "eternity/game/actor.hpp"
#include <array>

namespace eternity::game {
// One retained creation owner, with the original leader and seven live slots.
struct ZoneGuildCreation {
    std::uint8_t phase=0;
    std::string name;
    Actor* leader=nullptr;
    std::array<Actor*,7> participants{};
};
class ZoneGuildCreationServices {
public:
    virtual ~ZoneGuildCreationServices()=default;
    // CUser+175c; every recipient refers to its canonical session field.
    virtual ZoneGuildCreation*& current(Actor&)=0;
    // Full 00433430 send: leader then seven slots, one shared packet buffer,
    // allocation/socket failure and per-connection reference ownership.
    virtual void broadcast(ZoneGuildCreation&,const net::Packet&)=0;
    // 00432a50 accepts null; actual manager/pool owns a non-null creation.
    virtual void release(ZoneGuildCreation*)=0;
};
// ps_game.exe CUser::LeaveZone 0044b19c..0044b1eb. The owner serializes all
// member fields, retains them and the creation across callbacks, and supplies
// initial stack bytes for the original 98-byte phase notification.
inline void zone_leave_guild_creation(Actor& actor,ZoneGuildCreationServices& services,
    std::array<std::uint8_t,98> scratch) {
    auto* creation=services.current(actor);if(!creation)return;
    net::Packet packet;
    if(creation->phase) {
        if(creation->name.size()>89 || creation->name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid guild creation name");
        packet.assign(scratch.begin(),scratch.end());packet[0]=1;packet[1]=0x0d;packet[2]=7;
        std::copy(creation->name.begin(),creation->name.end(),packet.begin()+8);packet[8+creation->name.size()]=0;
    } else packet={3,0x0d,3};
    services.broadcast(*creation,packet);
    creation=services.current(actor);if(!creation)throw std::logic_error("Creation disappeared during notification");
    if(creation->leader){services.current(*creation->leader)=nullptr;creation->leader=nullptr;}
    for(auto& member:creation->participants)if(member){services.current(*member)=nullptr;member=nullptr;}
    services.release(services.current(actor));
}
}
