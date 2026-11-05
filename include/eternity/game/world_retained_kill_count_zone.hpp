#pragma once
#include "eternity/game/world_kill_count_composition.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include <map>

namespace eternity::game {
// CZone::ApplyKillCountAllUser 00422f00 and
// CZone::SendAllCountryWithEnterWaitUser 00420600. Sessions and root use the
// same serialized world strand; apply must perform real UpdateKCStatus work.
template<class Root,class Session> class WorldRetainedKillCountZone final:public WorldKillCountZonePorts<Root> {
public:
    using State=WorldKillCountState<Root>;
    using Apply=std::function<void(Session&,const State&)>;
    WorldRetainedKillCountZone(std::shared_ptr<Root> root,VerticalSessionRegistry<Session>& registry,Apply apply)
        :root_(root),registry_(registry),apply_(std::move(apply)) {
        if(!root || !apply_)throw std::invalid_argument("Kill-count zone requires actual root, sessions and status owner");
    }
    std::shared_ptr<void> retain_actor(Actor& actor) override{return pin_actor(actor);}
    void apply_actor(Actor& actor,const State& state) override{auto pin=pin_actor(actor);apply_(*pin->session,state);}
    std::shared_ptr<void> retain_waiting(WorldAdmissionUser& user) override{return pin_waiting(user);}
    std::uint8_t waiting_country(WorldAdmissionUser& user) override{return pin_waiting(user)->actor->appearance().identity.country;}
    void apply_waiting(WorldAdmissionUser& user,const State& state) override{auto pin=pin_waiting(user);apply_(*pin->session,state);}
    void send_country(RuntimeZone& zone,std::uint8_t country,const net::Packet& packet) override {
        auto root=root_.lock();if(!root || !root->world_send_running())throw std::logic_error("Kill-count send requires a running world");
        if(!zone.users || country>1 || packet.empty())throw std::invalid_argument("Kill-count send requires zone, country and packet");
        zone.users->visit_all([&](Actor& actor){return retain_actor(actor);},[&](Actor& actor){
            if(root->world_send_running() && actor.appearance().identity.country==country)send(*pin_actor(actor),packet);
        });
        if(!root->world_send_running())return;
        root->admission().visit_zone_waiting(zone,[&](WorldAdmissionUser& user){return retain_waiting(user);},[&](WorldAdmissionUser& user){
            if(root->world_send_running()) {auto pin=pin_waiting(user);if(pin->actor->appearance().identity.country==country)send(*pin,packet);}
        });
    }
private:
    struct Pin {std::shared_ptr<Session> session;Actor* actor;};
    static void send(Pin& pin,const net::Packet& packet) {
        auto& channel=pin.session->channel();if(!channel.send)throw std::logic_error("Kill-count session has no send channel");channel.send(packet);
    }
    void prune() {
        for(auto it=actors_.begin();it!=actors_.end();)if(it->second.expired())it=actors_.erase(it);else ++it;
        for(auto it=waiting_.begin();it!=waiting_.end();)if(it->second.expired())it=waiting_.erase(it);else ++it;
    }
    std::shared_ptr<Pin> pin_actor(Actor& actor) {
        if(const auto found=actors_.find(&actor);found!=actors_.end()) {
            if(auto pin=found->second.lock()) {if(!pin->session->world_user() || &pin->session->world_user()->actor!=&actor)throw std::logic_error("Kill-count actor changed pinned session");return pin;}
            actors_.erase(found);
        }
        prune();auto session=registry_.retain_actor(actor);
        if(!session || !session->world_user() || &session->world_user()->actor!=&actor)throw std::logic_error("Kill-count actor lost its session");
        auto pin=std::make_shared<Pin>(Pin{std::move(session),&actor});actors_[&actor]=pin;return pin;
    }
    std::shared_ptr<Pin> pin_waiting(WorldAdmissionUser& user) {
        if(const auto found=waiting_.find(&user);found!=waiting_.end()) {
            if(auto pin=found->second.lock()) {if(&pin->session->admission()!=&user || !pin->session->world_user() || &pin->session->world_user()->actor!=pin->actor)throw std::logic_error("Kill-count waiter changed pinned session");return pin;}
            waiting_.erase(found);
        }
        prune();auto session=registry_.find_account(user.user_id);
        if(!session || &session->admission()!=&user || !session->world_user())throw std::logic_error("Kill-count waiter lost its session");
        auto pin=pin_actor(session->world_user()->actor);if(pin->session!=session)throw std::logic_error("Kill-count waiter changed session");
        waiting_[&user]=pin;return pin;
    }
    std::weak_ptr<Root> root_;VerticalSessionRegistry<Session>& registry_;Apply apply_;
    std::map<Actor*,std::weak_ptr<Pin>> actors_;std::map<WorldAdmissionUser*,std::weak_ptr<Pin>> waiting_;
};
}
