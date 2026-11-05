#pragma once
#include "eternity/game/user_cell_snapshot.hpp"
#include "eternity/game/corpses.hpp"
#include "eternity/game/zone_mobs.hpp"
#include "eternity/game/zone_npcs.hpp"
#include "eternity/game/ground_items.hpp"
#include "eternity/game/zone_environment.hpp"

namespace eternity::game {
// All registries must refer to the same map and share the serialized world
// owner. Subjects, registries and services must survive all sends and callbacks;
// each registry's documented cursor and retention contract also applies.
class CellSnapshot {
    class Viewer final:public CorpseViewer,public MobViewer,public NpcViewer,public ItemViewer {
    public:
        explicit Viewer(UserCellSnapshotServices& services):services_(services){}
        void send_self(const net::Packet& packet) override {services_.send_self(packet);}
    private:UserCellSnapshotServices& services_;
    };
public:
    CellSnapshot(ZoneUsers& users,ZoneCorpses& corpses,ZoneMobs& mobs,ZoneNpcs& npcs,ZoneItems& items,ZoneEnvironment& environment,UserCellSnapshotServices& services)
        :users_(users),corpses_(corpses),mobs_(mobs),npcs_(npcs),items_(items),environment_(environment),services_(services){}
    void enter(CellCoordinates cell) {
        // ps_game.exe CCell::SendCellInfoEnter 00423aa0.
        Viewer viewer(services_);user_cell_snapshot_enter(users_,cell,services_);
        corpses_.send_enter(cell,viewer);mobs_.send_enter(cell,viewer);npcs_.send_enter(cell,viewer);items_.send_enter(cell,viewer);
        environment_.send_cell_doors(cell,[&](const net::Packet& packet){services_.send_self(packet);});
    }
    void leave(CellCoordinates cell) {
        // ps_game.exe CCell::SendCellInfoLeave 00424130.
        Viewer viewer(services_);user_cell_snapshot_leave(users_,cell,services_);
        corpses_.send_leave(cell,viewer);mobs_.send_leave(cell,viewer);npcs_.send_leave(cell,viewer);items_.send_leave(cell,viewer);
    }
private:
    ZoneUsers& users_;ZoneCorpses& corpses_;ZoneMobs& mobs_;ZoneNpcs& npcs_;ZoneItems& items_;ZoneEnvironment& environment_;UserCellSnapshotServices& services_;
};
}
