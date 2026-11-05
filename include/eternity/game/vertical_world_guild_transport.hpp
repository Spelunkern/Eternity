#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/guild_membership.hpp"

namespace eternity::game {
// Guild membership uses the real world index and retained actor owner. The
// world strand, indexes, session registry, allocation services and this adapter
// outlive the GuildCatalog and its synchronous callbacks.
// ps_game.exe CGuild::Send 0042c050 and SendCharacterGuild 00476390.
class VerticalWorldGuildTransport final {
public:
    using Allocate=std::function<bool(const net::Packet&)>;
    using Release=std::function<void()>;
    VerticalWorldGuildTransport(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        WorldUsers& users,Allocate allocate,Release release)
        :strand_(std::move(strand)),registry_(registry),users_(users),allocate_(std::move(allocate)),release_(std::move(release)) {
        if(!allocate_ || !release_)throw std::invalid_argument("Missing guild packet allocation services");
    }
    VerticalWorldGuildTransport(const VerticalWorldGuildTransport&)=delete;
    VerticalWorldGuildTransport& operator=(const VerticalWorldGuildTransport&)=delete;
    GuildMemberPorts ports() {
        return {[this](const net::Packet& packet){require_strand();return allocate_(packet);},
            [this]{require_strand();release_();},
            [this](GuildUser& user,net::Packet packet){send(user,std::move(packet));}};
    }
    void send(GuildUser& user,net::Packet packet) {
        require_strand();auto* world=users_.find(user.character_id);if(!world)throw std::logic_error("Guild user is absent from world index");
        auto owner=registry_.retain_actor(world->actor);
        if(!owner || owner->world_user()!=world || &owner->guild_user()!=&user || !owner->channel().send)
            throw std::logic_error("Guild user lost its retained session");
        owner->channel().send(std::move(packet));
    }
private:
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Guild transport outside world strand");}
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;WorldUsers& users_;Allocate allocate_;Release release_;
};
}
