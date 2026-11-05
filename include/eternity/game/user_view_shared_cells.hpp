#pragma once
#include "eternity/game/user_view_cell_transport.hpp"
#include "eternity/game/user_view_shared_buffer.hpp"

namespace eternity::game {
// Shared original-style packet buffers bound to the live cell publication path.
// The real world owner supplies actor/session pinning, live appearance, entity
// snapshots, and an actor-to-connection lookup retained through enqueue.
class UserViewSharedCellServices:public UserViewCellServices {
public:
    explicit UserViewSharedCellServices(UserViewSharedBuffers& buffers):buffers_(buffers){}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet packet) final{return buffers_.allocate(packet);}
    void enqueue(Actor& actor,UserViewBuffer& buffer) final {
        auto& user=connection(actor);
        if(user.id!=actor.id())throw std::logic_error("Cell recipient connection belongs to another actor");
        buffers_.enqueue(user,buffer);
    }
    virtual PartyMembershipUser& connection(Actor&)=0;
private:
    UserViewSharedBuffers& buffers_;
};
}
