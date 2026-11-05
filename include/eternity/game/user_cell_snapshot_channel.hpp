#pragma once
#include "eternity/game/user_cell_snapshot.hpp"
#include "eternity/net/world_server.hpp"

namespace eternity::game {
// A channel is the moving character's actual WorldChannel. Retention and live
// appearance come from its serialized world owner, and the byte policy is an
// explicit choice until original-client behavior is verified.
class UserCellSnapshotChannel final:public UserCellSnapshotServices {
public:
    using Appearance=std::function<UserViewLiveAppearance(const Actor&)>;
    using StackBytes=std::function<std::array<std::uint8_t,4>()>;
    UserCellSnapshotChannel(net::WorldChannel channel,ZoneUsers::RetainActor retain,Appearance appearance,StackBytes bytes)
        :channel_(std::move(channel)),retain_(std::move(retain)),appearance_(std::move(appearance)),bytes_(std::move(bytes)) {
        if(!channel_.send || !retain_ || !appearance_ || !bytes_)throw std::invalid_argument("Snapshot channel requires live providers");
    }
    std::shared_ptr<void> retain(Actor& actor) override{return retain_(actor);}
    UserViewLiveAppearance appearance(const Actor& actor) override{return appearance_(actor);}
    std::array<std::uint8_t,4> speed_stack_bytes() override{return bytes_();}
    void send_self(const net::Packet& packet) override{channel_.send(packet);}
private:
    net::WorldChannel channel_;ZoneUsers::RetainActor retain_;Appearance appearance_;StackBytes bytes_;
};
}
