#include "eternity/game/zone_mobs.hpp"
#include "eternity/game/mob_pool.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t value,unsigned int n=4) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::int32_t coordinate(float value) {const auto v=std::trunc(static_cast<double>(value)/40.0);return !std::isfinite(v) || v<INT32_MIN || v>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(v);}
struct View {std::int32_t min_x,max_x,min_z,max_z;bool outside_both(CellCoordinates c) const {return (c.x<min_x || c.x>max_x) && (c.z<min_z || c.z>max_z);}};
View view(CellCoordinates c,std::int32_t n) {return {std::max(c.x-2,0),std::min(c.x+2,n-1),std::max(c.z-2,0),std::min(c.z+2,n-1)};}
template<class F> void each(const View& v,F f) {for(auto z=v.min_z;z<=v.max_z;++z) for(auto x=v.min_x;x<=v.max_x;++x) f(CellCoordinates{x,z});}
float difference(float a,float b) {return static_cast<float>(static_cast<double>(a)-b);}
}
net::Packet mob_appearance(const MobRuntime& m,std::uint8_t appearance) {net::Packet p{1,6};append(p,m.id);p.push_back(appearance);append(p,m.type,2);append(p,std::bit_cast<std::uint32_t>(m.position.x));append(p,std::bit_cast<std::uint32_t>(m.position.z));return p;}
net::Packet mob_removed(std::uint32_t id) {net::Packet p{2,6};append(p,id);return p;}
std::int32_t mob_status(const MobRuntime& m) noexcept {return !m.state_stack?m.status:m.state_stack->empty()?0:m.state_stack->back();}
ZoneMobs::ZoneMobs(const MapGeometry& geometry,ZoneMobServices& services):geometry_(geometry),services_(services) {}
ZoneMobs::~ZoneMobs() {for(auto& [mob,entry]:members_) {mob->zone=nullptr;mob->cell.reset();mob->membership_owner_=nullptr;}}
std::size_t ZoneMobs::key(CellCoordinates c) const {const auto n=geometry_.cell_count();if(c.x<0 || c.z<0 || c.x>=n || c.z>=n) throw std::out_of_range("Invalid mob cell");return static_cast<std::size_t>(c.z)*static_cast<std::size_t>(n)+static_cast<std::size_t>(c.x);}
void ZoneMobs::link_cell(MobRuntime& mob,Member& member,CellCoordinates c) {
    if(cell_sequence_==UINT64_MAX) throw std::overflow_error("Mob cell sequence exhausted");
    member.cell=c;member.cell_ordinal=++cell_sequence_;cells_[key(c)].members.emplace(member.cell_ordinal,&mob);
}
void ZoneMobs::unlink_cell(const Member& member) {auto& cell=cells_.at(key(member.cell));const auto entry=cell.members.find(member.cell_ordinal);if(cell.cursor==entry->first) cell.cursor=entry==cell.members.begin()?0:std::prev(entry)->first;cell.members.erase(entry);}
bool ZoneMobs::enter(MobRuntime& mob) {
    const CellCoordinates c{coordinate(mob.position.x),coordinate(mob.position.z)};const auto n=geometry_.cell_count();if(c.x<0 || c.z<0 || c.x>=n || c.z>=n) return false;
    if(mob.membership_owner_ || members_.contains(&mob)) throw std::invalid_argument("Mob already has cell membership");
    if(sequence_==UINT64_MAX) throw std::overflow_error("Mob zone sequence exhausted");
    mob.membership_owner_=this;mob.cell=c;mob.zone=this;mob.respawn_position=mob.position;Member member;member.ordinal=++sequence_;link_cell(mob,member,c);members_.emplace(&mob,member);ordered_.emplace(member.ordinal,&mob);index_.emplace(mob.id,&mob);
    services_.send_cell_view(c,mob_appearance(mob,1));if(mob.lua_ai!=0) services_.lua_init(mob);return true;
}
bool ZoneMobs::leave(MobRuntime& mob) {
    if(mob.zone!=this || !mob.cell || !members_.contains(&mob)) throw std::invalid_argument("Wrong mob ownership on leave");
    const auto member=members_.at(&mob);index_.erase(mob.id);ordered_.erase(member.ordinal);unlink_cell(member);members_.erase(&mob);mob.membership_owner_=nullptr;mob.zone=nullptr;mob.cell.reset();return true;
}
void ZoneMobs::release_to_pool(MobPool& pool) {
    for(auto& [key,cell]:cells_) cell.members.clear();
    index_.clear();
    while(!ordered_.empty()) {
        auto* mob=ordered_.begin()->second;ordered_.erase(ordered_.begin());members_.erase(mob);mob->membership_owner_=nullptr;
        // Preserve raw zone/cell/respawn state, including during AI cleanup.
        pool.release(*mob);
    }
}
void ZoneMobs::transition(const MobRuntime& mob,CellCoordinates previous,CellCoordinates next) {
    // SendMoveMob 0041f860 excludes a cell only when it is outside on BOTH axes.
    const auto old_view=view(previous,geometry_.cell_count()),new_view=view(next,geometry_.cell_count());
    if(auto p=services_.allocate_broadcast(mob_removed(mob.id),std::nullopt)) each(old_view,[&](CellCoordinates c){if(new_view.outside_both(c)) p->send_cell(c);});
    if(auto p=services_.allocate_broadcast(mob_appearance(mob,0),std::nullopt)) each(new_view,[&](CellCoordinates c){if(old_view.outside_both(c)) p->send_cell(c);});
}
bool ZoneMobs::move(MobRuntime& mob,UserPosition destination,std::uint8_t mode) {
    if(!mob.cell || !members_.contains(&mob) || (mob.zone && mob.zone!=this)) throw std::invalid_argument("Mob movement requires owned cell membership");
    if(!geometry_.safe_position(destination.x,destination.z)) return false;
    const auto previous=*mob.cell;
    if(!geometry_.view(previous).contains(destination.x,destination.z)) {
        const CellCoordinates next{coordinate(destination.x),coordinate(destination.z)};const auto n=geometry_.cell_count();
        if(next.x>=0 && next.z>=0 && next.x<n && next.z<n && (next.x!=previous.x || next.z!=previous.z)) {auto& member=members_.at(&mob);unlink_cell(member);link_cell(mob,member,next);transition(mob,previous,next);mob.cell=next;}
    }
    mob.previous_position=mob.position;mob.position=destination;mob.move_direction={difference(destination.x,mob.previous_position.x),0,difference(destination.z,mob.previous_position.z)};mob.previous_move_time=services_.tick_ms();
    // Normalize spills squared length to float before sqrt, whereas move_range
    // keeps the squared planar distance as double until its separate sqrt.
    const auto x=mob.move_direction.x,z=mob.move_direction.z;
    if(x!=0 || z!=0) {const auto squared=static_cast<float>(static_cast<double>(x)*x+static_cast<double>(z)*z);const auto length=static_cast<float>(std::sqrt(static_cast<double>(squared)));mob.move_direction={static_cast<float>(static_cast<double>(x)/length),static_cast<float>(0.0/static_cast<double>(length)),static_cast<float>(static_cast<double>(z)/length)};}
    else mob.move_direction={};
    const auto back_x=difference(mob.previous_position.x,mob.position.x),back_z=difference(mob.previous_position.z,mob.position.z);mob.move_range=static_cast<float>(std::sqrt(static_cast<double>(back_z)*back_z+static_cast<double>(back_x)*back_x));
    if(mob.zone) {net::Packet p{3,6};append(p,mob.id);p.push_back(mode);append(p,std::bit_cast<std::uint32_t>(mob.position.x));append(p,std::bit_cast<std::uint32_t>(mob.position.z));if(auto buffer=services_.allocate_broadcast(p,6)) each(view(*mob.cell,geometry_.cell_count()),[&](CellCoordinates c){buffer->send_cell(c);});}return true;
}
MobRuntime* ZoneMobs::find(std::uint32_t id) const noexcept {const auto at=index_.find(id);return at==index_.end()?nullptr:at->second;}
MobRuntime* ZoneMobs::find_cell(CellCoordinates c,std::uint32_t id) {const auto at=cells_.find(key(c));if(at==cells_.end()) return nullptr;auto& cell=at->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;while(cell.cursor) {auto* mob=cell.members.at(cell.cursor);if(mob->id==id) return mob;const auto next=cell.members.upper_bound(cell.cursor);cell.cursor=next==cell.members.end()?0:next->first;}return nullptr;}
std::vector<MobRuntime*> ZoneMobs::all() const {std::vector<MobRuntime*> out;for(const auto& [ordinal,mob]:ordered_) out.push_back(mob);return out;}
std::vector<MobRuntime*> ZoneMobs::in_cell(CellCoordinates c) const {std::vector<MobRuntime*> out;const auto at=cells_.find(key(c));if(at!=cells_.end()) for(const auto& [ordinal,mob]:at->second.members) out.push_back(mob);return out;}
void ZoneMobs::send(CellCoordinates c,MobViewer& viewer,bool entering) {
    const auto at=cells_.find(key(c));if(at==cells_.end()) return;auto& cell=at->second;cell.cursor=cell.members.empty()?0:cell.members.begin()->first;
    while(cell.cursor) {const auto* mob=cell.members.at(cell.cursor);if(!entering || mob_status(*mob)!=2) viewer.send_self(entering?mob_appearance(*mob,0):mob_removed(mob->id));const auto next=cell.cursor==0?cell.members.begin():cell.members.upper_bound(cell.cursor);cell.cursor=next==cell.members.end()?0:next->first;}
}
void ZoneMobs::send_enter(CellCoordinates c,MobViewer& viewer) {send(c,viewer,true);}
void ZoneMobs::send_leave(CellCoordinates c,MobViewer& viewer) {send(c,viewer,false);}
}
