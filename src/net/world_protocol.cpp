#include "eternity/net/world_protocol.hpp"
#include <algorithm>
#include <bit>

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
    if(owner_) { registry_->release_world(owner_->user_id,owner_->session_id,connection_); owner_->world_reservation.reset(); }
}

WorldEvent WorldProtocol::receive(Packet payload, std::uint64_t now_ms,bool retained_world_live) {
    WorldEvent event;
    const auto fail=[&] { close(); event.close=true; return event; };
    if(state_==State::closed || payload.size()<2 || payload.size()>maximum_frame_size-2) return fail();
    if(cipher_) cipher_->decrypt(payload);
    const auto opcode=static_cast<std::uint16_t>(payload[0] | (static_cast<unsigned int>(payload[1])<<8));
    // ps_game.exe CUser::OnRecv 00466d20 calls PacketSystem after AES,
    // before authentication and location-specific PacketProcessing.
    if(opcode==0x0002) {close();event.close=true;event.close_reason=1;return event;}
    if(opcode==0x0003) {
        // ps_game.exe SReply::CheckRecv 004dd6c0; accepted-agent reply
        // parameters are 300000/300000/600000 (CServer::Start 00405a30).
        const auto tick=static_cast<std::uint32_t>(now_ms);
        const auto elapsed=std::bit_cast<std::int32_t>(tick-last_reply_tick_);
        const auto reason=last_reply_tick_==0 ? 0U : elapsed<0 ? 10U : elapsed>900000 ? 8U : 0U;
        last_reply_tick_=tick;
        if(reason) {close();event.close=true;event.close_reason=reason;}
        return event;
    }
    if(opcode==0xa302) {
        if(payload.size()<18) return fail();
        // A fresh CUser has no NProtect object (CUser::CUser 00449b88).
        // NProtectCSARecv 0044b7e0 returns immediately when it is absent;
        // there is no authenticated application owner to dispatch to yet.
        if(state_==State::connecting) return event;
        event.nprotect=std::move(payload);return event;
    }
    if(state_==State::connecting) {
        // ps_game.exe CUser::PacketProcessing 00466da0: an unknown opcode
        // or A301 with a zero/already-live account increments the undefined count. The
        // eleventh such packet closes with reason 0x65.
        const auto undefined=[&] {
            if(undefined_packets_>=10) {close();event.close=true;event.close_reason=0x65;}
            else ++undefined_packets_;
            return event;
        };
        if(opcode!=0xa301) return undefined();
        if(payload.size()<6) return fail();
        std::uint32_t user=0;
        for(unsigned int index=0;index<4;++index) user|=static_cast<std::uint32_t>(payload[2+index])<<(8*index);
        if(user==0) return undefined();
        // The native registry is the current live billing-user index. The
        // original CWorld::FindUserBill 0040d570 precedes manager admission.
        const auto live=[&] {
            const auto found=registry_->find(user);
            return registry_->world_reserved(user,realm_) || (found && found->realm==realm_ && found->location==session::LoginSession::Location::world);
        };
        if(retained_world_live || live()) return undefined();
        if(payload.size()<22) return fail();
        std::array<std::uint8_t,16> token{};
        std::copy_n(payload.begin()+6,16,token.begin());
        auto claim=registry_->claim_world(user,token,realm_,connection_,now_ms,disconnect_);
        if(claim.duplicate) return undefined();
        owner_=std::move(claim.session);
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

bool WorldProtocol::reply_timed_out(std::uint64_t now_ms) const noexcept {
    if(state_==State::closed) return false;
    const auto tick=static_cast<std::uint32_t>(now_ms);
    // SConnection::SConnection 004d4c50 initializes +0x40 to zero;
    // SReply::CheckTime 004dd670 compares the first deadline unsigned.
    if(last_reply_tick_==0) return tick>900000;
    const auto elapsed=std::bit_cast<std::int32_t>(tick-last_reply_tick_);
    return elapsed>900000;
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
