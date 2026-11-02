#include "eternity/game/world_users.hpp"

namespace eternity::game {
WorldUsers::WorldUsers(WorldUserPorts ports):ports_(std::move(ports)) {
    if(!ports_.entered || !ports_.left) throw std::invalid_argument("World users require entry and leave services");
}
std::string WorldUsers::key(std::string_view value) {
    value=value.substr(0,value.find('\0'));
    if(value.size()>31) throw std::invalid_argument("World name exceeds original stString storage");
    return std::string(value);
}
void WorldUsers::enter(WorldUser& user) {
    const auto name=key(user.actor.appearance().identity.name);
    ids_.emplace(user.actor.id(),&user);names_.emplace(name,&user);ports_.entered(user);
}
void WorldUsers::leave(WorldUser& user) {
    const auto name=key(user.actor.appearance().identity.name);
    ids_.erase(user.actor.id());names_.erase(name);ports_.left(user);
}
WorldUser* WorldUsers::find(std::uint32_t id) const {const auto found=ids_.find(id);return found==ids_.end()?nullptr:found->second;}
WorldUser* WorldUsers::find(std::string_view name) const {const auto found=names_.find(key(name));return found==names_.end()?nullptr:found->second;}
WorldUser* WorldUsers::zone(WorldUser* user) {return user && user->admission.location==4?user:nullptr;}
WorldUser* WorldUsers::movable(WorldUser* user) {user=zone(user);return user && user->admission.logout_tick==0?user:nullptr;}
WorldUser* WorldUsers::find_zone(std::uint32_t id) const {return zone(find(id));}
WorldUser* WorldUsers::find_zone(std::string_view name) const {return zone(find(name));}
WorldUser* WorldUsers::find_zone_move(std::uint32_t id) const {return movable(find(id));}
WorldUser* WorldUsers::find_zone_move(std::string_view name) const {return movable(find(name));}
}
