#pragma once
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/equipment.hpp"

namespace eternity::game {
struct GuildBankSavePorts;
// Original DB-agent 240-slot guild bank, including its temporary duplicate list.
// Pool outlives this owner. One DB executor serializes loads and later mutations.
class GuildBankReplica {
public:
    explicit GuildBankReplica(InventoryReplicaPool& pool):pool_(pool) {}
    ~GuildBankReplica();
    GuildBankReplica(const GuildBankReplica&)=delete;
    GuildBankReplica& operator=(const GuildBankReplica&)=delete;
    void begin_load();
    void load(PlayerItem,std::int32_t slot);
    void finish_load(bool success); // Success relocates duplicates; failure deletes them.
    // Caller resolves the guild envelope before dispatching these bank commands.
    InventoryReplicaResult apply(std::span<const std::uint8_t>);
    std::uint32_t save(std::uint32_t guild,GuildBankSavePorts&);
    const std::vector<std::uint64_t>& deleted() const noexcept {return deleted_;}
    std::vector<CachedInventoryItem> snapshot() const;
    std::vector<net::Packet> packets(std::uint32_t guild) const; // SendGuildBank 0040f7d0.
private:
    using Item=std::unique_ptr<CachedInventoryItem>;
    InventoryReplicaPool& pool_;std::array<Item,240> slots_{};std::vector<Item> pending_;std::vector<std::uint64_t> deleted_;bool loading_=false;
};
struct GuildBankServices {
    const ItemDefinitions& definitions;const EnchantValues& enchant;GroundItemPool& pool;
    std::function<void(net::Packet)> send_db;
    std::function<void(std::uint8_t type,std::uint8_t id)> fatal_definition;
    std::function<void(std::uint8_t slot,std::uint8_t before,std::uint8_t after)> corrected_count;
};
enum class GuildBankUpdate {applied,rejected,fatal};
// Game PacketGuild 00408e00 case 0b0d. Shared definitions/enchant/pool/services
// outlive the owner. Overwriting a slot strands the previous source allocation.
class GuildBank {
public:
    explicit GuildBank(GuildBankServices&);
    ~GuildBank();
    GuildBank(const GuildBank&)=delete;GuildBank& operator=(const GuildBank&)=delete;
    GuildBankUpdate apply(std::span<const std::uint8_t>);
    std::vector<net::Packet> client_packets() const; // SendBankItemList 0042f250.
    const GroundItem* find(std::uint8_t slot) const noexcept;
private:
    GuildBankServices& services_;std::array<GroundItem*,240> slots_{};
};
}
