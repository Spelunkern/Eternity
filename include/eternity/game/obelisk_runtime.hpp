#pragma once
#include "eternity/game/mobs.hpp"
#include "eternity/net/framing.hpp"
#include <span>
#include <string_view>

namespace eternity::game {
// Explicit projection of stMapObelisk (0x70) / stMapObeliskMob (0x58).
// Storage and pointed-to mobs must remain stable through provider callbacks.
// The map loader binds principal respawn.type/index and obelisk=true; companion
// respawns bind obelisk_source to their stable ObeliskRuntimeSpawn address.
struct ObeliskRuntimeSpawn {
    UserPosition center; float radius=0; std::int32_t radius_twice=1,count=0;
    std::array<const MobRuntimeTemplate*,3> definitions{};
    MobRespawn respawn;
    std::vector<MobRuntime*> members; // Empty projects the original nullable list.
    void add(MobRuntime*); // stMapObeliskMob::AddMob 00429930: first empty slot.
    void remove(MobRuntime*); // RemoveMob 0041d700: first matching slot.
};
struct ObeliskRuntime {
    std::int32_t country=2,last_destroy_country=2,base_country=2;
    std::uint32_t change_time=0,base_id=0,id=0; // id is stMapObelisk::dwID, distinct from base_id.
    std::uint32_t reset_time=0; // Original dwResetTime +000c: remaining ownership milliseconds.
    UserPosition center;
    std::array<const MobRuntimeTemplate*,3> definitions{};
    MobRespawn respawn;
    std::vector<ObeliskRuntimeSpawn> spawns;
    MobRuntime* principal=nullptr;
};
struct ObeliskRuntimeMap {
    bool zone_present=false,reset_send=false;
    std::uint16_t id=0;
    std::int32_t size=0;
    std::span<const std::uint8_t> blocked_tiles;
};
class ObeliskRuntimeWorld {
public:
    virtual ~ObeliskRuntimeWorld()=default;
    virtual std::string_view mob_name(const MobRuntime&)=0;
    virtual void remove_by_id(std::uint32_t)=0;
    virtual void send_change(std::size_t)=0;
    virtual void add_kill_count(std::int32_t country,std::int32_t amount)=0;
    virtual bool kill_count_status14(std::int32_t country)=0;
    virtual void subtract_kill_count(std::int32_t country,std::int32_t amount)=0;
    virtual void broadcast(const net::Packet&)=0;
    virtual void game_log(const net::Packet&)=0;
    virtual void database(const net::Packet&)=0;
    virtual void update_portal_links()=0;
    // true: CObjectMgr::CreateMob (principal and periodic replenishment);
    // false: raw heap allocation (companions during complete respawn only).
    virtual MobRuntime* allocate(bool manager_create)=0;
    // Raw heap path only (including startup principals): assign world ID and
    // seed drop random with time^ID. Historical method name includes both.
    virtual void prepare_companion(MobRuntime&)=0;
    virtual void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&)=0;
    virtual void enter(MobRuntime&)=0;
    virtual std::int32_t random()=0; // Original CRT rand(), nonnegative.
};
// ps_game.exe CMap::ObeliskDestroy 0042a410 / ObeliskRespawn 0042a6f0.
// Tick dispatch, SQL configuration and world-service implementations are owned
// by callers; no required provider has a default no-op.
void destroy_obelisk(ObeliskRuntimeMap&,ObeliskRuntime&,std::size_t index,
                     const MobRuntime& destroyed,std::int32_t country,bool reset,
                     const std::array<std::int32_t,2>& kill_count_add,ObeliskRuntimeWorld&);
void respawn_obelisk(ObeliskRuntimeMap&,ObeliskRuntime&,ObeliskRuntimeWorld&);
// CMap::InitObelisk 00429a60. Raw heap allocation/ID preparation for principal
// and companions, no spawn log. Normal map records bind to their primary zone.
void initialize_obelisks(ObeliskRuntimeMap&,std::span<ObeliskRuntime>,ObeliskRuntimeWorld&);
// CMap::UpdateRespawnObelisk 00429da0. Unsigned deadline <= now; one attempt
// per eligible companion group per invocation. Calls complete respawn only for
// principal current < 1, and never replenishes that group's companions that tick.
void update_obelisk_respawns(ObeliskRuntimeMap&,std::span<ObeliskRuntime>,
                             std::uint32_t now,ObeliskRuntimeWorld&);
// CMap::ObeliskReset 0042aa40. Clock is sampled once after the zone guard,
// including an empty map. remove_by_id must execute actual zone removal,
// including any nested destroy callback while reset_send is true.
void reset_obelisks(ObeliskRuntimeMap&,std::span<ObeliskRuntime>,
                     const std::function<std::uint32_t()>& tick,ObeliskRuntimeWorld&);
}
