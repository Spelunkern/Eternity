#pragma once
#include "eternity/game/vertical_world_inventory_pickup.hpp"
#include "eternity/game/vertical_world_party_runtime.hpp"
#include <map>
#include <cmath>
#include <algorithm>

namespace eternity::game {
// ps_game.exe CParty::Send(range) 00445700. Position reads must retain their
// actual session and refresh from the live owner; sends must consume one buffer
// reference even on a reported transport failure. Calls are strand-serialized.
template<class Position>
void vertical_inventory_party_send_range(PartyRegistryParty& party,PartyMembershipUser& source,
    const net::Packet& packet,PartyMembershipSendWorld& world,Position position) {
    if(packet.size()>2046)throw std::length_error("Party send exceeds original buffer capacity");
    const std::uint16_t capacity=packet.size()<31?32:packet.size()<127?128:2048;
    auto* buffer=world.allocate_send_buffer(capacity);
    if(!buffer)throw std::runtime_error("Pickup party send buffer unavailable");
    buffer->references.store(1,std::memory_order_seq_cst);buffer->custom_references=0;buffer->capacity=capacity;
    bool locked=false;
    try {
        buffer->frame.resize(packet.size()+2);const auto length=static_cast<std::uint16_t>(buffer->frame.size());
        buffer->frame[0]=static_cast<std::uint8_t>(length);buffer->frame[1]=static_cast<std::uint8_t>(length>>8);
        std::copy(packet.begin(),packet.end(),buffer->frame.begin()+2);
        world.lock(party);locked=true;
        for(std::int32_t i=0;i<party.count;++i) {
            if(i>=30)throw std::out_of_range("Party member count exceeds retained slots");
            auto* member=i<7?party.slots[static_cast<std::size_t>(i)]:party.extra_slots[static_cast<std::size_t>(i-7)];
            if(!member || member==&source)continue;
            const auto target=position(*member);const auto origin=position(source);
            const float dx=target[0]-origin[0],dz=target[2]-origin[2];
            const double squared=static_cast<double>(static_cast<long double>(dx)*dx+static_cast<long double>(dz)*dz);
            const float distance=static_cast<float>(std::sqrt(squared));
            if(distance<100.F && member->send_socket!=-1) {
                buffer->references.fetch_add(1,std::memory_order_seq_cst);
                world.enqueue_send(*member,*buffer);
            }
        }
        world.unlock(party);locked=false;
    } catch(...) {
        if(locked)world.unlock(party);release_party_membership_buffer(*buffer,world);throw;
    }
    release_party_membership_buffer(*buffer,world);
}

// Real retained party membership/lock owner for pickup. Remaining required
// services are delegated unchanged.
// All calls and party runtime shutdown are serialized on the world strand;
// this object and the runtime outlive every outstanding lock token.
class VerticalWorldInventoryParty final:public ItemPickupWorld {
public:
    VerticalWorldInventoryParty(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& sessions,
        const std::shared_ptr<VerticalWorldSession>& session,VerticalWorldPartyRuntime& parties,
        std::shared_ptr<ItemPickupWorld> services)
        :strand_(std::move(strand)),sessions_(sessions),session_(session),parties_(parties),services_(std::move(services)) {
        if(!strand_.running_in_this_thread() || !session || !services_)throw std::invalid_argument("Missing inventory party owners");
        parties_.retain_inventory();
    }
    VerticalWorldInventoryParty(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& sessions,
        const std::shared_ptr<VerticalWorldSession>& session,std::shared_ptr<VerticalWorldPartyRuntime> parties,
        std::shared_ptr<ItemPickupWorld> services)
        :VerticalWorldInventoryParty(std::move(strand),sessions,session,required(parties),std::move(services)) {
        retained_parties_=std::move(parties);
    }
    VerticalWorldInventoryParty(const VerticalWorldInventoryParty&)=delete;
    VerticalWorldInventoryParty& operator=(const VerticalWorldInventoryParty&)=delete;
    ~VerticalWorldInventoryParty() override {if(!locks_.empty())std::terminate();parties_.release_inventory();}
    std::uint32_t pick_tick_ms() override {return services_->pick_tick_ms();}
    std::uint32_t pick_party_id(Actor& actor) override {auto owner=checked(actor);auto* party=owner->party_user().party;return party?party->id:0;}
    UserPosition pick_position(const Actor& actor) override {checked(actor);return services_->pick_position(actor);}
    bool pick_quest_active(Actor& actor,std::uint16_t id) override {checked(actor);return services_->pick_quest_active(actor,id);}
    std::uint32_t lock_inventory_party(Actor& actor) override {
        auto owner=checked(actor);auto* captured=owner->party_user().party;if(!captured)return 0;
        auto& registry=parties_.registry();
        std::uint32_t token=next_;do {if(++next_==0)++next_;if(token==0)token=next_;if(!locks_.contains(token))break;token=next_;}while(true);
        auto [entry,inserted]=locks_.emplace(token,captured);(void)inserted;
        try {registry.lock_party(*captured);}catch(...) {locks_.erase(entry);throw;}
        // ItemGet retains the captured lock only if the user's party is still
        // non-null; the current pointer may differ from the captured pointer.
        if(!owner->party_user().party) {registry.unlock_party(*captured);locks_.erase(entry);return 0;}
        return token;
    }
    void unlock_inventory_party(std::uint32_t token) noexcept override {
        auto entry=locks_.find(token);if(entry==locks_.end())std::terminate();
        parties_.registry().unlock_party(*entry->second);locks_.erase(entry);
    }
    std::optional<bool> inventory_party_kind(const Actor& actor) override {
        auto owner=checked(actor);auto* party=owner->party_user().party;if(!party)return {};
        if(party->experience.kind>1)throw std::logic_error("Invalid inventory party kind");return party->experience.kind==1;
    }
    void send_inventory_party(Actor& actor,const net::Packet& packet) override {
        auto owner=checked(actor);auto* party=owner->party_user().party;if(!party)return;
        // Registry owns all membership objects as PartyRegistryParty; reject a
        // foreign pointer before downcasting it.
        auto& registry=parties_.registry();registry.lock_party(*party);registry.unlock_party(*party);
        auto& membership=parties_.membership();auto& directory=parties_.directory();
        vertical_inventory_party_send_range(*static_cast<PartyRegistryParty*>(party),owner->party_user(),packet,membership,
            [&](PartyMembershipUser& member) {
                auto view=directory.retain(member);auto* session=static_cast<VerticalWorldSession*>(view.lifetime.get());
                if(!session || &session->party_user()!=&member)throw std::logic_error("Party pickup lost its retained member");
                if(!member.in_zone)throw std::logic_error("Party pickup member has no live position");
                return member.position;
            });
    }
    void send_inventory_database(const net::Packet& p) override {services_->send_inventory_database(p);}
    void send_money_database(const net::Packet& p) override {services_->send_money_database(p);}
    std::string inventory_item_name(const ItemDefinition& i) override {return services_->inventory_item_name(i);}
    std::string inventory_drop_user_name(Actor& a,std::uint32_t id,bool lookup) override {checked(a);return services_->inventory_drop_user_name(a,id,lookup);}
    bool return_inventory_item(Actor& a,GroundItem& i) override {checked(a);return services_->return_inventory_item(a,i);}
    GameLogSubject money_log_subject(const Actor& a) override {checked(a);return services_->money_log_subject(a);}
    std::chrono::sys_seconds money_local_calendar() override {return services_->money_local_calendar();}
    void send_money_log(const net::Packet& p) override {services_->send_money_log(p);}
private:
    static VerticalWorldPartyRuntime& required(const std::shared_ptr<VerticalWorldPartyRuntime>& parties) {
        if(!parties)throw std::invalid_argument("Missing inventory party runtime");return *parties;
    }
    std::shared_ptr<VerticalWorldSession> checked(const Actor& actor) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Inventory party outside world strand");
        auto owner=session_.lock();
        if(!owner || owner->closing() || !owner->world_user() || &owner->world_user()->actor!=&actor || sessions_.retain_actor(actor)!=owner)
            throw std::logic_error("Inventory party lost its retained actor");
        return owner;
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& sessions_;std::weak_ptr<VerticalWorldSession> session_;
    VerticalWorldPartyRuntime& parties_;std::shared_ptr<ItemPickupWorld> services_;std::shared_ptr<VerticalWorldPartyRuntime> retained_parties_;
    std::map<std::uint32_t,PartyNormalMembership*> locks_;std::uint32_t next_=1;
};
}
