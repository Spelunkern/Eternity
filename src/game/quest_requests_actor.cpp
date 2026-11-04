#include "eternity/game/quest_requests_actor.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
QuestRequestInventory QuestRequestsActorInventory::refresh() {
    if(callback_) throw std::logic_error("Reentrant quest inventory consumption is unsupported");
    const auto bags=actor_.status().skills.max_bags;
    if(bags>9) throw std::out_of_range("Quest inventory exceeds original Actor bags");
    projection_.assign(bags>0?static_cast<std::size_t>(bags):0,{});borrowed_.assign(projection_.size(),{});
    for(std::size_t b=0;b<projection_.size();++b) for(std::uint8_t s=0;s<24;++s) {
        const auto bag=static_cast<std::uint8_t>(b+1);const auto held=actor_.retained_inventory_item(bag,s);
        if(!held) {if(!actor_.inventory_slot_empty(bag,s)) throw std::logic_error("Quest inventory requires retained Actor items");continue;}
        const auto& item=*held->item;if(!item.definition) throw std::logic_error("Quest inventory lacks item definition");
        projection_[b][s]={true,item.item.type,item.item.type_id,item.item.count,item.definition->real_type};
        borrowed_[b][s]={held->item,held->pool,item.item.uid,item.definition};
    }
    return {actor_.account_id(),projection_};
}
GroundItem& QuestRequestsActorInventory::validate(std::uint8_t bag,std::uint8_t slot) {
    if(!bag || bag>borrowed_.size() || slot>=24) throw std::out_of_range("Quest consumption slot outside projection");
    const auto& expected=borrowed_[bag-1][slot];const auto held=actor_.retained_inventory_item(bag,slot);
    if(!expected.item || !held || held->item!=expected.item || held->pool!=expected.pool || held->item->item.uid!=expected.uid || held->item->definition!=expected.definition) throw std::logic_error("Quest inventory allocation changed during callback");
    return *held->item;
}
void QuestRequestsActorInventory::validate_snapshot() {
    const auto bags=actor_.status().skills.max_bags;
    if((bags>0?static_cast<std::size_t>(bags):0)!=projection_.size()) throw std::logic_error("Quest unlocked bags changed during callback");
    for(std::size_t b=0;b<projection_.size();++b) for(std::uint8_t slot=0;slot<24;++slot) {
        const auto bag=static_cast<std::uint8_t>(b+1);const auto& projected=projection_[b][slot];
        if(!projected.present) {
            if(!actor_.inventory_slot_empty(bag,slot)) throw std::logic_error("Quest inventory slot appeared during callback");
        } else {
            const auto& item=validate(bag,slot).item;
            if(item.count!=projected.count || item.type!=projected.type || item.type_id!=projected.id) throw std::logic_error("Quest inventory payload changed during callback");
        }
    }
}
bool QuestRequestsActorInventory::find_item(int type,int id,bool consume) {return quest_find_item(refresh(),type,id,consume,*this);}
bool QuestRequestsActorInventory::find_items(RestoredQuest& q,bool consume) {
    if(!q.definition) throw std::invalid_argument("Quest inventory query lacks definition");
    return quest_find_items(refresh(),q.definition->record(),q.packet.counters,consume,*this);
}
void QuestRequestsActorInventory::send_database(const net::Packet& p) {
    if(p.size()!=9 || p[0]!=2 || p[1]!=7) throw std::invalid_argument("Quest inventory expected 0702 packet");
    const auto bag=p[6],slot=p[7];auto& item=validate(bag,slot);const auto count=projection_[bag-1][slot].count;
    if(!actor_.set_inventory_item_count(bag,slot,count,&item)) throw std::logic_error("Quest Actor count synchronization failed");
    callback_=true;struct Reset {bool& value;~Reset(){value=false;}} reset{callback_};
    transport_.send_database(actor_,p);
    validate_snapshot();
}
void QuestRequestsActorInventory::send_self(const net::Packet& p) {
    if(p.size()!=7 || p[0]!=6 || p[1]!=2) throw std::invalid_argument("Quest inventory expected 0206 packet");
    const auto bag=p[2],slot=p[3];(void)validate(bag,slot);
    callback_=true;struct Reset {bool& value;~Reset(){value=false;}} reset{callback_};
    actor_.send_self(p);
    validate_snapshot();
}
void QuestRequestsActorInventory::release_item(std::uint8_t bag,std::uint8_t slot) {
    auto& item=validate(bag,slot);
    if(!actor_.release_inventory_item(bag,slot,&item)) throw std::logic_error("Quest direct Actor release failed");
    borrowed_[bag-1][slot]={};
}
const std::array<std::uint8_t,464>* QuestRequestsActorWorld::definition(std::uint16_t id) {const auto* q=definitions_.find(id);return q?&q->record():nullptr;}
RestoredQuest* QuestRequestsActorWorld::find_quest(std::uint16_t id) {
    lock_active_quests();struct Unlock {QuestRequestsActorWorld& world;~Unlock(){world.unlock_active_quests();}} unlock{*this};
    return runtime_.find_active(id);
}
void QuestRequestsActorWorld::send_database(const net::Packet& p) {
    if(p.size()>=2 && p[0]==2 && p[1]==7) items_.send_database(p);else ports_.send_database(actor_,p);
}
void QuestRequestsActorWorld::send_self(const net::Packet& p) {
    if(p.size()>=2 && p[0]==6 && p[1]==2) items_.send_self(p);else actor_.send_self(p);
}
void QuestRequestsActorWorld::remove_quest(RestoredQuest& q,bool success) {runtime_.remove(q,success,*this);}
void QuestRequestsActorWorld::award(RestoredQuest& q,std::uint8_t variant,std::uint32_t context) {award_quest_reward(q,variant,context,actor_.account_id(),actor_.money(),ports_.reward_world(actor_));}
}
