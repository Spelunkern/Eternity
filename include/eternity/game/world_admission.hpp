#pragma once
#include "eternity/game/zones.hpp"
#include <atomic>

namespace eternity::game {
struct WorldAdmissionUser {
    WorldAdmissionUser()=default;
    WorldAdmissionUser(const WorldAdmissionUser&)=delete;
    WorldAdmissionUser& operator=(const WorldAdmissionUser&)=delete;
    std::uint32_t user_id=0,location=1,logout_type=0,logout_tick=0;
    std::uint16_t map=0;
    RuntimeZone* zone=nullptr;
    ZoneUserAccess access;
    std::atomic<std::uint32_t>* guild_rank_entries=nullptr;
private:
    friend class WorldAdmission;
    struct List;
    List* list_=nullptr;WorldAdmissionUser* previous_=nullptr;WorldAdmissionUser* next_=nullptr;
};
enum class WorldAdmissionError {wait_state,missing_zone,move_state,map_selection};
class WorldAdmissionServices {
public:
    virtual ~WorldAdmissionServices()=default;
    virtual void packets(WorldAdmissionUser&)=0;
    virtual RuntimeZone* select_zone(WorldAdmissionUser&)=0;
    virtual void register_world(WorldAdmissionUser&)=0;
    virtual bool enter_zone(WorldAdmissionUser&,RuntimeZone&)=0;
    virtual void close(WorldAdmissionUser&,std::uint32_t reason)=0;
    virtual void diagnostic(WorldAdmissionUser&,WorldAdmissionError)=0;
    virtual std::optional<std::uint32_t> rank_window(RuntimeZone&)=0;
    virtual bool lock_guild(WorldAdmissionUser&)=0;
    virtual void unlock_guild(WorldAdmissionUser&)=0;
};
// Serialized world executor. Users/zones/services outlive their membership and
// CWorld::WaitUser 0040ce70 / UpdatePackets 0040bf90 own the waiting transitions.
// callbacks may move/untrack users, but must not destroy them during iteration.
// Lists retain the original shared cursor, including removal during processing.
// Service callbacks must not throw; invalid native ownership raises logic_error.
class WorldAdmission {
public:
    explicit WorldAdmission(WorldAdmissionServices&);
    ~WorldAdmission();
    WorldAdmission(const WorldAdmission&)=delete;
    WorldAdmission& operator=(const WorldAdmission&)=delete;
    void track_connected(WorldAdmissionUser&); // location 0, before authentication.
    void track_selection(WorldAdmissionUser&); // location 1, after roster loading.
    void untrack(WorldAdmissionUser&);
    bool wait(WorldAdmissionUser&);
    // CWorld::ZoneLeaveUserMoveLocal 0040d1d0 after successful source leave.
    // The caller has already updated map/position and notified the party.
    void wait_after_zone_leave(WorldAdmissionUser&);
    void update_packets();
    // CWorld::UpdateNetSend 0040c1b0: connected users, then selection users.
    void visit_global_users(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
        const std::function<void(WorldAdmissionUser&)>& visit);
    bool visit_global_users_while(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
        const std::function<void(WorldAdmissionUser&)>& visit,const std::function<bool()>& keep_running);
    bool visit_connected_users_while(const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
        const std::function<void(WorldAdmissionUser&)>& visit,const std::function<bool()>& keep_running);
    // CZone::UpdatePackets 004215e0: visit that zone's waiting users with the
    // shared cursor. The caller owns the zone update cadence and serialization.
    void update_zone_wait_packets(RuntimeZone&);
    // Visit the actual zone-wait list with a retained session for each user.
    void visit_zone_waiting(RuntimeZone&,
        const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
        const std::function<void(WorldAdmissionUser&)>& visit);
    // CZone logout update 00421830: live zone-wait cursor. The callback may
    // untrack the current user but must keep that user alive until it returns.
    void update_zone_wait_logout_deadlines(RuntimeZone&,std::uint32_t tick,
        const std::function<void(WorldAdmissionUser&)>& logout);
    // Pin before clearing a due deadline; a missing session must leave it due.
    void update_zone_wait_logout_deadlines(RuntimeZone&,std::uint32_t tick,
        const std::function<std::shared_ptr<void>(WorldAdmissionUser&)>& retain,
        const std::function<void(WorldAdmissionUser&)>& logout);
    bool client_enter(WorldAdmissionUser&); // CUser::ZoneEnterUserMove 0044b8a0.
    bool world_enter(WorldAdmissionUser&); // CWorld::ZoneEnterUserMove 0040d000.
    // True only when all global and per-zone admission lists are empty.
    bool empty() const noexcept;
    std::size_t connected_count() const;
    std::size_t selection_count() const;
    std::size_t waiting_count() const;
    std::size_t zone_waiting_count(const RuntimeZone&) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
