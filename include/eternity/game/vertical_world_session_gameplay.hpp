#pragma once
#include "eternity/game/vertical_world_session_packets.hpp"
#include "eternity/game/vertical_move_session.hpp"

namespace eternity::game {
// One admitted session's actual movement dispatcher. Actor, canonical entry and
// motion state, zone, view transport, packet services and registry must outlive
// this object and be serialized on the world strand. Other opcodes are sent to
// the required live dispatcher; no gameplay result is synthesized.
// ps_game.exe CUser::PacketPC 0046a230 and CZone::MoveUserByPacket 00416550.
class VerticalWorldSessionGameplay final:public std::enable_shared_from_this<VerticalWorldSessionGameplay> {
public:
    using Other=std::function<void(VerticalWorldSession&,const net::Packet&)>;
    static std::shared_ptr<VerticalWorldSessionGameplay> create(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> owner,RuntimeZone& zone,
        ZoneEntryState& entry,VerticalMotionState& motion,UserViewActorTransport& view,
        VerticalMovePacketServices& movement,Other other) {
        auto& actor=checked(strand,registry,owner,zone,entry,other);
        auto retained=std::make_shared<VerticalMoveSession>(actor,zone,owner->admission(),entry,motion,view,movement);
        return make(std::move(strand),registry,owner,actor,zone,[retained](std::span<const std::uint8_t> packet){return retained->receive(packet);},std::move(other));
    }
    // The factory must construct the live retained movement owner from these
    // exact session/actor/zone/admission/entry objects. This permits the
    // coordinator's VerticalMoveRetainedWorld or VerticalMoveRetainedSession
    // without creating a second movement or view state here.
    template<class Factory> static std::shared_ptr<VerticalWorldSessionGameplay> create_retained(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> owner,RuntimeZone& zone,
        ZoneEntryState& entry,Factory factory,Other other) {
        auto& actor=checked(strand,registry,owner,zone,entry,other);
        auto retained=factory(*owner,actor,zone,owner->admission(),entry);
        if(!retained)throw std::invalid_argument("Missing retained movement owner");
        return make(std::move(strand),registry,owner,actor,zone,[retained=std::move(retained)](std::span<const std::uint8_t> packet){return retained->receive(packet);},std::move(other));
    }
    std::optional<VerticalMoveResult> dispatch(VerticalWorldSession& supplied,const net::Packet& packet) {
        require_strand();auto self=shared_from_this();auto owner=owner_.lock();
        if(!owner || owner.get()!=&supplied || (owner->closing() || owner->failed()) || !owner->world_user() || &owner->world_user()->actor!=actor_ ||
           owner->admission().location!=4 || owner->admission().zone!=&zone_ || registry_.retain_actor(*actor_)!=owner)
            throw std::logic_error("Gameplay lost its admitted session");
        if(packet.size()<2)throw std::invalid_argument("Malformed gameplay packet");
        if(packet[0]==1 && packet[1]==5) {
            if(in_movement_)throw std::logic_error("Reentrant movement for session");
            struct Guard {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}} guard(in_movement_);
            auto result=movement_(packet);if(!result)throw std::logic_error("Retained movement owner ignored 0501");return result;
        }
        other_(*owner,packet);return {};
    }
    static VerticalWorldSessionPackets::Dispatch bind(std::shared_ptr<VerticalWorldSessionGameplay> owner) {
        if(!owner)throw std::invalid_argument("Missing session gameplay owner");
        return [owner=std::move(owner)](VerticalWorldSession& session,const net::Packet& packet){(void)owner->dispatch(session,packet);};
    }
private:
    using Movement=std::function<std::optional<VerticalMoveResult>(std::span<const std::uint8_t>)>;
    static Actor& checked(const VerticalWorldSession::Strand& strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& owner,RuntimeZone& zone,ZoneEntryState& entry,const Other& other) {
        if(!strand.running_in_this_thread() || !owner || owner->closing() || owner->failed() || !owner->character() || !owner->world_user() || !other)
            throw std::invalid_argument("Incomplete session gameplay services");
        auto* actor=owner->character()->pipeline().result().actor.get();
        if(!actor || &owner->world_user()->actor!=actor || owner->admission().location!=4 || owner->admission().zone!=&zone ||
           !zone.users || zone.users->find_actor(actor->id())!=actor || !entry.bound || entry.character_id!=actor->id() ||
           registry.retain_actor(*actor)!=owner)
            throw std::logic_error("Gameplay requires the registered admitted actor");
        return *actor;
    }
    static std::shared_ptr<VerticalWorldSessionGameplay> make(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& owner,Actor& actor,RuntimeZone& zone,Movement movement,Other other) {
        return std::shared_ptr<VerticalWorldSessionGameplay>(new VerticalWorldSessionGameplay(std::move(strand),registry,owner,actor,zone,std::move(movement),std::move(other)));
    }
    VerticalWorldSessionGameplay(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& owner,Actor& actor,RuntimeZone& zone,Movement movement,Other other)
        :strand_(std::move(strand)),registry_(registry),owner_(owner),actor_(&actor),zone_(zone),movement_(std::move(movement)),other_(std::move(other)){}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session gameplay outside world strand");}
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::weak_ptr<VerticalWorldSession> owner_;Actor* actor_;RuntimeZone& zone_;
    Movement movement_;Other other_;bool in_movement_=false;
};
}
