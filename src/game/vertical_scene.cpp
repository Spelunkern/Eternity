#include "eternity/game/vertical_scene.hpp"
#include <stdexcept>

namespace eternity::game {
VerticalScene::VerticalScene(net::WorldChannel channel,ZoneUsers& zone,PartyMembershipUser& user,
    PartyMembershipSendWorld& party_world,VerticalSceneServices& services)
    :channel_(std::move(channel)),zone_(zone),user_(user),party_world_(party_world),services_(services) {
    if(!channel_.send) throw std::invalid_argument("Scene requires the real connection send channel");
}
void VerticalScene::bind(Actor& a) {
    if(actor_ || a.id()!=user_.id || zone_.find_actor(a.id())) throw std::invalid_argument("Invalid scene Actor binding");
    actor_=&a;
}
void VerticalScene::unbind() {
    if(!actor_) throw std::logic_error("Scene is not bound");
    if(zone_.find_actor(actor_->id())==actor_) throw std::logic_error("Detach Actor from zone before scene unbind");
    actor_=nullptr;
}
Actor& VerticalScene::actor() const {if(!actor_) throw std::logic_error("Scene has no retained Actor");return *actor_;}
void VerticalScene::send_self(const net::Packet& p) noexcept {channel_.send(p);}
ZoneUsers* VerticalScene::user_zone() {
    auto& a=actor();auto* member=zone_.find_actor(a.id());
    if(member && member!=&a) throw std::logic_error("Zone Actor identity changed");
    return member?&zone_:nullptr;
}
void VerticalScene::send_nearby(const net::Packet& p) {if(auto* z=user_zone()) services_.nearby(*z,actor(),p);}
void VerticalScene::send_cell_view(const net::Packet& p) {if(auto* z=user_zone()) services_.cell_view(*z,actor(),p);}
std::optional<bool> VerticalScene::party_kind() const noexcept {
    auto* p=services_.party(user_);if(!p) {if(user_.party) std::terminate();return {};}
    party_world_.lock(*p);std::optional<bool> result;
    if(user_.party==p) {if(p->experience.kind>1) std::terminate();result=p->experience.kind==1;}
    party_world_.unlock(*p);return result;
}
void VerticalScene::send_party(const net::Packet& packet) noexcept {
    auto* p=services_.party(user_);if(!p) {if(user_.party) std::terminate();return;}
    // CParty::Send 00445680: one actual pooled buffer, captured recursive
    // party lock, live member slots/count, source excluded, release after send.
    auto* buffer=allocate_party_membership_send_buffer(packet,party_world_);if(!buffer) return;
    party_world_.lock(*p);
    if(user_.party==p) {
        for(std::int32_t i=0;i<p->count;++i) {
            if(i>=30 || (p->experience.kind==0 && i>=7) || p->experience.kind>1) std::terminate();
            auto* member=i<7?p->slots[static_cast<std::size_t>(i)]:p->extra_slots[static_cast<std::size_t>(i-7)];
            if(member && member!=&user_ && member->send_socket!=-1) {
                buffer->references.fetch_add(1,std::memory_order_seq_cst);party_world_.enqueue_send(*member,*buffer);
            }
        }
    }
    party_world_.unlock(*p);release_party_membership_buffer(*buffer,party_world_);
}
void VerticalScene::reset_damage() {services_.reset_damage(actor());}
std::optional<std::uint32_t> VerticalScene::allocate_effect_id() noexcept {return services_.allocate_effect_id();}
void VerticalScene::release_effect_id(std::uint32_t id) noexcept {services_.release_effect_id(id);}
std::uint32_t VerticalScene::tick_ms() const noexcept {return services_.tick_ms();}
void VerticalScene::stop_target_mob(std::uint32_t id) {services_.stop_target_mob(actor(),id);}
void VerticalScene::taunt(std::uint16_t aggro,float range) {services_.taunt(actor(),aggro,range);}
UserShapeSocial VerticalScene::user_shape_social() {return services_.user_shape_social(actor());}
void VerticalScene::finish_periodic_damage(EffectStatus& status,const SkillDefinition& skill,const EffectSource& source,std::uint32_t elapsed) {services_.finish_periodic_damage(actor(),status,skill,source,elapsed);}
}
