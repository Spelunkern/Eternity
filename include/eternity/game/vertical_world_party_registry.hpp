#pragma once
#include "eternity/game/party_runtime_registry.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace eternity::game {
// Owns native party objects and recursive locks for the recovered registry.
// Operations run on the common world strand; callbacks finish before shutdown;
// shutdown rejects live registrations or members. Returned objects remain
// stable even after removal from the registry until shutdown or heap release.
// ps_game.exe CPartyMgr 004473f0, Create 004474b0, Remove 00447540.
class VerticalWorldPartyRegistry final:public PartyRegistryPorts {
    struct Entry {PartyRegistryParty party;std::recursive_mutex mutex;};
public:
    VerticalWorldPartyRegistry() {initialize_party_registry(registry_,*this);}
    VerticalWorldPartyRegistry(const VerticalWorldPartyRegistry&)=delete;
    VerticalWorldPartyRegistry& operator=(const VerticalWorldPartyRegistry&)=delete;
    ~VerticalWorldPartyRegistry() {
        // Destruction while a user still holds a party pointer would leave a
        // dangling session record. The world owner must drain first.
        if(!registry_.entries.empty())std::terminate();
        for(const auto& [key,value]:objects_)if(value->party.count!=0)std::terminate();
    }
    PartyRegistry& registry() noexcept {return registry_;}
    void unregister(PartyNormalMembership& party) {(void)entry(party);unregister_registry_membership(registry_,party,*this);}
    void lock_party(PartyNormalMembership& party) {entry(party).mutex.lock();}
    void unlock_party(PartyNormalMembership& party) noexcept {entry(party).mutex.unlock();}
    void shutdown() {
        if(!registry_.entries.empty())throw std::logic_error("Party registry still has active entries");
        for(const auto& [key,value]:objects_)if(value->party.count!=0)throw std::logic_error("Party registry still has members");
        destroy_party_registry(registry_,*this);objects_.clear();
    }
    void initialize_lock(PartyRegistryLock,PartyRegistryParty*,std::uint32_t) override {
        // Mutexes are constructed with the owner and each allocated entry.
    }
    void destroy_lock(PartyRegistryLock,PartyRegistryParty*) override {
        // Their storage remains valid until the corresponding owner is freed.
    }
    void lock_registry(PartyRegistryLock kind,PartyRegistryParty* party) override {mutex(kind,party).lock();}
    void unlock_registry(PartyRegistryLock kind,PartyRegistryParty* party) noexcept override {mutex(kind,party).unlock();}
    PartyRegistryParty* allocate_object() override {
        try {auto value=std::make_unique<Entry>();auto* party=&value->party;objects_.emplace(party,std::move(value));return party;}
        catch(const std::bad_alloc&) {return nullptr;}
    }
    void deallocate_object(PartyRegistryParty& party) override {objects_.erase(&party);}
private:
    Entry& entry(PartyNormalMembership& party) {return *objects_.at(&party);}
    std::recursive_mutex& mutex(PartyRegistryLock kind,PartyRegistryParty* party) {
        if(kind==PartyRegistryLock::heap)return heap_;
        if(kind==PartyRegistryLock::map)return map_;
        if(!party)throw std::logic_error("Party lock has no owner");
        return entry(*party).mutex;
    }
    PartyRegistry registry_;std::recursive_mutex heap_,map_;std::unordered_map<PartyNormalMembership*,std::unique_ptr<Entry>> objects_;
};
}
