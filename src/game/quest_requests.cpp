#include "eternity/game/quest_requests.hpp"
#include <algorithm>

namespace eternity::game {
namespace {
void remove(QuestRequestInventory inventory,std::size_t b,std::size_t s,std::uint8_t amount,QuestRequestItems& world,bool retain_released_count=false) {
    auto& item=inventory.bags[b][s];
    const auto remaining=static_cast<std::uint8_t>(item.count-amount);
    if(remaining || !retain_released_count) item.count=remaining;
    const auto bag=static_cast<std::uint8_t>(b+1),slot=static_cast<std::uint8_t>(s);
    net::Packet database{2,7};
    for(unsigned shift=0;shift<32;shift+=8) database.push_back(static_cast<std::uint8_t>(inventory.account_id>>shift));
    database.insert(database.end(),{bag,slot,amount});
    world.send_database(database);
    world.send_self(net::Packet{6,2,bag,slot,remaining?item.type:std::uint8_t{0},remaining?item.id:std::uint8_t{0},remaining});
    if(!remaining) {world.release_item(bag,slot);item={};}
}
bool matches(const QuestRequestItem& item,const std::array<std::uint8_t,464>& d,std::size_t i) {
    return item.type==d[0x14c+3*i] && item.id==d[0x14d+3*i];
}
}
// ps_game.exe CUser::QuestFindItem 0047f790.
bool quest_find_item(QuestRequestInventory inventory,int type,int id,bool consume,QuestRequestItems& world) {
    for(std::size_t b=0;b<inventory.bags.size();++b) for(std::size_t s=0;s<24;++s) {
        auto& item=inventory.bags[b][s];
        if(item.present && item.type==type && item.id==id) {
            if(consume) remove(inventory,b,s,1,world);
            return true;
        }
    }
    return false;
}
// ps_game.exe CUser::QuestFindItem 0047f8e0 (including original counter overwrite rules).
bool quest_find_items(QuestRequestInventory inventory,const std::array<std::uint8_t,464>& d,std::array<std::uint8_t,3>& counters,bool consume,QuestRequestItems& world) {
    std::array<unsigned,3> totals{},remaining{};
    for(const auto& bag:inventory.bags) for(const auto& item:bag) if(item.present) {
        for(std::size_t i=0;i<3;++i) if(matches(item,d,i)) {totals[i]+=item.count;break;}
    }
    for(std::size_t i=0;i<3;++i) if(totals[i]<d[0x14e + 3*i]) return false;
    for(std::size_t i=0;i<3;++i) {
        counters[i]=totals[i]>counters[i]?d[0x14e + 3*i]:static_cast<std::uint8_t>(totals[i]);
        remaining[i]=d[0x14e + 3*i];
    }
    if(!consume) return true;
    if(d[0x15c]!=1 && d[0x15d]==0) for(int i=2;i>=0;--i) {
        const auto k=static_cast<std::size_t>(i),offset=0x163+32*k;
        const auto selector=d[offset],required=d[offset+1];
        if(d[0x147]==1 && required && selector>=1 && selector<=3) {
            counters[k]=static_cast<std::uint8_t>(totals[k]);
            if(counters[selector-1]>=required) {
                const auto destination=d[offset];
                // Original writes the three consumption locals for selectors 1..3.
                remaining[destination-1]=required;
                break;
            }
        }
    }
    for(std::size_t b=0;b<inventory.bags.size();++b) for(std::size_t s=0;s<24;++s) {
        auto& item=inventory.bags[b][s];
        if(!item.present) continue;
        if(item.effect==27) {
            bool match=false;
            for(std::size_t i=0;i<3;++i) match=match||matches(item,d,i);
            if(match) { const auto count=item.count;remove(inventory,b,s,count,world,true); }
            continue;
        }
        for(std::size_t i=0;i<3 && item.present;++i) if(remaining[i] && matches(item,d,i)) {
            const auto amount=static_cast<std::uint8_t>(std::min<unsigned>(remaining[i],item.count));
            remaining[i]-=amount;
            remove(inventory,b,s,amount,world,true);
        }
    }
    return true;
}
void quest_end_request(std::uint16_t id,std::uint32_t npc,std::int32_t count,QuestRequestWorld& world) {
    if(!id || id>count) return;
    const auto* dispatch=world.definition(id);
    if(!dispatch) return;
    auto* quest=world.find_quest(id);
    if(!quest || !quest->definition) return;
    const auto& d=quest->definition->record();
    const auto type=(*dispatch)[0x147];
    if(type>3 || (type==0 && !(*dispatch)[0x148])) return;
    if(type==0 || npc) {
        const auto found=world.find_npc(npc);
        const auto end_id=static_cast<std::uint16_t>(d[0x14a]|(d[0x14b]<<8));
        if(!found || found->type!=d[0x148] || found->id!=end_id) return;
    }
    if(type==1) {
        if(!quest_find_items(world.inventory(),d,quest->packet.counters,d[0x15d]==0 || d[0x148]==0,world)) return;
    } else if(type==2) {
        if(quest->packet.counters[0]<d[0x15a] || quest->packet.counters[1]<d[0x15b]) return;
    } else if(type==3 && quest->packet.counters[0]<d[0x155]) return;
    world.complete_quest(*quest,type>=2?0:npc,true);
}
}
