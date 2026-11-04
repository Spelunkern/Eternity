#include "eternity/net/world_protocol.hpp"
#include <algorithm>

namespace eternity::net {
WorldProtocol::WorldProtocol(std::shared_ptr<session::Registry> registry, std::uint8_t realm,
    std::uint64_t connection, std::shared_ptr<const WorldXor> world_xor, std::function<void()> disconnect)
    : registry_(std::move(registry)), realm_(realm), connection_(connection),
      world_xor_(std::move(world_xor)), disconnect_(std::move(disconnect)) {
    if(!registry_ || !world_xor_ || realm_>=254 || connection_==0)
        throw std::invalid_argument("Invalid world connection configuration");
}
WorldProtocol::~WorldProtocol() { close(); }

void WorldProtocol::close() noexcept {
    state_=State::closed;
    if(owner_) registry_->release_world(owner_->user_id,owner_->session_id,connection_);
}

WorldEvent WorldProtocol::receive(Packet payload, std::uint64_t now_ms) {
    WorldEvent event;
    const auto fail=[&] { close(); event.close=true; return event; };
    if(state_==State::closed || payload.size()<2 || payload.size()>maximum_frame_size-2) return fail();
    if(cipher_) cipher_->decrypt(payload);
    const auto opcode=static_cast<std::uint16_t>(payload[0] | (static_cast<unsigned int>(payload[1])<<8));
    if(state_==State::connecting) {
        // CUser::PacketProcessing, ps_game.exe 00466da0: eleven undefined packets close the socket.
        if(opcode!=0xa301) {
            if(++undefined_packets_>10) return fail();
            return event;
        }
        if(payload.size()<22) return fail();
        std::uint32_t user=0;
        for(unsigned int index=0;index<4;++index) user|=static_cast<std::uint32_t>(payload[2+index])<<(8*index);
        std::array<std::uint8_t,16> token{};
        std::copy_n(payload.begin()+6,16,token.begin());
        owner_=registry_->claim(user,token,realm_,connection_,now_ms,disconnect_);
        if(!owner_) return fail();
        try {
            cipher_=std::make_unique<WorldCipher>(owner_->game_key,world_xor_);
            state_=State::character_selection;
            // CWorld::LoginUser, ps_game.exe 0040c510: TP_GAME_CONNECT, result=0, mode=2, seed.
            Packet reply{0x01,0xa3,0,2};
            reply.insert(reply.end(),world_xor_->seed().begin(),world_xor_->seed().end());
            event.replies.push_back(outgoing(std::move(reply)));
            event.connected=true;
        } catch(...) { close(); throw; }
        return event;
    }
    // The admission owner publishes location independently of outbound cipher mode.
    if(!admission_dispatch_ && (opcode&0xff00)!=0x0100) return event;
    event.application=std::move(payload);
    return event;
}

Packet WorldProtocol::outgoing(Packet payload) {
    if(!cipher_ || state_==State::closed) throw std::logic_error("World session is not connected");
    if(payload.size()<2 || payload.size()>maximum_frame_size-2) throw std::invalid_argument("Invalid world payload size");
    cipher_->encrypt(payload);
    return payload;
}
void WorldProtocol::set_admission_dispatch(bool enabled) {
    if(state_!=State::character_selection && state_!=State::world) throw std::logic_error("Admission routing requires a connected world session");
    admission_dispatch_=enabled;
}
void WorldProtocol::enter_world() {
    if(state_!=State::character_selection) throw std::logic_error("World entry requires character selection");
    cipher_->enter_world(); state_=State::world;
}
void WorldProtocol::enter_character_selection() {
    if(state_!=State::world) throw std::logic_error("Character selection requires a connected world session");
    cipher_->enter_character_selection();admission_dispatch_=false;state_=State::character_selection;
}
}
