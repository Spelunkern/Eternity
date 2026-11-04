#pragma once
#include "eternity/game/quest_start_items_actor_world.hpp"
#include "eternity/game/quest_requests_actor.hpp"
#include "eternity/game/mob_quests.hpp"
#include "eternity/game/zone_npcs.hpp"

namespace eternity::game {
// Retention gate over the existing zone and NPC pool. All retirement of NPCs
// in this zone must pass through retire(), including reentrant callbacks.
// Pool, zone and their services must outlive this guard and all leases.
class VerticalQuestNpcGuard final : public std::enable_shared_from_this<VerticalQuestNpcGuard> {
public:
    VerticalQuestNpcGuard(ZoneNpcs& zone,NpcPool& pool):zone_(zone),pool_(pool) {}
    std::shared_ptr<Npc> acquire(std::uint32_t id);
    bool retire(Npc& npc);
    bool owns(const ZoneNpcs& zone) const noexcept {return &zone_==&zone;}
private:
    struct Entry {std::size_t leases=0;bool retired=false;};
    void unpin(Npc&) noexcept;
    ZoneNpcs& zone_;NpcPool& pool_;std::map<Npc*,Entry> entries_;
};
class VerticalQuestPorts : public QuestStartItemsActorPorts, public QuestRequestsActorTransport {
public:
    virtual void send_database(Actor&,const net::Packet&) override=0;
    virtual void lock_quests(Actor&)=0;
    virtual void unlock_quests(Actor&) noexcept=0;
    virtual std::uint32_t quest_tick_ms()=0;
    virtual void fill_game_log_main(Actor&,std::span<std::uint8_t>)=0;
    virtual void send_game_log(Actor&,const net::Packet&)=0;
    virtual QuestRewardWorld& reward_world(Actor&)=0;
    virtual QuestStartLogContext start_log_context(Actor&)=0;
    virtual void send_start_log(Actor&,const std::array<std::uint8_t,144>&)=0;
};
// All owners, including use-state, zone and ports, refer to the same live user.
// Serialize calls; route all zone NPC retirement through the same guard.
// The reward provider must remove through this same QuestRuntime. The actor
// lease must also retain its use-state; definitions, pool and services must
// outlive all runtime and item leases.
// ps_game.exe: QuestStartRequest 0047f220, QuestUpdateKillMOB 00480140,
// QuestEndRequest 0047f2a0, QuestEnd 0047ec40, QuestEndSelect 0047ebe0.
class VerticalQuests final : private QuestStartItemsActorWorldPorts, private QuestRequestsActorPorts, private MobQuestWorld {
public:
    VerticalQuests(QuestStartItemsActorWorldOwners,QuestStartItemsActorUseState,ZoneNpcs&,std::shared_ptr<VerticalQuestNpcGuard>,VerticalQuestPorts&);
    bool start(std::uint16_t row,std::uint32_t npc);
    void end(std::uint16_t row,std::uint32_t npc);
    void select_reward(std::uint16_t quest,std::uint8_t selection);
    void kill_mob(std::uint16_t mob,const MobQuestLinks&);
    void kill_player();
    void pick_item(std::int32_t type,std::int32_t id,const ItemQuestLinks&);
    void update_timers(std::uint32_t tick);
private:
    std::optional<QuestStartNpc> find_start_npc(Actor&,std::uint32_t) override;
    bool require_start_item(Actor&,std::uint8_t,std::uint8_t,bool) override;
    QuestStartLogContext start_log_context(Actor&) override;
    QuestStartLogNpc start_log_npc(Actor&,const QuestStartNpc&) override;
    void send_start_log(Actor&,const std::array<std::uint8_t,144>&) override;
    std::uint32_t local_make_time(Actor&) override;
    std::uint32_t tick_ms(Actor&) override;
    void send_database(Actor&,const net::Packet&) override;
    void send_client(Actor&,const net::Packet&) override;
    void lock_quests(Actor&) override;
    void unlock_quests(Actor&) noexcept override;
    std::optional<QuestRequestNpc> find_npc(Actor&,std::uint32_t) override;
    void fill_game_log_main(Actor&,std::span<std::uint8_t>) override;
    void send_game_log(Actor&,const net::Packet&) override;
    QuestRewardWorld& reward_world(Actor&) override;
    std::uint32_t tick_ms() override;
    RestoredQuest* find_quest(std::uint16_t) override;
    void send_database(const net::Packet&) override;
    void send_self(const net::Packet&) override;
    void complete_quest(RestoredQuest&,std::int32_t,bool) override;
    QuestStartItemsActorWorldOwners owners_; ZoneNpcs& npcs_; std::shared_ptr<VerticalQuestNpcGuard> npc_guard_; VerticalQuestPorts& ports_;
    QuestRequestsActorWorld requests_; QuestStartItemsActorWorld starts_;
    struct StartFrame {std::shared_ptr<Npc> npc;std::string name;}; StartFrame* frame_=nullptr;
};
}
