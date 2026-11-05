#pragma once
#include "eternity/game/world_send_phase.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"
#include "eternity/game/world_connection_priority_flush.hpp"
#include "eternity/net/world_server.hpp"

namespace eternity::game {
// The transport owner pins each actual session and binds its channel and
// priority queue. Methods run on the serialized world executor; callbacks
// must not synchronously invalidate the retained actor or zone.
class WorldNativeSendPorts {
public:
    virtual ~WorldNativeSendPorts()=default;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    virtual net::WorldChannel& channel(Actor&)=0;
    virtual UserViewSharedBuffers& buffers()=0;
    virtual PartyMembershipUser& connection(Actor&)=0;
    virtual UserPriorityQueue& priority_queue(Actor&)=0;
    virtual PartyMembershipSendWorld& priority_pool()=0;
    virtual bool can_submit_priority(Actor&)=0;
    virtual std::shared_ptr<void> retain_waiting(WorldAdmissionUser&)=0;
    // These four objects must belong to the same retained waiting session.
    virtual PartyMembershipUser& waiting_connection(WorldAdmissionUser&)=0;
    virtual UserPriorityQueue& waiting_priority_queue(WorldAdmissionUser&)=0;
    virtual net::WorldChannel& waiting_channel(WorldAdmissionUser&)=0;
    virtual bool can_submit_waiting_priority(WorldAdmissionUser&)=0;
};
// CZone::SendAll 00420870 / SendAllCountry 00420590 visit in-zone users only.
// WorldChannel owns its plaintext copy and performs socket-side framing.
class WorldNativeSendServices final:public WorldSendServices {
public:
    explicit WorldNativeSendServices(std::shared_ptr<WorldNativeSendPorts> ports):ports_(std::move(ports)) {
        if(!ports_)throw std::invalid_argument("World send requires actual session and priority transport owners");
    }
    void deliver(RuntimeZone& zone,const WorldZoneSendMessage& message) override {
        if(!message.payload || message.payload->empty())throw std::invalid_argument("World send payload is missing");
        const bool priority=message.kind==WorldZoneSendKind::priority || message.kind==WorldZoneSendKind::priority_country;
        const bool country=message.kind==WorldZoneSendKind::country || message.kind==WorldZoneSendKind::priority_country;
        if(!priority && message.kind!=WorldZoneSendKind::all && message.kind!=WorldZoneSendKind::country)throw std::invalid_argument("Unknown world send route");
        if(!zone.users)throw std::logic_error("World send zone lacks its user owner");
        std::unique_ptr<UserViewBuffer> buffer;UserViewSharedBuffers* buffers=nullptr;
        if(priority) {buffers=&ports_->buffers();buffer=buffers->allocate(*message.payload);if(!buffer)throw std::runtime_error("World priority send buffer allocation failed");}
        zone.users->visit_all([&](Actor& actor){return ports_->retain_actor(actor);},[&](Actor& actor){
            if(country && actor.appearance().identity.country!=message.country)return;
            if(!priority || actor.id()==message.source || actor.id()==message.destination) {
                auto& channel=ports_->channel(actor);if(!channel.send)throw std::logic_error("World send actor lacks its socket channel");
                channel.send(*message.payload);return;
            }
            auto& connection=ports_->connection(actor);
            if(connection.id!=actor.id())throw std::logic_error("World priority recipient belongs to another actor");
            (void)buffers->enqueue_priority(connection,*buffer,ports_->priority_queue(actor),message.priority,65000.0F);
        });
    }
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override{return ports_->retain_waiting(user);}
    void flush_waiting(WorldAdmissionUser& user) override{
        auto pin=ports_->retain_waiting(user);if(!pin)throw std::logic_error("World flush waiter lacks its session");
        auto& connection=ports_->waiting_connection(user);
        flush_world_connection_priority(connection.send_socket,ports_->waiting_priority_queue(user),ports_->priority_pool(),ports_->waiting_channel(user),[&]{return ports_->can_submit_waiting_priority(user);});
    }
    std::shared_ptr<void> retain_actor(Actor& actor) override{return ports_->retain_actor(actor);}
    void flush_actor(Actor& actor) override{
        auto pin=ports_->retain_actor(actor);if(!pin)throw std::logic_error("World flush actor lacks its session");
        auto& connection=ports_->connection(actor);
        if(connection.id!=actor.id())throw std::logic_error("World flush recipient belongs to another actor");
        flush_world_connection_priority(connection.send_socket,ports_->priority_queue(actor),ports_->priority_pool(),ports_->channel(actor),[&]{return ports_->can_submit_priority(actor);});
    }
private:
    std::shared_ptr<WorldNativeSendPorts> ports_;
};
}
