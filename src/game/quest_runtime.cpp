#include "eternity/game/quest_runtime.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t changed(std::int32_t value,std::uint32_t delta) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)+delta);}
bool recycles_on_removal(const QuestDefinition& definition) {
    const auto& record=definition.record();return std::any_of(record.begin()+0x134,record.begin()+0x138,[](auto value){return value!=0;});
}
void append(net::Packet& packet,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
RestoredQuest* QuestRuntimePool::allocate() {
    if(!free_.empty()) {auto* result=free_.back();free_.pop_back();return result;}
    auto quest=services_.construct();++attempts_;if(!quest) return nullptr;
    free_.reserve(storage_.size()+1);
    auto* result=quest.get();storage_.push_back(std::move(quest));owners_.emplace(result,nullptr);return result;
}
void QuestRuntimePool::claim(RestoredQuest& quest,QuestRuntime* owner) {
    const auto at=owners_.find(&quest);
    if(at==owners_.end() || at->second || std::find(free_.begin(),free_.end(),&quest)!=free_.end()) throw std::invalid_argument("Quest cannot enter runtime ownership");
    at->second=owner;
}
void QuestRuntimePool::unclaim(RestoredQuest& quest) {
    const auto at=owners_.find(&quest);if(at==owners_.end() || !at->second) throw std::invalid_argument("Quest lacks runtime ownership");at->second=nullptr;
}
void QuestRuntimePool::release(RestoredQuest& quest) {
    const auto at=owners_.find(&quest);
    if(at==owners_.end() || at->second || std::find(free_.begin(),free_.end(),&quest)!=free_.end()) throw std::invalid_argument("Quest cannot return to heap");
    free_.push_back(&quest);
}
void QuestRuntimePool::discard(RestoredQuest& quest) {
    const auto at=std::find_if(storage_.begin(),storage_.end(),[&](const auto& item){return item.get()==&quest;});
    if(at==storage_.end()) throw std::invalid_argument("Quest lacks heap storage");
    owners_.erase(&quest);storage_.erase(at);
}
QuestRuntime::~QuestRuntime() {for(auto* quest:active_) pool_.discard(*quest);for(auto* quest:finished_) pool_.discard(*quest);}
void QuestRuntime::add_active(RestoredQuest& quest) {
    if(!quest.definition) throw std::invalid_argument("Active quest lacks definition");
    pool_.claim(quest,this);
    try {active_.push_back(&quest);}catch(...) {pool_.unclaim(quest);throw;}
    if(quest.definition->end_type()==2) mob_kill_=changed(mob_kill_,1);
    else if(quest.definition->end_type()==3) player_kill_=changed(player_kill_,1);
}
void QuestRuntime::add_finished(RestoredQuest& quest) {
    if(!quest.definition) throw std::invalid_argument("Finished quest lacks definition");
    pool_.claim(quest,this);
    try {finished_.push_back(&quest);}catch(...) {pool_.unclaim(quest);throw;}
}
RestoredQuest* QuestRuntime::load_active(const RestoredQuest& state) {
    if(!state.definition) throw std::invalid_argument("Active quest lacks definition");
    auto* quest=pool_.allocate();if(quest) {*quest=state;try {add_active(*quest);}catch(...) {pool_.release(*quest);throw;}}return quest;
}
RestoredQuest* QuestRuntime::load_finished(const RestoredQuest& state) {
    if(!state.definition) throw std::invalid_argument("Finished quest lacks definition");
    auto* quest=pool_.allocate();if(quest) {*quest=state;try {add_finished(*quest);}catch(...) {pool_.release(*quest);throw;}}return quest;
}
RestoredQuest* QuestRuntime::first_active() noexcept {cursor_=active_.empty()?nullptr:active_.front();return cursor_;}
RestoredQuest* QuestRuntime::next_active() noexcept {
    if(!cursor_) return first_active();
    const auto at=std::find(active_.begin(),active_.end(),cursor_);cursor_=at==active_.end() || std::next(at)==active_.end()?nullptr:*std::next(at);return cursor_;
}
RestoredQuest* QuestRuntime::find_active(std::uint16_t id) noexcept {for(auto* quest=first_active();quest;quest=next_active()) if(quest->packet.id==id) return quest;return nullptr;}
void QuestRuntime::remove(RestoredQuest& quest,std::uint8_t success,QuestRuntimeWorld& world) {
    const auto at=std::find(active_.begin(),active_.end(),&quest);
    if(at==active_.end() || !quest.definition) throw std::invalid_argument("QuestRemove requires a bound active quest");
    const bool repeat=recycles_on_removal(*quest.definition);if(!repeat) finished_.reserve(finished_.size()+1);
    if(cursor_==&quest) cursor_=at==active_.begin()?nullptr:*std::prev(at);
    active_.erase(at);quest.success=repeat?2:success;
    if(quest.definition->end_type()==3) player_kill_=changed(player_kill_,UINT32_MAX);
    else if(quest.definition->end_type()==2) mob_kill_=changed(mob_kill_,UINT32_MAX);
    net::Packet packet{2,10};append(packet,user_id_,4);append(packet,quest.packet.id,2);append(packet,quest.success,1);
    if(repeat) {pool_.unclaim(quest);pool_.release(quest);}else finished_.push_back(&quest);
    world.send_database(packet);
}
}
