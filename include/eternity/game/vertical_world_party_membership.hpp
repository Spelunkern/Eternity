#pragma once
#include "eternity/game/party_runtime_raid.hpp"
#include "eternity/game/user_view_world_channel.hpp"
#include "eternity/game/vertical_world_party_directory.hpp"
#include "eternity/game/vertical_world_party_registry.hpp"

namespace eternity::game {
// Real party membership dispatch over the retained registry and channels.
// All providers and packet buffers outlive callbacks and queued sends. Calls
// use the world strand; the live socket field is maintained by its owner.
// ps_game.exe CParty::Send 004455d0 / SConnection::Send 004d4d20.
class VerticalWorldPartyMembership final:public PartyRaidMembershipWorld {
public:
    struct Services {
        std::function<std::uint32_t()> tick;
        std::function<void(PartyMembershipUser&)> lock_effects,unlock_effects,cleanup_guild_creation;
        std::function<void(PartyMembershipUser&,const net::Packet&)> send_scene;
        std::function<void(PartyMembershipUser&,std::span<std::uint8_t>)> fill_log;
        std::function<void(const net::Packet&)> send_log;
        std::function<void(PartyMembershipUser&,std::int32_t)> close_socket;
        // Must report the fault and initiate real connection/world failure;
        // it must not throw or destroy the current owner synchronously.
        std::function<void(PartyMembershipUser&,std::exception_ptr)> send_failure;
    };
    template<class Pool> VerticalWorldPartyMembership(VerticalWorldPartyDirectory& directory,VerticalWorldPartyRegistry& registry,
        Pool& pool,Services services)
        :directory_(directory),registry_(registry),allocate_([&pool](std::uint16_t capacity){return pool.allocate_send_buffer(capacity);}),
         free_([&pool](PartyMembershipSendBuffer& buffer){pool.free_send_buffer(buffer);}),services_(std::move(services)) {
        if(!services_.tick || !services_.lock_effects || !services_.unlock_effects || !services_.cleanup_guild_creation ||
           !services_.send_scene || !services_.fill_log || !services_.send_log || !services_.close_socket || !services_.send_failure)
            throw std::invalid_argument("Incomplete party membership services");
    }
    VerticalWorldPartyMembership(const VerticalWorldPartyMembership&)=delete;
    VerticalWorldPartyMembership& operator=(const VerticalWorldPartyMembership&)=delete;
    std::uint32_t tick() override {return services_.tick();}
    void lock(PartyNormalMembership& party) override {registry_.lock_party(party);}
    void unlock(PartyNormalMembership& party) noexcept override {registry_.unlock_party(party);}
    void lock_effects(PartyMembershipUser& user) override {services_.lock_effects(user);}
    void unlock_effects(PartyMembershipUser& user) noexcept override {services_.unlock_effects(user);}
    void send_scene(PartyMembershipUser& user,const net::Packet& packet) override {services_.send_scene(user,packet);}
    void unregister_party(PartyNormalMembership& party) override {registry_.unregister(party);}
    void cleanup_guild_creation(PartyMembershipUser& user) override {services_.cleanup_guild_creation(user);}
    void fill_log(PartyMembershipUser& user,std::span<std::uint8_t> packet) override {services_.fill_log(user,packet);}
    void send_log(const net::Packet& packet) override {services_.send_log(packet);}
    void close_send_socket(PartyMembershipUser& user,std::int32_t socket) override {services_.close_socket(user,socket);}
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override {return allocate_(capacity);}
    void free_send_buffer(PartyMembershipSendBuffer& buffer) override {free_(buffer);}
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override {
        try {
            auto view=directory_.retain(user);auto* session=static_cast<VerticalWorldSession*>(view.lifetime.get());
            if(!session || &session->party_user()!=&user)throw std::logic_error("Party send lost its session");
            enqueue_user_view_world_channel(buffer,*this,session->channel());
        } catch(...) {
            auto error=std::current_exception();release_party_membership_buffer(buffer,*this);services_.send_failure(user,error);
        }
    }
private:
    VerticalWorldPartyDirectory& directory_;VerticalWorldPartyRegistry& registry_;
    std::function<PartyMembershipSendBuffer*(std::uint16_t)> allocate_;std::function<void(PartyMembershipSendBuffer&)> free_;Services services_;
};
}
