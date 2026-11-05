#pragma once
#include "eternity/game/world_zone_send_queue.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/vertical_world.hpp"

namespace eternity::game {
class WorldSendServices {
public:
    virtual ~WorldSendServices()=default;
    // Delivery implements the four real transport routes for this zone.
    virtual void deliver(RuntimeZone&,const WorldZoneSendMessage&)=0;
    virtual std::shared_ptr<void> retain_waiting(WorldAdmissionUser&)=0;
    virtual void flush_waiting(WorldAdmissionUser&)=0;
    virtual std::shared_ptr<void> retain_actor(Actor&)=0;
    virtual void flush_actor(Actor&)=0;
};
// CWorldThread::Update 004079e8..00407a4c: all world queues first, then
// CZone::UpdateNetSend 00421730 only when the original dirty flag is set.
class WorldSendPhase final {
public:
    explicit WorldSendPhase(std::shared_ptr<WorldSendServices> services):services_(std::move(services)) {
        if(!services_)throw std::invalid_argument("World sends require actual queue and session owners");
    }
    template<class Root> bool update(Root& root,bool net_send_due) {
        auto& world=root.world();if(!world.running())throw std::logic_error("World is not running");
        bool complete=true;
        world.serialized_update([&](VerticalWorld& current){
            const bool queues=root.visit_active_zones([&](RuntimeZone& zone){
                if(!current.running()){complete=false;return;}
                zone.world_sends.drain([&](const WorldZoneSendMessage& message){services_->deliver(zone,message);});
                if(!current.running())complete=false;
            });
            if(!queues || !complete){complete=false;return;}
            if(!net_send_due)return;
            const bool flushed=root.visit_active_zones([&](RuntimeZone& zone){
                if(!current.running()){complete=false;return;}
                if(!zone.users)throw std::logic_error("World send zone lacks its user owner");
                root.admission().visit_zone_waiting(zone,[&](WorldAdmissionUser& user){return services_->retain_waiting(user);},[&](WorldAdmissionUser& user){if(current.running())services_->flush_waiting(user);});
                if(!current.running()){complete=false;return;}
                zone.users->visit_all([&](Actor& actor){return services_->retain_actor(actor);},[&](Actor& actor){if(current.running())services_->flush_actor(actor);});
                if(!current.running())complete=false;
            });
            if(!flushed)complete=false;
        });
        return complete;
    }
private:
    std::shared_ptr<WorldSendServices> services_;
};
}
