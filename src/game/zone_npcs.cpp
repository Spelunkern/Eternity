#include "eternity/game/zone_npcs.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t truncate(double value) {
    const auto integral=std::trunc(value);
    if(!std::isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX) return INT32_MIN;
    return static_cast<std::int32_t>(integral);
}
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int size) {
    for(unsigned int i=0;i<size;++i) packet.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));
}
bool blocked_guard(const MapDefinition& map,UserPosition position) {
    const auto x=truncate(position.x),z=truncate(position.z);
    if(x<0 || z<0 || x>=map.size || z>=map.size) return true;
    const auto tile=static_cast<std::size_t>(z)*static_cast<std::size_t>(map.size)+static_cast<std::size_t>(x);
    if(tile/8>=map.tiles.size()) throw std::invalid_argument("Missing original NPC guard tiles");
    return ((map.tiles[tile/8]>>(tile%8))&1)!=0;
}
}
Npc* NpcPool::allocate() {
    // SSyncHeap<CNpc>::Alloc 0040acc0: recycled objects are not reset.
    if(!available_.empty()) {auto* result=available_.back();available_.pop_back();return result;}
    auto object=std::make_unique<Npc>();auto* result=object.get();objects_.push_back(std::move(object));return result;
}
void NpcPool::release(Npc& npc) {
    if(npc.zone || npc.cell) throw std::invalid_argument("Cannot recycle an attached NPC");
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==&npc;}) || std::find(available_.begin(),available_.end(),&npc)!=available_.end())
        throw std::invalid_argument("NPC does not belong to the active pool");
    available_.push_back(&npc);
}
net::Packet npc_appearance(const Npc& npc) {
    // CZone::EnterNpc 00416d70: packed 23-byte TP_NPC_ENTER, angle is a WORD.
    net::Packet packet(23);put(packet,0,0xe01,2);put(packet,2,npc.id,4);
    put(packet,6,static_cast<std::uint32_t>(npc.type),1);put(packet,7,static_cast<std::uint32_t>(npc.type_id),2);
    put(packet,9,std::bit_cast<std::uint32_t>(npc.position.x),4);put(packet,13,std::bit_cast<std::uint32_t>(npc.position.y),4);put(packet,17,std::bit_cast<std::uint32_t>(npc.position.z),4);
    put(packet,21,static_cast<std::uint32_t>(npc.angle),2);return packet;
}
ZoneNpcs::ZoneNpcs(const MapGeometry& geometry,ZoneNpcServices& services):geometry_(geometry),services_(services) {}
ZoneNpcs::~ZoneNpcs() {for(auto* npc:members_) {npc->zone=nullptr;npc->cell.reset();}}
std::size_t ZoneNpcs::key(CellCoordinates cell) const {return static_cast<std::size_t>(cell.z)*static_cast<std::size_t>(geometry_.cell_count())+static_cast<std::size_t>(cell.x);}
void ZoneNpcs::initialize(const MapDefinition& map,const NpcDefinitions& definitions) {
    // CMap::InitNPC 00429130: only the first path position is used at creation.
    if(map.size!=geometry_.size()) throw std::invalid_argument("NPC map geometry mismatch");
    for(const auto& source:map.npcs) {
        if(source.type==5) continue;
        const auto* definition=definitions.find(source.type,source.type_id);
        if(!definition || source.positions.empty()) continue;
        const auto& first=source.positions.front();if(!geometry_.safe_position(first.position.x,first.position.z)) continue;
        const bool disabled=source.type==8 && blocked_guard(map,first.position);
        if(disabled) services_.invalid_guard_position(source);
        auto* npc=services_.allocate();if(!npc) continue;
        if(npc->id==0) npc->id=services_.next_id();
        npc->source=&source;npc->definition=definition;npc->type=source.type;npc->type_id=source.type_id;
        npc->position=first.position;npc->angle=truncate(first.angle);npc->zone=this;
        if(disabled) npc->disable_time=UINT32_MAX;
        enter(*npc);
    }
}
bool ZoneNpcs::enter(Npc& npc) {
    const auto count=geometry_.cell_count();
    const auto x=truncate(static_cast<double>(npc.position.x)/40.0);
    const auto z=truncate(static_cast<double>(npc.position.z)/40.0);
    // Original entry checks the cell extent rather than nominal world bounds.
    if(x<0 || z<0 || x>=count || z>=count) return false;
    if(npc.cell || (npc.zone && npc.zone!=this)) throw std::invalid_argument("NPC already belongs to a zone");
    npc.cell=CellCoordinates{x,z};npc.zone=this;
    cells_[key(*npc.cell)].push_back(&npc);members_.push_back(&npc);index_.emplace(npc.id,&npc);
    services_.send_cell_view(*npc.cell,npc_appearance(npc));return true;
}
bool ZoneNpcs::leave(Npc& npc) {
    // LeaveNpc 00416e90 unlinks before broadcasting; cell coordinates clear after.
    if(npc.zone!=this || !npc.cell) return false;
    const auto cell=*npc.cell;index_.erase(npc.id);std::erase(members_,&npc);auto& contents=cells_.at(key(cell));std::erase(contents,&npc);
    if(contents.empty()) cells_.erase(key(cell));
    npc.zone=nullptr;
    net::Packet packet(6);put(packet,0,0xe02,2);put(packet,2,npc.id,4);services_.send_cell_view(cell,packet);npc.cell.reset();return true;
}
Npc* ZoneNpcs::find(std::uint32_t id) const noexcept {const auto found=index_.find(id);return found==index_.end()?nullptr:found->second;}
std::span<Npc* const> ZoneNpcs::in_cell(CellCoordinates cell) const noexcept {const auto found=cells_.find(key(cell));return found==cells_.end()?std::span<Npc* const>{}:std::span<Npc* const>(found->second);}
}
