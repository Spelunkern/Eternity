#include "eternity/game/mob_pool.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
MobRuntime* MobPool::allocate() {
    MobRuntime* mob=nullptr;
    if(!available_.empty()) {mob=available_.back();available_.pop_back();}
    else {auto object=services_.construct();++construction_attempts_;if(!object) return nullptr;mob=object.get();objects_.push_back(std::move(object));}
    mob->id=++counter_;mob->drop_random.seed(services_.unix_time()^mob->id);return mob;
}
void MobPool::release(MobRuntime& mob) {
    if(mob.zone || mob.cell) throw std::invalid_argument("Cannot recycle an attached mob");
    if(std::none_of(objects_.begin(),objects_.end(),[&](const auto& owned){return owned.get()==&mob;}) || std::find(available_.begin(),available_.end(),&mob)!=available_.end()) throw std::invalid_argument("Mob does not belong to the active pool");
    services_.release_compiled_ai(mob);
    if(mob.state_stack) {services_.release_stack_ai(mob);mob.state_stack.reset();}
    if(mob.lua_ai!=0) {mob.lua_ai=0;mob.only_lua_attack=0;}
    if(mob.lua_state) {services_.close_lua(mob.lua_state);mob.lua_state=nullptr;}
    available_.push_back(&mob);
}
}
