#pragma once
#include "eternity/game/vertical_save_core.hpp"
#include "eternity/game/user_skill_runtime_persist.hpp"
#include "eternity/game/quest_runtime_save.hpp"
#include "eternity/game/user_quick_slots.hpp"

namespace eternity::game {
class VerticalSaveCoreSkillServices:public VerticalSaveCoreServices,public UserSkillPersistWorld {
public:
    // One ordered transport implements both DB-agent phase interfaces.
    virtual void send_database(const net::Packet&) override=0;
    virtual void lock_effects()=0;
    virtual void unlock_effects()=0;
    virtual void lock_quests()=0;
    virtual void unlock_quests()=0;
};
// First phases of ps_game.exe SendDBAgentCharGetInfo 0046c980..0046cb63.
// The returned tick is required by the later effect and quest phases. Their
// ownership, ordering and persistence must be supplied before sending 0302.
inline std::uint32_t vertical_save_core_and_skills(Actor& actor,const ZoneEntryState& entry,
    UserSkillRuntime& skills,VerticalSaveCoreSkillServices& services,std::span<std::uint8_t,775> scratch) {
    if(&skills.actor()!=&actor)throw std::invalid_argument("Save skill runtime belongs to another actor");
    vertical_save_core(actor,entry,services);
    return user_skill_runtime_persist(skills,services,scratch);
}
// Also emits 0904 for the canonical active-effect list using the same tick.
// Quest and any subsequent phases remain required before the exit request.
inline std::uint32_t vertical_save_core_skills_effects(Actor& actor,const ZoneEntryState& entry,
    UserSkillRuntime& skills,VerticalSaveCoreSkillServices& services,std::span<std::uint8_t,775> scratch) {
    const auto tick=vertical_save_core_and_skills(actor,entry,skills,services,scratch);
    actor.effects().save_database(actor.account_id(),tick,{[&]{services.lock_effects();},[&]{services.unlock_effects();},[&](const net::Packet& packet){services.send_database(packet);}});
    return tick;
}
// Adds the active-quest phase with the same captured tick. Further original
// phases and the exit request remain the owning session's responsibility.
inline std::uint32_t vertical_save_core_skills_effects_quests(Actor& actor,const ZoneEntryState& entry,
    UserSkillRuntime& skills,QuestRuntime& quests,VerticalSaveCoreSkillServices& services,std::span<std::uint8_t,775> scratch) {
    const auto tick=vertical_save_core_skills_effects(actor,entry,skills,services,scratch);
    quest_runtime_save(quests,actor.account_id(),tick,{[&]{services.lock_quests();},[&]{services.unlock_quests();},[&](const net::Packet& packet){services.send_database(packet);}});
    return tick;
}

// Final 0609 phase after quest persistence, before the DB exit request.
inline std::uint32_t vertical_save_character(Actor& actor,const ZoneEntryState& entry,UserSkillRuntime& skills,
    QuestRuntime& quests,UserQuickSlots& quick,VerticalSaveCoreSkillServices& services,std::span<std::uint8_t,775> scratch) {
    const auto tick=vertical_save_core_skills_effects_quests(actor,entry,skills,quests,services,scratch);
    quick.save(actor.account_id(),[&](const net::Packet& packet){services.send_database(packet);});return tick;
}

}
