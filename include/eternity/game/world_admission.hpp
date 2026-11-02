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
    void update_packets();
    bool client_enter(WorldAdmissionUser&); // CUser::ZoneEnterUserMove 0044b8a0.
    bool world_enter(WorldAdmissionUser&); // CWorld::ZoneEnterUserMove 0040d000.
    std::size_t connected_count() const;
    std::size_t selection_count() const;
    std::size_t waiting_count() const;
    std::size_t zone_waiting_count(const RuntimeZone&) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
