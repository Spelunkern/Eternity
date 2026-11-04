#pragma once
#include "eternity/game/party_runtime_membership.hpp"
#include <atomic>

namespace eternity::game {
struct PartyMembershipSendBuffer {
    std::atomic<std::uint32_t> references{0}; // SPacketBufferSend +0x8
    std::uint32_t custom_references{};             // +0xc, PDB m_nCustomRef
    std::uint16_t capacity{};                // original pool size class
    net::Packet frame;                       // +0x4 points to length-prefixed bytes
};
// Explicit real pool/worker provider. Queued sends own one reference each;
// owners must retain the buffer until completion and release that reference.
// Callbacks must not throw or invalidate the serialized party/user projection.
class PartyMembershipSendWorld:public PartyMembershipWorld {
public:
    virtual PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity)=0;
    // May destroy/recycle the object; caller never dereferences it afterwards.
    virtual void free_send_buffer(PartyMembershipSendBuffer&)=0;
    virtual void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer&)=0;
    void send_party(PartyNormalMembership&,const net::Packet&) override;
};
// ps_game.exe SPacketBufferSend::AllocBuffer 004df9f0, CParty::Send 004455d0,
// SConnection::Send 004d4cf0. Valid original payload fits the 2048-byte pool.
// This normal-owner adapter requires every visited live count to fit its seven
// slots; it does not supply the larger raid member projection.
void send_party_membership(PartyNormalMembership&,std::span<const std::uint8_t>,PartyMembershipSendWorld&);
// Recovered allocation/copy shared by party broadcast and direct roster send.
PartyMembershipSendBuffer* allocate_party_membership_send_buffer(std::span<const std::uint8_t>,PartyMembershipSendWorld&);
// Original InterlockedDecrement / SPacketBufferSend::Free boundary 00445659.
void release_party_membership_buffer(PartyMembershipSendBuffer&,PartyMembershipSendWorld&);
}
