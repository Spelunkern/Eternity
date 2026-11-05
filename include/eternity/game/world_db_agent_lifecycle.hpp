#pragma once
#include "eternity/game/world_db_agent_disconnect.hpp"
#include "eternity/game/world_db_agent_save_driver.hpp"
#include "eternity/game/world_db_agent_users.hpp"

namespace eternity::game {
// Retains the DB-agent world, user registry and periodic save driver together.
// ps_dbAgent.exe CUserSaveThread::Run 0041d5b0 and CUserMgr::SaveAll 0041d2f0.
// Connection, SQL, guild and user-job owners remain required external owners.
template<class Connection,class User,class World> class WorldDbAgentLifecycle final:public std::enable_shared_from_this<WorldDbAgentLifecycle<Connection,User,World>> {
public:
    using Users=WorldDbAgentUsers<Connection,User>;
    using Driver=WorldDbAgentSaveDriver<World>;
    using Strand=typename Driver::Strand;
    static std::shared_ptr<WorldDbAgentLifecycle> create(Strand strand,std::shared_ptr<World> world,
        typename Driver::Tick tick,typename Driver::Report report,typename Driver::Failure failure) {
        if(!world || !tick || !report || !failure)throw std::invalid_argument("DB-agent lifecycle requires world, clock, report and failure owners");
        auto self=std::shared_ptr<WorldDbAgentLifecycle>(new WorldDbAgentLifecycle(std::move(strand),std::move(world)));
        std::weak_ptr<WorldDbAgentLifecycle> weak=self;
        self->driver_=Driver::create(self->strand_,self->world_,std::move(tick),[weak](std::uint32_t now){
            auto locked=weak.lock();if(!locked)throw std::logic_error("DB-agent lifecycle lost its user registry");locked->users_.update_user_save(now);
        },std::move(report),std::move(failure));
        return self;
    }
    Users& users(){require_strand();return users_;}
    void start(){require_strand();driver_->start();}
    void stop_periodic(){require_strand();driver_->stop();}
    WorldDbAgentSaveResult removed_game_connection(Connection& connection) {
        require_strand();WorldDbAgentDisconnect<Connection,World> disconnect(*world_,[this](Connection& owner){users_.connection_removed(owner);});
        return disconnect.removed_game_connection(connection);
    }
private:
    WorldDbAgentLifecycle(Strand strand,std::shared_ptr<World> world):strand_(std::move(strand)),world_(std::move(world)) {}
    void require_strand()const {if(!strand_.running_in_this_thread())throw std::logic_error("DB-agent lifecycle requires its DB strand");}
    Strand strand_;std::shared_ptr<World> world_;Users users_;std::shared_ptr<Driver> driver_;
};
}
