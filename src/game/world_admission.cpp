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
};
struct WorldAdmission::Impl {
    WorldAdmissionServices& services;
    WorldAdmissionUser::List connected,selection,waiting;
    std::map<const RuntimeZone*,std::unique_ptr<WorldAdmissionUser::List>> zone_wait;
    explicit Impl(WorldAdmissionServices& owner):services(owner) {}
    WorldAdmissionUser::List& list(RuntimeZone& zone) {auto& value=zone_wait[&zone];if(!value) value=std::make_unique<WorldAdmissionUser::List>();return *value;}
    bool enter(WorldAdmissionUser& user,RuntimeZone& zone) {
        auto& queue=list(zone);queue.remove(user);
        if(services.enter_zone(user,zone)) {user.zone=&zone;user.location=4;return true;}
        queue.add(user);return false;
    }
};
WorldAdmission::WorldAdmission(WorldAdmissionServices& services):impl_(std::make_unique<Impl>(services)) {}
WorldAdmission::~WorldAdmission()=default;
void WorldAdmission::track_connected(WorldAdmissionUser& user) {if(user.location!=0) throw std::invalid_argument("Connected world user must have location 0");impl_->connected.add(user);}
void WorldAdmission::track_selection(WorldAdmissionUser& user) {if(user.location!=1) throw std::invalid_argument("Selecting world user must have location 1");impl_->selection.add(user);}
void WorldAdmission::untrack(WorldAdmissionUser& user) {if(user.list_) user.list_->remove(user);}
bool WorldAdmission::wait(WorldAdmissionUser& user) {
    if(user.location!=1) {impl_->services.close(user,9);impl_->services.diagnostic(user,WorldAdmissionError::wait_state);return false;}
    user.logout_type=user.logout_tick=0;user.location=2;impl_->selection.remove(user);impl_->waiting.add(user);impl_->services.register_world(user);return true;
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
std::size_t WorldAdmission::connected_count() const {return impl_->connected.count;}
std::size_t WorldAdmission::selection_count() const {return impl_->selection.count;}
std::size_t WorldAdmission::waiting_count() const {return impl_->waiting.count;}
std::size_t WorldAdmission::zone_waiting_count(const RuntimeZone& zone) const {const auto it=impl_->zone_wait.find(&zone);return it==impl_->zone_wait.end()?0:it->second->count;}
}
