#pragma once
#include "eternity/game/user_priority_area.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"

namespace eternity::game {
// Real world owner supplies the actor's retained connection and its exact
// per-connection queue. The pool, queue and users survive all callbacks and
// pending sends; all access is serialized by the connection/world owner.
class UserPrioritySharedCellServices:public UserPriorityAreaServices {
public:
    explicit UserPrioritySharedCellServices(UserViewSharedBuffers& buffers):buffers_(buffers){}
    std::unique_ptr<UserViewBuffer> allocate(const net::Packet& packet) final{return buffers_.allocate(packet);}
    void immediate(Actor& actor,UserViewBuffer& buffer) final {
        auto& user=connection(actor);validate(actor,user);buffers_.enqueue(user,buffer);
    }
    void priority(Actor& actor,UserViewBuffer& buffer,std::uint8_t priority,float distance) final {
        auto& user=connection(actor);validate(actor,user);
        // CUser::PSend 004bd6f0 checks the closed socket before accessing its queue.
        if(user.send_socket==-1)return;
        buffers_.enqueue_priority(user,buffer,connection_queue(actor),priority,distance);
    }
    virtual PartyMembershipUser& connection(Actor&)=0;
    virtual UserPriorityQueue& connection_queue(Actor&)=0;
private:
    static void validate(const Actor& actor,const PartyMembershipUser& user) {
        if(actor.id()!=user.id)throw std::logic_error("Priority recipient belongs to another actor");
    }
    UserViewSharedBuffers& buffers_;
};
}
