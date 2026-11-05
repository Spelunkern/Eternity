#pragma once
#include "eternity/game/world_send_update.hpp"
#include "eternity/game/world_admission.hpp"

namespace eternity::game {
// The transport must implement both original SendNow and priority flush/reset.
class WorldGlobalSendServices {
public:
    virtual ~WorldGlobalSendServices()=default;
    virtual std::shared_ptr<void> retain(WorldAdmissionUser&)=0;
    virtual void flush(WorldAdmissionUser&)=0;
};
class WorldSendOtherServices {
public:
    virtual ~WorldSendOtherServices()=default;
    virtual void update_session_lost(std::uint32_t)=0;
    virtual void update_kill_count(std::uint32_t)=0;
    virtual void check_guild_rank_notices()=0;
    virtual bool check_guild_rank_status()=0;
    virtual void update_guild_point_rank()=0;
};
class WorldSendRemainingServices {
public:
    virtual ~WorldSendRemainingServices()=default;
    virtual void update_kill_count(std::uint32_t)=0;
    virtual void check_guild_rank_notices()=0;
    virtual bool check_guild_rank_status()=0;
    virtual void update_guild_point_rank()=0;
};
class WorldSendGuildServices {
public:
    virtual ~WorldSendGuildServices()=default;
    virtual void check_guild_rank_notices()=0;
    virtual bool check_guild_rank_status()=0;
    virtual void update_guild_point_rank()=0;
};
template<class Root> class WorldSendRetainedRemaining final:public WorldSendRemainingServices {
public:
    WorldSendRetainedRemaining(std::shared_ptr<Root> root,std::shared_ptr<WorldSendGuildServices> guild)
        :root_(root),guild_(std::move(guild)) {if(!root || !guild_)throw std::invalid_argument("World send requires actual guild services");}
    void update_kill_count(std::uint32_t now) override {
        auto root=root_.lock();if(!root)throw std::logic_error("World kill-count update lost its root");
        if(!root->update_world_kill_count_timer(now) && root->world_send_running())throw std::logic_error("World kill-count update interrupted unexpectedly");
    }
    void check_guild_rank_notices() override{guild_->check_guild_rank_notices();}
    bool check_guild_rank_status() override{return guild_->check_guild_rank_status();}
    void update_guild_point_rank() override{guild_->update_guild_point_rank();}
private:std::weak_ptr<Root> root_;std::shared_ptr<WorldSendGuildServices> guild_;
};
template<class Root> class WorldSendRetainedOther final:public WorldSendOtherServices {
public:
    WorldSendRetainedOther(std::shared_ptr<Root> root,std::shared_ptr<WorldSendRemainingServices> remaining)
        :root_(root),remaining_(std::move(remaining)) {if(!root || !remaining_)throw std::invalid_argument("World send requires actual remaining services");}
    void update_session_lost(std::uint32_t now) override {
        auto root=root_.lock();if(!root)throw std::logic_error("World session-lost update lost its root");
        if(!root->update_session_lost(now) && root->world_send_running())throw std::logic_error("Session-lost update interrupted unexpectedly");
    }
    void update_kill_count(std::uint32_t now) override {remaining_->update_kill_count(now);}
    void check_guild_rank_notices() override {remaining_->check_guild_rank_notices();}
    bool check_guild_rank_status() override {return remaining_->check_guild_rank_status();}
    void update_guild_point_rank() override {remaining_->update_guild_point_rank();}
private:std::weak_ptr<Root> root_;std::shared_ptr<WorldSendRemainingServices> remaining_;
};
// CWorld::UpdatePackets 0040bf90 and UpdateNetSend 0040c1b0 use the same
// actual world admission lists; all other phases remain mandatory real owners.
template<class Root> class WorldSendRetainedUpdate final:public WorldSendUpdateServices {
public:
    WorldSendRetainedUpdate(std::shared_ptr<Root> root,std::shared_ptr<WorldGlobalSendServices> send,
        std::shared_ptr<WorldSendOtherServices> other):root_(root),send_(std::move(send)),other_(std::move(other)) {
        if(!root || !send_ || !other_)throw std::invalid_argument("World send composition requires retained world and services");
    }
    void update_packets() override {owner()->update_admission();}
    void update_net_send() override {
        auto root=owner();(void)root->admission().visit_global_users_while([&](WorldAdmissionUser& user){return send_->retain(user);},
            [&](WorldAdmissionUser& user){send_->flush(user);},[&]{return root->world_send_running();});
    }
    void update_session_lost(std::uint32_t now) override {other_->update_session_lost(now);}
    void update_kill_count(std::uint32_t now) override {other_->update_kill_count(now);}
    void check_guild_rank_notices() override {other_->check_guild_rank_notices();}
    bool check_guild_rank_status() override {return other_->check_guild_rank_status();}
    void update_guild_point_rank() override {other_->update_guild_point_rank();}
private:
    std::shared_ptr<Root> owner() {auto root=root_.lock();if(!root)throw std::logic_error("World send composition lost its root");return root;}
    std::weak_ptr<Root> root_;std::shared_ptr<WorldGlobalSendServices> send_;std::shared_ptr<WorldSendOtherServices> other_;
};
}
