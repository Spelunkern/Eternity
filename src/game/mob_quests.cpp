#include "eternity/game/mob_quests.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned bytes) {for(unsigned i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
const QuestDefinition& definition(const RestoredQuest& quest) {
    if(!quest.definition) throw std::invalid_argument("Mob kill quest lacks its definition");
    return *quest.definition;
}
}
MobQuestLinks MobQuestLinks::build(const QuestDefinitions& definitions,std::int32_t count,const MobRuntimeTemplates& mobs) {
    MobQuestLinks result;if(count<=0) return result;
    if(count>65536) throw std::invalid_argument("Mob quest index exceeds original WORD identifiers");
    result.records_.resize(static_cast<std::size_t>(count));
    for(std::size_t i=1;i<=definitions.size();++i) {
        const auto* quest=definitions.find(static_cast<std::uint16_t>(i));if(quest->end_type()!=2) continue;
        const auto targets=quest->mob_targets();
        // Original second-target guard checks target[0], even if it is invalid.
        if(targets[0]!=0) for(auto target:targets) if(target<count && mobs.find(target)) {
            auto& linked=result.records_[target];const auto id=static_cast<std::uint16_t>(i);
            if(std::find(linked.begin(),linked.end(),id)!=linked.end()) continue;
            const auto at=std::find_if(linked.begin(),linked.end(),[&](auto existing){return definitions.find(existing)->minimum_level()>quest->minimum_level();});
            linked.insert(at,id); // AddEnd: unique IDs, ascending minimum level, stable ties.
        }
    }
    return result;
}
const std::vector<std::uint16_t>* MobQuestLinks::find(std::uint16_t mob) const noexcept {return mob>0 && mob<records_.size()?&records_[mob]:nullptr;}
void update_mob_kill_quests(std::uint32_t user_id,std::int32_t have,std::uint16_t mob,const MobQuestLinks& links,MobQuestWorld& world) {
    if(have<=0) return;
    const auto* linked=links.find(mob);if(!linked) return;
    const auto now=world.tick_ms();
    for(auto id:*linked) {
        auto* quest=world.find_quest(id);if(!quest) continue;
        const auto& info=definition(*quest);
        if(info.end_type()!=2 || (info.minimum_minutes()!=0 && now>quest->success_tick)) continue;
        for(unsigned slot=0;slot<2;++slot) {
            if(definition(*quest).mob_targets()[slot]!=mob || quest->packet.counters[slot]==255) continue;
            ++quest->packet.counters[slot];
            net::Packet database{5,10};append(database,user_id,4);append(database,quest->packet.id,2);append(database,slot,1);append(database,quest->packet.counters[slot],1);world.send_database(database);
            net::Packet client{5,9};append(client,quest->packet.id,2);append(client,slot,1);append(client,quest->packet.counters[slot],1);world.send_self(client);
            const auto& current=definition(*quest);const auto required=current.mob_requirements();
            if(current.end_npc_type()==0 && quest->packet.counters[0]>=required[0] && quest->packet.counters[1]>=required[1]) world.complete_quest(*quest,0,true);
        }
    }
}
}
