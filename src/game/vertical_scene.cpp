#include "eternity/game/vertical_scene.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
VerticalScene::VerticalScene(net::WorldChannel channel,ZoneUsers& zone,PartyMembershipUser& user,
    PartyMembershipSendWorld& party_world,VerticalSceneServices& services,ActorEffectDatabase& effect_database)
    :VerticalScene(std::move(channel),[&zone]{return &zone;},user,party_world,services,effect_database) {}
VerticalScene::VerticalScene(net::WorldChannel channel,std::function<ZoneUsers*()> zone,PartyMembershipUser& user,
    PartyMembershipSendWorld& party_world,VerticalSceneServices& services,ActorEffectDatabase& effect_database)
    :channel_(std::move(channel)),zone_(std::move(zone)),user_(user),party_world_(party_world),services_(services),effect_database_(effect_database) {
    if(!channel_.send || !zone_) throw std::invalid_argument("Scene requires the real connection and zone resolver");
}
void VerticalScene::send_effect_database(const net::Packet& packet) noexcept {effect_database_.send_effect_database(packet);}
void VerticalScene::bind(Actor& a) {
    auto* zone=zone_();if(actor_ || a.id()!=user_.id || a.status().in_zone || (zone && zone->find_actor(a.id()))) throw std::invalid_argument("Invalid scene Actor binding");
    actor_=&a;
}
void VerticalScene::unbind() {
    if(!actor_) throw std::logic_error("Scene is not bound");
    auto* zone=zone_();if(actor_->status().in_zone || (zone && zone->find_actor(actor_->id())==actor_)) throw std::logic_error("Detach Actor from zone before scene unbind");
    actor_=nullptr;
}
Actor& VerticalScene::actor() const {if(!actor_) throw std::logic_error("Scene has no retained Actor");return *actor_;}
void VerticalScene::send_self(const net::Packet& p) noexcept {
    try {channel_.send(p);} catch(...) {services_.transport_failure(VerticalSceneFailure::self_send,std::current_exception());}
}
ZoneUsers* VerticalScene::user_zone() {
    auto& a=actor();auto* zone=zone_();if(!zone){if(a.status().in_zone)throw std::logic_error("Attached actor has no zone owner");return nullptr;}auto* member=zone->find_actor(a.id());
    if(member && member!=&a) throw std::logic_error("Zone Actor identity changed");
    if(!member && a.status().in_zone) throw std::logic_error("Actor is attached to another zone");
    return member?zone:nullptr;
}
void VerticalScene::send_nearby(const net::Packet& p) {if(auto* z=user_zone()) services_.nearby(*z,actor(),p);}
void VerticalScene::send_cell_view(const net::Packet& p) {if(auto* z=user_zone()) services_.cell_view(*z,actor(),p);}
std::optional<bool> VerticalScene::party_kind() const noexcept {
    bool locked=false;PartyRegistryParty* p=nullptr;
    try {
        p=services_.party(user_);if(!p) {if(user_.party) throw std::logic_error("Missing retained party owner");return {};}
        party_world_.lock(*p);locked=true;std::optional<bool> result;
        if(user_.party==p) {if(p->experience.kind>1) throw std::logic_error("Invalid party kind");result=p->experience.kind==1;}
        party_world_.unlock(*p);return result;
    } catch(...) {
        auto error=std::current_exception();if(locked) party_world_.unlock(*p);
        services_.transport_failure(VerticalSceneFailure::party_state,error);return {};
    }
}
void VerticalScene::send_party(const net::Packet& packet) noexcept {
    PartyRegistryParty* p=nullptr;PartyMembershipSendBuffer* buffer=nullptr;bool locked=false;
    const auto release=[&](PartyMembershipSendBuffer* b) noexcept {
        try {release_party_membership_buffer(*b,party_world_);}
        catch(...) {services_.transport_failure(VerticalSceneFailure::party_send,std::current_exception());}
    };
    try {
        p=services_.party(user_);if(!p) {if(user_.party) throw std::logic_error("Missing retained party owner");return;}
        // Same buffer sizes and framing as allocate_party_membership_send_buffer;
        // keep the allocated reference visible if vector allocation throws.
        if(packet.size()>2046) throw std::length_error("Party send exceeds original buffer capacity");
        const std::uint16_t capacity=packet.size()<31?32:packet.size()<127?128:2048;
        buffer=party_world_.allocate_send_buffer(capacity);
        if(!buffer) {services_.transport_failure(VerticalSceneFailure::party_buffer_unavailable,{});return;}
        buffer->references.store(1,std::memory_order_seq_cst);buffer->custom_references=0;buffer->capacity=capacity;
        buffer->frame.resize(packet.size()+2);
        const auto size=static_cast<std::uint16_t>(buffer->frame.size());
        buffer->frame[0]=static_cast<std::uint8_t>(size);buffer->frame[1]=static_cast<std::uint8_t>(size>>8);
        std::copy(packet.begin(),packet.end(),buffer->frame.begin()+2);
        // CParty::Send 00445680: shared buffer, captured recursive party lock,
        // live member slots/count, source excluded, release after send.
        party_world_.lock(*p);locked=true;
        if(user_.party==p) for(std::int32_t i=0;i<p->count;++i) {
            if(i>=30) throw std::out_of_range("Party member count exceeds retained slots");
            auto* member=i<7?p->slots[static_cast<std::size_t>(i)]:p->extra_slots[static_cast<std::size_t>(i-7)];
            if(member && member!=&user_ && member->send_socket!=-1) {
                buffer->references.fetch_add(1,std::memory_order_seq_cst);
                try {party_world_.enqueue_send(*member,*buffer);}
                catch(...) {release(buffer);throw;}
            }
        }
        party_world_.unlock(*p);locked=false;auto* completed=buffer;buffer=nullptr;release(completed);
    } catch(...) {
        auto error=std::current_exception();if(locked) party_world_.unlock(*p);
        if(buffer) release(buffer);
        services_.transport_failure(VerticalSceneFailure::party_send,error);
    }
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
