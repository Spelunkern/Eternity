#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/quest_requests.hpp"
#include "eternity/game/quest_requests_completion.hpp"
#include "eternity/game/quest_requests_progress.hpp"
#include "eternity/game/quest_requests_timers.hpp"
#include "eternity/game/quest_runtime.hpp"
#include "eternity/game/quest_rewards.hpp"

namespace eternity::game {
class QuestRequestsActorTransport {
public:
    virtual ~QuestRequestsActorTransport()=default;
    virtual void send_database(Actor&,const net::Packet&)=0;
};
// Operation-scoped borrowed projection, not an inventory owner. Actor/pools/item
// definitions and the affected allocations must survive serialized callbacks;
// callbacks may reenter quest lookup, but must not mutate searched bag contents
// or their unlocked-bag layout until consumption returns.
class QuestRequestsActorInventory final:public QuestRequestItems {
public:
    QuestRequestsActorInventory(Actor& actor,QuestRequestsActorTransport& transport):actor_(actor),transport_(transport) {}
    QuestRequestInventory refresh();
    bool find_item(int type,int id,bool consume);
    bool find_items(RestoredQuest&,bool consume);
    void send_database(const net::Packet&) override;
    void send_self(const net::Packet&) override;
    void release_item(std::uint8_t bag,std::uint8_t slot) override;
private:
    struct Borrow {GroundItem* item=nullptr;GroundItemPool* pool=nullptr;std::uint64_t uid=0;const ItemDefinition* definition=nullptr;};
    GroundItem& validate(std::uint8_t bag,std::uint8_t slot);
    void validate_snapshot();
    Actor& actor_;QuestRequestsActorTransport& transport_;bool callback_=false;
    std::vector<std::array<QuestRequestItem,24>> projection_;
    std::vector<std::array<Borrow,24>> borrowed_;
};
class QuestRequestsActorPorts:public QuestRequestsActorTransport {
public:
    virtual void lock_quests(Actor&)=0;
    virtual void unlock_quests(Actor&) noexcept=0;
    virtual std::uint32_t tick_ms()=0;
    virtual std::optional<QuestRequestNpc> find_npc(Actor&,std::uint32_t)=0;
    virtual void fill_game_log_main(Actor&,std::span<std::uint8_t>)=0;
    virtual void send_game_log(Actor&,const net::Packet&)=0;
    virtual QuestRewardWorld& reward_world(Actor&)=0;
};
// Concrete shared owner for requests, PC/item progress, expiration and core
// completion. No substitute inventory, quest pool, rewards, scene or transport.
// Caller retains Actor/runtime/definitions/ports across every nested callback.
class QuestRequestsActorWorld final:public QuestRequestWorld,public QuestRequestsProgressWorld,public QuestRequestsTimerWorld,public QuestCompletionWorld,public QuestRuntimeWorld {
public:
    QuestRequestsActorWorld(Actor& actor,QuestRuntime& runtime,const QuestDefinitions& definitions,QuestRequestsActorPorts& ports):actor_(actor),runtime_(runtime),definitions_(definitions),ports_(ports),items_(actor,ports) {}
    bool find_item(int type,int id,bool consume) {return items_.find_item(type,id,consume);}
    const std::array<std::uint8_t,464>* definition(std::uint16_t) override;
    RestoredQuest* find_quest(std::uint16_t) override;
    RestoredQuest* find_active(std::uint16_t id) override {return find_quest(id);}
    std::optional<QuestRequestNpc> find_npc(std::uint32_t id) override {return ports_.find_npc(actor_,id);}
    QuestRequestInventory inventory() override {return items_.refresh();}
    void send_database(const net::Packet&) override;
    void send_self(const net::Packet&) override;
    void release_item(std::uint8_t bag,std::uint8_t slot) override {items_.release_item(bag,slot);}
    void lock_active_quests() override {ports_.lock_quests(actor_);}
    void unlock_active_quests() noexcept override {ports_.unlock_quests(actor_);}
    RestoredQuest* first_active_quest() override {return runtime_.first_active();}
    RestoredQuest* next_active_quest() override {return runtime_.next_active();}
    std::uint32_t tick_ms() override {return ports_.tick_ms();}
    bool find_required_items(RestoredQuest& q,bool consume) override {return items_.find_items(q,consume);}
    void complete_quest(RestoredQuest& q,std::uint32_t npc,bool success) override {quest_requests_complete(q,npc,success,*this);}
    void end_quest(RestoredQuest& q,std::uint32_t npc,bool success) override {complete_quest(q,npc,success);}
    void remove_quest(RestoredQuest&,bool success) override;
    void fill_game_log_main(std::span<std::uint8_t> p) override {ports_.fill_game_log_main(actor_,p);}
    void send_game_log(const net::Packet& p) override {ports_.send_game_log(actor_,p);}
    void award(RestoredQuest&,std::uint8_t variant,std::uint32_t context) override;
private:
    Actor& actor_;QuestRuntime& runtime_;const QuestDefinitions& definitions_;QuestRequestsActorPorts& ports_;QuestRequestsActorInventory items_;
};
}
