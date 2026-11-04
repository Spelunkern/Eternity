#include "eternity/game/mob_drops_creation.hpp"
#include <algorithm>

namespace eternity::game {
void create_mob_drop_craft(GroundItem& item,RandomSource& random) {
    const auto* definition=item.definition;
    if(!definition || definition->server==0 || definition->required_wisdom==0 || definition->required_wisdom>=100) return;
    const auto real=definition->real_type;
    const bool six=(real>=1 && real<=15) || real==22 || real==23 || real==25;
    const unsigned choices=six?6U:9U,server=definition->server;
    std::array<bool,9> selected{};
    if(server==1) selected[static_cast<unsigned>(random.next())%choices]=true;
    else if(server>=2 && server<=choices-2) {
        std::array<unsigned,9> indices{0,1,2,3,4,5,6,7,8};auto remaining=choices;
        for(unsigned i=0;i<server;++i) {
            const auto at=static_cast<unsigned>(random.next())%remaining;selected[indices[at]]=true;
            indices[at]=indices[remaining-1];--remaining;
        }
    } else if(server==choices-1) {
        std::fill_n(selected.begin(),choices,true);selected[static_cast<unsigned>(random.next())%choices]=false;
    } else if(six || server==9) std::fill_n(selected.begin(),choices,true);
    std::string name(20,'0');
    for(std::size_t i=0;i<selected.size();++i) if(selected[i]) {
        const auto bound=static_cast<std::int32_t>(definition->required_wisdom)+1;
        auto value=random.next()%bound;value=std::min(value,random.next()%bound);value=std::min(value,random.next()%bound);
        item.craft_additions[i]=static_cast<std::uint16_t>(value);
        name[i*2]=static_cast<char>('0'+value/10);name[i*2+1]=static_cast<char>('0'+value%10);
    }
    set_ground_item_craft(item,name);
}
namespace {
void position(GroundItem& item,const MobDropCreationContext& context,RandomSource& random) {
    // Original position coordinates are read AFTER both CRT draws.
    const auto offset=mob_drop_position(0,0,random);item.position={context.x+offset.x,0,context.z+offset.z};
}
}
GroundItem* create_mob_drop_item(const MobDropCreationContext& context,const ItemDefinition& definition,
    std::uint32_t owner,std::uint32_t party,GroundItemPool& pool,RandomSource& random,MobDropCreationWorld& world) {
    auto* item=pool.allocate();if(!item) return nullptr;
    item->definition=&definition;const auto time=world.make_time();
    initialize_ground_item(*item,definition.type,definition.type_id,1,time,'D');item->item.id=definition.id;
    const auto raw=world.raw_unique_id();item->item.uid=raw+context.server_id;item->user_id=owner;
    position(*item,context,random);item->drop_type=1;item->drop_id=context.mob_id;item->party_id=party;
    create_mob_drop_craft(*item,random);return item;
}
GroundItem* create_mob_drop_money(const MobDropCreationContext& context,std::uint32_t owner,
    std::uint32_t amount,GroundItemPool& pool,RandomSource& random) {
    auto* item=pool.allocate();if(!item) return nullptr;
    item->definition=nullptr;item->item.type=26;item->item.type_id=0;item->item.count=1;item->item.quality=0;item->item.make_type="D";item->user_id=owner;
    position(*item,context,random);item->drop_type=1;item->drop_id=context.mob_id;item->party_id=0;item->money=amount;
    // Original CreateCraftExpansion returns immediately for this null definition.
    create_mob_drop_craft(*item,random);return item;
}
}
