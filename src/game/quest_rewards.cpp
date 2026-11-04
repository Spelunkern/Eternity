#include "eternity/game/quest_rewards.hpp"
#include "eternity/game/money.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));}
const QuestDefinition& definition(const RestoredQuest& quest) {
    if(!quest.definition) throw std::invalid_argument("Quest reward lacks its definition");
    return *quest.definition;
}
std::uint32_t dword(const QuestDefinition& info,std::size_t at) {std::uint32_t value=0;for(unsigned i=0;i<4;++i) value|=std::uint32_t(info.record()[at+i])<<(8*i);return value;}
void text(net::Packet& packet,std::size_t at,const std::string& value) {
    if(value.size()>31 || value.find('\0')!=std::string::npos) throw std::invalid_argument("Quest reward name exceeds original field");
    std::copy(value.begin(),value.end(),packet.begin()+static_cast<std::ptrdiff_t>(at));
}
}
void award_quest_reward(RestoredQuest& quest,std::uint8_t variant,std::uint32_t context,std::uint32_t user_id,std::uint32_t& money,QuestRewardWorld& world) {
    if(variant>=3) throw std::invalid_argument("Quest reward variant exceeds definition storage");
    const auto offset=std::size_t(variant)*32;const auto& initial=definition(quest);
    auto experience=dword(initial,0x174+offset);if(initial.record()[0x107]<=2) experience+=experience;
    net::Packet client(22,0);put(client,0,0x903,2);put(client,2,context,4);put(client,6,quest.packet.id,2);client[8]=1;client[9]=variant;put(client,10,experience,4);put(client,14,dword(initial,0x178+offset),4);
    net::Packet log(150,0);put(log,0,0x502,2);put(log,74,quest.packet.id,2);text(log,76,initial.name());log[108]=1;
    world.add_experience(0,std::bit_cast<std::int32_t>(experience),true);
    const auto gained_money=dword(definition(quest),0x178+offset);
    if(gained_money!=0) {add_money(money,gained_money);world.send_database(money_database_packet(user_id,money));put(log,109,dword(definition(quest),0x178+offset),4);}
    world.fill_game_log_main(std::span<std::uint8_t>(log).first(74));
    const auto& item_info=definition(quest).record();
    const auto item=world.receive_item(item_info[0x17c+offset],item_info[0x17d+offset],1);
    if(item) {
        if(!item->definition) throw std::invalid_argument("Successful quest item receipt lacks its definition");
        client[18]=item->bag;client[19]=item->slot;client[20]=item->definition->type;client[21]=item->definition->type_id;
        put(log,113,item->definition->id,4);log[117]=1;text(log,118,item->name);
    }
    world.send_self(client);world.remove_quest(quest,true);world.send_game_log(log);
}
}
