#include "eternity/game/world_lifecycle.hpp"
#include <cstdio>

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned int count=4) {for(unsigned int i=0;i<count;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet header(std::uint16_t opcode,std::size_t size,const WorldLifecycleState& state,const WorldLifecyclePorts& port) {
    net::Packet packet(size,0);write_game_log_main(packet,state.subject,port.local_calendar());packet[0]=static_cast<std::uint8_t>(opcode);packet[1]=static_cast<std::uint8_t>(opcode>>8);return packet;
}
void manager(const WorldLifecycleState& state,const WorldLifecyclePorts& port,const char* kind,std::uint32_t old) {
    net::Packet packet(259,0);packet[0]=0x0e;packet[1]=8;const bool error=std::string_view(kind)=="Money Error";packet[2]=error?1:0;
    std::snprintf(reinterpret_cast<char*>(packet.data()+3),256,"%s : Char=%s, Old=%u, Now=%u",kind,state.subject.character_name.c_str(),old,state.money);port.manager(std::move(packet));
}
void new_user(bool entering,const net::Packet& initial,const WorldLifecycleState& state,const WorldLifecyclePorts& port) {
    auto packet=initial;packet.resize(74);packet[0]=entering?0x0f:0x10;packet[1]=3;port.game_log(std::move(packet));
    net::Packet notice(259,0);notice[0]=0x0e;notice[1]=8;
    std::snprintf(reinterpret_cast<char*>(notice.data()+3),256,"NewUser %s : Char=%s, Lv=%d, Map=%d, X=%.1f, Z=%.1f",entering?"Enter":"Leave",state.subject.character_name.c_str(),int(state.subject.level),int(state.subject.map),double(state.subject.position.x),double(state.subject.position.z));port.manager(std::move(notice));
}
void points(net::Packet& packet,const WorldLifecycleState& state) {
    append(packet,state.skill_points,2);append(packet,state.stat_points,2);for(auto value:state.stats) append(packet,value,2);
}
void validate(const WorldLifecyclePorts& port) {
    if(!port.admin_enter || !port.step || !port.close || !port.local_calendar || !port.game_log || !port.manager || !port.take_money || !port.save_money) throw std::invalid_argument("Incomplete world lifecycle services");
}
}
void OfflineMoney::save(std::uint32_t character,std::uint32_t money) {money_[character]=money;}
std::optional<std::uint32_t> OfflineMoney::take(std::uint32_t character) {auto node=money_.extract(character);return node.empty()?std::nullopt:std::optional{node.mapped()};}
bool enter_world(WorldLifecycleState& state,const WorldLifecyclePorts& port) {
    validate(port);
    if(state.permission<=10 && !port.admin_enter()) {port.close(9);return false;}
    port.step(WorldLifecycleStep::guild_list);
    auto log=header(0x306,74,state,port);log.push_back(state.billing_mode);append(log,state.disconnected_seconds);append(log,state.money);append(log,0);append(log,state.cash_points);points(log,state);append(log,state.ip);append(log,state.subject.level,2);port.game_log(log);
    if(state.auto_character==0) {
        const auto old=port.take_money(state.subject.character_id);
        if(old && *old<state.money && state.money-*old>=10000) {
            auto error=header(0x30d,74,state,port);append(error,state.money);append(error,*old);port.game_log(std::move(error));manager(state,port,"Money Error",*old);
        }
        if(state.entry_type<=3) new_user(true,log,state,port);
    }
    return true;
}
void leave_world(WorldLifecycleState& state,const WorldLifecyclePorts& port) {
    validate(port);
    if(state.party) port.step(*state.party?WorldLifecycleStep::leave_raid:WorldLifecycleStep::leave_party);
    port.step(WorldLifecycleStep::release_guild);port.step(WorldLifecycleStep::release_friends);port.step(WorldLifecycleStep::leave_dead);
    if(state.permission<=10) {
        port.step(WorldLifecycleStep::admin_end);
        if(state.admin_question) port.step(WorldLifecycleStep::admin_not_answered);
        port.step(WorldLifecycleStep::admin_leave);
    }
    if(state.vehicle_mode==2) port.step(WorldLifecycleStep::cancel_vehicle);
    else if(state.vehicle_mode!=0) port.step(WorldLifecycleStep::cancel_vehicle_pair);
    auto log=header(0x307,74,state,port);append(log,state.money);append(log,state.bank_money);append(log,state.cash_points);points(log,state);append(log,state.subject.level,2);append(log,state.initial_money);port.game_log(log);
    if(state.reconnect_ready!=1) {
        port.save_money(state.subject.character_id,state.money);
        if(state.initial_money<state.money && state.money-state.initial_money>=1000000000) {
            auto error=header(0x30e,74,state,port);append(error,state.money);append(error,state.initial_money);port.game_log(std::move(error));manager(state,port,"Money Over",state.initial_money);
        }
        if(state.entry_type<=3) new_user(false,log,state,port);
    }
}
}
