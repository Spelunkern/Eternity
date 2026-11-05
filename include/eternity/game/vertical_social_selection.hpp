#pragma once
#include "eternity/game/vertical_social_replica.hpp"

namespace eternity::game {
struct VerticalSocialSelection {SocialCatalog catalog;bool friends_loaded=false,blocks_loaded=false;};
// DB social slots belong to the account owner and survive character-selection
// cycles. A missing/failed catalog leaves its retained slots unchanged, matching
// CUser::LoadCharacter's ignored DBFriend failures. A malformed row can leave
// earlier rows updated, as in the original; the caller logs the false result.
inline VerticalSocialSelection vertical_load_social_selection(const PlayerSnapshot& snapshot,VerticalSocialReplica& retained) {
    if(snapshot.core.user_id!=retained.account_id())throw std::invalid_argument("Social selection account mismatch");
    bool friends=false,blocks=false;
    if(const auto it=snapshot.catalogs.find("Friends");it!=snapshot.catalogs.end() && it->second.columns.size()==7)
        friends=retained.load_friends(it->second.rows);
    if(const auto it=snapshot.catalogs.find("Blocked");it!=snapshot.catalogs.end() && it->second.columns.size()==3)
        blocks=retained.load_blocked(it->second.rows);
    return {retained.game_catalog(snapshot.core.character.id),friends,blocks};
}
}
