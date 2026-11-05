#pragma once
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/world_users.hpp"
#include "eternity/game/user_view_world_channel.hpp"

namespace eternity::game {
// Adapts the existing shared buffer/party owner to actual session channels.
// The world strand serializes registry, index and connection projections. The
// original pool, all sessions and this adapter outlive queued buffer references.
template<class Session> class VerticalMoveChannelSendWorld final:public PartyMembershipSendWorld {
public:
    using Connection=std::function<PartyMembershipUser&(Session&)>;
    using Channel=std::function<net::WorldChannel&(Session&)>;
    VerticalMoveChannelSendWorld(PartyMembershipSendWorld& pool,WorldUsers& users,VerticalSessionRegistry<Session>& registry,
        Connection connection,Channel channel):pool_(pool),users_(users),registry_(registry),connection_(std::move(connection)),channel_(std::move(channel)) {
        if(!connection_ || !channel_)throw std::invalid_argument("Movement send requires session connection and channel");
    }
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override{return pool_.allocate_send_buffer(capacity);}
    void free_send_buffer(PartyMembershipSendBuffer& buffer) override{pool_.free_send_buffer(buffer);}
    // The transferred reference is released only after the channel accepts its
    // copy. On exceptions ownership stays with the caller as required by Send.
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override {
        auto* world=users_.find(user.id);
        if(!world)throw std::logic_error("Movement recipient is absent from world index");
        auto owner=registry_.retain_actor(world->actor);
        if(!owner || &connection_(*owner)!=&user)throw std::logic_error("Movement recipient connection is not retained");
        enqueue_user_view_world_channel(buffer,*this,channel_(*owner));
    }
    std::uint32_t tick() override{return pool_.tick();}
    void lock(PartyNormalMembership& party) override{pool_.lock(party);}
    void unlock(PartyNormalMembership& party) noexcept override{pool_.unlock(party);}
    void lock_effects(PartyMembershipUser& user) override{pool_.lock_effects(user);}
    void unlock_effects(PartyMembershipUser& user) noexcept override{pool_.unlock_effects(user);}
    void send_user(PartyMembershipUser& user,const net::Packet& packet) override{pool_.send_user(user,packet);}
    std::uint8_t party_type(PartyMembershipUser& user) override{return pool_.party_type(user);}
    void send_scene(PartyMembershipUser& user,const net::Packet& packet) override{pool_.send_scene(user,packet);}
    void unregister_party(PartyNormalMembership& party) override{pool_.unregister_party(party);}
    void cleanup_guild_creation(PartyMembershipUser& user) override{pool_.cleanup_guild_creation(user);}
    void fill_log(PartyMembershipUser& user,std::span<std::uint8_t> packet) override{pool_.fill_log(user,packet);}
    void send_log(const net::Packet& packet) override{pool_.send_log(packet);}
private:
    PartyMembershipSendWorld& pool_;WorldUsers& users_;VerticalSessionRegistry<Session>& registry_;Connection connection_;Channel channel_;
};
}
