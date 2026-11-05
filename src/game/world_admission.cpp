#include "eternity/game/world_admission.hpp"
#include <stdexcept>

namespace eternity::game {
struct WorldAdmissionUser::List {
    WorldAdmissionUser* front=nullptr;WorldAdmissionUser* back=nullptr;WorldAdmissionUser* cursor=nullptr;
    std::size_t count=0;
    ~List() {while(front) remove(*front);}
    void add(WorldAdmissionUser& user) {
        if(user.list_) throw std::logic_error("World user already belongs to an admission list");
        user.list_=this;user.previous_=back;user.next_=nullptr;if(back) back->next_=&user;else front=&user;back=&user;++count;
    }
    void remove(WorldAdmissionUser& user) {
        if(user.list_!=this) throw std::logic_error("World user does not belong to this admission list");
        if(cursor==&user) cursor=user.previous_;
        if(user.previous_) user.previous_->next_=user.next_;else front=user.next_;
        if(user.next_) user.next_->previous_=user.previous_;else back=user.previous_;
        user.list_=nullptr;user.previous_=user.next_=nullptr;--count;
    }
    WorldAdmissionUser* pop() {auto* user=front;if(user) remove(*user);return user;}
    void visit(const std::function<void(WorldAdmissionUser&)>& callback) {
        cursor=nullptr;for(auto* user=front;user;user=cursor?cursor->next_:front) {cursor=user;callback(*user);}cursor=nullptr;
    }
    bool visit_while(const std::function<bool(WorldAdmissionUser&)>& callback) {
        cursor=nullptr;struct Clear {WorldAdmissionUser*& cursor;~Clear(){cursor=nullptr;}} clear{cursor};
        for(auto* user=front;user;user=cursor?cursor->next_:front) {cursor=user;if(!callback(*user))return false;}
        return true;
    }
};
struct WorldAdmission::Impl {
    WorldAdmissionServices& services;
    WorldAdmissionUser::List connected,selection,waiting;
    std::map<const RuntimeZone*,std::unique_ptr<WorldAdmissionUser::List>> zone_wait;
    std::function<void(WorldAdmissionUser&,RuntimeZone&)> entry_observer;
    explicit Impl(WorldAdmissionServices& owner):services(owner) {}
    WorldAdmissionUser::List& list(RuntimeZone& zone) {auto& value=zone_wait[&zone];if(!value) value=std::make_unique<WorldAdmissionUser::List>();return *value;}
    bool enter(WorldAdmissionUser& user,RuntimeZone& zone) {
        auto& queue=list(zone);queue.remove(user);
        if(services.enter_zone(user,zone)) {user.zone=&zone;user.location=4;if(entry_observer)entry_observer(user,zone);return true;}
        queue.add(user);return false;
    }
};
WorldAdmission::WorldAdmission(WorldAdmissionServices& services):impl_(std::make_unique<Impl>(services)) {}
WorldAdmission::~WorldAdmission()=default;
void WorldAdmission::install_zone_entry_observer(std::function<void(WorldAdmissionUser&,RuntimeZone&)> observer) {
    if(!observer || impl_->entry_observer || !empty())throw std::logic_error("Zone entry observer must be installed once before admission");
    impl_->entry_observer=std::move(observer);
}
void WorldAdmission::track_connected(WorldAdmissionUser& user) {if(user.location!=0) throw std::invalid_argument("Connected world user must have location 0");impl_->connected.add(user);}
void WorldAdmission::track_selection(WorldAdmissionUser& user) {if(user.location!=1) throw std::invalid_argument("Selecting world user must have location 1");impl_->selection.add(user);}
void WorldAdmission::untrack(WorldAdmissionUser& user) {if(user.list_) user.list_->remove(user);}
bool WorldAdmission::wait(WorldAdmissionUser& user) {
    if(user.location!=1) {impl_->services.close(user,9);impl_->services.diagnostic(user,WorldAdmissionError::wait_state);return false;}
    user.logout_type=user.logout_tick=0;user.location=2;impl_->selection.remove(user);impl_->waiting.add(user);impl_->services.register_world(user);return true;
}
void WorldAdmission::wait_after_zone_leave(WorldAdmissionUser& user) {
    if(user.location!=4 || user.zone || user.list_)
        throw std::logic_error("Transfer requires a detached location-4 user");
    impl_->waiting.add(user);user.location=2;
}
void WorldAdmission::update_packets() {
    impl_->connected.visit([&](WorldAdmissionUser& user){impl_->services.packets(user);});
    impl_->selection.visit([&](WorldAdmissionUser& user){impl_->services.packets(user);});
    while(auto* user=impl_->waiting.pop()) {
        auto* zone=impl_->services.select_zone(*user);
        if(zone) {user->zone=zone;impl_->list(*zone).add(*user);user->location=3;}
        else {impl_->services.diagnostic(*user,WorldAdmissionError::map_selection);impl_->selection.add(*user);user->location=1;impl_->services.close(*user,9);}
    }
}
void WorldAdmission::update_zone_wait_packets(RuntimeZone& zone) {
    const auto it=impl_->zone_wait.find(&zone);
    if(it!=impl_->zone_wait.end())
        it->second->visit([&](WorldAdmissionUser& user){impl_->services.packets(user);});
}
void WorldAdmission::visit_global_users(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
    const std::function<void(WorldAdmissionUser&)>& visit) {
    if(!retain || !visit)throw std::invalid_argument("Global user traversal requires session retention and visitor");
    auto pinned=[&](WorldAdmissionUser& user){auto pin=retain(user);if(!pin)throw std::logic_error("Global user traversal lost its session");visit(user);};
    impl_->connected.visit(pinned);impl_->selection.visit(pinned);
}
bool WorldAdmission::visit_global_users_while(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
    const std::function<void(WorldAdmissionUser&)>& visit,const std::function<bool()>& keep_running) {
    if(!retain || !visit || !keep_running)throw std::invalid_argument("Global user traversal requires retention, visitor and continuation");
    auto pinned=[&](WorldAdmissionUser& user){
        if(!keep_running())return false;
        auto pin=retain(user);if(!keep_running())return false;
        if(!pin)throw std::logic_error("Global user traversal lost its session");
        visit(user);return keep_running();
    };
    if(!keep_running() || !impl_->connected.visit_while(pinned) || !keep_running())return false;
    return impl_->selection.visit_while(pinned) && keep_running();
}
bool WorldAdmission::visit_connected_users_while(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
    const std::function<void(WorldAdmissionUser&)>& visit,const std::function<bool()>& keep_running) {
    if(!retain || !visit || !keep_running)throw std::invalid_argument("Connected traversal requires retention, visitor and continuation");
    if(!keep_running())return false;
    const bool completed=impl_->connected.visit_while([&](WorldAdmissionUser& user){
        if(!keep_running())return false;
        auto pin=retain(user);if(!keep_running())return false;
        if(!pin)throw std::logic_error("Connected traversal lost its session");
        visit(user);return keep_running();
    });
    return completed && keep_running();
}
void WorldAdmission::visit_zone_waiting(RuntimeZone& zone,
    const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
    const std::function<void(WorldAdmissionUser&)>& visit) {
    if(!retain || !visit) throw std::invalid_argument("Zone-wait traversal requires session retention and visitor");
    const auto it=impl_->zone_wait.find(&zone);
    if(it!=impl_->zone_wait.end()) it->second->visit([&](WorldAdmissionUser& user){
        auto pin=retain(user);if(!pin)throw std::logic_error("Zone-wait traversal lost its session");visit(user);
    });
}
void WorldAdmission::update_zone_wait_logout_deadlines(RuntimeZone& zone,std::uint32_t tick,
    const std::function<void(WorldAdmissionUser&)>& logout) {
    if(!logout) throw std::invalid_argument("Logout update requires a callback");
    const auto it=impl_->zone_wait.find(&zone);
    if(it!=impl_->zone_wait.end()) it->second->visit([&](WorldAdmissionUser& user) {
        const auto deadline=user.logout_tick;
        if(user.logout_type==2 && deadline!=0 && deadline<=tick) {
            user.logout_tick=0;
            logout(user);
        }
    });
}
void WorldAdmission::update_zone_wait_logout_deadlines(RuntimeZone& zone,std::uint32_t tick,
    const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
    const std::function<void(WorldAdmissionUser&)>& logout) {
    if(!retain || !logout) throw std::invalid_argument("Logout update requires a retained session and callback");
    const auto it=impl_->zone_wait.find(&zone);
    if(it!=impl_->zone_wait.end()) it->second->visit([&](WorldAdmissionUser& user) {
        const auto deadline=user.logout_tick;
        if(user.logout_type==2 && deadline!=0 && deadline<=tick) {
            auto pin=retain(user);if(!pin) throw std::logic_error("Zone-wait logout requires a retained session");
            user.logout_tick=0;logout(user);
        }
    });
}
bool WorldAdmission::client_enter(WorldAdmissionUser& user) {
    auto* zone=user.zone;
    if(!zone) {impl_->services.diagnostic(user,WorldAdmissionError::missing_zone);impl_->services.close(user,9);return false;}
    if(!zone->map) throw std::logic_error("World admission zone has no map");
    const auto mode=zone->map->settings.create_type;
    if(mode==3 || mode==5 || mode==6) {if(user.access.permission<=10 && user.access.instance_owner) user.access.instance_owner=0;}
    if(mode==5) if(const auto window=impl_->services.rank_window(*zone)) {
        user.access.last_rank_entry=*window;const bool locked=impl_->services.lock_guild(user);
        if(user.guild_rank_entries) user.guild_rank_entries->fetch_add(1);
        if(locked) impl_->services.unlock_guild(user);
    }
    if(user.location!=3) {impl_->services.diagnostic(user,WorldAdmissionError::move_state);impl_->services.close(user,9);return false;}
    return impl_->enter(user,*zone);
}
bool WorldAdmission::world_enter(WorldAdmissionUser& user) {
    auto* zone=impl_->services.select_zone(user);
    if(user.location!=3 || !zone) {impl_->services.diagnostic(user,WorldAdmissionError::move_state);impl_->services.close(user,9);return false;}
    return impl_->enter(user,*zone);
}
bool WorldAdmission::empty() const noexcept {
    if(impl_->connected.count || impl_->selection.count || impl_->waiting.count) return false;
    for(const auto& entry:impl_->zone_wait) if(entry.second && entry.second->count) return false;
    return true;
}
std::size_t WorldAdmission::connected_count() const {return impl_->connected.count;}
std::size_t WorldAdmission::selection_count() const {return impl_->selection.count;}
std::size_t WorldAdmission::waiting_count() const {return impl_->waiting.count;}
std::size_t WorldAdmission::zone_waiting_count(const RuntimeZone& zone) const {const auto it=impl_->zone_wait.find(&zone);return it==impl_->zone_wait.end()?0:it->second->count;}
}
