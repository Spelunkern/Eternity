#include "eternity/game/party_runtime_registry.hpp"
namespace eternity::game {
void reset_registry_party(PartyRegistryParty& party) noexcept {
    party.count=0;party.leader=0;party.item_division=0;party.item_cursor=0;
    party.experience.experience_level=0;
    party.slot_indices.fill(0xffff);party.slots.fill(nullptr);
    party.extra_indices.fill(0xffff);party.extra_slots.fill(nullptr);
    party.experience.kind=0;party.auto_join=false;party.sub_leader=1;
}
void construct_registry_party(PartyRegistryParty& party,PartyRegistryPorts& ports) {
    ports.initialize_lock(PartyRegistryLock::party,&party,4000);
    party.id=0;reset_registry_party(party);
}
void initialize_party_registry(PartyRegistry& registry,PartyRegistryPorts& ports) {
    ports.initialize_lock(PartyRegistryLock::heap,nullptr,4000);
    registry.available=nullptr;registry.allocation_attempts=0;registry.available_count=0;
    ports.initialize_lock(PartyRegistryLock::map,nullptr,4000);
    registry.entries.clear();registry.id_counter.store(1,std::memory_order_seq_cst);
}
PartyRegistryParty* allocate_registry_party(PartyRegistry& registry,PartyRegistryPorts& ports) {
    ports.lock_registry(PartyRegistryLock::heap,nullptr);
    auto* party=registry.available;
    if(party) {
        registry.available=party->next;--registry.available_count;
        ports.unlock_registry(PartyRegistryLock::heap,nullptr);return party;
    }
    party=ports.allocate_object();
    if(party)construct_registry_party(*party,ports);
    ++registry.allocation_attempts; // original increments even on allocation failure
    ports.unlock_registry(PartyRegistryLock::heap,nullptr);return party;
}
PartyRegistryParty* create_registry_party(PartyRegistry& registry,PartyMembershipUser* user,std::int32_t division,PartyRegistryPorts& ports,PartyMembershipWorld& world) {
    if(!user)return nullptr;
    auto* party=allocate_registry_party(registry,ports);if(!party)return nullptr;
    if(party->id==0)party->id=registry.id_counter.fetch_add(1,std::memory_order_seq_cst)+1U;
    reset_registry_party(*party);party->item_division=division;
    enter_normal_party(*party,*user,world);
    ports.lock_registry(PartyRegistryLock::map,nullptr);
    registry.entries.emplace(party->id,party); // insert does not replace duplicates
    ports.unlock_registry(PartyRegistryLock::map,nullptr);return party;
}
void remove_registry_party(PartyRegistry& registry,PartyRegistryParty* party,PartyRegistryPorts& ports) {
    if(!party)return;
    ports.lock_registry(PartyRegistryLock::map,nullptr);
    registry.entries.erase(party->id);
    ports.unlock_registry(PartyRegistryLock::map,nullptr);
}
PartyRegistryParty* find_registry_party(PartyRegistry& registry,std::uint32_t id,PartyRegistryPorts& ports) {
    ports.lock_registry(PartyRegistryLock::map,nullptr);
    const auto found=registry.entries.find(id);auto* party=found==registry.entries.end()?nullptr:found->second;
    ports.unlock_registry(PartyRegistryLock::map,nullptr);return party;
}
void unregister_registry_membership(PartyRegistry& registry,PartyNormalMembership& party,PartyRegistryPorts& ports) {
    remove_registry_party(registry,static_cast<PartyRegistryParty*>(&party),ports);
}
void release_registry_heap(PartyRegistry& registry,PartyRegistryPorts& ports) {
    ports.lock_registry(PartyRegistryLock::heap,nullptr);
    while(registry.available) {
        auto* party=registry.available;registry.available=party->next;
        ports.destroy_lock(PartyRegistryLock::party,party);ports.deallocate_object(*party);
    }
    registry.available=nullptr;registry.allocation_attempts=0;registry.available_count=0;
    ports.unlock_registry(PartyRegistryLock::heap,nullptr);
}
void destroy_party_registry(PartyRegistry& registry,PartyRegistryPorts& ports) {
    ports.lock_registry(PartyRegistryLock::map,nullptr);registry.entries.clear();
    ports.unlock_registry(PartyRegistryLock::map,nullptr);
    ports.destroy_lock(PartyRegistryLock::map,nullptr);
    release_registry_heap(registry,ports);
    ports.destroy_lock(PartyRegistryLock::heap,nullptr);
}
}
