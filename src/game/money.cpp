#include "eternity/game/money.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/item_pool.hpp"
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int length) {for(unsigned int i=0;i<length;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));}
std::uint32_t receive(Actor& actor,std::uint32_t amount,std::uint8_t type_id,std::uint8_t count,MoneyWorld& world) {
    add_received_money(actor.money(),amount);actor.send_money_database(world);actor.send_self(money_received_packet(amount,type_id,count));
    net::Packet log(82,0);const auto subject=world.money_log_subject(actor);write_game_log_main(log,subject,world.money_local_calendar());
    put(log,0,0x801,2);put(log,74,amount,4);put(log,78,actor.money(),4);world.send_money_log(log);return amount;
}
}
bool money_addable(std::uint32_t current,std::uint32_t amount) noexcept {return amount<=std::numeric_limits<std::uint32_t>::max()-current;} // ChkAddableMoney 00438460.
bool money_sufficient(std::uint32_t current,std::uint32_t amount) noexcept {return amount<=current;} // IsvaildMoney 0044a040.
void add_received_money(std::uint32_t& current,std::uint32_t& amount) noexcept {
    // ChkAddMoneyGet 0045b230, ps_game.exe.
    const auto room=std::numeric_limits<std::uint32_t>::max()-current;if(amount>room) amount=room;current+=amount;
}
void add_money(std::uint32_t& current,std::uint32_t amount) noexcept {add_received_money(current,amount);} // ChkAddMoney 00478360.
net::Packet money_database_packet(std::uint32_t account_id,std::uint32_t amount) {
    // SendDBMoney 0046c700: account UID, not character/runtime ID.
    net::Packet packet(10,0);put(packet,0,0x603,2);put(packet,2,account_id,4);put(packet,6,amount,4);return packet;
}
net::Packet money_received_packet(std::uint32_t accepted,std::uint8_t type_id,std::uint8_t count) {
    // ItemGetMoney 0045ec70 and the money branch of ItemGet(CItem*) 0045df40.
    net::Packet packet(16,0);put(packet,0,0x205,2);packet[2]=1;packet[4]=26;packet[5]=type_id;packet[6]=count;put(packet,7,accepted,4);return packet;
}
bool MoneyReplica::apply(std::span<const std::uint8_t> packet) {
    // ps_dbAgent.exe CUser::PacketUserSetStatus 00405b00 case 0603 assigns
    // the cached DWORD. Account binding belongs to the packet's owning user.
    if(packet.size()!=10 || packet[0]!=3 || packet[1]!=6) return false;
    const auto get=[&](std::size_t offset) {std::uint32_t value=0;for(unsigned int i=0;i<4;++i) value|=static_cast<std::uint32_t>(packet[offset+i])<<(8*i);return value;};
    if(get(2)!=account_id_) return false;
    money_=get(6);return true;
}
void Actor::send_money_database(MoneyWorld& world) {world.send_money_database(money_database_packet(account_id_,money_));}
std::uint32_t Actor::credit_money(std::uint32_t amount,std::uint8_t type_id,std::uint8_t count,MoneyWorld& world) {return receive(*this,amount,type_id,count,world);}
std::uint32_t Actor::receive_money(std::uint32_t amount,MoneyWorld& world) {return receive(*this,amount,0,1,world);} // ItemGetMoney 0045ec70.
std::uint32_t Actor::receive_money_item(GroundItem& item,MoneyWorld& world,GroundItemPool& pool) {
    // Money branch of ItemGet(CItem*) 0045df40, through 0045e085.
    if(item.item.type!=26 || item.zone || item.cell) throw std::invalid_argument("Money credit requires a detached money item");
    const auto accepted=receive(*this,item.money,item.item.type_id,item.item.count,world);pool.release(item);return accepted;
}
}
