#pragma once
#include "eternity/game/quests.hpp"
#include "eternity/game/mobs.hpp"

namespace eternity::game {
class MobQuestLinks {
public:
    explicit MobQuestLinks(std::vector<std::vector<std::uint16_t>> records={}):records_(std::move(records)) {}
    // CMobQuestData::Init 004c9ed0. Count is original MobMaxID_R + 1.
    static MobQuestLinks build(const QuestDefinitions&,std::int32_t mob_count,const MobRuntimeTemplates&);
    const std::vector<std::uint16_t>* find(std::uint16_t mob) const noexcept;
    std::span<const std::vector<std::uint16_t>> records() const noexcept {return records_;}
private:
    std::vector<std::vector<std::uint16_t>> records_;
};
class MobQuestWorld {
public:
    virtual ~MobQuestWorld()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual RestoredQuest* find_quest(std::uint16_t)=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void send_self(const net::Packet&)=0;
    // QuestEnd receives selection 0, success=true. It may unlink the quest,
    // but its object/definition storage must survive the ongoing update call.
    virtual void complete_quest(RestoredQuest&,std::int32_t selection,bool success)=0;
};
// CUser::QuestUpdateKillMOB 00480140. Retains link order and duplicate targets,
// and does not substitute the required QuestEnd/reward provider.
void update_mob_kill_quests(std::uint32_t user_id,std::int32_t have_mob_quests,std::uint16_t mob,const MobQuestLinks&,MobQuestWorld&);
}
