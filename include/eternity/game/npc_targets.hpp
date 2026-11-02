#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/zone_npcs.hpp"

namespace eternity::game {
// The mob runtime must supply live FSM state, identity, position and its own
// shared guard-target deadline. A snapshot cannot replace the two status reads.
class NpcMobTarget {
public:
    virtual ~NpcMobTarget() = default;
    virtual std::uint32_t id() const noexcept = 0;
    virtual UserPosition position() const noexcept = 0;
    virtual std::int32_t country() const noexcept = 0;
    virtual std::int32_t status() const noexcept = 0;
    virtual std::uint32_t& guard_target_after() noexcept = 0;
};
class NpcMobCells {
public:
    virtual ~NpcMobCells() = default;
    // Visit in cell insertion order; return true only when the visitor stops.
    virtual bool visit_cell(CellCoordinates,const std::function<bool(NpcMobTarget&)>&) = 0;
};
void set_npc_target(Npc&,Actor&,const NpcClock&);
void set_npc_target(Npc&,NpcMobTarget&,const NpcClock&);
// CNpc::FindTarget 004bc180. Actors come directly from the native user grid;
// ordered mob cells and the clock are required services of the world executor.
bool find_npc_target(Npc&,const MapDefinition&,ZoneUsers&,NpcMobCells&,const NpcClock&);
}
