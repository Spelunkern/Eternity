#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/item_ids.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/quest_start_items.hpp"
#include "eternity/game/quest_start_add.hpp"
#include "eternity/game/quest_runtime.hpp"
#include <memory>
namespace eternity::game {
// Root supplies real calendar/clock and connection routing. User execution must
// stay serialized. Actor leases must alias the actual retained user owner.
class QuestStartItemsActorPorts {
public:
    virtual ~QuestStartItemsActorPorts()=default;
    virtual std::uint32_t local_make_time(Actor&)=0;
    virtual std::uint32_t tick_ms(Actor&)=0;
    virtual void send_database(Actor&,const net::Packet&)=0;
    virtual void send_client(Actor&,const net::Packet&)=0;
};
struct QuestStartItemsActorOwners {
    std::shared_ptr<const ItemDefinitions> definitions;
    std::shared_ptr<GroundItemPool> pool;
    std::shared_ptr<ItemIds> ids;
    std::shared_ptr<Actor> actor; // Destroy this lease before catalog/pool leases.
};
// Actual QuestAddItem coordinator over existing Actor retained inventory.
// No pool, UID generator, item catalog or inventory mirror is created here.
// Root retains scene/clock/services/catalog/pool for any Actor/runtime leases
// that outlive this adapter. Invoke grant(), which scopes ownership per call.
class QuestStartItemsActor final:private QuestStartItemsWorld {
public:
    QuestStartItemsActor(QuestStartItemsActorOwners,QuestStartItemsActorPorts&);
    bool grant(std::int32_t type,std::int32_t id,std::int32_t count);
private:
    struct Frame {GroundItem* unclaimed=nullptr;GroundItem* published=nullptr;};
    const ItemDefinition* find_definition(std::int32_t,std::int32_t) override;
    bool slot_empty(std::int32_t,std::int32_t) override;
    GroundItem* create_item() override;
    std::uint32_t local_make_time() override;
    std::uint64_t next_uid() override;
    void store(std::int32_t,std::int32_t,GroundItem&) override;
    void send_database(const net::Packet&) override;
    void send_self(const net::Packet&) override;
    QuestStartItemsActorOwners owners_;QuestStartItemsActorPorts& ports_;Frame* frame_=nullptr;
};
struct QuestStartItemsActorAddOwners {
    std::shared_ptr<const QuestDefinitions> definitions;
    std::shared_ptr<QuestRuntimePool> pool;
    std::shared_ptr<Actor> actor;
    std::shared_ptr<QuestRuntime> runtime; // Pool/definitions must outlive all runtime leases.
};
// Bind request ROW, not embedded definition ID, to the existing quest owner.
// add_active_unaccounted appends; quest_start_add updates live counters once.
class QuestStartItemsActorAdd final:private QuestStartAddWorld {
public:
    QuestStartItemsActorAdd(QuestStartItemsActorAddOwners,QuestStartItemsActorPorts&);
    bool add(std::uint16_t positional_row);
private:
    struct Frame {RestoredQuest* unclaimed=nullptr;};
    RestoredQuest* allocate() override;
    std::uint32_t tick_ms() override;
    void insert_active(RestoredQuest&) override;
    void send_database(const std::array<std::uint8_t,10>&) override;
    QuestStartItemsActorAddOwners owners_;QuestStartItemsActorPorts& ports_;Frame* frame_=nullptr;
};
}
