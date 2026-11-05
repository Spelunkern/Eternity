#pragma once
#include "eternity/game/zones.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <stdexcept>

namespace eternity::game {
// One serialized world-thread list owner; zones remain owned by their actual
// ZoneRegistry. The owner must unlink active instances before recycling them.
// The queue and traversal cursor follow CWorldThread 004072b0/004072f0 and
// Update 00407390. No zone may be destroyed while linked here.
class WorldThreadZones final:public ZoneActiveListRemoval {
public:
    WorldThreadZones()=default;
    WorldThreadZones(const WorldThreadZones&)=delete;
    WorldThreadZones& operator=(const WorldThreadZones&)=delete;
    void add_active(RuntimeZone& zone) {
        require_unlinked(zone);
        if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("World zone order exhausted");
        active_.emplace(sequence_+1,Entry{&zone,zone.map->settings.create_type==0});++sequence_;
    }
    void enqueue_wait(RuntimeZone& zone) {require_unlinked(zone);pending_.push_back(&zone);}
    // CWorldThread::Update 004073a9..00407467. Zero initialized DWORD deadline;
    // promotions are FIFO, and the deadline advances even when none are ready.
    std::size_t promote(std::uint32_t now) {
        if(now<next_promotion_) return 0;
        const auto count=promote_pending();
        next_promotion_=now+100U;return count;
    }
    // Once the frame driver is stopped, pending zones must remain reachable
    // by the real active-list retirement path. This does not advance time.
    std::size_t promote_pending_for_shutdown() {return promote_pending();}
private:
    std::size_t promote_pending() {
        std::size_t count=0;
        while(!pending_.empty()) {
            if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("World zone order exhausted");
            auto* zone=pending_.front();active_.emplace(sequence_+1,Entry{zone,zone->map->settings.create_type==0});++sequence_;pending_.pop_front();++count;
        }
        return count;
    }
public:
    // Live original shared cursor, including removal of the current node.
    // Callers serialize the world and retain each zone through the callback.
    void visit_active(const std::function<void(RuntimeZone&)>& visitor) {
        if(!visitor) throw std::invalid_argument("World zone visitor is required");
        cursor_=0;
        while(true) {
            auto it=active_.upper_bound(cursor_);if(it==active_.end()) break;
            cursor_=it->first;visitor(*it->second.zone);
        }
        cursor_=0;
    }
    void remove_active(RuntimeZone& zone) noexcept override {
        auto it=std::find_if(active_.begin(),active_.end(),[&](const auto& item){return item.second.zone==&zone;});
        if(it==active_.end()) std::terminate();
        if(cursor_==it->first) cursor_=it==active_.begin()?0:std::prev(it)->first;
        active_.erase(it);
    }
    bool contains_active(const RuntimeZone& zone)const noexcept {
        return std::any_of(active_.begin(),active_.end(),[&](const auto& entry){return entry.second.zone==&zone;});
    }
    std::size_t active_count()const noexcept{return active_.size();}
    std::size_t pending_count()const noexcept{return pending_.size();}
    std::uint32_t next_promotion()const noexcept{return next_promotion_;}
    // Actual instances must already have been retired and removed before
    // primary shutdown; pending dynamic zones must not be silently discarded.
    bool has_active_instances()const noexcept {
        for(const auto& entry:active_) if(!entry.second.primary) return true;
        return false;
    }
    bool primary_shutdown_ready()const noexcept {
        return pending_.empty() && !has_active_instances();
    }
    void clear_drained_primaries() {
        if(!primary_shutdown_ready()) throw std::logic_error("Live instances prevent clearing world zones");
        active_.clear();cursor_=0;
    }
private:
    struct Entry {RuntimeZone* zone;bool primary;};
    void require_unlinked(RuntimeZone& zone)const {
        if(!zone.map) throw std::logic_error("World zone has no retained map");
        if(std::find(pending_.begin(),pending_.end(),&zone)!=pending_.end() ||
           std::any_of(active_.begin(),active_.end(),[&](const auto& entry){return entry.second.zone==&zone;}))
            throw std::logic_error("World zone already linked");
    }
    std::list<RuntimeZone*> pending_;std::map<std::uint64_t,Entry> active_;
    std::uint64_t sequence_=0,cursor_=0;std::uint32_t next_promotion_=0;
};
}
