#pragma once
#include "eternity/game/cell_snapshot.hpp"
#include "eternity/game/user_view_cell_transport.hpp"

namespace eternity::game {
// Bridges the existing moving-user publication to complete cell snapshots.
// The owner must bind snapshot services to the moving user's real session, and
// cell services to the real recipient connections. All objects outlive callbacks.
class UserViewSnapshotTransport final:public UserViewCellServices {
public:
    UserViewSnapshotTransport(UserViewCellServices& recipients,CellSnapshot& snapshot):recipients_(recipients),snapshot_(snapshot){}
    UserViewLiveAppearance appearance(const Actor& actor) override{return recipients_.appearance(actor);}
    std::unique_ptr<UserViewBuffer> allocate(net::Packet packet) override{return recipients_.allocate(std::move(packet));}
    std::shared_ptr<void> retain(Actor& actor) override{return recipients_.retain(actor);}
    void enqueue(Actor& actor,UserViewBuffer& buffer) override{recipients_.enqueue(actor,buffer);}
    void leave_snapshot(CellCoordinates cell) override{snapshot_.leave(cell);}
    void enter_snapshot(CellCoordinates cell) override{snapshot_.enter(cell);}
private:UserViewCellServices& recipients_;CellSnapshot& snapshot_;
};
}
