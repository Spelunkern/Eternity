#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/net/framing.hpp"
#include <memory>

namespace eternity::game {
net::Packet mob_appearance(const MobRuntime&,std::uint8_t appearance);
net::Packet mob_removed(std::uint32_t id);
std::int32_t mob_status(const MobRuntime&) noexcept;
class MobBroadcast {
public:
    virtual ~MobBroadcast()=default;
    virtual void send_cell(CellCoordinates)=0;
};
class MobViewer {
public:
    virtual ~MobViewer()=default;
    virtual void send_self(const net::Packet&)=0;
};
class ZoneMobServices {
public:
    virtual ~ZoneMobServices()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual void send_cell_view(CellCoordinates,const net::Packet&)=0;
    virtual std::unique_ptr<MobBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t> priority)=0;
    virtual void lua_init(MobRuntime&)=0; // LuaCall("Init"), after entry broadcast.
};
// Serialized zone owner; objects/services outlive membership. Delivery may inspect
// live state but must not move/remove the moving subject during transition sends.
// Cell-list sends/finds preserve the original shared cursor and reentrant lookup.
class ZoneMobs {
public:
    ZoneMobs(const MapGeometry&,ZoneMobServices&);
    ~ZoneMobs();
    ZoneMobs(const ZoneMobs&)=delete;ZoneMobs& operator=(const ZoneMobs&)=delete;
    bool enter(MobRuntime&); // EnterMob 00416860; 0601 appearance 1.
    bool leave(MobRuntime&); // LeaveMob 00416990; no packet or recycling.
    bool move(MobRuntime&,UserPosition,std::uint8_t mode); // MoveMob 004169d0.
    MobRuntime* find(std::uint32_t) const noexcept;
    MobRuntime* find_cell(CellCoordinates,std::uint32_t);
    std::vector<MobRuntime*> all() const;
    std::vector<MobRuntime*> in_cell(CellCoordinates) const;
    void send_enter(CellCoordinates,MobViewer&); // CellInfoEnter, appearance 0, skip status 2.
    void send_leave(CellCoordinates,MobViewer&); // CellInfoLeave sends all IDs.
    std::size_t size() const noexcept {return members_.size();}
private:
    struct Cell {std::map<std::uint64_t,MobRuntime*> members;std::uint64_t cursor=0;};
    struct Member {std::uint64_t ordinal=0,cell_ordinal=0;CellCoordinates cell;};
    std::size_t key(CellCoordinates) const;
    void unlink_cell(const Member&);
    void link_cell(MobRuntime&,Member&,CellCoordinates);
    void transition(const MobRuntime&,CellCoordinates,CellCoordinates);
    void send(CellCoordinates,MobViewer&,bool entering);
    MapGeometry geometry_;ZoneMobServices& services_;
    std::map<std::uint32_t,MobRuntime*> index_;
    std::map<std::uint64_t,MobRuntime*> ordered_;
    std::map<MobRuntime*,Member> members_;std::map<std::size_t,Cell> cells_;
    std::uint64_t sequence_=0,cell_sequence_=0;
};
}
