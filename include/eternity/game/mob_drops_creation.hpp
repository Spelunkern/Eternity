#pragma once
#include "eternity/game/mob_drops.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/equipment.hpp"

namespace eternity::game {
// ps_game.exe CItem::CreateCraftExpansion 004ba2e0, reached by mob drops.
// Unlike CItem::Init, unselected addition WORDs are deliberately retained.
void create_mob_drop_craft(GroundItem&,RandomSource&);
struct MobDropCreationContext {
    std::uint16_t mob_id=0;std::uint8_t server_id=0;float x=0,z=0;
};
class MobDropCreationWorld {
public:
    virtual ~MobDropCreationWorld()=default;
    virtual std::uint32_t make_time()=0;
    // Shared original SUniqueIDGen::GenID value BEFORE adding server ID.
    // Canonical ItemIds timing/wrap compatibility is a coordinator dependency;
    // do not substitute per-drop generators or add server twice.
    virtual std::uint64_t raw_unique_id()=0;
};
// CMob::ItemDrop 004a662a..004a66cc. Uses the real shared GroundItemPool and
// CItem initialization; returns an unlinked item for party delivery/world entry.
GroundItem* create_mob_drop_item(const MobDropCreationContext&,const ItemDefinition&,
    std::uint32_t owner,std::uint32_t party,GroundItemPool&,RandomSource&,MobDropCreationWorld&);
// Money branch 004a645b..004a64cc. No make-time/UID calls; retained UID/money
// pool metadata follow original CItem reset semantics. Caller enters appearance 2.
GroundItem* create_mob_drop_money(const MobDropCreationContext&,std::uint32_t owner,
    std::uint32_t amount,GroundItemPool&,RandomSource&);
}
