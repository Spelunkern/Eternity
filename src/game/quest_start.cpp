#include "eternity/game/quest_start.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
namespace {
std::uint16_t word(const QuestStartRecord& q,std::size_t p) {return static_cast<std::uint16_t>(q[p]|(unsigned(q[p+1])<<8));}
bool prerequisites(const QuestStartUser& u,const QuestStartRecord& q) {
    const std::array ids{word(q,0x118),word(q,0x110),word(q,0x112)};
    if(std::all_of(ids.begin(),ids.end(),[](auto id){return id==0;})) return true;
    return std::any_of(u.finished.begin(),u.finished.end(),[&](auto f){return f.success!=0 && std::any_of(ids.begin(),ids.end(),[&](auto id){return id!=0 && id==f.id;});});
}
}
// CUser::QuestEnableStart 0047f080: sex/party fields are not tested here.
bool quest_enable_start(const QuestStartUser& u,const QuestStartRecord& q) {
    if(!q[0x1cc] || u.level<word(q,0x102) || u.level>word(q,0x104)) return false;
    switch(q[0x106]) {
    case 0:if(u.country!=0) return false;break;
    case 1:if(u.country!=1) return false;break;
    case 2:if(u.country!=0 && u.country!=1) return false;break;
    case 3:if(u.country!=3) return false;break;
    case 4:if(u.country!=2) return false;break;
    case 5:if(u.country!=3 && u.country!=2) return false;break;
    default:break;
    }
    // Original directly indexes memory; reject invalid projections outside the six jobs.
    if(u.job>=6) throw std::invalid_argument("Quest start requires a valid job (0..5)");
    if(!q[0x10a+u.job]) return false;
    if((u.grow==0 && q[0x107]!=0) || (u.grow==1 && q[0x107]>1) || (u.grow==2 && q[0x107]==3)) return false;
    return prerequisites(u,q);
}
// CUser::QuestStart 0047ea70 and QuestGetStartItem 0047eb70.
void quest_start(const QuestStartRecord& q,const std::optional<QuestStartNpc>& npc,QuestStartWorld& w) {
    std::array<std::uint8_t,8> packet{2,9,0,0,0,0,q[0],q[1]};
    const auto handle=npc?npc->handle:0;
    for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(handle>>(8*i));
    w.prepare_start_log(q,npc);
    w.send_self(packet);w.log_start(q,npc);
    for(std::size_t p=0x13e;p<=0x144;p+=3) if(q[p]) (void)w.give_item(q[p],q[p+1],q[p+2]);
}
// CUser::QuestStartRequest 0047f220; checks 0047f390/400/490/500/570.
bool quest_start_request(const QuestStartUser& u,std::span<const QuestStartRecord> definitions,std::uint16_t request,std::uint32_t target,QuestStartWorld& w) {
    if(request==0 || request>definitions.size()) return false;
    const auto& q=definitions[request-1];const auto type=q[0x138];const auto id=word(q,0);
    if(type>4) return false;
    std::optional<QuestStartNpc> npc;
    if(type<=2) {
        auto found=w.find_npc(target);if(!found) return false;
        if((type!=0 || q[0x139]!=0) && (found->type!=q[0x139] || found->id!=word(q,0x13a))) return false;
        if(type==0) {if(!prerequisites(u,q)) return false;} else npc=found;
    } else if(target!=q[0x13d]) return false;
    if(w.active(id) || w.finished(id) || !quest_enable_start(u,q)) return false;
    if((type==2 || type==3) && !w.find_item(q[0x13c],q[0x13d],type==2)) return false;
    if(type==4 && (u.last_item!=q[0x13d] || w.tick_ms()>u.last_item_deadline)) return false;
    if(!w.add(q)) return false;
    quest_start(q,npc,w);return true;
}
}
