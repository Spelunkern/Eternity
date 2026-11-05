#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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
struct UserCell {std::int32_t x=0,z=0;};
bool same_team(const Actor& subject,const Actor& other,std::int32_t map_team_mode);
// User cell membership, position bounds and hidden-user queries. World admission,
// movement authorization, cell broadcasts and NPC/mob membership remain separate.
// One world executor still serializes actor state. Membership uses a recursive
// lock; borrowed actors/scenes outlive attachment and borrowed cell callbacks.
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
    // CZone::LeaveUser removes both memberships before the user leave callback
    // and broadcast, but keeps the user's zone pointer until after broadcast.
    // The serialized caller must clear actor.status().in_zone on completion.
    bool detach_for_zone_leave(std::uint32_t id);
    void move(std::uint32_t id,std::int32_t cell_x,std::int32_t cell_z,UserPosition);
    bool move_at_position(std::uint32_t id,UserPosition);
    std::optional<UserPosition> position(std::uint32_t id) const;
    // Current canonical membership cell; callers must serialize multi-step
    // movement and retain the Actor/zone across any callbacks.
    std::optional<UserCell> cell(std::uint32_t id) const;
    void detect_hidden(std::uint32_t source,std::uint8_t range);
    // Ordered, live iteration using the original cell's shared cursor. True stops.
    bool visit_cell(std::int32_t x,std::int32_t z,const std::function<bool(Actor&,UserPosition)>&);
    // Global insertion-order traversal, CZone::UpdateUserAttack 00421930.
    // The retention callback MUST pin this actor and its scene before returning
    // a nonempty token, without changing membership. The visitor may reenter
    // detach/append/find/nested visits; the zone itself survives them. Current
    // actor stays alive through callback return. No actor access follows it.
    using RetainActor=std::function<std::shared_ptr<void>(Actor&)>;
    // CCell::PSend 004243d0 uses each current user's live next link, rather
    // than the shared cell cursor used by visit_cell. The current Actor must be
    // pinned through the callback; retain must not change membership. Cross-cell reentry of that Actor is not yet
    // recovered and raises; no actor/zone may be destroyed during this call.
    void broadcast_cell(std::int32_t x,std::int32_t z,const RetainActor&,const std::function<void(Actor&)>&);
    void visit_all(const RetainActor&,const std::function<void(Actor&)>&);
    // Borrowed same-zone lookup. It does not change the global visit cursor;
    // callers crossing callbacks must acquire their own owner lease.
    Actor* find_actor(std::uint32_t id) const;
    std::size_t size() const noexcept;
private:
    struct Member {Actor* actor;UserPosition position;std::size_t cell;std::uint64_t ordinal,arrival;};
    struct Cell {std::map<std::uint64_t,std::uint32_t> members;std::uint64_t cursor=0;};
    std::size_t index(std::int32_t x,std::int32_t z) const;
    void unlink(const Member&);
    bool detach_impl(std::uint32_t,bool preserve_zone_status);
    std::int32_t width_,height_,team_mode_;
    std::optional<std::int32_t> map_size_;
    // Same existing Member objects own both cell and arrival ordinals. No
    // secondary actor list/index. Cell moves do not change arrival ordering.
    mutable std::recursive_mutex mutex_;
    std::uint64_t sequence_=0,user_cursor_=0;
    std::vector<Cell> cells_;
    std::map<std::uint32_t,Member> users_;
};
}
