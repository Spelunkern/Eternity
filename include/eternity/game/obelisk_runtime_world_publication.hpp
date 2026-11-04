#pragma once
#include "eternity/game/obelisk_runtime_portal.hpp"
#include "eternity/game/obelisk_runtime_publication.hpp"
namespace eternity::game {
// CWorld::SendDBObeliskList 00413b90. One required DB port for all phases:
// initial portal rows first, then ordinary records in map-table order.
// Counts/backing storage stay stable; row values remain live across callbacks.
void publish_world_obelisk_database_lists(std::span<const ObeliskRuntimePortalLink>,
    std::span<const std::span<const ObeliskRuntime>> maps,ObeliskRuntimePublication&);
}
