#pragma once
#include "eternity/game/npcs.hpp"
#include "eternity/net/framing.hpp"
#include <map>
#include <memory>

namespace eternity::game {
class ZoneNpcs;
struct Npc {
    std::uint32_t id=0;
    UserPosition position;
    ZoneNpcs* zone=nullptr;
    std::optional<CellCoordinates> cell;
    std::int32_t type=0,type_id=0,angle=0;
    const NpcDefinition* definition=nullptr;
    const MapNpc* source=nullptr;
    std::uint32_t disable_time=0;
};
// Original SSyncHeap<CNpc> uses a LIFO free list and preserves IDs/object state.
// The owner must detach an NPC before returning it; reset is a separate operation.
class NpcPool {
public:
    Npc* allocate();
    void release(Npc&);
private:
    std::vector<std::unique_ptr<Npc>> objects_;
    std::vector<Npc*> available_;
};
class ZoneNpcServices {
public:
    virtual ~ZoneNpcServices() = default;
    virtual Npc* allocate() = 0;
    virtual std::uint32_t next_id() = 0;
    virtual void invalid_guard_position(const MapNpc&) = 0;
    virtual void send_cell_view(CellCoordinates,const net::Packet&) = 0;
};
net::Packet npc_appearance(const Npc&);
// One world executor serializes operations. Maps/definitions, services and all
// allocated NPCs must outlive the registry. Movement/AI remain separate.
class ZoneNpcs {
public:
    ZoneNpcs(const MapGeometry&,ZoneNpcServices&);
    ~ZoneNpcs();
    ZoneNpcs(const ZoneNpcs&)=delete;
    ZoneNpcs& operator=(const ZoneNpcs&)=delete;
    void initialize(const MapDefinition&,const NpcDefinitions&);
    bool enter(Npc&);
    bool leave(Npc&);
    Npc* find(std::uint32_t) const noexcept;
    std::span<Npc* const> all() const noexcept {return members_;}
    std::span<Npc* const> in_cell(CellCoordinates) const noexcept;
private:
    std::size_t key(CellCoordinates) const;
    MapGeometry geometry_;ZoneNpcServices& services_;
    std::vector<Npc*> members_;
    std::map<std::uint32_t,Npc*> index_;
    std::map<std::size_t,std::vector<Npc*>> cells_;
};
}
