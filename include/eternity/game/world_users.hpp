#pragma once
#include "eternity/game/actor.hpp"
#include "eternity/game/world_admission.hpp"
#include <map>
#include <string_view>

namespace eternity::game {
// Stable identity wrapper. Actor and admission state outlive every index entry.
struct WorldUser {Actor& actor;WorldAdmissionUser& admission;};
struct WorldUserPorts {
    std::function<void(WorldUser&)> entered,left;
};
// Serialized world executor. ps_game.exe EnterUserWorld 0040d450 /
// LeaveUserWorld 0040d4d0. Independent insert-only ID/name indexes deliberately
// preserve duplicate-key behavior. Callers must prevent invalid duplicate owners.
class WorldUsers {
public:
    explicit WorldUsers(WorldUserPorts);
    void enter(WorldUser&);
    void leave(WorldUser&);
    WorldUser* find(std::uint32_t) const;
    WorldUser* find(std::string_view) const;
    WorldUser* find_zone(std::uint32_t) const;
    WorldUser* find_zone(std::string_view) const;
    WorldUser* find_zone_move(std::uint32_t) const;
    WorldUser* find_zone_move(std::string_view) const;
    std::size_t id_count() const noexcept {return ids_.size();}
    std::size_t name_count() const noexcept {return names_.size();}
private:
    static std::string key(std::string_view);
    static WorldUser* zone(WorldUser*);
    static WorldUser* movable(WorldUser*);
    WorldUserPorts ports_;
    std::map<std::uint32_t,WorldUser*> ids_;
    std::map<std::string,WorldUser*,std::less<>> names_;
};
}
