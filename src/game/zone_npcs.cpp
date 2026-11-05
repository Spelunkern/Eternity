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
    return map.blocked({x,z});
}
struct View {
    std::int32_t min_x,max_x,min_z,max_z;
    bool outside_both(CellCoordinates cell) const {return (cell.x<min_x || cell.x>max_x) && (cell.z<min_z || cell.z>max_z);}
};
View view(CellCoordinates cell,std::int32_t count) {return {std::max(cell.x-2,0),std::min(cell.x+2,count-1),std::max(cell.z-2,0),std::min(cell.z+2,count-1)};}
template<class Function> void each_cell(const View& area,Function function) {
    for(auto z=area.min_z;z<=area.max_z;++z) for(auto x=area.min_x;x<=area.max_x;++x) function(CellCoordinates{x,z});
}
}
NpcPool::Lease::~Lease() {if(pool_) pool_->unpin(*npc_);}
NpcPool::Lease::Lease(Lease&& other) noexcept:pool_(other.pool_),npc_(other.npc_) {other.pool_=nullptr;other.npc_=nullptr;}
NpcPool::Lease& NpcPool::Lease::operator=(Lease&& other) noexcept {
    if(this!=&other) {if(pool_) pool_->unpin(*npc_);pool_=other.pool_;npc_=other.npc_;other.pool_=nullptr;other.npc_=nullptr;}return *this;
}
NpcPool::~NpcPool() {if(!pins_.empty()) std::terminate();}
NpcPool::Lease NpcPool::pin(Npc& npc) {
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==&npc;}) ||
       std::find(available_.begin(),available_.end(),&npc)!=available_.end() ||
       std::find(deferred_.begin(),deferred_.end(),&npc)!=deferred_.end())
        throw std::invalid_argument("Cannot pin an unowned or released NPC");
    ++pins_[&npc];return Lease(*this,npc);
}
void NpcPool::unpin(Npc& npc) noexcept {
    const auto it=pins_.find(&npc);if(it==pins_.end() || !it->second) std::terminate();
    if(--it->second) return;
    pins_.erase(it);const auto deferred=std::find(deferred_.begin(),deferred_.end(),&npc);
    if(deferred!=deferred_.end()) {deferred_.erase(deferred);available_.push_back(&npc);}
}
Npc* NpcPool::allocate() {
    // SSyncHeap<CNpc>::Alloc 0040acc0: recycled objects are not reset.
    if(!available_.empty()) {auto* result=available_.back();available_.pop_back();return result;}
    // Release/unpin do not allocate, including from Lease's noexcept destructor.
    const auto required=objects_.size()+1;
    if(available_.capacity()<required) available_.reserve(std::max(required,available_.capacity()*2));
    if(deferred_.capacity()<required) deferred_.reserve(std::max(required,deferred_.capacity()*2));
    auto object=std::make_unique<Npc>();auto* result=object.get();objects_.push_back(std::move(object));return result;
}
void NpcPool::release(Npc& npc) {
    if(npc.zone || npc.cell) throw std::invalid_argument("Cannot recycle an attached NPC");
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& object){return object.get()==&npc;}) ||
       std::find(available_.begin(),available_.end(),&npc)!=available_.end() ||
       std::find(deferred_.begin(),deferred_.end(),&npc)!=deferred_.end())
        throw std::invalid_argument("NPC does not belong to the active pool");
    if(pins_.contains(&npc)) deferred_.push_back(&npc);else available_.push_back(&npc);
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
    link_cell(npc,*npc.cell);members_.push_back(&npc);index_.emplace(npc.id,&npc);
    services_.send_cell_view(*npc.cell,npc_appearance(npc));return true;
}
bool ZoneNpcs::leave(Npc& npc) {
    // LeaveNpc 00416e90 unlinks before broadcasting; cell coordinates clear after.
    if(npc.zone!=this || !npc.cell) return false;
    const auto cell=*npc.cell;index_.erase(npc.id);
    const auto position=std::find(members_.begin(),members_.end(),&npc);
    if(global_cursor_==&npc) {
        if(position==members_.end()) throw std::logic_error("NPC traversal cursor lost membership");
        global_cursor_=position==members_.begin()?nullptr:*std::prev(position);
    }
    if(position!=members_.end()) members_.erase(position);
    unlink_cell(npc,cell);
    npc.zone=nullptr;
    net::Packet packet(6);put(packet,0,0xe02,2);put(packet,2,npc.id,4);services_.send_cell_view(cell,packet);npc.cell.reset();return true;
}
void ZoneNpcs::visit_all(NpcPool& pool,const std::function<void(Npc&)>& visit) {
    if(!visit) throw std::invalid_argument("NPC traversal requires a visitor");
    global_cursor_=members_.empty()?nullptr:members_.front();
    while(global_cursor_) {
        auto* current=global_cursor_;auto lease=pool.pin(*current);visit(*current);
        // LeaveNpc may have backed the shared cursor to the predecessor; nested
        // traversals can also replace it. Never dereference the released NPC.
        auto next=global_cursor_?std::find(members_.begin(),members_.end(),global_cursor_):members_.end();
        if(global_cursor_ && next==members_.end()) throw std::logic_error("NPC traversal cursor lost membership");
        next=global_cursor_?std::next(next):members_.begin();
        global_cursor_=next==members_.end()?nullptr:*next;
    }
}
void ZoneNpcs::announce_transition(const Npc& npc,CellCoordinates previous,CellCoordinates next) {
    // SendMoveNpc 0041faa0 requires exclusion on BOTH axes, rather than ordinary
    // rectangle subtraction. Adjacent horizontal/vertical shifts announce nothing.
    const auto old_view=view(previous,geometry_.cell_count()),new_view=view(next,geometry_.cell_count());
    net::Packet leave(6);put(leave,0,0xe02,2);put(leave,2,npc.id,4);
    if(auto buffer=services_.allocate_broadcast(leave,std::nullopt))
        each_cell(old_view,[&](CellCoordinates cell){if(new_view.outside_both(cell)) buffer->send_cell(cell);});
    if(auto buffer=services_.allocate_broadcast(npc_appearance(npc),std::nullopt))
        each_cell(new_view,[&](CellCoordinates cell){if(old_view.outside_both(cell)) buffer->send_cell(cell);});
}
bool ZoneNpcs::move(Npc& npc,UserPosition destination,std::uint8_t mode) {
    // MoveNpc 00416f30 checks the DESTINATION's view, then derives a replacement
    // cell from the STORED position. Position assignment follows transition notices.
    if(!npc.cell || (npc.zone && npc.zone!=this)) throw std::invalid_argument("NPC movement requires this zone's cell membership");
    if(!geometry_.safe_position(destination.x,destination.z)) return false;
    const auto previous=*npc.cell;
    if(!geometry_.view(previous).contains(destination.x,destination.z)) {
        const auto x=truncate(static_cast<double>(npc.position.x)/40.0),z=truncate(static_cast<double>(npc.position.z)/40.0);
        if(x>=0 && z>=0 && x<geometry_.cell_count() && z<geometry_.cell_count() && (x!=previous.x || z!=previous.z)) {
            const CellCoordinates next{x,z};unlink_cell(npc,previous);link_cell(npc,next);
            announce_transition(npc,previous,next);npc.cell=next;
        }
    }
    npc.position=destination;
    net::Packet packet(19);put(packet,0,0xe03,2);put(packet,2,npc.id,4);put(packet,6,mode,1);
    put(packet,7,std::bit_cast<std::uint32_t>(destination.x),4);put(packet,11,std::bit_cast<std::uint32_t>(destination.y),4);put(packet,15,std::bit_cast<std::uint32_t>(destination.z),4);
    if(npc.zone) if(auto buffer=services_.allocate_broadcast(packet,std::uint8_t{6}))
        each_cell(view(*npc.cell,geometry_.cell_count()),[&](CellCoordinates cell){buffer->send_cell(cell);});
    return true;
}
void ZoneNpcs::link_cell(Npc& npc,CellCoordinates coordinates) {
    if(cell_sequence_==UINT64_MAX) throw std::overflow_error("NPC cell sequence exhausted");
    auto& cell=cells_[key(coordinates)];const auto ordinal=++cell_sequence_;
    cell.members.push_back(&npc);cell.ordered.emplace(ordinal,&npc);cell_ordinals_.emplace(&npc,ordinal);
}
void ZoneNpcs::unlink_cell(Npc& npc,CellCoordinates coordinates) {
    auto& cell=cells_.at(key(coordinates));const auto ordinal=cell_ordinals_.at(&npc);
    const auto it=cell.ordered.find(ordinal);
    if(cell.cursor==ordinal) cell.cursor=it==cell.ordered.begin()?0:std::prev(it)->first;
    cell.ordered.erase(it);std::erase(cell.members,&npc);cell_ordinals_.erase(&npc);
}
void ZoneNpcs::send_cell_snapshot(CellCoordinates coordinates,NpcViewer& viewer,bool entering) {
    const auto found=cells_.find(key(coordinates));if(found==cells_.end()) return;
    auto& cell=found->second;cell.cursor=cell.ordered.empty()?0:cell.ordered.begin()->first;
    while(cell.cursor) {
        const auto* npc=cell.ordered.at(cell.cursor);
        if(entering) viewer.send_self(npc_appearance(*npc));
        else {net::Packet packet(6);put(packet,0,0xe02,2);put(packet,2,npc->id,4);viewer.send_self(packet);}
        const auto next=cell.cursor==0?cell.ordered.begin():cell.ordered.upper_bound(cell.cursor);
        cell.cursor=next==cell.ordered.end()?0:next->first;
    }
}
void ZoneNpcs::send_enter(CellCoordinates cell,NpcViewer& viewer) {send_cell_snapshot(cell,viewer,true);}
void ZoneNpcs::send_leave(CellCoordinates cell,NpcViewer& viewer) {send_cell_snapshot(cell,viewer,false);}
Npc* ZoneNpcs::find(std::uint32_t id) const noexcept {const auto found=index_.find(id);return found==index_.end()?nullptr:found->second;}
std::span<Npc* const> ZoneNpcs::in_cell(CellCoordinates cell) const noexcept {const auto found=cells_.find(key(cell));return found==cells_.end()?std::span<Npc* const>{}:std::span<Npc* const>(found->second.members);}
}
