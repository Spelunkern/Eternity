#pragma once
#include "eternity/game/maps.hpp"
#include "eternity/game/players.hpp"

namespace eternity::game {
struct ItemDefinition;
class ZoneItems;
struct GroundItem {
    PlayerItem item;
    const ItemDefinition* definition=nullptr;
    UserPosition position;
    std::uint32_t user_id=0,party_id=0,drop_id=0,money=0;
    std::uint8_t drop_type=0;
    std::uint32_t id=0,expires_at=0; // CItem+14/+6c; id is distinct from the definition's ItemID.
    ZoneItems* zone=nullptr;
    std::optional<CellCoordinates> cell;
    std::array<std::uint8_t,21> craft_storage{}; // CItem+4c; bytes after the first NUL survive reuse.
    std::array<std::uint16_t,11> craft_additions{}; // CItem+84.
};
net::Packet ground_item_appearance(const GroundItem&,std::uint8_t appearance);
net::Packet ground_item_removed(std::uint32_t id);
class ItemViewer {
public:
    virtual ~ItemViewer() = default;
    virtual void send_self(const net::Packet&) = 0;
};
class ZoneItemServices {
public:
    virtual ~ZoneItemServices() = default;
    virtual std::uint32_t tick_ms() = 0;
    virtual void send_cell_view(CellCoordinates,const net::Packet&) = 0;
    virtual void release(GroundItem&) = 0;
};
// Serialized zone executor. Allocations, geometry and services must outlive the view.
// The zone owner releases items before recycling the zone; leaving alone does not free.
class ZoneItems {
public:
    ZoneItems(const MapGeometry&,ZoneItemServices&);
    ~ZoneItems();
    ZoneItems(const ZoneItems&) = delete;
    ZoneItems& operator=(const ZoneItems&) = delete;
    bool enter(GroundItem&,std::uint8_t appearance,std::uint32_t lifetime=60000);
    bool leave(GroundItem&);
    GroundItem* find(std::uint32_t id) const noexcept;
    GroundItem* find_cell(CellCoordinates,std::uint32_t id);
    void update(std::uint32_t tick);
    void send_enter(CellCoordinates,ItemViewer&);
    void send_leave(CellCoordinates,ItemViewer&);
    std::size_t size() const noexcept {return members_.size();}
private:
    struct Cell {std::map<std::uint64_t,GroundItem*> members;std::uint64_t cursor=0;};
    std::size_t key(CellCoordinates) const;
    void send(CellCoordinates,ItemViewer&,bool entering);
    const MapGeometry& geometry_;ZoneItemServices& services_;
    std::map<std::uint32_t,GroundItem*> index_;
    std::map<std::uint64_t,GroundItem*> ordered_;
    std::map<GroundItem*,std::uint64_t> members_;
    std::map<std::size_t,Cell> cells_;
    std::uint64_t sequence_=0,cursor_=0;
};
}
