#pragma once
#include "eternity/game/quest_start_items_actor.hpp"
#include "eternity/game/quest_start.hpp"
#include "eternity/game/quest_start_log.hpp"
namespace eternity::game {
// Bind these references to the real retained user/item-use owner (+57d4/+57d0).
// The Actor aliasing user lease must retain that owner through every callback.
struct QuestStartItemsActorUseState {const std::uint8_t& item;const std::uint32_t& deadline;};
class QuestStartItemsActorWorldPorts:public QuestStartItemsActorPorts {
public:
    virtual std::optional<QuestStartNpc> find_start_npc(Actor&,std::uint32_t handle)=0;
    virtual bool require_start_item(Actor&,std::uint8_t type,std::uint8_t id,bool consume)=0;
    virtual QuestStartLogContext start_log_context(Actor&)=0;
    virtual QuestStartLogNpc start_log_npc(Actor&,const QuestStartNpc&)=0;
    virtual void send_start_log(Actor&,const std::array<std::uint8_t,144>&)=0;
};
struct QuestStartItemsActorWorldOwners {
    std::shared_ptr<const ItemDefinitions> items;
    std::shared_ptr<GroundItemPool> item_pool;
    std::shared_ptr<ItemIds> ids;
    std::shared_ptr<const QuestDefinitions> quests;
    std::shared_ptr<QuestRuntimePool> quest_pool;
    std::shared_ptr<Actor> actor;
    std::shared_ptr<QuestRuntime> runtime;
};
// Concrete QuestStartWorld: actual lists, admission fields, allocation, grants
// and log serialization; required root NPC/inventory/transport services remain
// explicit. No replacement user, inventory, quest or definition owner is created.
class QuestStartItemsActorWorld final:private QuestStartWorld {
public:
    QuestStartItemsActorWorld(QuestStartItemsActorWorldOwners,QuestStartItemsActorUseState,QuestStartItemsActorWorldPorts&);
    bool request(std::uint16_t positional_row,std::uint32_t target);
private:
    struct Frame {
        std::uint16_t row=0;const QuestDefinition* definition=nullptr;
        QuestStartUser user;std::vector<QuestStartFinished> finished;
        std::array<std::uint8_t,144> log{};
    };
    void refresh_admission();
    std::optional<QuestStartNpc> find_npc(std::uint32_t) override;
    bool active(std::uint16_t) override;
    bool finished(std::uint16_t) override;
    bool find_item(std::uint8_t,std::uint8_t,bool) override;
    std::uint32_t tick_ms() override;
    bool add(const QuestStartRecord&) override;
    void prepare_start_log(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override;
    void send_self(const std::array<std::uint8_t,8>&) override;
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override;
    bool give_item(std::uint8_t,std::uint8_t,std::uint8_t) override;
    QuestStartItemsActorWorldOwners owners_;QuestStartItemsActorUseState use_;QuestStartItemsActorWorldPorts& ports_;
    QuestStartItemsActor grants_;QuestStartItemsActorAdd admission_;Frame* frame_=nullptr;
};
}
