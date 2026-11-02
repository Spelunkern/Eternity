#pragma once
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/entry_catalogs.hpp"
#include <vector>

namespace eternity::game {
struct GuildMetadata {
    GuildListing listing;
    std::uint32_t etin=0,keep_etin=0,return_count=0,officers=0;
    std::uint8_t use_house=0,buy_house=0;
};
struct LoadedGuild {
    GuildMetadata metadata;
    GuildMembership membership;
    LoadedGuild(GuildMetadata value,GuildMemberPorts port):metadata(std::move(value)),membership(metadata.listing.id,std::move(port)) {}
};
struct GuildCatalogPorts {
    GuildMemberPorts transport;
    std::function<bool()> allocate_guild,allocate_member;
};
// Game-side 0b01/0b02/0b03 load owners, ps_game.exe PacketGuild 00408e00,
// InitAddGuild 00431870 and InitAddUser 0042cf70. One world executor owns this
// catalog; all live users must leave before it is destroyed. Bank/NPC packets
// are handled separately, not accepted as completed guild initialization here.
class GuildCatalog {
public:
    explicit GuildCatalog(GuildCatalogPorts);
    EntryCatalogUpdate apply(std::span<const std::uint8_t>);
    LoadedGuild* find(std::uint32_t) const;
    LoadedGuild* find(std::string_view name) const;
    GuildDirectory& directory() noexcept {return directory_;}
    std::uint32_t maximum_id() const noexcept {return maximum_;}
    std::map<std::uint32_t,GuildListing> listings() const;
private:
    static std::string name_key(std::string_view);
    GuildCatalogPorts ports_;GuildDirectory directory_;std::uint32_t maximum_=0;
    std::vector<std::unique_ptr<LoadedGuild>> retained_;
    std::vector<GuildMembership::Record> retained_members_;
    std::map<std::uint32_t,LoadedGuild*> ids_;
    std::map<std::string,LoadedGuild*> names_;
};
}
