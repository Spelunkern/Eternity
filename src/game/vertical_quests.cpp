#include "eternity/game/vertical_quests.hpp"
#include <limits>
#include <stdexcept>
namespace eternity::game {
namespace {
QuestStartItemsActorWorldOwners checked(QuestStartItemsActorWorldOwners o) {
    if(!o.items || !o.item_pool || !o.ids || !o.quests || !o.quest_pool || !o.actor || !o.runtime) throw std::invalid_argument("Quest owner missing");
    return o;
}
}
std::shared_ptr<Npc> VerticalQuestNpcGuard::acquire(std::uint32_t id) {
    auto* npc=zone_.find(id);if(!npc) return {};
    auto self=shared_from_this();auto [it,inserted]=entries_.try_emplace(npc);(void)inserted;
    if(it->second.retired) throw std::logic_error("Cannot lease a retired NPC");
    ++it->second.leases;
    return std::shared_ptr<Npc>(npc,[self](Npc* n){self->unpin(*n);});
}
bool VerticalQuestNpcGuard::retire(Npc& npc) {
    if(npc.zone!=&zone_) return false;
    auto it=entries_.find(&npc);
    if(it!=entries_.end()) it->second.retired=true;
    if(!zone_.leave(npc)) {if(it!=entries_.end()) it->second.retired=false;return false;}
    if(it==entries_.end()) pool_.release(npc);
    return true;
}
void VerticalQuestNpcGuard::unpin(Npc& npc) noexcept {
    const auto it=entries_.find(&npc);
    if(it==entries_.end() || !it->second.leases) std::terminate();
    if(--it->second.leases) return;
    const bool retired=it->second.retired;entries_.erase(it);
    if(retired) pool_.release(npc);
}
VerticalQuests::VerticalQuests(QuestStartItemsActorWorldOwners o,QuestStartItemsActorUseState use,ZoneNpcs& zone,std::shared_ptr<VerticalQuestNpcGuard> guard,VerticalQuestPorts& p)
    :owners_(checked(std::move(o))),npcs_(zone),npc_guard_(std::move(guard)),ports_(p),requests_(*owners_.actor,*owners_.runtime,*owners_.quests,*this),starts_(owners_,use,*this) {if(!npc_guard_ || !npc_guard_->owns(zone)) throw std::invalid_argument("NPC retention guard missing or wrong zone");}
bool VerticalQuests::start(std::uint16_t row,std::uint32_t npc) {
    StartFrame frame;auto* previous=frame_;frame_=&frame;
    try {const bool result=starts_.request(row,npc);frame_=previous;return result;} catch(...) {frame_=previous;throw;}
}
void VerticalQuests::end(std::uint16_t row,std::uint32_t npc) {
    if(owners_.quests->size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::overflow_error("Quest catalog too large");
    quest_end_request(row,npc,static_cast<std::int32_t>(owners_.quests->size()),requests_);
}
void VerticalQuests::select_reward(std::uint16_t id,std::uint8_t selection) {quest_requests_complete_selection(id,selection,requests_);}
void VerticalQuests::kill_mob(std::uint16_t mob,const MobQuestLinks& links) {update_mob_kill_quests(owners_.actor->account_id(),owners_.runtime->mob_kill_quests(),mob,links,*this);}
void VerticalQuests::kill_player() {quest_requests_progress_kill_pc(owners_.actor->account_id(),owners_.runtime->player_kill_quests(),requests_);}
void VerticalQuests::pick_item(std::int32_t type,std::int32_t id,const ItemQuestLinks& links) {quest_requests_progress_pick_item(owners_.actor->account_id(),type,id,links,requests_);}
void VerticalQuests::update_timers(std::uint32_t tick) {quest_requests_update_timers(tick,requests_);}
std::optional<QuestStartNpc> VerticalQuests::find_start_npc(Actor&,std::uint32_t id) {frame_->npc=npc_guard_->acquire(id);auto* n=frame_->npc.get();if(!n) return {};return QuestStartNpc{n->id,static_cast<std::uint32_t>(n->type),static_cast<std::uint32_t>(n->type_id)};}
bool VerticalQuests::require_start_item(Actor&,std::uint8_t type,std::uint8_t id,bool consume) {return requests_.find_item(type,id,consume);}
QuestStartLogContext VerticalQuests::start_log_context(Actor& a) {return ports_.start_log_context(a);}
QuestStartLogNpc VerticalQuests::start_log_npc(Actor&,const QuestStartNpc& npc) {auto* n=frame_->npc.get();if(!n || n->id!=npc.handle || !n->definition) throw std::logic_error("Quest start NPC lost its definition");frame_->name=n->definition->name();return {static_cast<std::uint32_t>(n->type),frame_->name};}
void VerticalQuests::send_start_log(Actor& a,const std::array<std::uint8_t,144>& p) {ports_.send_start_log(a,p);}
std::uint32_t VerticalQuests::local_make_time(Actor& a) {return ports_.local_make_time(a);}
std::uint32_t VerticalQuests::tick_ms(Actor& a) {return ports_.tick_ms(a);}
void VerticalQuests::send_database(Actor& a,const net::Packet& p) {ports_.send_database(a,p);}
void VerticalQuests::send_client(Actor& a,const net::Packet& p) {ports_.send_client(a,p);}
void VerticalQuests::lock_quests(Actor& a) {ports_.lock_quests(a);}
void VerticalQuests::unlock_quests(Actor& a) noexcept {ports_.unlock_quests(a);}
std::optional<QuestRequestNpc> VerticalQuests::find_npc(Actor&,std::uint32_t id) {auto* n=npcs_.find(id);if(!n) return {};return QuestRequestNpc{n->type,n->type_id};}
void VerticalQuests::fill_game_log_main(Actor& a,std::span<std::uint8_t> p) {ports_.fill_game_log_main(a,p);}
void VerticalQuests::send_game_log(Actor& a,const net::Packet& p) {ports_.send_game_log(a,p);}
QuestRewardWorld& VerticalQuests::reward_world(Actor& a) {return ports_.reward_world(a);}
std::uint32_t VerticalQuests::tick_ms() {return ports_.quest_tick_ms();}
RestoredQuest* VerticalQuests::find_quest(std::uint16_t id) {return requests_.find_quest(id);}
void VerticalQuests::send_database(const net::Packet& p) {requests_.send_database(p);}
void VerticalQuests::send_self(const net::Packet& p) {requests_.send_self(p);}
void VerticalQuests::complete_quest(RestoredQuest& q,std::int32_t npc,bool success) {requests_.complete_quest(q,static_cast<std::uint32_t>(npc),success);}
}
