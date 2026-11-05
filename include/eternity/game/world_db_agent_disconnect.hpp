#pragma once
#include "eternity/game/world_db_agent_save.hpp"
#include <functional>
#include <stdexcept>

namespace eternity::game {
// ps_dbAgent.exe CServer::EndRemoveConnection 0040c950 and
// CUserMgr::SaveAll 0041d2f0. Call only for a removed game connection, after
// its connection-owner unlink; user traversal posts saves but does not drain.
template<class Connection,class World> class WorldDbAgentDisconnect final {
public:
    using TraverseUsers=std::function<void(Connection&)>;
    WorldDbAgentDisconnect(World& world,TraverseUsers users):world_(world),users_(std::move(users)) {
        if(!users_)throw std::invalid_argument("DB-agent disconnect requires real connection-specific user traversal");
    }
    WorldDbAgentSaveResult removed_game_connection(Connection& connection) {
        users_(connection);
        return world_.save_all();
    }
private:
    World& world_;TraverseUsers users_;
};
}
