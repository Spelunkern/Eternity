#pragma once
#include "eternity/game/vertical_world_party_directory.hpp"
#include "eternity/game/party_runtime_raid_requests.hpp"
#include "eternity/game/world_lifecycle.hpp"
#include "eternity/game/vertical_world_session_gameplay.hpp"

namespace eternity::game {
// Real recovered normal/raid request dispatcher around retained session views.
// Packet checks cover recovered field reads; callers serialize on the world
// strand. Party, lock, connection and raw raid owners outlive this adapter.
// ps_game.exe CUser::PacketParty 004676c0 and SetRaidParty 004458f0.
class VerticalWorldPartyRequests final:public PartyRaidRequestsWorld {
public:
    using Close=std::function<void(const PartyRequestUser&,std::int32_t,std::int32_t)>;
    using Remaining=std::function<void(const PartyRequestUser&,net::Packet&)>;
    VerticalWorldPartyRequests(VerticalWorldPartyDirectory& directory,PartyMembershipWorld& membership,
        PartyRegistry& registry,PartyRegistryPorts& registry_ports,Close close,Remaining remaining)
        :directory_(directory),membership_(membership),registry_(registry),registry_ports_(registry_ports),close_(std::move(close)),remaining_(std::move(remaining)) {
        if(!close_ || !remaining_)throw std::invalid_argument("Incomplete live party request services");
    }
    VerticalWorldPartyRequests(const VerticalWorldPartyRequests&)=delete;
    VerticalWorldPartyRequests& operator=(const VerticalWorldPartyRequests&)=delete;
    // The world composition owns this counter and serializes all accesses.
    // It must outlive this dispatcher and its bound lifecycle callbacks.
    void bind_activity_counter(std::size_t& counter) {
        if(activity_)throw std::logic_error("Party activity counter already bound");activity_=&counter;
    }
    void receive(VerticalWorldSession& session,net::Packet& packet) {
        Activity activity(activity_);
        if(packet.size()<2)throw std::invalid_argument("Malformed party request");
        if(packet[1]!=0x0b)throw std::invalid_argument("Not a party request");
        const auto required=minimum_size(packet[0]);
        if(packet.size()<required)throw std::invalid_argument("Truncated party request");
        auto self=directory_.retain(session.party_user());
        if(self.lifetime.get()!=&session || session.closing())throw std::logic_error("Party request session is not live");
        dispatch_party_request(std::move(self),packet,*this);
    }
    // Compose with the gameplay owner's mandatory remaining-opcode service.
    // The decoded packet is copied because the original name handlers modify
    // byte 22. This adapter and all services must outlive the returned function.
    VerticalWorldSessionGameplay::Other bind_gameplay(VerticalWorldSessionGameplay::Other next) {
        if(!next)throw std::invalid_argument("Missing remaining gameplay service");
        return [this,next=std::move(next)](VerticalWorldSession& session,const net::Packet& packet) {
            if(packet.size()<2)throw std::invalid_argument("Malformed gameplay packet");
            if(packet[1]!=0x0b){next(session,packet);return;}
            auto copy=packet;receive(session,copy);
        };
    }
    // Snapshot immediately before LeaveWorld on the serialized world strand.
    std::optional<bool> lifecycle_party(VerticalWorldSession& session) {
        auto view=directory_.retain(session.party_user());
        if(view.lifetime.get()!=&session)throw std::logic_error("Party lifecycle lost its session");
        auto* party=view.member.party;if(!party)return {};
        auto* registered=find_registry_party(registry_,party->id,registry_ports_);
        if(!registered || static_cast<PartyNormalMembership*>(registered)!=party || party->experience.kind>1)
            throw std::logic_error("Party lifecycle has foreign membership");
        return party->experience.kind==1;
    }
    WorldLifecyclePorts bind_world_release(VerticalWorldSession& session,WorldLifecyclePorts ports) {
        if(!ports.step)throw std::invalid_argument("Missing world lifecycle service");
        (void)directory_.retain(session.party_user());
        std::weak_ptr<VerticalWorldSession> weak=session.shared_from_this();auto next=std::move(ports.step);
        auto started=std::make_shared<bool>(false);
        ports.step=[this,weak,next=std::move(next),started](WorldLifecycleStep step) {
            if(step!=WorldLifecycleStep::leave_party && step!=WorldLifecycleStep::leave_raid){next(step);return;}
            Activity activity(activity_);
            if(*started)throw std::logic_error("Party release already started");
            auto owner=weak.lock();if(!owner)throw std::logic_error("Party release lost its session");
            auto kind=lifecycle_party(*owner);
            if(!kind || *kind!=(step==WorldLifecycleStep::leave_raid))throw std::logic_error("Party release kind changed");
            auto& member=owner->party_user();auto* registered=find_registry_party(registry_,member.party->id,registry_ports_);
            if(!registered || static_cast<PartyNormalMembership*>(registered)!=member.party)throw std::logic_error("Party release lost membership");
            *started=true;
            const bool removed=*kind?leave_raid_party(*registered,member,membership_):leave_normal_party(*registered,member,membership_);
            if(!removed || member.party)throw std::logic_error("Party release did not remove member");
        };
        return ports;
    }
    PartyMembershipWorld& membership_world() override {return membership_;}
    PartyRegistry& registry() override {return registry_;}
    PartyRegistryPorts& registry_ports() override {return registry_ports_;}
    std::optional<PartyRequestUser> find_zone_user(const PartyRequestUser& source,std::uint32_t id) override {return directory_.find_zone(source,id);}
    std::optional<PartyRequestUser> find_user_name(std::string_view name) override {return directory_.find_name(name);}
    std::optional<PartyRequestUser> find_active_raid_user_name(std::string_view name) override {return directory_.find_active_name(name);}
    PartyRequestUser retain_member(PartyMembershipUser& user) override {return directory_.retain(user);}
    void close_connection(const PartyRequestUser& user,std::int32_t reason,std::int32_t detail) override {close_(user,reason,detail);}
    void remaining_raid_request(const PartyRequestUser& user,net::Packet& packet) override {remaining_(user,packet);}
private:
    struct Activity {
        std::size_t* value;
        explicit Activity(std::size_t* counter):value(counter) {
            if(value) {if(*value==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Party activity overflow");++*value;}
        }
        ~Activity(){if(value)--*value;}
    };
    static std::size_t minimum_size(std::uint8_t opcode) noexcept {
        switch(opcode) {
        case 0x03: case 0x10: case 0x15:return 3;
        case 0x02: case 0x06: case 0x07: case 0x0f: case 0x13: case 0x14: case 0x17: case 0x1a:return 6;
        case 0x0e:return 8;
        case 0x12:return 10;
        case 0x09: case 0x11: case 0x18:return 23;
        default:return 2;
        }
    }
    VerticalWorldPartyDirectory& directory_;PartyMembershipWorld& membership_;PartyRegistry& registry_;PartyRegistryPorts& registry_ports_;Close close_;Remaining remaining_;std::size_t* activity_=nullptr;
};
}
