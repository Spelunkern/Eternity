#pragma once
#include "eternity/game/ground_items.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <map>

namespace eternity::game {
void initialize_ground_item(GroundItem&);
void initialize_ground_item(GroundItem&,std::uint8_t type,std::uint8_t type_id,std::uint8_t count,std::uint32_t made_at,std::uint8_t make_type);
// Writes a CItem craft string without clearing bytes beyond its terminating NUL.
void set_ground_item_craft(GroundItem&,std::string_view);
// CItem::InitCraftExpansion 004ba280: craftable definitions start with twenty zeros.
void initialize_ground_item_craft(GroundItem&);
struct ItemPoolStatistics {std::uint32_t serial=1,fresh_attempts=0;std::size_t owned=0,available=0;};
// One server-wide item pool, with its own CObjectMgr+4 counter. Reused IDs stay
// stable; IDs here are distinct from persisted item UIDs and definition IDs.
class GroundItemPool {
public:
    using Allocator=std::function<std::unique_ptr<GroundItem>()>;
    explicit GroundItemPool(std::uint32_t serial=1,Allocator allocator={});
    GroundItem* allocate();
    // Native ownership check: object belongs to this pool and is not free.
    bool is_allocated(const GroundItem&) const;
    // Pin prevents reuse while a serialized operation holds the object.
    bool pin(GroundItem&);
    void unpin(GroundItem&) noexcept;
    void release(GroundItem*);
    void release(GroundItem& item) {release(&item);}
    ItemPoolStatistics statistics() const;
private:
    mutable std::mutex mutex_;
    std::uint32_t serial_,fresh_attempts_=0;
    Allocator allocator_;
    std::vector<std::unique_ptr<GroundItem>> objects_;
    std::vector<GroundItem*> available_;
    std::map<GroundItem*,std::size_t> pins_;
};
}
