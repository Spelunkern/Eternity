#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/party_runtime_registry.hpp"
#include "eternity/game/party_runtime_membership_send.hpp"
#include "eternity/net/world_server.hpp"
#include <exception>

namespace eternity::game {
// All methods are required live owners. The user-effect CSkill pool and the
// original cell/priority broadcaster are not yet exposed by canonical owners.
// Views must use this exact zone and actor, including the original direct and
// priority sends, cell cursor and packet-buffer behavior; do not silently succeed.
enum class VerticalSceneFailure {self_send,party_state,party_buffer_unavailable,party_send};
class VerticalSceneServices {
public:
    virtual ~VerticalSceneServices()=default;
    // Must record the fault and initiate connection/world failure handling.
    // A null exception identifies an unavailable party buffer. Party failure
    // can follow partial delivery. The callback must not throw or destroy the
    // scene synchronously; retained subjects survive until this call returns.
    virtual void transport_failure(VerticalSceneFailure,std::exception_ptr) noexcept=0;
    virtual void nearby(ZoneUsers&,Actor&,const net::Packet&)=0;
    virtual void cell_view(ZoneUsers&,Actor&,const net::Packet&)=0;
    // Returns the actual retained registry party corresponding to user.party.
    // A returned pointer and its lock remain valid throughout the scene call.
    virtual PartyRegistryParty* party(PartyMembershipUser&) noexcept=0;
    virtual std::optional<std::uint32_t> allocate_effect_id() noexcept=0;
    virtual void release_effect_id(std::uint32_t) noexcept=0;
    virtual std::uint32_t tick_ms() const noexcept=0;
    virtual void reset_damage(Actor&)=0; // Actor already reset its own ledger.
    virtual void stop_target_mob(Actor&,std::uint32_t)=0;
    virtual void taunt(Actor&,std::uint16_t,float)=0;
    virtual UserShapeSocial user_shape_social(Actor&)=0;
    virtual void finish_periodic_damage(Actor&,EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t)=0;
};
// Per-connection scene: all calls and membership/party mutations must run on
// the common serialized world executor. The channel is the actual WorldServer
// channel; sends are asynchronous and can become inert after disconnect.
// Zone, channel owner, user, party send pool, services and scene outlive Actor,
// including destructor effect-ID releases. Bind once immediately after Actor
// construction, detach the Actor from ZoneUsers before unbind/destruction, and
// unbind before destroying the Actor. All callbacks must retain their subjects
// through synchronous reentry. Party membership and scene must not be recycled
// during callbacks. A failing free must leave the buffer in an owner-defined
// safe state; its exception is reported without attempting a second release.
// Unlock is noexcept and must not throw.
// A throwing party lock must not leave the lock held; a throwing enqueue must
// not retain the reserved buffer reference or enqueue it. Successful enqueue
// owns its reference, even if it completes synchronously.
class VerticalScene final:public ActorScene {
public:
    VerticalScene(net::WorldChannel,ZoneUsers&,PartyMembershipUser&,PartyMembershipSendWorld&,VerticalSceneServices&);
    void bind(Actor&);
    void unbind();
    void send_self(const net::Packet&) noexcept override;
    void send_nearby(const net::Packet&) override;
    void send_cell_view(const net::Packet&) override;
    std::optional<bool> party_kind() const noexcept override;
    void send_party(const net::Packet&) noexcept override;
    void reset_damage() override;
    std::optional<std::uint32_t> allocate_effect_id() noexcept override;
    void release_effect_id(std::uint32_t) noexcept override;
    std::uint32_t tick_ms() const noexcept override;
    void stop_target_mob(std::uint32_t) override;
    void taunt(std::uint16_t,float) override;
    ZoneUsers* user_zone() override;
    UserShapeSocial user_shape_social() override;
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override;
private:
    Actor& actor() const;
    net::WorldChannel channel_;ZoneUsers& zone_;PartyMembershipUser& user_;
    PartyMembershipSendWorld& party_world_;VerticalSceneServices& services_;Actor* actor_=nullptr;
};
}
