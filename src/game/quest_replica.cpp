#include "eternity/game/quest_replica.hpp"
#include <algorithm>
#include <new>

namespace eternity::game {
std::unique_ptr<CachedQuest> QuestReplicaPool::allocate() {
    std::lock_guard lock(mutex_);if(!available_.empty()) {auto result=std::move(available_.back());available_.pop_back();return result;}
    ++attempts_;try{return allocator_?allocator_():std::make_unique<CachedQuest>();}catch(const std::bad_alloc&){return nullptr;}
}
void QuestReplicaPool::release(std::unique_ptr<CachedQuest> item) {if(item){std::lock_guard lock(mutex_);available_.push_back(std::move(item));}}
QuestReplicaPoolStatistics QuestReplicaPool::statistics() const {std::lock_guard lock(mutex_);return {attempts_,available_.size()};}
QuestReplica::~QuestReplica() {for(auto& item:active_) pool_.release(std::move(item));for(auto& item:finished_) pool_.release(std::move(item));}
bool QuestReplica::load(PersistedQuest row) {auto item=pool_.allocate();if(!item) return false;item->save_type=2;item->id=row.id;item->delay=row.delay;item->counters=row.counters;active_.push_back(std::move(item));return true;}
bool QuestReplica::load_finished(FinishedQuest row) {auto item=pool_.allocate();if(!item) return false;item->save_type=0;item->id=row.id;item->success=row.success;finished_.push_back(std::move(item));return true;}
std::vector<CachedQuest> QuestReplica::active() const {std::vector<CachedQuest> result;for(const auto& item:active_) result.push_back(*item);return result;}
std::vector<CachedQuest> QuestReplica::finished() const {std::vector<CachedQuest> result;for(const auto& item:finished_) result.push_back(*item);return result;}
void QuestReplica::save_success() noexcept {for(auto& item:active_) item->save_type=2;for(auto& item:finished_) item->save_type=0;}
InventoryReplicaResult QuestReplica::apply(std::span<const std::uint8_t> p) {
    // ps_dbAgent.exe PacketUserQuest 00406590, AddDeleteQuest 00405380.
    if(p.size()<6) return InventoryReplicaResult::rejected;
    const auto word=[&](std::size_t at){return static_cast<std::uint16_t>(p[at]|(static_cast<unsigned int>(p[at+1])<<8));};
    const auto account=static_cast<std::uint32_t>(word(2))|(static_cast<std::uint32_t>(word(4))<<16);
    if(account!=account_) return InventoryReplicaResult::rejected;
    const auto op=word(0);std::size_t size=0;
    switch(op) {case 0xa01:case 0xa05:size=10;break;case 0xa02:size=9;break;case 0xa03:size=13;break;case 0xa04:if(p.size()<7) return InventoryReplicaResult::rejected;size=7+7U*p[6];break;default:return InventoryReplicaResult::unsupported;}
    if(p.size()!=size || (op==0xa05 && p[8]>=3)) return InventoryReplicaResult::rejected;
    const auto update=[&](Item& item,std::size_t at){item->delay=word(at);std::copy_n(p.begin()+static_cast<std::ptrdiff_t>(at+2),3,item->counters.begin());};
    if(op==0xa01) {
        // Allocate before consuming the previous completion, even when allocation fails.
        auto item=pool_.allocate();if(item) {item->id=word(6);item->delay=word(8);item->counters.fill(0);item->save_type=1;active_.push_back(std::move(item));}
        const auto found=std::find_if(finished_.begin(),finished_.end(),[&](const auto& value){return value->id==word(6);});
        if(found!=finished_.end()) {journal(*found);pool_.release(std::move(*found));finished_.erase(found);}
    } else if(op==0xa04) {
        // Unlike applied-skill bulk updates, quest bulk updates consume each match.
        std::size_t current=0;for(std::size_t at=7;at<p.size();at+=7) {while(current<active_.size() && active_[current]->id!=word(at)) ++current;if(current<active_.size()) {update(active_[current],at+2);++current;}}
    } else {
        const auto found=std::find_if(active_.begin(),active_.end(),[&](const auto& item){return item->id==word(6);});
        if(found!=active_.end()) {
            if(op==0xa02) {if(p[8]==2) {journal(*found);pool_.release(std::move(*found));}else {(*found)->success=p[8];finished_.push_back(std::move(*found));}active_.erase(found);}
            else if(op==0xa03) update(*found,8);
            else (*found)->counters[p[8]]=p[9];
        }
    }
    return InventoryReplicaResult::applied;
}
}
