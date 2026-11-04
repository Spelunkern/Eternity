#include "eternity/game/quest_requests_progress.hpp"
#include <stdexcept>

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
const QuestDefinition& definition(const RestoredQuest& quest) {
    if(!quest.definition) throw std::invalid_argument("Live quest progress lacks its definition");
    return *quest.definition;
}
void increment(std::uint32_t account,RestoredQuest& quest,unsigned slot,QuestRequestsProgressWorld& world) {
    ++quest.packet.counters[slot];
    net::Packet database{5,10};append(database,account,4);append(database,quest.packet.id,2);append(database,slot,1);append(database,quest.packet.counters[slot],1);
    world.send_database(database);
    // Original reloads ID/count after the DB callback, then definition after
    // the client callback. Never reuse the DB packet's counter for the client.
    net::Packet client{5,9};append(client,quest.packet.id,2);append(client,slot,1);append(client,quest.packet.counters[slot],1);
    world.send_self(client);
}
}
void quest_requests_progress_kill_pc(std::uint32_t account,std::int32_t have,QuestRequestsProgressWorld& world) {
    if(have<=0) return;
    world.lock_active_quests();
    struct Unlock {QuestRequestsProgressWorld& world;~Unlock(){world.unlock_active_quests();}} unlock{world};
    auto* quest=world.first_active_quest();
    while(quest && definition(*quest).end_type()!=3) quest=world.next_active_quest();
    if(!quest || quest->packet.counters[0]==255) return;
    increment(account,*quest,0,world);
    const auto& d=definition(*quest).record();
    if(d[0x148]==0 && quest->packet.counters[0]>=d[0x155]) world.complete_quest(*quest,0,true);
}
void quest_requests_progress_pick_item(std::uint32_t account,std::int32_t type,std::int32_t id,const ItemQuestLinks& links,QuestRequestsProgressWorld& world) {
    const auto* link=links.find(type,id);if(!link) return;
    const auto now=world.tick_ms();
    for(std::size_t index=0;index<link->end.size();++index) {
        auto* quest=world.find_quest(link->end[index]);if(!quest) continue;
        const auto& info=definition(*quest);
        if(info.end_type()!=1 || (info.minimum_minutes()!=0 && now>quest->success_tick)) continue;
        for(unsigned slot=0;slot<3;++slot) {
            const auto& d=definition(*quest).record();
            if(d[0x14c+3*slot]!=type || d[0x14d+3*slot]!=id || quest->packet.counters[slot]==255) continue;
            increment(account,*quest,slot,world);
            if(definition(*quest).end_npc_type()==0 && world.find_required_items(*quest,true)) world.complete_quest(*quest,0,true);
        }
    }
}
}
