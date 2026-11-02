#include "eternity/game/character_entry.hpp"

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned int bytes=4) {for(unsigned int i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
net::Packet value_packet(std::uint16_t opcode,std::uint32_t value) {net::Packet packet;append(packet,opcode,2);append(packet,value);return packet;}
}
CharacterSelectionResult request_character_selection(CharacterSelectionState& state,std::span<const std::uint8_t> packet,const CharacterSelectionPorts& port) {
    if(!port.send_db || !port.close) throw std::invalid_argument("Incomplete character selection ports");
    if(packet.size()<6 || packet[0]!=4 || packet[1]!=1) {port.close(9);return CharacterSelectionResult::closed;}
    if(!state.enabled) return CharacterSelectionResult::ignored;
    if(state.country==2) {port.close(9);return CharacterSelectionResult::closed;}
    state.enabled=false;auto request=value_packet(0x407,state.user_id);request.insert(request.end(),packet.begin()+2,packet.begin()+6);port.send_db(std::move(request));return CharacterSelectionResult::requested;
}
void complete_character_selection(CharacterEntryState& state,const CharacterEntryPorts& port) {
    if(!port.step || !port.send_client || !port.send_manager || !port.world_day || !port.system_time || !port.lock_guild || !port.unlock_guild) throw std::invalid_argument("Incomplete character entry services");
    using Step=CharacterEntryStep;state.runtime_id=state.character_id;port.send_manager(value_packet(0x803,state.user_id));
    for(auto step:{Step::equipment,Step::passive,Step::effects,Step::billing}) port.step(step);
    for(std::size_t i=0;i<3;++i) if(state.current[i]>state.maximum[i]) state.current[i]=state.maximum[i];
    net::Packet selected{4,1,0};append(selected,state.character_id);port.send_client(std::move(selected));port.send_client(value_packet(0x404,port.world_day()));
    for(auto step:{Step::stop_attack,Step::wait_user,Step::init_guild,Step::init_friends}) port.step(step);
    if(state.auto_character_id) {auto automatic=value_packet(0x112,state.runtime_id);append(automatic,state.level,2);port.send_client(std::move(automatic));}
    for(auto step:{Step::data,Step::items,Step::bank,Step::skills,Step::effect_packets,Step::quests,Step::finished_quests,Step::friends,Step::speed,Step::kc,Step::save_points,Step::honor,Step::purchase_history,Step::stored_points}) port.step(step);
    const bool locked=port.lock_guild();
    try {
        if(state.guild_present) {port.step(Step::guild_bank);port.step(Step::guild_npcs);}
        port.send_client(value_packet(0x114,port.system_time()));port.send_client({0x1b,2,static_cast<std::uint8_t>(0U-state.flag_584c),static_cast<std::uint8_t>(1U-state.flag_584d)});
        port.step(Step::pvp_reset);if(state.motion==13) port.step(Step::run_mode);
        if(!state.auto_character_id) port.step(Step::quick);else port.send_client({0x13,1});
        state.ready=true;port.step(Step::flush);
    }catch(...) {if(locked) port.unlock_guild();throw;}
    if(locked) port.unlock_guild();
}
}
