#include "eternity/game/skill_replica.hpp"
#include <algorithm>
#include <new>

namespace eternity::game {
std::unique_ptr<CachedSkill> SkillReplicaPool::allocate() {
    std::lock_guard lock(mutex_);if(!available_.empty()) {auto value=std::move(available_.back());available_.pop_back();return value;}
    ++attempts_;try{return allocator_?allocator_():std::make_unique<CachedSkill>();}catch(const std::bad_alloc&){return nullptr;}
}
void SkillReplicaPool::release(std::unique_ptr<CachedSkill> item) {if(item){std::lock_guard lock(mutex_);available_.push_back(std::move(item));}}
void SkillReplicaPool::strand(std::unique_ptr<CachedSkill> item) {if(item){std::lock_guard lock(mutex_);stranded_.push_back(std::move(item));}}
SkillReplicaPoolStatistics SkillReplicaPool::statistics() const {std::lock_guard lock(mutex_);return {attempts_,available_.size(),stranded_.size()};}
SkillReplica::~SkillReplica() {for(auto& item:learned_) pool_.release(std::move(item));for(auto& item:effects_) pool_.release(std::move(item));}
std::vector<CachedSkill> SkillReplica::learned() const {std::vector<CachedSkill> result;for(std::size_t i=0;i<count_;++i) if(learned_[i]) result.push_back(*learned_[i]);return result;}
std::vector<CachedSkill> SkillReplica::effects() const {std::vector<CachedSkill> result;for(const auto& item:effects_) result.push_back(*item);return result;}
bool SkillReplica::load(std::span<const PlayerSkill> learned,std::span<const PersistedEffect> effects) {
    if(count_ || !effects_.empty() || reset_) throw std::logic_error("Skill replica is already loaded");
    // DBCharacter::LoadCharacter 00420610 aborts on its 256th learned row.
    for(const auto& row:learned) {auto item=pool_.allocate();if(!item) return false;*item={row.id,row.level,static_cast<std::uint8_t>(count_),row.delay};learned_[count_++]=std::move(item);if(count_>255) return false;}
    for(const auto& row:effects) {auto item=pool_.allocate();if(!item) return false;item->id=row.skill_id;item->level=row.level;item->delay=row.remaining;effects_.push_back(std::move(item));}
    saved_count_=count_;return true;
}
net::Packet SkillReplica::finish_reset(std::uint16_t points) {
    for(std::size_t i=0;i<count_;++i) {pool_.release(std::move(learned_[i]));}
    count_=0;points_=points;reset_=true;
    net::Packet packet{0x0a,0x05};for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(account_>>(8*i)));return packet;
}
InventoryReplicaResult SkillReplica::apply(std::span<const std::uint8_t> p) {
    // PacketUserSkill 00406100 / PacketUserSkillApply 00406250.
    if(p.size()<6) return InventoryReplicaResult::rejected;
    const auto word=[&](std::size_t at){return static_cast<std::uint16_t>(p[at]|(static_cast<unsigned int>(p[at+1])<<8));};
    const auto dword=[&](std::size_t at){return static_cast<std::uint32_t>(word(at))|(static_cast<std::uint32_t>(word(at+2))<<16);};
    if(dword(2)!=account_) return InventoryReplicaResult::rejected;
    const auto op=word(0);std::size_t size=0;
    switch(op) {case 0x801:size=11;break;case 0x802:size=8;break;case 0x803:size=9;break;case 0x804:if(p.size()<7) return InventoryReplicaResult::rejected;size=7+3U*p[6];break;
        case 0x901:case 0x903:size=13;break;case 0x902:size=9;break;case 0x904:if(p.size()<7) return InventoryReplicaResult::rejected;size=7+7U*p[6];break;case 0x905:size=6;break;default:return InventoryReplicaResult::unsupported;}
    if(p.size()!=size || (op==0x801 && p[10]==1 && count_==256)) return InventoryReplicaResult::rejected;
    const auto matches=[&](const Item& item,std::size_t at){return item->id==word(at) && item->level==p[at+2];};
    switch(op) {
        case 0x801:if(p[10]==1) {auto item=pool_.allocate();if(item) {*item={word(8),p[10],static_cast<std::uint8_t>(count_),0};learned_[count_++]=std::move(item);}}
            else for(std::size_t i=0;i<count_;++i) if(learned_[i] && learned_[i]->id==word(8)) {learned_[i]->level=p[10];learned_[i]->delay=0;}
            points_=word(6);break;
        case 0x802:break; // The original command is deliberately a no-op; reset is a separate DB job.
        case 0x803:if(learned_[p[6]]) learned_[p[6]]->delay=word(7);break;
        case 0x804:for(std::size_t at=7;at<p.size();at+=3) if(p[at]<count_ && learned_[p[at]]) learned_[p[at]]->delay=word(at+1);break;
        case 0x901:case 0x903:{const auto found=std::find_if(effects_.begin(),effects_.end(),[&](const auto& item){return matches(item,6);});
            if(found!=effects_.end()) (*found)->delay=dword(9);
            else if(op==0x901) {auto item=pool_.allocate();if(item) {item->id=word(6);item->level=p[8];item->delay=dword(9);effects_.push_back(std::move(item));}}
            break;}
        case 0x902:{const auto found=std::find_if(effects_.begin(),effects_.end(),[&](const auto& item){return matches(item,6);});if(found!=effects_.end()) {pool_.strand(std::move(*found));effects_.erase(found);}break;}
        case 0x904:{std::size_t current=0;for(std::size_t at=7;at<p.size();at+=7) {while(current<effects_.size() && !matches(effects_[current],at)) ++current;if(current<effects_.size()) effects_[current]->delay=dword(at+3);}break;}
        case 0x905:for(auto& item:effects_) pool_.release(std::move(item));effects_.clear();break;
        default:break;
    }
    return InventoryReplicaResult::applied;
}
}
