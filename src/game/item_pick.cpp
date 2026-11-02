#include "eternity/game/item_pick.hpp"
#include "eternity/game/actor.hpp"
#include <cmath>

namespace eternity::game {
bool ground_item_pick_enabled(const GroundItem& item,std::uint32_t user_id,std::uint32_t party_id,bool checks,ItemPickClock& clock) {
    // CItem::PickEnable 004ba240. A permitted owner/group does not sample time.
    if(item.user_id==0 || item.user_id==user_id || (checks && item.party_id!=0 && item.party_id==party_id)) return true;
    const auto tick=clock.pick_tick_ms();return item.expires_at-30000U<tick;
}
GroundItem* ZoneItems::pick(Actor& user,std::uint32_t id,std::int32_t& error,bool checks,const ItemQuestLinks& links,ItemPickWorld& world) {
    // CZone::GetPickItem 004174a0: missing -> party lookup -> ownership ->
    // optional horizontal distance/quest tests -> ordinary leave without release.
    auto* item=find(id);if(!item) {error=3;return nullptr;}
    const auto party=world.pick_party_id(user);
    if(!ground_item_pick_enabled(*item,user.id(),party,checks,world)) {error=0;return nullptr;}
    if(checks) {
        const auto position=world.pick_position(user);const float x=item->position.x-position.x,z=item->position.z-position.z;
        const double x_squared=static_cast<double>(x)*x,z_squared=static_cast<double>(z)*z;
        const auto distance=static_cast<float>(std::sqrt(z_squared+x_squared));
        if(distance>10.0F) {error=4;return nullptr;}
        if(item->item.type!=26 && item->definition && item->definition->real_type==27 && !item->definition->quest_start) {
            const auto* link=links.find(item->definition->type,item->definition->type_id);bool active=false;
            if(link) for(const auto quest:link->end) if(world.pick_quest_active(user,quest)) {active=true;break;}
            if(!active) {error=2;return nullptr;}
        }
    }
    leave(*item);return item; // Success preserves the caller's existing error word.
}
}
