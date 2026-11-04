#include "eternity/game/quest_start_add.hpp"
#include <bit>
namespace eternity::game {
bool quest_start_add(const QuestDefinition& definition,QuestStartAddUser& user,QuestStartAddWorld& world) {
    auto* quest=world.allocate();if(!quest) return false;
    quest->definition=&definition;quest->packet.id=definition.id();quest->success=0;quest->packet.counters={0,0,0};
    quest->packet.timed=definition.maximum_minutes()!=0;
    quest->start_tick=world.tick_ms();
    quest->packet.end_tick=quest->packet.timed?quest->start_tick+definition.maximum_minutes()*60000u:0;
    quest->success_tick=definition.minimum_minutes()!=0?quest->start_tick+definition.minimum_minutes()*60000u:0;
    world.insert_active(*quest);
    const auto increment=[](std::int32_t value) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)+1u);};
    if(definition.end_type()==3) user.player_kill_quests=increment(user.player_kill_quests);
    else if(definition.end_type()==2) user.mob_kill_quests=increment(user.mob_kill_quests);
    std::array<std::uint8_t,10> packet{1,10};
    for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(user.billing_id>>(8*i));
    const auto id=definition.id();const auto delay=definition.maximum_minutes();
    packet[6]=static_cast<std::uint8_t>(id);packet[7]=static_cast<std::uint8_t>(id>>8);
    packet[8]=static_cast<std::uint8_t>(delay);packet[9]=static_cast<std::uint8_t>(delay>>8);
    world.send_database(packet);return true;
}
}
