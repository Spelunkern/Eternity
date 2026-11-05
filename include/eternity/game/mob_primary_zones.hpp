#pragma once
#include "eternity/game/zones.hpp"
#include "eternity/game/mob_applied_skills_obelisk_maps.hpp"
#include <mutex>
namespace eternity::game {
struct MobPrimaryZoneTransport {
    std::function<void(const net::Packet&)> database,world;
    std::function<void(const ConfiguredMap&,ZoneMobs&,const net::Packet&)> map;
};
class MobPrimaryZones;
// Retains the actual ZoneRegistry authority/dependencies. No raw owning zone
// pointer or cross-thread mutex ownership escapes. visit() serializes execution.
// This protects zone resources, not an individual recycled actor identity.
class MobPrimaryZoneLease {
public:
    MobPrimaryZoneLease()=default;
    ~MobPrimaryZoneLease();
    MobPrimaryZoneLease(MobPrimaryZoneLease&&)noexcept;
    MobPrimaryZoneLease& operator=(MobPrimaryZoneLease&&)noexcept;
    MobPrimaryZoneLease(const MobPrimaryZoneLease&)=delete;
    MobPrimaryZoneLease& operator=(const MobPrimaryZoneLease&)=delete;
    explicit operator bool()const noexcept{return bool(owner_);}
    std::uint64_t generation()const noexcept{return generation_;}
    void visit(const std::function<void(RuntimeZone&)>&)const;
    void reset()noexcept;
private:
    friend class MobPrimaryZones;
    MobPrimaryZoneLease(std::shared_ptr<MobPrimaryZones>,std::size_t,std::uint64_t);
    std::shared_ptr<MobPrimaryZones> owner_;std::size_t map_=0;std::uint64_t generation_=0;
};
// An instance lease pins this exact registry-owned zone against recycling.
class MobInstanceZoneLease {
public:
    MobInstanceZoneLease()=default;
    ~MobInstanceZoneLease();
    MobInstanceZoneLease(MobInstanceZoneLease&&)noexcept;
    MobInstanceZoneLease& operator=(MobInstanceZoneLease&&)noexcept;
    MobInstanceZoneLease(const MobInstanceZoneLease&)=delete;
    MobInstanceZoneLease& operator=(const MobInstanceZoneLease&)=delete;
    explicit operator bool()const noexcept{return bool(owner_);}
    std::uint64_t generation()const noexcept{return generation_;}
    void visit(const std::function<void(RuntimeZone&)>&)const;
    void reset()noexcept;
private:
    friend class MobPrimaryZones;
    MobInstanceZoneLease(std::shared_ptr<MobPrimaryZones>,RuntimeZone*,std::uint64_t);
    std::shared_ptr<MobPrimaryZones> owner_;RuntimeZone* zone_=nullptr;std::uint64_t generation_=0;
};
enum class MobInstanceExpiry {occupied,waiting,deferred,retired};
enum class MobPrimaryZoneClose {closed,deferred,absent};
// Root-owned retained facade over the EXISTING ZoneRegistry, not a second zone
// registry. Transfer the ONE existing bound pool/configuration/router. Shared
// ZoneServices must own/alias the real counters/catalog/heaps/Init/AI assemblies
// borrowed by these objects. Avoid owning backreferences to this
// facade; use weak references or scoped leases. No backend defaults are supplied.
class MobPrimaryZones final:public ObeliskRuntimeMapsServices,public std::enable_shared_from_this<MobPrimaryZones> {
public:
    static std::shared_ptr<MobPrimaryZones> create(std::shared_ptr<ServerMaps>,std::shared_ptr<ZoneServices>,std::unique_ptr<MobPool>,MobPrimaryZoneTransport);
    ~MobPrimaryZones(); // All initialized primaries MUST have completed drain().
    bool initialize(); // Actual InitZones; failure cannot be retried on this owner.
    void attach_obelisks(std::unique_ptr<ObeliskRuntimeMaps>,std::unique_ptr<MobAppliedSkillsObeliskMaps>);
    MobPrimaryZoneLease lease(std::size_t);
    // Membership-safe acquisition for an actual active-list entry.
    MobPrimaryZoneLease lease_primary(RuntimeZone&);
    MobInstanceZoneLease lease_instance(RuntimeZone&);
    // Selection enqueues a new instance through the bound real ZoneServices.
    // The lease must survive until admission installs actual zone membership.
    MobInstanceZoneLease select_instance(std::uint16_t,const ZoneUserAccess&);
    // The caller must supply this actual active thread list and serialize its
    // cursor. A live lease/callback defers recycling. Failed release is retried
    // with retry_instance_retirement, without repeating active-list removal.
    MobInstanceExpiry expire_instance(RuntimeZone&,WorldAdmission&,std::uint32_t,ZoneActiveListRemoval&);
    MobInstanceExpiry retry_instance_retirement(WorldAdmission&,std::uint32_t,ZoneActiveListRemoval&);
    void visit_retiring_instance(const std::function<void(RuntimeZone&)>&);
    bool has_retiring_instance()const;
    std::size_t instance_count()const;
    // Registry order includes pending instances; pins the snapshot for the
    // whole callback sequence. New instances appear on the next traversal.
    void visit_logout_zones(const std::function<void(RuntimeZone&)>&);
    bool startup(std::size_t);
    bool tick(std::size_t,std::uint32_t); // Obelisk timers, not a complete AI frame.
    bool reset(std::size_t,const std::function<std::uint32_t()>&);
    // Sets closing immediately. Existing leases/callbacks defer physical work;
    // caller retains this root, releases leases/stops producers, then retries.
    MobPrimaryZoneClose drain(std::size_t,const std::function<std::uint32_t()>&);
    bool drain_all(const std::function<std::uint32_t()>&);
    bool reopen(std::size_t); // Actual backend InitObjects on same permanent zone.
    void serialized(const std::function<void()>&);
    std::uint32_t serial()const;
    // Resolve only a map owned by this authority.
    std::size_t map_index(const ConfiguredMap&)const;
    MobPool& mob_pool()const noexcept{return *pool_;} // Borrow under root lifetime/serialization.
    // Passive borrowed route only, NOT a lease. Legacy consumers must run
    // inside a root dispatch/serialized scope and never retain this raw pointer.
    ZoneMobs* primary_zone(const ConfiguredMap&)const noexcept override;
    void database(const net::Packet&)override;
    void world_broadcast(const net::Packet&)override;
    void map_broadcast(const ConfiguredMap&,ZoneMobs&,const net::Packet&)override;
private:
    friend class MobPrimaryZoneLease;
    friend class MobInstanceZoneLease;
    struct Life {bool primary=false,closing=false;unsigned stage=0;std::size_t leases=0;std::uint64_t generation=1;};
    MobPrimaryZones(std::shared_ptr<ServerMaps>,std::shared_ptr<ZoneServices>,std::unique_ptr<MobPool>,MobPrimaryZoneTransport);
    RuntimeZone* actual(std::size_t)const;
    std::size_t index(const ConfiguredMap&)const;
    void visit(std::size_t,std::uint64_t,const std::function<void(RuntimeZone&)>&);
    void release(std::size_t)noexcept;
    void visit_instance(RuntimeZone*,std::uint64_t,const std::function<void(RuntimeZone&)>&);
    void release_instance(RuntimeZone*,std::uint64_t)noexcept;
    void attached()const;
    std::shared_ptr<ServerMaps> maps_;std::shared_ptr<ZoneServices> services_;MobPrimaryZoneTransport transport_;
    struct InstanceLife {std::uint64_t generation=0;std::size_t leases=0,visits=0;};
    mutable std::recursive_mutex mutex_;std::vector<Life> life_;std::map<RuntimeZone*,InstanceLife> instances_;
    RuntimeZone* retiring_instance_=nullptr;std::uint64_t instance_generation_=0;
    std::size_t dispatch_=0;bool started_=false,ready_=false,stopping_=false;
    // Explicit teardown destroys router, then pool while retained source objects
    // and registry containers are still alive. Partial Init uses backend RAII.
    // Metadata/map/services survive all actor destruction.
    std::vector<std::unique_ptr<MobPopulation>> retired_population_;
    std::vector<std::unique_ptr<LinkedMobPopulation>> retired_linked_;
    std::unique_ptr<ObeliskRuntimeMaps> records_;std::unique_ptr<MobAppliedSkillsObeliskMaps> obelisks_;
    std::unique_ptr<MobPool> pool_;std::unique_ptr<ZoneRegistry> registry_;
};
}
