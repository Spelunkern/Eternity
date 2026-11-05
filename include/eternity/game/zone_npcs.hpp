#pragma once
#include "eternity/game/npcs.hpp"
#include "eternity/game/pathfinding.hpp"
#include "eternity/net/framing.hpp"
#include <functional>
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
    std::int32_t users_using=0,next_position=0;
    bool forward=true;
    std::uint32_t next_move=0;
    std::int32_t status=0,target_kind=0;
    std::uint32_t target_id=0,next_chase=0;
    AStarPath path;
    ChaseTrail trail;
};
class NpcMovementWorld {
public:
    virtual ~NpcMovementWorld() = default;
    virtual bool move(Npc&,UserPosition,std::uint8_t mode) = 0;
};
class NpcClock {
public:
    virtual ~NpcClock() = default;
    virtual std::uint32_t tick_ms() const noexcept = 0;
};
class NpcNavigationWorld:public NpcMovementWorld,public NpcClock {};
// Original SSyncHeap<CNpc> uses a LIFO free list and preserves IDs/object state.
// The owner must detach an NPC before returning it; reset is a separate operation.
class NpcPool {
public:
    class Lease {
    public:
        Lease()=default;
        ~Lease();
        Lease(const Lease&)=delete;
        Lease& operator=(const Lease&)=delete;
        Lease(Lease&&) noexcept;
        Lease& operator=(Lease&&) noexcept;
        Npc* get() const noexcept {return npc_;}
    private:
        friend class NpcPool;
        Lease(NpcPool& pool,Npc& npc):pool_(&pool),npc_(&npc) {}
        NpcPool* pool_=nullptr;Npc* npc_=nullptr;
    };
    NpcPool()=default;
    ~NpcPool();
    NpcPool(const NpcPool&)=delete;
    NpcPool& operator=(const NpcPool&)=delete;
    // Serialized operations. The pool must outlive all leases; a detached NPC
    // released while pinned is returned to the LIFO free list after its last lease.
    Lease pin(Npc&);
    Npc* allocate();
    void release(Npc&);
private:
    void unpin(Npc&) noexcept;
    std::vector<std::unique_ptr<Npc>> objects_;
    std::vector<Npc*> available_,deferred_;
    std::map<Npc*,std::size_t> pins_;
};
class NpcBroadcast {
public:
    virtual ~NpcBroadcast() = default;
    virtual void send_cell(CellCoordinates) = 0;
};
class NpcViewer {
public:
    virtual ~NpcViewer()=default;
    virtual void send_self(const net::Packet&)=0;
};
class ZoneNpcServices {
public:
    virtual ~ZoneNpcServices() = default;
    virtual Npc* allocate() = 0;
    virtual std::uint32_t next_id() = 0;
    virtual std::uint32_t tick_ms() const noexcept = 0;
    virtual void invalid_guard_position(const MapNpc&) = 0;
    virtual void send_cell_view(CellCoordinates,const net::Packet&) = 0;
    // A null buffer drops that broadcast. Priority 6 follows PSendAll with both
    // forced immediate recipient IDs zero and timeout 65000; nullopt follows SendAll.
    virtual std::unique_ptr<NpcBroadcast> allocate_broadcast(const net::Packet&,std::optional<std::uint8_t> priority) = 0;
};
net::Packet npc_appearance(const Npc&);
// One world executor serializes operations. Maps/definitions, services and all
// allocated NPCs must outlive the registry. Patrol ticks/guard AI remain separate.
class ZoneNpcs:public NpcNavigationWorld {
public:
    ZoneNpcs(const MapGeometry&,ZoneNpcServices&);
    ~ZoneNpcs();
    ZoneNpcs(const ZoneNpcs&)=delete;
    ZoneNpcs& operator=(const ZoneNpcs&)=delete;
    void initialize(const MapDefinition&,const NpcDefinitions&);
    bool enter(Npc&);
    bool leave(Npc&);
    bool move(Npc&,UserPosition destination,std::uint8_t mode) override;
    std::uint32_t tick_ms() const noexcept override {return services_.tick_ms();}
    Npc* find(std::uint32_t) const noexcept;
    std::span<Npc* const> all() const noexcept {return members_;}
    // CZone::UpdateNpcIdle 00422400 uses the zone's shared global cursor.
    // The serialized caller retains this zone; pool pins the current NPC across
    // callbacks, including detach/release. Nested visits share the cursor.
    void visit_all(NpcPool&,const std::function<void(Npc&)>&);
    std::span<Npc* const> in_cell(CellCoordinates) const noexcept;
    // Caller serializes the zone and retains each current NPC through callbacks.
    // Nested snapshot traversal shares the original cell cursor.
    void send_enter(CellCoordinates,NpcViewer&);
    void send_leave(CellCoordinates,NpcViewer&);
private:
    std::size_t key(CellCoordinates) const;
    void announce_transition(const Npc&,CellCoordinates previous,CellCoordinates next);
    void link_cell(Npc&,CellCoordinates);
    void unlink_cell(Npc&,CellCoordinates);
    void send_cell_snapshot(CellCoordinates,NpcViewer&,bool);
    struct Cell {std::vector<Npc*> members;std::map<std::uint64_t,Npc*> ordered;std::uint64_t cursor=0;};
    MapGeometry geometry_;ZoneNpcServices& services_;
    std::vector<Npc*> members_;Npc* global_cursor_=nullptr;
    std::map<std::uint32_t,Npc*> index_;
    std::map<std::size_t,Cell> cells_;
    std::map<Npc*,std::uint64_t> cell_ordinals_;
    std::uint64_t cell_sequence_=0;
};
}
