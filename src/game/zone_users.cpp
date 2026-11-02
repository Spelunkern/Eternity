#include "eternity/game/zone_users.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/maps.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace eternity::game {
bool same_team(const Actor& subject,const Actor& other,std::int32_t map_team_mode) {
    // ps_game.exe CUser::IsTeam(CUser*) 0044bdf0: relation checks are directional.
    if(&subject==&other) return true;
    const auto& a=subject.relations();const auto& b=other.relations();
    if(a.duel_state==4 && a.duel_opponent==other.id()) return false;
    if(a.guild_id!=0 && b.guild_id!=0 && b.guild_id==a.opposing_guild_id && b.opposing_guild_id==a.guild_id &&
        a.guild_duel_ready && b.guild_duel_ready) return false;
    return map_team_mode!=0 || subject.appearance().identity.country==other.appearance().identity.country;
}
ZoneUsers::ZoneUsers(std::int32_t width,std::int32_t height,std::int32_t mode):width_(width),height_(height),team_mode_(mode) {
    if(width<=0 || height<=0 || static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)>std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("Invalid zone user-grid dimensions");
    cells_.resize(static_cast<std::size_t>(width)*static_cast<std::size_t>(height));
}
ZoneUsers::~ZoneUsers() {for(auto& [id,user]:users_) {static_cast<void>(id);user.actor->status().in_zone=false;}}
ZoneUsers::ZoneUsers(const MapGeometry& geometry,std::int32_t mode):ZoneUsers(geometry.cell_count(),geometry.cell_count(),mode) {map_size_=geometry.size();}
std::size_t ZoneUsers::index(std::int32_t x,std::int32_t z) const {
    if(x<0 || x>=width_ || z<0 || z>=height_) throw std::out_of_range("User cell outside zone grid");
    return static_cast<std::size_t>(z)*static_cast<std::size_t>(width_)+static_cast<std::size_t>(x);
}
void ZoneUsers::attach(Actor& actor,std::int32_t x,std::int32_t z,UserPosition position) {
    const auto cell=index(x,z);
    if(users_.contains(actor.id()) || actor.status().in_zone) throw std::invalid_argument("Actor already belongs to a zone");
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Zone insertion sequence exhausted");
    const auto ordinal=++sequence_;auto [entry,inserted]=users_.emplace(actor.id(),Member{&actor,position,cell,ordinal});static_cast<void>(inserted);
    try {cells_[cell].members.emplace(ordinal,actor.id());}catch(...) {users_.erase(entry);throw;}
    actor.status().in_zone=true;
}
void ZoneUsers::unlink(const Member& member) {
    auto& cell=cells_[member.cell];const auto entry=cell.members.find(member.ordinal);
    if(cell.cursor==member.ordinal) cell.cursor=entry==cell.members.begin()?0:std::prev(entry)->first;
    cell.members.erase(entry);
}
bool ZoneUsers::attach_at_position(Actor& actor,UserPosition position) {
    if(!map_size_) throw std::logic_error("Position attachment requires original map geometry");
    const auto cell=MapGeometry(*map_size_).cell(position.x,position.z);if(!cell) return false;
    attach(actor,cell->x,cell->z,position);return true;
}
bool ZoneUsers::move_at_position(std::uint32_t id,UserPosition position) {
    if(!map_size_) throw std::logic_error("Position movement requires original map geometry");
    const MapGeometry geometry(*map_size_);auto& member=users_.at(id);
    if(!geometry.safe_position(position.x,position.z)) return false;
    member.position=position; // Original stores position before validating a changed cell.
    const CellCoordinates previous{static_cast<std::int32_t>(member.cell%static_cast<std::size_t>(width_)),static_cast<std::int32_t>(member.cell/static_cast<std::size_t>(width_))};
    const auto cell=geometry.cell_after_move(previous,position.x,position.z);if(!cell) return false;
    move(id,cell->x,cell->z,position);return true;
}
std::optional<UserPosition> ZoneUsers::position(std::uint32_t id) const {
    const auto member=users_.find(id);if(member==users_.end()) return {};return member->second.position;
}
bool ZoneUsers::visit_cell(std::int32_t x,std::int32_t z,const std::function<bool(Actor&,UserPosition)>& visitor) {
    auto& cell=cells_.at(index(x,z));cell.cursor=0;
    for(auto entry=cell.members.begin();entry!=cell.members.end();entry=cell.cursor==0?cell.members.begin():cell.members.upper_bound(cell.cursor)) {
        cell.cursor=entry->first;const auto& member=users_.at(entry->second);
        if(visitor(*member.actor,member.position)) return true;
    }
    cell.cursor=0;return false;
}
bool ZoneUsers::detach(std::uint32_t id) {
    const auto entry=users_.find(id);if(entry==users_.end()) return false;
    unlink(entry->second);entry->second.actor->status().in_zone=false;users_.erase(entry);return true;
}
void ZoneUsers::move(std::uint32_t id,std::int32_t x,std::int32_t z,UserPosition position) {
    const auto next=index(x,z);auto& member=users_.at(id);
    if(next!=member.cell) {
        if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Zone insertion sequence exhausted");
        const auto ordinal=++sequence_;cells_[next].members.emplace(ordinal,id);unlink(member);member.cell=next;member.ordinal=ordinal;
    }
    member.position=position;
}
void ZoneUsers::detect_hidden(std::uint32_t source_id,std::uint8_t range) {
    // ChkFindHide 00486e00 visits the clipped 3x3 cells, row first, insertion order within each.
    const auto source=users_.find(source_id);if(source==users_.end()) return;
    Actor* const source_actor=source->second.actor;const auto cell=source->second.cell;
    const auto x=static_cast<std::int32_t>(cell%static_cast<std::size_t>(width_));
    const auto z=static_cast<std::int32_t>(cell/static_cast<std::size_t>(width_));
    for(auto row=std::max(z-1,0);row<=std::min(z+1,height_-1);++row) for(auto column=std::max(x-1,0);column<=std::min(x+1,width_-1);++column) {
        auto& current=cells_[index(column,row)];current.cursor=0;
        for(auto entry=current.members.begin();entry!=current.members.end();entry=current.cursor==0?current.members.begin():current.members.upper_bound(current.cursor)) {
            current.cursor=entry->first;auto& candidate=users_.at(entry->second);auto& actor=*candidate.actor;
            if(actor.attack_state().mode==1 || same_team(actor,*source_actor,team_mode_) || !actor.appearance().attackable || !actor.appearance().visible) continue;
            const auto live_source=users_.find(source_id);if(live_source==users_.end()) return;
            // x87 stores differences and sqrt result as float, but evaluates squares/sum at 53-bit precision.
            const float dx=candidate.position.x-live_source->second.position.x,dz=candidate.position.z-live_source->second.position.z;
            const auto distance=static_cast<float>(std::sqrt(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz));
            if(distance<=static_cast<float>(range)) actor.reveal();
        }
        current.cursor=0;
    }
}
}
