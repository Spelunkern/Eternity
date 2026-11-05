#pragma once
#include "eternity/game/vertical_move_world_send_ports.hpp"

namespace eternity::game {
struct WorldZoneSendMessage;
// Base is WorldSendServices and Native is WorldNativeSendServices. Delegate
// actual zone delivery and worker policy to the coordinator; guard its queue
// flush with the same movement lifecycle fence used by disconnect.
template<class Base,class Native,class Ports>
class VerticalMoveWorldSendServices final:public Base {
public:
    explicit VerticalMoveWorldSendServices(std::shared_ptr<Ports> ports)
        :ports_(std::move(ports)),native_(ports_) {
        if(!ports_)throw std::invalid_argument("World send requires retained movement owners");
    }
    void deliver(RuntimeZone& zone,const WorldZoneSendMessage& message) override {native_.deliver(zone,message);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override {return ports_->retain_waiting(user);}
    void flush_waiting(WorldAdmissionUser& user) override {
        auto owner=ports_->waiting_session(user);
        ports_->with_priority_flush(*owner,[&]{native_.flush_waiting(user);});
    }
    std::shared_ptr<void> retain_actor(Actor& actor) override {return ports_->retain_actor(actor);}
    void flush_actor(Actor& actor) override {
        auto owner=ports_->actor_session(actor);
        ports_->with_priority_flush(*owner,[&]{native_.flush_actor(actor);});
    }
private:
    std::shared_ptr<Ports> ports_;Native native_;
};
}
