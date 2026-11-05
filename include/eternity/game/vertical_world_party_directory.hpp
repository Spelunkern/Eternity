#pragma once
#include "eternity/game/vertical_world_session.hpp"

namespace eternity::game {
// Retained lookups for PartyRequestWorld. The actual party registry, raid
// handlers, packet transport and connection state remain required services.
// All operations use the world strand and the real zone owners outlive calls.
// ps_game.exe CUser::PacketParty 004676c0 / CWorld::FindUserZoneMove.
class VerticalWorldPartyDirectory final {
public:
    VerticalWorldPartyDirectory(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,WorldUsers& users)
        :strand_(std::move(strand)),registry_(registry),users_(users) {}
    PartyRequestUser retain(PartyMembershipUser& member) {
        require_strand();auto owner=registry_.find_character(member.id);
        if(!owner || &owner->party_user()!=&member)throw std::logic_error("Party member lost its retained session");
        return bind(std::move(owner));
    }
    std::optional<PartyRequestUser> find_zone(const PartyRequestUser& source,std::uint32_t character) {
        require_strand();auto source_owner=registry_.find_character(source.member.id);
        if(!source_owner || source.lifetime.get()!=source_owner.get() || &source_owner->party_user()!=&source.member || &source_owner->party_requests()!=&source.state)
            throw std::logic_error("Party request lost its source session");
        (void)bind(source_owner);
        auto* world=users_.find_zone(character);if(!world || !source_owner->admission().zone || world->admission.zone!=source_owner->admission().zone)return {};
        auto owner=registry_.retain_actor(world->actor);if(!owner || owner->world_user()!=world)throw std::logic_error("Party target lost its retained session");
        return bind(std::move(owner));
    }
    std::optional<PartyRequestUser> find_name(std::string_view name) {
        require_strand();auto* world=users_.find_zone_move(name);if(!world)return {};
        auto owner=registry_.retain_actor(world->actor);if(!owner || owner->world_user()!=world)throw std::logic_error("Party target lost its retained session");
        return bind(std::move(owner));
    }
    // Raid auto-join uses the active world name index without a zone filter.
    std::optional<PartyRequestUser> find_active_name(std::string_view name) {
        require_strand();auto* world=users_.find(name);if(!world)return {};
        auto owner=registry_.retain_actor(world->actor);if(!owner || owner->world_user()!=world)throw std::logic_error("Raid target lost its retained session");
        return bind(std::move(owner));
    }
private:
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Party directory outside world strand");}
    PartyRequestUser bind(std::shared_ptr<VerticalWorldSession> owner) {
        auto* world=owner->world_user();if(!world || registry_.retain_actor(world->actor)!=owner)throw std::logic_error("Party request has no retained actor");
        auto& actor=world->actor;auto& member=owner->party_user();auto& state=owner->party_requests();
        if(member.id!=actor.id())throw std::logic_error("Party member and actor identities differ");
        const auto& shape=actor.appearance().identity;const auto current=actor.resources();const auto maximum=actor.status().skills.equipment.base.maximum;
        member.name=shape.name;member.level=actor.status().level;member.job=shape.job;
        member.hp=static_cast<std::uint16_t>(current.hp);member.mp=static_cast<std::uint16_t>(current.mp);member.sp=static_cast<std::uint16_t>(current.sp);
        member.maximum_hp=static_cast<std::uint16_t>(maximum.hp);member.maximum_mp=static_cast<std::uint16_t>(maximum.mp);member.maximum_sp=static_cast<std::uint16_t>(maximum.sp);
        member.in_zone=actor.status().in_zone;state.country=shape.country;state.scene_state=world->admission.location;
        if(state.scene_state==4 && !member.in_zone)throw std::logic_error("Party user is published in a zone without membership");
        state.zone=reinterpret_cast<std::uintptr_t>(world->admission.zone);
        if(member.in_zone) {
            auto* zone=world->admission.zone;if(!zone || !zone->users || zone->users->find_actor(actor.id())!=&actor)throw std::logic_error("Party user lost its zone actor");
            auto position=zone->users->position(actor.id());if(!position)throw std::logic_error("Party user lost its zone position");
            member.position={position->x,position->y,position->z};
        }
        return {member,state,std::move(owner)};
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;WorldUsers& users_;
};
}
