#include "eternity/game/vertical_protocol.hpp"
#include <bit>

namespace eternity::game {
void deliver_vertical_entry(CharacterEntryState& state,ActorLoadPipeline& pipeline,const PlayerSnapshot& player,
    const SocialCatalog& social,const EntryCatalogs& catalogs,ExperienceBounds experience,std::string guild_name,
    const VerticalProtocolServices& services) {
    const auto& loaded=pipeline.result();
    if(!loaded.actor || loaded.learned.invalid || state.user_id!=player.core.user_id || state.character_id!=player.core.character.id)
        throw std::invalid_argument("Entry requires the matching successfully restored actor");
    if(!services.lifecycle || !services.send_manager || !services.world_day || !services.system_time || !services.lock_guild || !services.unlock_guild)
        throw std::invalid_argument("Incomplete vertical entry services");
    EntryDelivery delivery(player,loaded,social,catalogs,experience,std::move(guild_name),services.delivery);
    CharacterEntryPorts ports;
    ports.send_client=services.delivery.send_self;ports.send_manager=services.send_manager;
    ports.world_day=services.world_day;ports.system_time=services.system_time;
    ports.lock_guild=services.lock_guild;ports.unlock_guild=services.unlock_guild;
    ports.step=[&](CharacterEntryStep step) {
        switch(step) {
        case CharacterEntryStep::equipment:pipeline.equipment();return;
        case CharacterEntryStep::passive:pipeline.passive();return;
        case CharacterEntryStep::effects:pipeline.effects();return;
        case CharacterEntryStep::billing: {
            pipeline.billing();const auto& actor=*loaded.actor;const auto max=actor.status().skills.equipment.base.maximum;const auto current=actor.resources();
            state.maximum={std::bit_cast<std::int32_t>(max.hp),std::bit_cast<std::int32_t>(max.mp),std::bit_cast<std::int32_t>(max.sp)};
            state.current={std::bit_cast<std::int32_t>(current.hp),std::bit_cast<std::int32_t>(current.mp),std::bit_cast<std::int32_t>(current.sp)};return;
        }
        case CharacterEntryStep::stop_attack:loaded.actor->stop_attack(loaded.actor->status());return;
        default:if(!delivery.send(step)) services.lifecycle(step,*loaded.actor);return;
        }
    };
    complete_character_selection(state,ports);
}
}

namespace eternity::game {
VerticalEntryRequest VerticalEntrySession::receive(std::span<const std::uint8_t> packet) {
    // PacketProcessing 00466da0: location 3 and opcode 0201 at 00466f01..00466f67.
    // Length checking is the native framing boundary; the original receives a pointer.
    if(packet.size()<2 || user_.location!=3 || packet[0]!=1 || packet[1]!=2) return VerticalEntryRequest::unhandled;
    return world_.client_enter(user_)?VerticalEntryRequest::entered:VerticalEntryRequest::rejected;
}
void update_vertical_admission(WorldAdmission& world) {world.update_packets();}
}

namespace eternity::game {
VerticalNProtectResult receive_vertical_nprotect(std::span<const std::uint8_t> packet,NProtectState& state,std::uint32_t location,const VerticalNProtectPorts& port) {
    if(packet.size()<18 || packet[0]!=0x02 || packet[1]!=0xa3) throw std::invalid_argument("Malformed NProtect answer");
    if(!state.present) return VerticalNProtectResult::absent;
    if(!port.close) throw std::invalid_argument("NProtect close service is required");
    if(location==0) {port.close(111);return VerticalNProtectResult::rejected;}
    if(!port.check_answer || !port.diagnostic) throw std::invalid_argument("NProtect answer backend is required");
    std::array<std::uint32_t,4> answer{};
    for(std::size_t i=0;i<answer.size();++i) for(unsigned j=0;j<4;++j) answer[i]|=std::uint32_t(packet[2+4*i+j])<<(8*j);
    const auto result=port.check_answer(answer);
    if(result==0) {state.received=true;return VerticalNProtectResult::accepted;}
    port.diagnostic(result);port.close(111);return VerticalNProtectResult::rejected;
}
}
