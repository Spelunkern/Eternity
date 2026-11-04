#pragma once
#include "eternity/game/mob_drops_creation.hpp"
#include "eternity/game/mob_drops_range.hpp"
#include "eternity/game/inventory.hpp"

namespace eternity::game {
struct MobDropDeliveryContext {
    MobDropCreationContext creation;
    std::uint32_t zone_identity=0;
};
struct MobDropInventoryTarget {Actor& actor;InventoryWorld& world;};
class MobDropDeliveryServices:public MobDropCreationWorld {
public:
    // Retained canonical definitions and Actor identities, not reconstructed
    // copies. Providers must keep subjects alive through nested delivery/logs.
    virtual const ItemDefinition& definition(const MobLootItem&)=0;
    virtual MobDropInventoryTarget recipient(MobLootUser&,GroundItem&)=0;
    // Item is already make='V'. Serialize/send original 0a06 using log owner.
    virtual void sequence_log(GroundItem&,MobLootUser*)=0;
    virtual void log_item(GroundItem&,MobLootUser*)=0;
};
enum class MobDropDestination {none,allocation_failed,inventory,ground,unlinked};
struct MobDropDeliveryResult {
    MobDropDestination destination=MobDropDestination::none;
    std::optional<InventoryReceipt> receipt;
    // Only ground/unlinked results expose a retained allocation. After inventory
    // admission storage can belong to the Actor/world or already be recycled.
    // Inventory destination means delegated to Actor; inspect receipt for returns.
    GroundItem* item=nullptr;
};
// Concrete composition of ps_game.exe CMob::ItemDrop 004a65a0: actual shared
// pool, CItem init/craft, zone/range, Actor::receive_item and ZoneItems::enter.
// Use on the serialized zone/party owner. Context/user/party projections remain
// live; required clock/UID/identity/log/transport providers have no defaults.
MobDropDeliveryResult deliver_mob_drop(const MobLootItem*,MobLootUser*,MobLootParty*,
    bool sequence,bool logged,const MobDropDeliveryContext&,GroundItemPool&,ZoneItems&,
    RandomSource&,MobDropDeliveryServices&);
// Actual ground-money creation/publication for MobLootWorld::ground_money.
MobDropDeliveryResult place_mob_drop_money(const MobDropCreationContext&,std::uint32_t owner,
    std::uint32_t amount,GroundItemPool&,ZoneItems&,RandomSource&);
}
