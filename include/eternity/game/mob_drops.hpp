#pragma once
#include "eternity/game/random.hpp"
#include "eternity/storage/sql_server.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace eternity::game {
struct MobDropSlot {std::uint16_t grade=0;std::uint32_t rate=0;};
using MobDropDefinition=std::array<MobDropSlot,10>;
// Projection of usp_Read_MobItems_R: MobID (smallint), ItemOrder (tinyint),
// Grade (smallint), DropRate (int). Preserve the SQL bit patterns.
struct MobDropRow {std::uint16_t mob=0;std::uint8_t order=0;std::uint16_t grade=0;std::uint32_t rate=0;};
struct MobDropDefinitions {
    std::vector<MobDropDefinition> mobs;
    std::vector<std::size_t> rejected_rows;
    static MobDropDefinitions load(std::size_t mob_count,std::span<const MobDropRow>);
    // Uses the four original positional result columns, not renamed aliases.
    static MobDropDefinitions load_sql(std::size_t mob_count,const storage::ResultSet&);
};
class MobDropProvider {
public:
    virtual ~MobDropProvider()=default;
    // Required CItemDrop::GetItem provider (catalog selection and global limits).
    // Zero means no definition was selected; no retry occurs for that slot.
    virtual std::uint32_t select_item(std::uint16_t grade)=0;
    // Required CMob::ItemDrop 004a65a0 delivery provider. The coordinator passes
    // count=1, sequence=false; user/party/mob context belongs to this provider.
    virtual void deliver_item(std::uint32_t item,std::uint8_t count,bool sequence)=0;
};
// ps_game.exe CMob::ItemDrop 004a5ea0, regular MobItems loop 004a60be..004a614b.
// Slot zero is loaded but never visited by this loop. Quest, sequence and money
// branches are separate and are not supplied by this function.
void mob_drop_slots(const MobDropDefinition&,RandomSource&,ServerRandom&,MobDropProvider&);

struct MobDropLimit {
    std::uint16_t maximum=0;
    std::int32_t count=0;
    std::uint32_t next_tick=0,delay=0,random_delay=1;
};
// CItemDrop::GetItem 004bdd10, after selection and GetTickCount, under the
// catalog lock. The caller must hold that lock. maximum==0 bypasses all limits.
bool mob_drop_limit(MobDropLimit&,std::uint32_t tick,RandomSource&);
}

namespace eternity::game {
struct MobLootItem {
    std::uint32_t id=0;std::uint8_t type=0,type_id=0;
    bool quest_start=false;std::uint16_t quality=0,dex=0;
    std::int32_t real_type=0;
};
struct MobLootUser {
    std::uint32_t id=0,map=0;std::uint16_t instance=0;
    std::int32_t dead=0,state=4,money_percent=0,money_mode=0;
    float x=0,z=0;
};
struct MobLootParty {
    std::uint32_t id=0;std::int32_t mode=0,leader=0,cursor=0;
    std::uint8_t type=0;
    std::vector<MobLootUser*> members;
};
struct MobLootContext {
    std::uint16_t mob_id=0,level=0,money_base=0,money_random=0;
    std::uint32_t object_id=0;std::int32_t zone_type=0;
    std::uint8_t quest_type=0,quest_type_id=0;
    bool logged=false;
    MobDropDefinition slots{};
    // Borrowed original definition storage; reload writes remain visible between
    // drop callbacks. Leave null only for explicit standalone projections.
    const MobDropDefinition* live_slots=nullptr;
};
// Required providers for the unreconstructed world/item/guild owners. There are
// deliberately no default success/no-op implementations.
class MobLootWorld {
public:
    virtual ~MobLootWorld()=default;
    virtual const MobLootItem* item(std::uint8_t type,std::uint8_t type_id)=0;
    virtual std::span<const std::uint16_t> end_quests(const MobLootItem&)=0;
    virtual bool has_quest(MobLootUser&,std::uint16_t)=0;
    virtual const MobLootItem* grade_item(std::uint16_t)=0;
    virtual const MobLootItem* sequence_item(std::uint16_t level,std::uint32_t object_id)=0;
    virtual void deliver(const MobLootItem&,MobLootUser*,MobLootParty*,bool sequence)=0;
    // Ranking owner must retain/lock/revalidate guild membership as in
    // 004a6182..004a61e8, then call CGuild::SendRankingZone 0042c6e0.
    virtual void guild_ranking(MobLootUser&,std::uint16_t points)=0;
    virtual void log_money(std::int32_t)=0;
    virtual void ground_money(std::uint32_t owner,std::int32_t amount)=0;
    virtual void give_money(MobLootUser&,std::int32_t)=0;
    virtual bool zone_item(MobLootUser& credited,MobLootUser* candidate)=0;
};
// Complete selection/quest/sequence/money coordinator, CMob::ItemDrop 004a5ea0.
// All projections must remain alive through callbacks; caller owns party locking.
void mob_loot(const MobLootContext&,MobLootUser*,MobLootParty*,RandomSource&,ServerRandom&,MobLootWorld&);

struct MobLootObject {
    const MobLootItem* definition=nullptr;
    std::uint32_t owner=0,party=0;
};
class MobLootDeliveryWorld {
public:
    virtual ~MobLootDeliveryWorld()=default;
    // Allocate, CItem::Init(count=1,make='D'), unique ID + server ID,
    // GetDropPosRand, drop type=1/mob ID, then CreateCraftExpansion.
    virtual MobLootObject* create(const MobLootItem&,std::uint32_t owner,std::uint32_t party)=0;
    // mark make='V' and emit exact 0x0a06 127-byte sequence log.
    virtual void sequence_log(MobLootObject&,MobLootUser*)=0;
    virtual bool zone_item(MobLootUser&,MobLootUser*)=0;
    virtual bool range_item(MobLootUser&,MobLootUser* credited,std::int32_t radius)=0;
    virtual void log_item(MobLootObject&,MobLootUser*)=0;
    virtual void give_item(MobLootUser&,MobLootObject&)=0;
    virtual void enter_item(MobLootObject&,std::uint8_t priority)=0;
};
// CMob::ItemDrop 004a65a0: ownership and actual party distribution policy.
// Allocation/initialization, persistence, transport and logs remain required ports.
void mob_deliver_item(const MobLootItem*,MobLootUser*,MobLootParty*,bool sequence,bool logged,RandomSource&,MobLootDeliveryWorld&);
}

namespace eternity::game {
struct MobDropCatalogEntry {MobLootItem item;MobDropLimit limit;};
struct MobDropSequence {std::uint32_t divisor=0;std::array<std::uint32_t,10> grades{};};
class MobDropCatalogWorld {
public:
    virtual ~MobDropCatalogWorld()=default;
    virtual std::uint32_t tick()=0;
    virtual void lock()=0;
    virtual void unlock() noexcept=0;
};
// Catalog entries are shared across grades; limits belong to the item, not the
// mob or grade. Rebuild/reset cadence belongs to CItemDrop's external owner.
class MobDropCatalog {
public:
    std::array<std::vector<MobDropCatalogEntry*>,1025> grades;
    std::array<std::size_t,1025> unrestricted_counts{};
    std::vector<MobDropSequence> sequences;
    const MobLootItem* select(std::uint32_t grade,RandomSource&,MobDropCatalogWorld&);
    const MobLootItem* sequence(std::uint16_t level,std::uint32_t object,RandomSource&,MobDropCatalogWorld&);
};
}

namespace eternity::game {
struct MobLootPosition {float x=0,y=0,z=0;};
// CMob::GetDropPosRand 00490c70: y is zero, independent of the mob's y.
MobLootPosition mob_drop_position(float x,float z,RandomSource&);
}
