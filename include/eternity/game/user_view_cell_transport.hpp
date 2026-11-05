#pragma once
#include "eternity/game/user_view_actor_publication.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
// Supplies the real retained user/session, shared packet pool, ordered socket
// queue and complete entity snapshots. The actor and zone survive every call;
// retain must not change membership, and queued sends retain their own buffer
// references. Provider methods must not throw or synchronously destroy subjects.
class UserViewCellServices {
public:
    virtual ~UserViewCellServices()=default;
    virtual UserViewLiveAppearance appearance(const Actor&)=0;
    virtual std::unique_ptr<UserViewBuffer> allocate(net::Packet)=0;
    virtual std::shared_ptr<void> retain(Actor&)=0;
    virtual void enqueue(Actor&,UserViewBuffer&)=0;
    virtual void leave_snapshot(CellCoordinates)=0;
    virtual void enter_snapshot(CellCoordinates)=0;
};
// Uses the canonical zone's local-link recipient traversal. No filtering is
// added here: the real enqueue service applies the connection/socket state.
class UserViewCellTransport final:public UserViewActorTransport {
public:
    UserViewCellTransport(ZoneUsers& zone,UserViewCellServices& services):zone_(zone),services_(services){}
    UserViewLiveAppearance appearance(const Actor& actor) override{return services_.appearance(actor);}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet packet) override{return services_.allocate(std::move(packet));}
    void broadcast(CellCoordinates cell,UserViewBuffer& buffer) override {
        zone_.broadcast_cell(cell.x,cell.z,[&](Actor& actor){return services_.retain(actor);},[&](Actor& actor){services_.enqueue(actor,buffer);});
    }
    void leave_snapshot(CellCoordinates cell) override{services_.leave_snapshot(cell);}
    void enter_snapshot(CellCoordinates cell) override{services_.enter_snapshot(cell);}
private:
    ZoneUsers& zone_;UserViewCellServices& services_;
};
}
