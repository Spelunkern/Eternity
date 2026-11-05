#include "eternity/game/quest_start_items_actor.hpp"
#include <stdexcept>
namespace eternity::game {
QuestStartItemsActor::QuestStartItemsActor(QuestStartItemsActorOwners owners,QuestStartItemsActorPorts& ports)
    :owners_(std::move(owners)),ports_(ports) {
    if(!owners_.definitions || !owners_.pool || !owners_.ids || !owners_.actor) throw std::invalid_argument("Quest item adapter requires retained actual owners");
}
bool QuestStartItemsActor::grant(std::int32_t type,std::int32_t id,std::int32_t count) {
    Frame current;auto* previous=frame_;frame_=&current;
    try {
        QuestStartItemsUser user{owners_.actor->account_id(),owners_.actor->status().skills.max_bags};
        const auto result=quest_start_items_grant(type,id,count,user,*this);frame_=previous;return result;
    } catch(...) {
        frame_=previous;if(current.unclaimed) owners_.pool->release(current.unclaimed);throw;
    }
}
bool QuestStartItemsActor::add(std::int32_t type,std::int32_t id,std::int32_t count,QuestStartItemsResult& output) {
    Frame current;auto* previous=frame_;frame_=&current;
    try {
        QuestStartItemsUser user{owners_.actor->account_id(),owners_.actor->status().skills.max_bags};
        const auto result=quest_start_items_add(type,id,count,output,user,*this);frame_=previous;return result;
    } catch(...) {
        frame_=previous;if(current.unclaimed) owners_.pool->release(current.unclaimed);throw;
    }
}
const ItemDefinition* QuestStartItemsActor::find_definition(std::int32_t type,std::int32_t id) {
    if(type<1 || type>255 || id<1 || id>255) return nullptr;
    return owners_.definitions->find(static_cast<std::uint8_t>(type),static_cast<std::uint8_t>(id));
}
bool QuestStartItemsActor::slot_empty(std::int32_t bag,std::int32_t slot) {
    if(bag<1 || bag>9 || slot<0 || slot>=24) throw std::out_of_range("Quest inventory projection exceeds Actor slots");
    return owners_.actor->inventory_slot_empty(static_cast<std::uint8_t>(bag),static_cast<std::uint16_t>(slot));
}
GroundItem* QuestStartItemsActor::create_item() {frame_->unclaimed=owners_.pool->allocate();return frame_->unclaimed;}
std::uint32_t QuestStartItemsActor::local_make_time() {return ports_.local_make_time(*owners_.actor);}
std::uint64_t QuestStartItemsActor::next_uid() {return owners_.ids->next();}
void QuestStartItemsActor::store(std::int32_t bag,std::int32_t slot,GroundItem& item) {
    if(frame_->unclaimed!=&item || !owners_.actor->claim_inventory_item(static_cast<std::uint8_t>(bag),static_cast<std::uint16_t>(slot),item,*owners_.pool)) throw std::logic_error("Quest item raw ownership claim failed");
    frame_->unclaimed=nullptr;frame_->published=&item;
}
void QuestStartItemsActor::send_database(const net::Packet& packet) {
    auto* item=frame_->published;
    if(!item || !owners_.actor->sync_inventory_item(item->item.bag,item->item.slot,item)) throw std::logic_error("Quest item retained mirror synchronization failed");
    // No borrowed item is accessed after transport: it may recurse or release it.
    frame_->published=nullptr;ports_.send_database(*owners_.actor,packet);
}
void QuestStartItemsActor::send_self(const net::Packet& packet) {ports_.send_client(*owners_.actor,packet);}
QuestStartItemsActorAdd::QuestStartItemsActorAdd(QuestStartItemsActorAddOwners owners,QuestStartItemsActorPorts& ports)
    :owners_(std::move(owners)),ports_(ports) {
    if(!owners_.definitions || !owners_.pool || !owners_.actor || !owners_.runtime) throw std::invalid_argument("Quest add adapter requires retained actual owners");
}
bool QuestStartItemsActorAdd::add(std::uint16_t row) {
    const auto* definition=owners_.definitions->find(row);if(!definition) return false;
    QuestStartAddUser user{owners_.actor->account_id(),owners_.runtime->mob_kill_quests(),owners_.runtime->player_kill_quests()};
    Frame current;auto* previous=frame_;frame_=&current;
    try {const auto result=quest_start_add(*definition,user,*this);frame_=previous;return result;}
    catch(...) {frame_=previous;if(current.unclaimed) owners_.pool->release(*current.unclaimed);throw;}
}
RestoredQuest* QuestStartItemsActorAdd::allocate() {frame_->unclaimed=owners_.pool->allocate();return frame_->unclaimed;}
std::uint32_t QuestStartItemsActorAdd::tick_ms() {return ports_.tick_ms(*owners_.actor);}
void QuestStartItemsActorAdd::insert_active(RestoredQuest& quest) {owners_.runtime->add_active_unaccounted(quest);frame_->unclaimed=nullptr;}
void QuestStartItemsActorAdd::send_database(const std::array<std::uint8_t,10>& packet) {ports_.send_database(*owners_.actor,net::Packet(packet.begin(),packet.end()));}
}
