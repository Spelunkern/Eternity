#pragma once
#include "eternity/game/entry_packets.hpp"
#include "eternity/game/user_view_packets.hpp"
#include "eternity/game/user_view_publication.hpp"

namespace eternity::game {
struct UserViewLiveAppearance {
    std::uint8_t permission;
    std::uint16_t direction;
    UserPosition position;
    std::int32_t guild_level;
};
// The common serialized world owner supplies the other live fields; position
// comes from the canonical zone membership, never the entry-time snapshot.
inline UserViewLiveAppearance capture_user_view_appearance(const ZoneUsers& zone,const Actor& actor,
    std::uint8_t permission,std::uint16_t direction,std::int32_t guild_level) {
    if(zone.find_actor(actor.id())!=&actor)throw std::logic_error("View actor is not a member of this zone");
    const auto position=zone.position(actor.id());
    if(!position)throw std::logic_error("View actor has no current zone position");
    return {permission,direction,*position,guild_level};
}
// Real owner must retain Actor and zone across all calls, serialize access,
// preserve original packet buffer references and implement both cell snapshots.
class UserViewActorTransport {
public:
    virtual ~UserViewActorTransport()=default;
    virtual UserViewLiveAppearance appearance(const Actor&)=0;
    virtual std::unique_ptr<UserViewBuffer> allocate(net::Packet)=0;
    virtual void broadcast(CellCoordinates,UserViewBuffer&)=0;
    virtual void leave_snapshot(CellCoordinates)=0;
    virtual void enter_snapshot(CellCoordinates)=0;
};
// Binds the canonical Actor to the already recovered publication orchestration.
// It does not supply the transport, cell snapshots or world executor.
class UserViewActorPublication final:public UserViewPublicationServices {
public:
    UserViewActorPublication(Actor& actor,UserViewActorTransport& transport):actor_(actor),transport_(transport){}
    std::unique_ptr<UserViewBuffer> allocate(UserViewBufferKind kind) override {
        switch(kind) {
        case UserViewBufferKind::leave:return transport_.allocate(user_view_leave_packet(actor_.id()));
        case UserViewBufferKind::appearance:{const auto live=transport_.appearance(actor_);return transport_.allocate(user_view_appearance_packet(actor_,live.permission,live.direction,live.position,live.guild_level));}
        case UserViewBufferKind::equipment:return transport_.allocate(character_speed(actor_.id(),actor_.status()));
        case UserViewBufferKind::shape:{auto packet=user_view_shape_packet(actor_);if(!packet)throw std::logic_error("Shape changed between predicate and allocation");return transport_.allocate(std::move(*packet));}
        }
        throw std::logic_error("Unknown user view buffer kind");
    }
    void broadcast(CellCoordinates cell,UserViewBuffer& buffer) override {transport_.broadcast(cell,buffer);}
    void leave_snapshot(CellCoordinates cell) override {transport_.leave_snapshot(cell);}
    void enter_snapshot(CellCoordinates cell) override {transport_.enter_snapshot(cell);}
    bool publish_shape() override {return user_view_shape_packet(actor_).has_value();}
private:
    Actor& actor_;UserViewActorTransport& transport_;
};
}
