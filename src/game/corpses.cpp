#include "eternity/game/corpses.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int length) {
    for(unsigned int i=0;i<length;++i) packet.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
}
std::int32_t cell_coordinate(float value) {
    const auto result=std::trunc(static_cast<double>(value)/40.0);
    if(!std::isfinite(result) || result<INT32_MIN || result>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(result);
}
}
Corpse* CorpsePool::allocate() {
    // SSyncHeap<CDead>::Alloc 0040aeb0. Fresh CDead ctor 004b9520 resets only
    // object membership fields; fresh appearance storage is unspecified there.
    if(!available_.empty()) {auto* corpse=available_.back();available_.pop_back();return corpse;}
    objects_.push_back(std::make_unique<Corpse>());return objects_.back().get();
}
void CorpsePool::release(Corpse& corpse) {
    // CObjectMgr::FreeDead 0040a6a0 retains all snapshot fields.
    if(corpse.zone || corpse.cell) throw std::invalid_argument("Cannot pool an attached corpse");
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& body){return body.get()==&corpse;}) || std::find(available_.begin(),available_.end(),&corpse)!=available_.end())
        throw std::invalid_argument("Invalid corpse pool release");
    available_.push_back(&corpse);
}
void capture_corpse(Corpse& corpse,const Actor& actor,UserPosition position,std::uint16_t direction) {
    // CUser::EnterDead 004584d0 overwrites only occupied equipment slots.
    const auto& identity=actor.appearance().identity;
    const auto name=std::string_view(identity.name).substr(0,identity.name.find('\0'));
    if(name.size()>20) throw std::invalid_argument("Original corpse name exceeds twenty bytes");
    corpse.id=actor.id();corpse.position=position;corpse.direction=direction;
    corpse.appearance={identity.country,identity.family,identity.hair,identity.face,identity.size,identity.job,identity.sex};
    std::array<bool,8> seen{};
    for(const auto& item:actor.inventory()) if(item.bag==0 && item.slot<8) {
        if(seen[item.slot]) throw std::invalid_argument("Unresolved duplicate corpse equipment");
        seen[item.slot]=true;corpse.equipment_type[item.slot]=item.type;corpse.equipment_id[item.slot]=item.type_id;
    }
    std::copy(name.begin(),name.end(),corpse.name.begin());corpse.name[name.size()]=0;
}
net::Packet corpse_appearance(const Corpse& corpse) {
    // CCell::SendCellInfoEnter 00423aa0, corpse block 00423ccb..00423e9e.
    const auto end=std::find(corpse.name.begin(),corpse.name.end(),0);
    if(end==corpse.name.end()) throw std::invalid_argument("Unterminated original corpse name");
    const auto length=static_cast<std::size_t>(end-corpse.name.begin());net::Packet packet(44+length,0);
    put(packet,0,0x405,2);put(packet,2,corpse.id,4);put(packet,6,std::bit_cast<std::uint32_t>(corpse.position.x),4);
    put(packet,10,std::bit_cast<std::uint32_t>(corpse.position.y),4);put(packet,14,std::bit_cast<std::uint32_t>(corpse.position.z),4);put(packet,18,corpse.direction,2);
    std::copy(corpse.appearance.begin(),corpse.appearance.end(),packet.begin()+20);
    std::copy(corpse.equipment_type.begin(),corpse.equipment_type.end(),packet.begin()+27);
    std::copy(corpse.equipment_id.begin(),corpse.equipment_id.end(),packet.begin()+35);
    std::copy_n(corpse.name.begin(),length+1,packet.begin()+43);return packet;
}
net::Packet corpse_removed(std::uint32_t id) {net::Packet packet(6,0);put(packet,0,0x406,2);put(packet,2,id,4);return packet;}
Corpse* WorldCorpses::find(std::uint32_t id) const noexcept {const auto entry=index_.find(id);return entry==index_.end()?nullptr:entry->second;}
bool WorldCorpses::leave(std::uint32_t id) {
    // CWorld::LeaveDead 0040d520 dispatches through the stored owning zone.
    auto* corpse=find(id);if(!corpse) return false;
    if(!corpse->zone) throw std::logic_error("Indexed corpse has no zone");
    return corpse->zone->leave(*corpse);
}
ZoneCorpses::ZoneCorpses(const MapGeometry& geometry,WorldCorpses& world,ZoneCorpseServices& services):geometry_(geometry),world_(world),services_(services) {}
ZoneCorpses::~ZoneCorpses() {
    // Non-owning view cleanup; the concrete zone owner releases pooled objects.
    for(const auto& [corpse,ordinal]:members_) {(void)ordinal;world_.index_.erase(corpse->id);corpse->zone=nullptr;corpse->cell.reset();}
}
std::size_t ZoneCorpses::key(CellCoordinates cell) const {
    const auto count=geometry_.cell_count();
    if(cell.x<0 || cell.z<0 || cell.x>=count || cell.z>=count) throw std::out_of_range("Invalid corpse cell");
    return static_cast<std::size_t>(cell.z)*static_cast<std::size_t>(count)+static_cast<std::size_t>(cell.x);
}
Corpse* ZoneCorpses::create(const Actor& actor,UserPosition position,std::uint16_t direction) {
    auto* corpse=services_.allocate();if(!corpse) return nullptr;
    capture_corpse(*corpse,actor,position,direction);enter(*corpse);return corpse;
}
bool ZoneCorpses::enter(Corpse& corpse) {
    // CZone::EnterDead 00417720 has no appearance broadcast. Duplicate IDs return
    // true after logging, without attaching the new object or replacing the old.
    const CellCoordinates coordinates{cell_coordinate(corpse.position.x),cell_coordinate(corpse.position.z)};
    if(coordinates.x>=geometry_.cell_count() || coordinates.z>=geometry_.cell_count()) return false;
    if(coordinates.x<0 || coordinates.z<0) throw std::invalid_argument("Invalid negative original corpse cell");
    if(world_.find(corpse.id)) {services_.diagnostic(CorpseIssue::duplicate,corpse.id);return true;}
    if(corpse.zone || corpse.cell) throw std::invalid_argument("Corpse already belongs to a zone");
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Corpse cell insertion sequence exhausted");
    corpse.zone=this;corpse.cell=coordinates;
    const auto ordinal=++sequence_;cells_[key(coordinates)].members.emplace(ordinal,&corpse);members_.emplace(&corpse,ordinal);
    world_.index_.emplace(corpse.id,&corpse);return true;
}
bool ZoneCorpses::leave(Corpse& corpse) {
    // CZone::LeaveDead 00417820 erases the global ID before cell unlink, then
    // clears zone, broadcasts 0406, clears coordinates and returns to the heap.
    const auto found=world_.index_.find(corpse.id);
    if(found==world_.index_.end()) {services_.diagnostic(CorpseIssue::missing,corpse.id);return false;}
    if(found->second!=&corpse || corpse.zone!=this || !corpse.cell) throw std::invalid_argument("Wrong corpse ownership on leave");
    world_.index_.erase(found);const auto coordinates=*corpse.cell;auto& cell=cells_.at(key(coordinates));
    const auto entry=cell.members.find(members_.at(&corpse));
    if(cell.cursor==entry->first) cell.cursor=entry==cell.members.begin()?0:std::prev(entry)->first;
    cell.members.erase(entry);members_.erase(&corpse);corpse.zone=nullptr;
    services_.send_cell_view(coordinates,corpse_removed(corpse.id));corpse.cell.reset();services_.release(corpse);return true;
}
Corpse* ZoneCorpses::find(std::uint32_t id) {
    // FindDead 00417640 rejects/logs cross-zone bodies; FindDeadWorld 004176c0
    // is the global WorldCorpses::find and does not apply this restriction.
    auto* corpse=world_.find(id);
    if(corpse && corpse->zone!=this) {services_.diagnostic(CorpseIssue::wrong_zone,id);return nullptr;}
    return corpse;
}
Corpse* ZoneCorpses::find_cell(CellCoordinates coordinates,std::uint32_t id) {
    const auto entry=cells_.find(key(coordinates));if(entry==cells_.end()) return nullptr;
    auto& cell=entry->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;
    while(cell.cursor) {
        auto* corpse=cell.members.at(cell.cursor);if(corpse->id==id) return corpse;
        const auto next=cell.members.upper_bound(cell.cursor);cell.cursor=next==cell.members.end()?0:next->first;
    }
    return nullptr;
}
void ZoneCorpses::send(CellCoordinates coordinates,CorpseViewer& viewer,bool entering) {
    const auto entry=cells_.find(key(coordinates));if(entry==cells_.end()) return;
    auto& cell=entry->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;
    while(cell.cursor) {
        const auto* corpse=cell.members.at(cell.cursor);viewer.send_self(entering?corpse_appearance(*corpse):corpse_removed(corpse->id));
        const auto next=cell.cursor==0?cell.members.begin():cell.members.upper_bound(cell.cursor);
        cell.cursor=next==cell.members.end()?0:next->first;
    }
}
void ZoneCorpses::send_enter(CellCoordinates coordinates,CorpseViewer& viewer) {send(coordinates,viewer,true);}
void ZoneCorpses::send_leave(CellCoordinates coordinates,CorpseViewer& viewer) {send(coordinates,viewer,false);}
}
