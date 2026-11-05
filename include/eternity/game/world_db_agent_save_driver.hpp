#pragma once
#include "eternity/game/world_db_agent_save.hpp"
#include "eternity/game/world_frame_driver.hpp"
#include <asio/steady_timer.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// ps_dbAgent.exe CUserSaveThread::Run 0041d5b0 and Shutdown 0041d680.
// Run on a dedicated serialized DB executor: world saves are synchronous.
template<class World> class WorldDbAgentSaveDriver final:public std::enable_shared_from_this<WorldDbAgentSaveDriver<World>> {
public:
    using Strand=WorldFrameDriver::Strand;
    using Tick=std::function<std::uint32_t()>;
    using UserSave=std::function<void(std::uint32_t)>;
    using Report=std::function<void(const WorldDbAgentSaveResult&)>;
    using Failure=std::function<void(std::exception_ptr)>;
    static std::shared_ptr<WorldDbAgentSaveDriver> create(Strand strand,std::shared_ptr<World> world,Tick tick,UserSave users,Report report,Failure failure) {
        if(!world || !tick || !users || !report || !failure)throw std::invalid_argument("DB-agent save driver requires real world, clock, user save, report and failure owners");
        return std::shared_ptr<WorldDbAgentSaveDriver>(new WorldDbAgentSaveDriver(std::move(strand),std::move(world),std::move(tick),std::move(users),std::move(report),std::move(failure)));
    }
    void start(){require_strand();if(started_)throw std::logic_error("DB-agent save driver already started or stopped");started_=true;try{schedule(0);}catch(...){stopped_=true;throw;}}
    void stop(){require_strand();started_=true;if(stopped_)return;stopped_=true;timer_.cancel();}
private:
    WorldDbAgentSaveDriver(Strand strand,std::shared_ptr<World> world,Tick tick,UserSave users,Report report,Failure failure)
        :strand_(std::move(strand)),timer_(strand_),world_(std::move(world)),tick_(std::move(tick)),users_(std::move(users)),report_(std::move(report)),failure_(std::move(failure)) {}
    void require_strand()const {if(!strand_.running_in_this_thread())throw std::logic_error("DB-agent save driver requires its DB strand");}
    void schedule(std::uint32_t delay){timer_.expires_after(std::chrono::milliseconds(delay));std::weak_ptr<WorldDbAgentSaveDriver> weak=this->shared_from_this();timer_.async_wait([weak](const asio::error_code& error){if(auto self=weak.lock())self->on_wait(error);});}
    void on_wait(const asio::error_code& error){
        if(stopped_)return;
        try {
            if(error)throw std::system_error(error);
            const auto start=tick_();
            if(start-last_user_save_>=1000U){users_(start);last_user_save_=start;}
            if(stopped_)return;
            const auto result=world_->periodic(start);if(result.due)report_(result);
            if(stopped_)return;
            const auto remaining=start-tick_()+1000U;schedule((remaining&0x80000000U)?0U:remaining);
        }catch(...){stopped_=true;failure_(std::current_exception());}
    }
    Strand strand_;asio::steady_timer timer_;std::shared_ptr<World> world_;Tick tick_;UserSave users_;Report report_;Failure failure_;
    std::uint32_t last_user_save_=0;bool started_=false,stopped_=false;
};
}
