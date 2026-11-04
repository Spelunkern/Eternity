#include "eternity/game/quest_start_items_actor_world.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace eternity::game {
QuestStartItemsActorWorld::QuestStartItemsActorWorld(QuestStartItemsActorWorldOwners owners,QuestStartItemsActorUseState use,QuestStartItemsActorWorldPorts& ports)
    :owners_(std::move(owners)),use_(use),ports_(ports),
    grants_({owners_.items,owners_.item_pool,owners_.ids,owners_.actor},ports),
    admission_({owners_.quests,owners_.quest_pool,owners_.actor,owners_.runtime},ports) {}
bool QuestStartItemsActorWorld::request(std::uint16_t row,std::uint32_t target) {
    const auto* definition=owners_.quests->find(row);if(!definition || definition->record()[0x138]>4) return false;
    Frame current;current.row=row;current.definition=definition;auto* previous=frame_;frame_=&current;
    try {
        refresh_admission();
        // One actual record suffices after canonical positional lookup. Keep the
        // original row for allocation; never reinterpret its embedded ID as row.
        const auto result=quest_start_request(current.user,std::span(&definition->record(),1),1,target,*this);
        frame_=previous;return result;
    } catch(...) {frame_=previous;throw;}
}
void QuestStartItemsActorWorld::refresh_admission() {
    const auto& identity=owners_.actor->appearance().identity;
    frame_->finished.clear();for(const auto* quest:owners_.runtime->finished()) frame_->finished.push_back({quest->packet.id,quest->success});
    // Legacy QuestStartUser.country is the family byte (+12e), not faction (+12d).
    frame_->user={owners_.actor->status().level,identity.family,identity.grow,identity.job,use_.item,use_.deadline,frame_->finished};
}
std::optional<QuestStartNpc> QuestStartItemsActorWorld::find_npc(std::uint32_t target) {
    auto npc=ports_.find_start_npc(*owners_.actor,target);refresh_admission();return npc;
}
bool QuestStartItemsActorWorld::active(std::uint16_t id) {return owners_.runtime->find_active(id)!=nullptr;}
bool QuestStartItemsActorWorld::finished(std::uint16_t id) {return std::any_of(owners_.runtime->finished().begin(),owners_.runtime->finished().end(),[&](auto* q){return q->packet.id==id;});}
bool QuestStartItemsActorWorld::find_item(std::uint8_t type,std::uint8_t id,bool consume) {return ports_.require_start_item(*owners_.actor,type,id,consume);}
std::uint32_t QuestStartItemsActorWorld::tick_ms() {
    const auto tick=ports_.tick_ms(*owners_.actor);
    // Item ID was checked before GetTickCount; only its deadline is read after
    // the callback (0047f537..0047f551). Do not repeat the item/admission checks.
    frame_->user.last_item_deadline=use_.deadline;return tick;
}
bool QuestStartItemsActorWorld::add(const QuestStartRecord& q) {
    if(&q!=&frame_->definition->record()) throw std::logic_error("Quest start lost its canonical definition binding");
    return admission_.add(frame_->row);
}
void QuestStartItemsActorWorld::prepare_start_log(const QuestStartRecord& q,const std::optional<QuestStartNpc>& npc) {
    auto context=ports_.start_log_context(*owners_.actor);
    // Own common strings before another root callback. The resulting packet is
    // a value snapshot before the client send, including recursive requests.
    const std::string account(context.account_name),character(context.character_name);context.account_name=account;context.character_name=character;
    std::optional<QuestStartLogNpc> target;if(npc) target=ports_.start_log_npc(*owners_.actor,*npc);
    frame_->log=quest_start_log_packet(context,q,target);
}
void QuestStartItemsActorWorld::send_self(const std::array<std::uint8_t,8>& p) {ports_.send_client(*owners_.actor,net::Packet(p.begin(),p.end()));}
void QuestStartItemsActorWorld::log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) {ports_.send_start_log(*owners_.actor,frame_->log);}
bool QuestStartItemsActorWorld::give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) {return grants_.grant(type,id,count);}
}
