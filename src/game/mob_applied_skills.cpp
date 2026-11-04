#include "eternity/game/mob_applied_skills.hpp"
#include <algorithm>

namespace eternity::game {
std::vector<MobAppliedSkill*> MobAppliedSkillPool::available() const {
    std::lock_guard lock(mutex_);
    return {free_.rbegin(),free_.rend()};
}
std::uint32_t MobAppliedSkillPool::construction_attempts() const {
    std::lock_guard lock(mutex_);return attempts_;
}
std::size_t MobAppliedSkillPool::owned_count() const {
    std::lock_guard lock(mutex_);return objects_.size();
}
MobAppliedSkill* MobAppliedSkillPool::acquire() {
    std::lock_guard lock(mutex_);
    if (!free_.empty()) {auto* result=free_.back();free_.pop_back();return result;}
    ++attempts_;
    auto object=services_.construct();
    if (!object) return nullptr;
    object->instance_id=0;
    auto* result=object.get();objects_.push_back(std::move(object));return result;
}
void MobAppliedSkillPool::recycle(MobAppliedSkill* skill) {
    std::lock_guard lock(mutex_);free_.push_back(skill);
}
void MobAppliedSkillPool::discard(MobAppliedSkill* skill) {
    std::lock_guard lock(mutex_);
    std::erase_if(objects_,[skill](const auto& object){return object.get()==skill;});
}
MobAppliedSkill* MobAppliedSkills::append_from_pool() {return append(nullptr);}
MobAppliedSkill* MobAppliedSkills::append_created(std::atomic<std::uint32_t>& counter) {
    return append(&counter);
}
MobAppliedSkill* MobAppliedSkills::append(std::atomic<std::uint32_t>* counter) {
    std::lock_guard lock(mutex_);
    auto* skill=pool_.acquire();
    if (skill) {
        if (counter && skill->instance_id==0)
            skill->instance_id=counter->fetch_add(1)+1U;
        try {records_.push_back(skill);} catch (...) {pool_.recycle(skill);throw;}
    }
    return skill;
}
std::vector<MobAppliedSkill*> MobAppliedSkills::records() const {
    std::lock_guard lock(mutex_);return records_;
}
// ps_game.exe CMob::ClearApplySkill 004a0990: log, unlink, Free, next.
// No RemApplySkill, status reversal, damage or client-packet dispatch here.
void MobAppliedSkills::clear(const MobAppliedSkillLogProjection& projection) {
    std::lock_guard lock(mutex_);
    while (!records_.empty()) {
        auto* skill=records_.front();
        if (projection.lua_ai || (projection.respawn_present && projection.respawn_logging))
            log_.removed(*skill,6);
        records_.erase(records_.begin());
        pool_.recycle(skill);
    }
}
// CMob::~CMob 0048ff10 -> SSyncList<CSkill>::Release 0044bc00:
// destruction deletes active records rather than returning them to the heap.
MobAppliedSkills::~MobAppliedSkills() {
    for (auto* skill:records_) pool_.discard(skill);
}
}
