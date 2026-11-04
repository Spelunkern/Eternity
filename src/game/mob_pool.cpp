#include "eternity/game/mob_pool.hpp"
#include "eternity/game/mob_applied_skills_runtime.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
MobRuntime* MobPool::allocate_raw() {
    MobRuntime* mob=nullptr;
    if(!available_.empty()) {mob=available_.back();available_.pop_back();}
    else {auto object=services_.construct();++construction_attempts_;if(!object) return nullptr;object->damage=std::make_unique<MobDamageLedger>(services_.damage_pool());if(skills_)skills_->bind(*object,services_);mob=object.get();objects_.push_back(std::move(object));}
    unprepared_.push_back(mob);return mob;
}
void MobPool::prepare_allocated(MobRuntime& mob) {
    if(mob.membership_owner_)throw std::invalid_argument("Cannot prepare an attached mob");
    const auto pending=std::find(unprepared_.begin(),unprepared_.end(),&mob);
    if(pending==unprepared_.end())throw std::invalid_argument("Mob is not an outstanding raw allocation");
    unprepared_.erase(pending);
    mob.id=++counter_;mob.drop_random.seed(services_.unix_time()^mob.id);
}
MobRuntime* MobPool::allocate() {
    auto* mob=allocate_raw();if(mob)prepare_allocated(*mob);return mob;
}
void MobPool::release(MobRuntime& mob) {
    if(mob.membership_owner_) throw std::invalid_argument("Cannot recycle an attached mob");
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& owned){return owned.get()==&mob;}) || std::find(available_.begin(),available_.end(),&mob)!=available_.end()) throw std::invalid_argument("Mob does not belong to the active pool");
    release_mob_applied_skills_runtime_ai(mob,services_);
    std::erase(unprepared_,&mob);
    available_.push_back(&mob);
}
}
