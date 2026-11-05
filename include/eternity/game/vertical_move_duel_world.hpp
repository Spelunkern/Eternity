#pragma once
#include "eternity/game/duel.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
// Same-zone duel position and lookup over retained actors. Drop allocation,
// random source, item insertion and logging remain the supplied real DuelWorld.
// The serialized world owner retains the zone, registry, backend and actors
// through all callbacks; callers pin the borrowed opponent before using it.
template<class Session> class VerticalMoveDuelWorld final:public DuelWorld {
public:
    VerticalMoveDuelWorld(ZoneUsers& users,VerticalSessionRegistry<Session>& registry,DuelWorld& backend)
        :users_(users),registry_(registry),backend_(backend) {}
    Actor* find_duel_user(Actor& requester,std::uint32_t id) override {
        auto owner=pin(requester);return users_.find_actor(id);
    }
    UserPosition duel_position(const Actor& actor) override {
        auto owner=pin(actor);auto position=users_.position(actor.id());
        if(!position)throw std::logic_error("Duel actor has no zone position");return *position;
    }
    std::int32_t drop_random() override{return backend_.drop_random();}
    DuelDropItem* allocate_drop_item() override{return backend_.allocate_drop_item();}
    bool enter_duel_item(Actor& owner,DuelDropItem& item,bool notify) override {
        auto pin_owner=pin(owner);return backend_.enter_duel_item(owner,item,notify);
    }
    GameLogSubject duel_log_subject(const Actor& actor) override {auto owner=pin(actor);return backend_.duel_log_subject(actor);}
    std::chrono::sys_seconds local_calendar() override{return backend_.local_calendar();}
    void send_duel_log(const net::Packet& packet) override{backend_.send_duel_log(packet);}
private:
    std::shared_ptr<Session> pin(const Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner || users_.find_actor(actor.id())!=&actor)throw std::logic_error("Duel actor is not retained in this zone");
        return owner;
    }
    ZoneUsers& users_;VerticalSessionRegistry<Session>& registry_;DuelWorld& backend_;
};
}
