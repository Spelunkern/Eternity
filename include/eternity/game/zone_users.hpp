#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace eternity::game {
class Actor;
class MapGeometry;
struct UserRelations {
    std::int32_t duel_state=0;
    std::uint32_t duel_opponent=0;
    std::uint32_t guild_id=0,opposing_guild_id=0;
    std::int32_t guild_duel_ready=0;
    std::int32_t duel_type=-1; // CUser+54d0.
};
struct UserPosition {float x=0,y=0,z=0;};
bool same_team(const Actor& subject,const Actor& other,std::int32_t map_team_mode);
// User cell membership, position bounds and hidden-user queries. World admission,
// movement authorization, cell broadcasts and NPC/mob membership remain separate.
// One executor must serialize all operations; attached actors must outlive this grid.
class ZoneUsers {
public:
    ZoneUsers(std::int32_t width,std::int32_t height,std::int32_t map_team_mode);
    ZoneUsers(const MapGeometry&,std::int32_t map_team_mode);
    ~ZoneUsers();
    ZoneUsers(const ZoneUsers&) = delete;
    ZoneUsers& operator=(const ZoneUsers&) = delete;
    void attach(Actor&,std::int32_t cell_x,std::int32_t cell_z,UserPosition);
    bool attach_at_position(Actor&,UserPosition);
    bool detach(std::uint32_t id);
    void move(std::uint32_t id,std::int32_t cell_x,std::int32_t cell_z,UserPosition);
    bool move_at_position(std::uint32_t id,UserPosition);
    std::optional<UserPosition> position(std::uint32_t id) const;
    void detect_hidden(std::uint32_t source,std::uint8_t range);
    // Ordered, live iteration using the original cell's shared cursor. True stops.
    bool visit_cell(std::int32_t x,std::int32_t z,const std::function<bool(Actor&,UserPosition)>&);
    std::size_t size() const noexcept {return users_.size();}
private:
    struct Member {Actor* actor;UserPosition position;std::size_t cell;std::uint64_t ordinal;};
    struct Cell {std::map<std::uint64_t,std::uint32_t> members;std::uint64_t cursor=0;};
    std::size_t index(std::int32_t x,std::int32_t z) const;
    void unlink(const Member&);
    std::int32_t width_,height_,team_mode_;
    std::optional<std::int32_t> map_size_;
    std::uint64_t sequence_=0;
    std::vector<Cell> cells_;
    std::map<std::uint32_t,Member> users_;
};
}
