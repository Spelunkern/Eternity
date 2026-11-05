#pragma once
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_move_session.hpp"
#include "eternity/game/user_view_zone_delivery.hpp"
#include "eternity/game/user_cell_snapshot_channel.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"
#include <new>

namespace eternity::game {
// Serialized world owner retains every registered session and its zone. Accessors
// return the session's actual admission projection and channel; removal must be
// serialized with dispatch. WorldChannel owns each packet copy after send.
template<class Session> class VerticalMoveRetainedSession final {
public:
    using Entry=std::function<ZoneEntryState&(Session&)>;
    using Channel=std::function<net::WorldChannel&(Session&)>;
    using StackBytes=UserCellSnapshotChannel::StackBytes;
    using Connection=std::function<PartyMembershipUser&(Session&)>;
private:
    class Recipients final:public UserViewCellServices {
        struct Buffer final:UserViewBuffer {explicit Buffer(net::Packet p):packet(std::move(p)){} net::Packet packet;};
    public:
        Recipients(VerticalSessionRegistry<Session>& registry,ZoneUsers& users,Entry& entry,Channel& channel,
            UserViewSharedBuffers* buffers,Connection& connection)
            :registry_(registry),users_(users),entry_(entry),channel_(channel),buffers_(buffers),connection_(connection) {}
        std::shared_ptr<Session> pin(const Actor& actor) {
            auto owner=registry_.retain_actor(actor);
            if(!owner)throw std::logic_error("View actor has no retained session");
            return owner;
        }
        UserViewLiveAppearance appearance(const Actor& actor) override {
            auto owner=pin(actor);const auto& state=entry_(*owner);
            if(!state.bound || state.character_id!=actor.id())throw std::logic_error("View actor has no matching admission");
            return capture_user_view_appearance(users_,actor,state.permission,state.direction,state.guild_user_level);
        }
        std::unique_ptr<UserViewBuffer> allocate(net::Packet packet) override {
            if(buffers_)return buffers_->allocate(packet);
            if(packet.size()>2046)throw std::length_error("User view packet exceeds original buffer capacity");
            try{return std::make_unique<Buffer>(std::move(packet));}catch(const std::bad_alloc&){return {};}
        }
        std::shared_ptr<void> retain(Actor& actor) override {return pin(actor);}
        void enqueue(Actor& actor,UserViewBuffer& abstract) override {
            auto owner=pin(actor);
            if(buffers_) {auto& user=connection_(*owner);if(user.id!=actor.id())throw std::logic_error("View connection belongs to another actor");buffers_->enqueue(user,abstract);return;}
            auto* buffer=dynamic_cast<Buffer*>(&abstract);if(!buffer)throw std::invalid_argument("Foreign view buffer");
            auto& channel=channel_(*owner);
            if(!channel.send)throw std::logic_error("View recipient has no channel");
            channel.send(buffer->packet);
        }
        void leave_snapshot(CellCoordinates) override {throw std::logic_error("Snapshot transport is not bound");}
        void enter_snapshot(CellCoordinates) override {throw std::logic_error("Snapshot transport is not bound");}
    private:
        VerticalSessionRegistry<Session>& registry_;ZoneUsers& users_;Entry& entry_;Channel& channel_;UserViewSharedBuffers* buffers_;Connection& connection_;
    };
    static ZoneUsers& users(RuntimeZone& zone) {
        if(!zone.users)throw std::invalid_argument("Movement requires zone users");return *zone.users;
    }
    static net::WorldChannel checked_channel(const Channel& channel,Session& session) {
        if(!channel)throw std::invalid_argument("Movement requires a channel accessor");
        auto result=channel(session);if(!result.send)throw std::invalid_argument("Movement requires a live channel");return result;
    }
public:
    VerticalMoveRetainedSession(VerticalSessionRegistry<Session>& registry,Session& owner,Actor& actor,
        RuntimeZone& zone,WorldAdmissionUser& user,ZoneEntryState& entry,VerticalMotionState& motion,
        VerticalMovePacketServices& movement,Entry entry_of,Channel channel_of,StackBytes bytes,
        UserViewSharedBuffers* buffers=nullptr,Connection connection_of={})
        :registry_(registry),owner_(owner),actor_(actor),entry_of_(std::move(entry_of)),channel_of_(std::move(channel_of)),connection_of_(std::move(connection_of)),
         recipients_(registry,users(zone),entry_of_,channel_of_,buffers,connection_of_),
         viewer_(checked_channel(channel_of_,owner),[this](Actor& a){return recipients_.retain(a);},
             [this](const Actor& a){return recipients_.appearance(a);},std::move(bytes)),
         delivery_(zone,recipients_,viewer_),movement_(actor,zone,user,entry,motion,delivery_.transport(),movement) {
        if((buffers && !connection_of_) || (!buffers && connection_of_))throw std::invalid_argument("Movement shared view requires buffer and connection owners");
        if(!entry_of_ || &entry_of_(owner)!=&entry || registry_.retain_actor(actor).get()!=&owner)
            throw std::logic_error("Movement requires the registered actor and admission owner");
    }
    std::optional<VerticalMoveResult> receive(std::span<const std::uint8_t> packet) {
        auto owner=registry_.retain_actor(actor_);
        if(owner.get()!=&owner_)throw std::logic_error("Movement session is no longer registered");
        if(packet.size()<2 || packet[0]!=1 || packet[1]!=5)return {};
        if(in_movement_)throw std::logic_error("Reentrant movement for retained actor");
        struct Guard {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}} guard(in_movement_);
        return movement_.receive(packet);
    }
    VerticalMoveRetainedSession(const VerticalMoveRetainedSession&)=delete;
    VerticalMoveRetainedSession& operator=(const VerticalMoveRetainedSession&)=delete;
private:
    VerticalSessionRegistry<Session>& registry_;Session& owner_;Actor& actor_;Entry entry_of_;Channel channel_of_;Connection connection_of_;
    Recipients recipients_;UserCellSnapshotChannel viewer_;UserViewZoneDelivery delivery_;VerticalMoveSession movement_;bool in_movement_=false;
};
}
