#include "eternity/game/quest_requests_completion.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
const QuestDefinition& definition(const RestoredQuest& q) {if(!q.definition) throw std::invalid_argument("Quest completion lacks its definition");return *q.definition;}
void put(net::Packet& p,std::size_t at,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) p[at+i]=static_cast<std::uint8_t>(value>>(8*i));}
}
void quest_requests_complete(RestoredQuest& q,std::uint32_t context,bool success,QuestCompletionWorld& world) {
    const auto& info=definition(q);const auto& d=info.record();
    if(!success) {
        net::Packet client(22,0);put(client,0,0x903,2);put(client,2,context,4);put(client,6,q.packet.id,2);world.send_self(client);
        world.remove_quest(q,false);
        net::Packet log(150,0);world.fill_game_log_main(std::span<std::uint8_t>(log).first(74));
        put(log,0,0x502,2);put(log,74,q.packet.id,2);
        const auto name=info.name();if(name.size()>31) throw std::invalid_argument("Quest failure name exceeds original field");
        std::copy(name.begin(),name.end(),log.begin()+76);world.send_game_log(log);return;
    }
    if(info.minimum_minutes()!=0) {
        const auto now=world.tick_ms();
        if(info.minimum_minutes()*60000U+q.start_tick>now) return;
    }
    if(d[0x15c]==1) {world.award(q,0,context);return;}
    if(d[0x15d]!=0) {net::Packet p{7,9,0,0};put(p,2,q.packet.id,2);q.success=1;world.send_self(p);return;}
    (void)world.tick_ms(); // Original reads/discards the clock before variant selection.
    for(int variant=2;variant>=0;--variant) {
        const auto at=0x160+static_cast<std::size_t>(variant)*32;
        unsigned selector=0,required=0;
        if(d[0x147]==1) {selector=d[at+3];required=d[at+4];if(selector>3) selector=0;}
        else if(d[0x147]==2) {selector=static_cast<unsigned>(d[at]|(d[at+1]<<8));required=d[at+2];if(selector>2) selector=0;}
        if(variant==0 || (selector!=0 && required!=0 && q.packet.counters[selector-1]>=required)) {world.award(q,static_cast<std::uint8_t>(variant),context);return;}
    }
}
void quest_requests_complete_selection(std::uint16_t id,std::uint8_t selection,QuestCompletionWorld& world) {
    auto* q=world.find_active(id);if(!q || q->success==0) return;
    const auto& d=definition(*q).record();if(selection>=d[0x15d]) return;
    if(d[0x148]!=0 && !world.find_required_items(*q,true)) return;
    world.award(*q,selection,0);
}
}
