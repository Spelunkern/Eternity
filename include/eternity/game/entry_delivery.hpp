#pragma once
#include "eternity/game/actor_load.hpp"
#include "eternity/game/character_entry.hpp"
#include "eternity/game/entry_catalogs.hpp"
#include "eternity/game/player_packets.hpp"

namespace eternity::game {
struct EntryKillCount {std::uint32_t points=0,expires=0;};
struct EntryDeliveryPorts {
    std::function<void(net::Packet)> send_self;
    std::function<void(std::uint32_t,net::Packet)> send_peer;
    SocialCatalog::Lookup social;
    std::function<std::uint32_t()> tick;
    std::function<EntryKillCount(std::uint8_t country)> kill_count;
};
// Binds the actual loaded actor/restored catalogs to the 0407 owner's delivery
// steps. Snapshot, actor/pipeline result, catalogs and ports outlive this adapter.
class EntryDelivery {
public:
    EntryDelivery(const PlayerSnapshot&,const LoadedActor&,const SocialCatalog&,const EntryCatalogs&,ExperienceBounds,std::string guild_name,EntryDeliveryPorts);
    // True only for implemented delivery/InitFriend steps. Other lifecycle/guild
    // services must be supplied by the caller, rather than treated as empty work.
    bool send(CharacterEntryStep) const;
private:
    const PlayerSnapshot& player_;const LoadedActor& loaded_;const SocialCatalog& social_;const EntryCatalogs& catalogs_;
    ExperienceBounds experience_;std::string guild_;EntryDeliveryPorts ports_;
};
}
