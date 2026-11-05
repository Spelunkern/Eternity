#pragma once
#include "eternity/game/character_replica.hpp"
#include "eternity/game/entry_catalogs.hpp"
#include <stdexcept>

namespace eternity::game {
// Replays the reconstructed DB-agent 0d01/0e01/0e04 packets into the game-side
// entry cache. Caller serializes the replica for this entire synchronous read.
// Social and guild catalogs have independent owners and are not synthesized.
inline EntryCatalogs vertical_restore_entry_catalogs(const CharacterReplica& replica) {
    EntryCatalogs result(replica.core().user_id);
    const auto apply=[&](const net::Packet& packet) {
        if(result.apply_db(packet)!=EntryCatalogUpdate::applied)throw std::logic_error("Entry catalog rejected its DB replica packet");
    };
    apply(replica.interface().save_points_packet());apply(replica.purchases().packet());
    if(const auto stored=replica.points().stored_packet())apply(*stored);
    return result;
}
}
