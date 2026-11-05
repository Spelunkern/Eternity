#include "eternity/game/item_pool.hpp"
#include "eternity/game/equipment.hpp"
#include <algorithm>
#include <iterator>
#include <new>
#include <stdexcept>

namespace eternity::game {
void initialize_ground_item(GroundItem& item) {
    // ps_game.exe CItem::Init 004b9f30. Definition, UID, runtime ID, world
    // metadata and craft bytes after the first character are deliberately retained.
    item.item.type=0;item.item.type_id=0;item.item.count=0;item.item.quality=0;item.item.gems.fill(0);
    item.item.craft.clear();item.craft_storage[0]=0;item.item.made_at=0;item.item.make_type.clear();item.craft_additions.fill(0);
}
void initialize_ground_item(GroundItem& item,std::uint8_t type,std::uint8_t type_id,std::uint8_t count,std::uint32_t made_at,std::uint8_t make_type) {
    // CItem::Init 004b9f80 reads the existing definition's full-quality WORD.
    if(!item.definition) throw std::invalid_argument("Item initialization needs its original definition");
    initialize_ground_item(item);item.item.type=type;item.item.type_id=type_id;item.item.count=count;item.item.quality=item.definition->quality[0];
    item.item.made_at=made_at;if(make_type!=0) item.item.make_type.assign(1,static_cast<char>(make_type));
}
void initialize_ground_item_craft(GroundItem& item) {
    const auto* definition=item.definition;
    if(!definition || definition->server==0 || definition->required_wisdom==0 || definition->required_wisdom>=100) return;
    set_ground_item_craft(item,"00000000000000000000");
}
void set_ground_item_craft(GroundItem& item,std::string_view name) {
    if(name.size()>20 || name.find('\0')!=std::string_view::npos) throw std::invalid_argument("Invalid original item craft string");
    item.item.craft.assign(name);std::copy(name.begin(),name.end(),item.craft_storage.begin());item.craft_storage[name.size()]=0;
}
GroundItemPool::GroundItemPool(std::uint32_t serial,Allocator allocator):serial_(serial),allocator_(std::move(allocator)) {}
GroundItem* GroundItemPool::allocate() {
    // SSyncHeap<CItem>::Alloc 0040adb0 is LIFO. The fresh-attempt counter
    // increments even when operator new returns null. Fresh native storage is zeroed.
    std::lock_guard lock(mutex_);GroundItem* item=nullptr;
    const auto reusable=std::find_if(available_.rbegin(),available_.rend(),[&](GroundItem* candidate){return !pins_.contains(candidate);});
    if(reusable!=available_.rend()) {item=*reusable;available_.erase(std::next(reusable).base());}
    else {
        ++fresh_attempts_;
        try {
            auto object=allocator_?allocator_():std::make_unique<GroundItem>();if(!object) return nullptr;
            // CItem constructor 004b9f00 initializes only ID and world binding.
            object->id=0;object->zone=nullptr;object->cell.reset();item=object.get();objects_.push_back(std::move(object));
        } catch(const std::bad_alloc&) {return nullptr;}
    }
    // CObjectMgr::CreateItem 0040a510 resets payload on every allocation and
    // increments only its item-specific counter when the retained runtime ID is zero.
    if(item->id==0) item->id=++serial_;
    initialize_ground_item(*item);return item;
}
bool GroundItemPool::is_allocated(const GroundItem& item) const {
    std::lock_guard lock(mutex_);
    return std::any_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==&item;}) &&
        std::find(available_.begin(),available_.end(),&item)==available_.end();
}
bool GroundItemPool::pin(GroundItem& item) {
    std::lock_guard lock(mutex_);
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==&item;}) ||
       std::find(available_.begin(),available_.end(),&item)!=available_.end())return false;
    ++pins_[&item];return true;
}
void GroundItemPool::unpin(GroundItem& item) noexcept {
    std::lock_guard lock(mutex_);auto entry=pins_.find(&item);
    if(entry==pins_.end() || entry->second==0)std::terminate();
    if(--entry->second==0)pins_.erase(entry);
}
void GroundItemPool::release(GroundItem* item) {
    // CObjectMgr::FreeItem 0040a590 accepts null and otherwise retains payload.
    if(!item) return;
    std::lock_guard lock(mutex_);
    if(item->zone || item->cell || std::none_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==item;}) ||
        std::find(available_.begin(),available_.end(),item)!=available_.end()) throw std::invalid_argument("Invalid item pool release");
    available_.push_back(item);
}
ItemPoolStatistics GroundItemPool::statistics() const {std::lock_guard lock(mutex_);return {serial_,fresh_attempts_,objects_.size(),available_.size()};}
}
