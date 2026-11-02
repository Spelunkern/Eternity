#include "eternity/game/ground_items.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int length) {for(unsigned int i=0;i<length;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));}
std::int32_t coordinate(float value) {
    const auto result=std::trunc(static_cast<double>(value)/40.0);
    if(!std::isfinite(result) || result<INT32_MIN || result>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(result);
}
}
net::Packet ground_item_appearance(const GroundItem& item,std::uint8_t appearance) {
    // ps_game.exe CZone::EnterItem 00417210/00417320, CCell::SendCellInfoEnter 00424018..004240a7.
    net::Packet packet(22,0);put(packet,0,0x401,2);put(packet,2,item.id,4);packet[6]=appearance;packet[7]=item.item.type;packet[8]=item.item.type_id;packet[9]=item.item.count;
    put(packet,10,std::bit_cast<std::uint32_t>(item.position.x),4);put(packet,14,std::bit_cast<std::uint32_t>(item.position.y),4);put(packet,18,std::bit_cast<std::uint32_t>(item.position.z),4);return packet;
}
net::Packet ground_item_removed(std::uint32_t id) {net::Packet packet(6,0);put(packet,0,0x402,2);put(packet,2,id,4);return packet;}
ZoneItems::ZoneItems(const MapGeometry& geometry,ZoneItemServices& services):geometry_(geometry),services_(services) {}
ZoneItems::~ZoneItems() {for(const auto& [item,ordinal]:members_) {(void)ordinal;item->zone=nullptr;item->cell.reset();}}
std::size_t ZoneItems::key(CellCoordinates cell) const {
    const auto count=geometry_.cell_count();if(cell.x<0 || cell.z<0 || cell.x>=count || cell.z>=count) throw std::out_of_range("Invalid item cell");
    return static_cast<std::size_t>(cell.z)*static_cast<std::size_t>(count)+static_cast<std::size_t>(cell.x);
}
bool ZoneItems::enter(GroundItem& item,std::uint8_t appearance,std::uint32_t lifetime) {
    // EnterItem rounds toward zero, writes membership/expiry, links cell, then
    // AddItem 004170b0 links the zone list and inserts the ID without replacement.
    const CellCoordinates coordinates{coordinate(item.position.x),coordinate(item.position.z)};
    if(coordinates.x>=geometry_.cell_count() || coordinates.z>=geometry_.cell_count()) return false;
    if(coordinates.x<0 || coordinates.z<0) throw std::invalid_argument("Invalid negative original item cell");
    if(item.zone || item.cell) throw std::invalid_argument("Item already belongs to a zone");
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Item insertion sequence exhausted");
    item.expires_at=services_.tick_ms()+lifetime;item.zone=this;item.cell=coordinates;
    const auto ordinal=++sequence_;cells_[key(coordinates)].members.emplace(ordinal,&item);ordered_.emplace(ordinal,&item);members_.emplace(&item,ordinal);index_.emplace(item.id,&item);
    services_.send_cell_view(coordinates,ground_item_appearance(item,appearance));return true;
}
bool ZoneItems::leave(GroundItem& item) {
    // LeaveItem 00417440: RemoveItem erases the ID even if another allocation
    // owns that duplicate key. Ordinary leave never frees the allocation.
    if(item.zone!=this || !item.cell || !members_.contains(&item)) throw std::invalid_argument("Wrong item ownership on leave");
    index_.erase(item.id);const auto ordinal=members_.at(&item);const auto position=ordered_.find(ordinal);
    if(cursor_==ordinal) cursor_=position==ordered_.begin()?0:std::prev(position)->first;
    ordered_.erase(position);const auto coordinates=*item.cell;auto& cell=cells_.at(key(coordinates));const auto entry=cell.members.find(ordinal);
    if(cell.cursor==ordinal) cell.cursor=entry==cell.members.begin()?0:std::prev(entry)->first;
    cell.members.erase(entry);members_.erase(&item);item.zone=nullptr;
    services_.send_cell_view(coordinates,ground_item_removed(item.id));item.cell.reset();return true;
}
GroundItem* ZoneItems::find(std::uint32_t id) const noexcept {const auto entry=index_.find(id);return entry==index_.end()?nullptr:entry->second;}
GroundItem* ZoneItems::find_cell(CellCoordinates coordinates,std::uint32_t id) {
    const auto entry=cells_.find(key(coordinates));if(entry==cells_.end()) return nullptr;
    auto& cell=entry->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;
    while(cell.cursor) {auto* item=cell.members.at(cell.cursor);if(item->id==id) return item;const auto next=cell.members.upper_bound(cell.cursor);cell.cursor=next==cell.members.end()?0:next->first;}return nullptr;
}
void ZoneItems::update(std::uint32_t tick) {
    // UpdateItem 00421e20: unsigned strict expiry, live shared zone cursor, then pool release.
    cursor_=ordered_.empty()?0:ordered_.begin()->first;
    while(cursor_) {
        auto* item=ordered_.at(cursor_);if(item->expires_at<tick) {leave(*item);services_.release(*item);}
        const auto next=cursor_==0?ordered_.begin():ordered_.upper_bound(cursor_);cursor_=next==ordered_.end()?0:next->first;
    }
}
void ZoneItems::send(CellCoordinates coordinates,ItemViewer& viewer,bool entering) {
    const auto entry=cells_.find(key(coordinates));if(entry==cells_.end()) return;auto& cell=entry->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;
    while(cell.cursor) {
        const auto* item=cell.members.at(cell.cursor);viewer.send_self(entering?ground_item_appearance(*item,0):ground_item_removed(item->id));
        const auto next=cell.cursor==0?cell.members.begin():cell.members.upper_bound(cell.cursor);cell.cursor=next==cell.members.end()?0:next->first;
    }
}
void ZoneItems::send_enter(CellCoordinates cell,ItemViewer& viewer) {send(cell,viewer,true);}
void ZoneItems::send_leave(CellCoordinates cell,ItemViewer& viewer) {send(cell,viewer,false);}
}
