#pragma once
#include "eternity/game/party_runtime_membership.hpp"
#include <atomic>
#include <map>

namespace eternity::game {
// Extends the existing normal-party projection; Reset still touches all 30
// original slots. The extra 23 slots do not implement raid membership.
struct PartyRegistryParty:PartyNormalMembership {
    PartyRegistryParty* previous{};
    PartyRegistryParty* next{}; // original intrusive +0x4
    std::array<std::uint32_t,23> extra_indices{};
    std::array<PartyMembershipUser*,23> extra_slots{};
    std::int32_t item_division{},sub_leader{};
    bool auto_join{};
};
struct PartyRegistry {
    std::atomic<std::uint32_t> id_counter{1};
    PartyRegistryParty* available{};
    std::uint32_t allocation_attempts{},available_count{};
    std::map<std::uint32_t,PartyRegistryParty*> entries;
};
enum class PartyRegistryLock {heap,map,party};
// Required real owner ports. Callbacks must not throw; locks are recursive.
// Registry and captured parties stay alive until their matching callbacks end.
class PartyRegistryPorts {
public:
    virtual ~PartyRegistryPorts()=default;
    virtual void initialize_lock(PartyRegistryLock,PartyRegistryParty*,std::uint32_t spin)=0;
    virtual void destroy_lock(PartyRegistryLock,PartyRegistryParty*)=0;
    virtual void lock_registry(PartyRegistryLock,PartyRegistryParty*)=0;
    virtual void unlock_registry(PartyRegistryLock,PartyRegistryParty*) noexcept=0;
    // Actual object allocator/deallocator. Returns a valid native object or
    // nullptr. Unregister never calls deallocate and never adds a free node.
    virtual PartyRegistryParty* allocate_object()=0;
    virtual void deallocate_object(PartyRegistryParty&)=0;
};
// ps_game.exe CParty::CParty 00444c60 / Reset 00444ca0. Reset retains ID,
// intrusive links and critical section; constructor sets ID zero first.
void construct_registry_party(PartyRegistryParty&,PartyRegistryPorts&);
void reset_registry_party(PartyRegistryParty&) noexcept;
void initialize_party_registry(PartyRegistry&,PartyRegistryPorts&); // CPartyMgr 004473f0
PartyRegistryParty* allocate_registry_party(PartyRegistry&,PartyRegistryPorts&); // 004475f0
// Create 004474b0 enters the existing native normal membership BEFORE publish.
// Its returned bool is ignored in the original. Reset always clears raid kind.
PartyRegistryParty* create_registry_party(PartyRegistry&,PartyMembershipUser*,std::int32_t item_division,PartyRegistryPorts&,PartyMembershipWorld&);
void remove_registry_party(PartyRegistry&,PartyRegistryParty*,PartyRegistryPorts&); // 00447540
PartyRegistryParty* find_registry_party(PartyRegistry&,std::uint32_t,PartyRegistryPorts&); // 00447560
// Direct bridge for PartyMembershipWorld::unregister_party. Every subject must
// be a PartyRegistryParty. Does not recycle/destroy; nested Leave remains live.
void unregister_registry_membership(PartyRegistry&,PartyNormalMembership&,PartyRegistryPorts&);
void release_registry_heap(PartyRegistry&,PartyRegistryPorts&); // 00448620: free chain only
void destroy_party_registry(PartyRegistry&,PartyRegistryPorts&); // 00447450
}
