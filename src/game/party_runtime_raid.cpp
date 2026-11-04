#include "eternity/game/party_runtime_raid.hpp"
#include <bit>
#include <stdexcept>
namespace eternity::game {
PartyMembershipUser*& raid_membership_slot(PartyRegistryParty& p,std::int32_t i) {
    if(i<0 || i>=30)throw std::invalid_argument("Invalid raid array position");
    return i<7?p.slots[static_cast<std::size_t>(i)]:p.extra_slots[static_cast<std::size_t>(i-7)];
}
std::uint32_t& raid_membership_index(PartyRegistryParty& p,std::int32_t i) {
    if(i<0 || i>=30)throw std::invalid_argument("Invalid raid array position");
    return i<7?p.slot_indices[static_cast<std::size_t>(i)]:p.extra_indices[static_cast<std::size_t>(i-7)];
}
namespace {
void append(net::Packet& p,std::uint32_t n,unsigned bytes=4) {for(unsigned i=0;i<bytes;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void refresh(PartyRegistryParty& p) {
    if(p.count<=1)return;
    if(p.count>30)throw std::invalid_argument("Invalid raid count");
    std::array<std::optional<std::uint16_t>,30> levels{};
    for(std::int32_t i=0;i<p.count;++i)if(auto* u=raid_membership_slot(p,i))levels[static_cast<std::size_t>(i)]=u->level;
    p.experience.check_experience_level(std::span(levels).first(static_cast<std::size_t>(p.count)));
}
void member(net::Packet& p,PartyMembershipUser& u,std::uint32_t now,unsigned cap,PartyMembershipWorld& w) {
    if(u.name.size()>20 || u.name.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid raid name");
    append(p,u.id);auto start=p.size();p.resize(start+21,0);
    for(std::size_t i=0;i<u.name.size();++i)p[start+i]=static_cast<std::uint8_t>(u.name[i]);
    append(p,u.level,2);append(p,u.job,1);
    for(auto n:{u.hp,u.maximum_hp,u.mp,u.maximum_mp,u.sp,u.maximum_sp,u.instance})append(p,n,2);
    for(auto n:u.position)append(p,std::bit_cast<std::uint32_t>(n));
    auto at=p.size();append(p,0,1);unsigned count=0;
    w.lock_effects(u);u.effect_cursor=0;
    while(u.effect_cursor<u.effects.size()) {
        auto& e=u.effects[u.effect_cursor];
        if(e.visibility!=1) {
            append(p,e.skill,2);append(p,e.level,1);
            auto delta=std::bit_cast<std::int32_t>(e.deadline-now)/1000;
            append(p,delta<0?0U:static_cast<std::uint32_t>(delta));
            if(++count==cap)break;
        }
        ++u.effect_cursor;
    }
    w.unlock_effects(u);p[at]=static_cast<std::uint8_t>(count);
}
void notify(PartyRegistryParty& p,std::uint16_t op,PartyMembershipUser& u,PartyMembershipWorld& w) {
    net::Packet b;append(b,op,2);append(b,u.id);w.send_party(p,b);
}
}
std::int32_t empty_raid_membership_slot(PartyRegistryParty& p) {
    for(std::int32_t candidate=0;candidate<30;++candidate) {
        bool found=false;
        for(std::int32_t i=0;i<p.count;++i)if(raid_membership_index(p,i)==static_cast<std::uint32_t>(candidate))found=true;
        if(!found)return candidate;
    }
    return -1;
}
bool enter_raid_party(PartyRegistryParty& p,PartyMembershipUser& user,PartyMembershipWorld& w) {
    if(p.count>=30)return false;
    const auto now=w.tick();w.lock(p);
    auto empty=empty_raid_membership_slot(p);
    if(empty==-1){w.unlock(p);return false;}
    raid_membership_slot(p,p.count)=&user;raid_membership_index(p,p.count)=static_cast<std::uint32_t>(empty);user.party=&p;
    if(p.count>0) {
        net::Packet b;append(b,0xb0c,2);append(b,raid_membership_index(p,p.count),2);member(b,user,now,40,w);
        w.send_party(p,b);w.send_user(user,b);
    }
    // Header is captured once. Original continuation flag depends on total
    // count+1 modulo six, so for a nonmultiple it is 1 on EVERY full chunk.
    net::Packet header;append(header,0xb0b,2);append(header,0,1);
    append(header,raid_membership_index(p,p.leader),1);append(header,raid_membership_index(p,p.sub_leader),1);
    append(header,static_cast<std::uint32_t>(p.item_division),2);append(header,p.experience.kind,1);append(header,p.auto_join?1U:0U,1);append(header,0,1);
    net::Packet roster=header;std::int32_t included=0,in_chunk=0;
    for(std::int32_t i=0;i<30;++i) {
        auto* u=raid_membership_slot(p,i);if(!u)continue;
        append(roster,raid_membership_index(p,i),2);member(roster,*u,now,35,w);++included;++in_chunk;
        if(in_chunk==6) {
            roster[2]=(p.count+1)%6==0?0:1;roster[9]=6;w.send_user(user,roster);
            roster=header;in_chunk=0;
        }
        if(included==p.count+1) {
            if(in_chunk>0){roster[2]=0;roster[9]=static_cast<std::uint8_t>(in_chunk);w.send_user(user,roster);}
            break;
        }
    }
    ++p.count;user.pending_request=0;refresh(p);w.unlock(p);
    notify_party_membership_scene(user,w);log_party_membership(p,user,0xf01,w);return true;
}
bool leave_raid_party(PartyRegistryParty& p,PartyMembershipUser& user,PartyMembershipWorld& w) {
    w.lock(p);std::int32_t removed=0;
    while(removed<p.count && raid_membership_slot(p,removed)!=&user)++removed;
    if(removed>=p.count){w.unlock(p);return false;}
    bool leader_changed=removed==p.leader;
    bool sub_changed=!leader_changed && removed==p.sub_leader;
    user.party=nullptr;notify(p,0xb0d,user,w);
    for(auto from=removed+1;from<p.count;++from) {
        raid_membership_slot(p,from-1)=raid_membership_slot(p,from);
        raid_membership_index(p,from-1)=raid_membership_index(p,from);
        if(from==p.leader)--p.leader;else if(from==p.sub_leader)--p.sub_leader;
    }
    if(removed+1<p.item_cursor)--p.item_cursor;
    --p.count;
    if(p.count==0)w.unregister_party(p);
    else if(p.count==1) {
        if(p.experience.kind==1)leave_raid_party(p,*raid_membership_slot(p,0),w);
        else leave_normal_party(p,*raid_membership_slot(p,0),w);
    } else if(leader_changed || sub_changed) {
        if(leader_changed)p.leader=p.sub_leader;
        const auto excluded=leader_changed?p.sub_leader:p.leader;
        for(std::int32_t i=0;i<p.count;++i)if(raid_membership_slot(p,i) && i!=excluded){p.sub_leader=i;sub_changed=true;break;}
    }
    if(user.guild_creation)w.cleanup_guild_creation(user);
    refresh(p);
    PartyMembershipUser* leader=nullptr;
    if(leader_changed && raid_membership_slot(p,p.leader)) {
        notify(p,0xb17,*raid_membership_slot(p,p.leader),w);leader=raid_membership_slot(p,p.leader);
    }
    if(sub_changed && raid_membership_slot(p,p.sub_leader))notify(p,0xb13,*raid_membership_slot(p,p.sub_leader),w);
    w.unlock(p);notify_party_membership_scene(user,w);if(leader)notify_party_membership_scene(*leader,w);
    log_party_membership(p,user,0xf02,w);return true;
}
std::uint8_t raid_membership_type(PartyMembershipUser& user,PartyMembershipWorld& w) {
    auto* retained=user.party;
    if(retained){w.lock(*retained);if(!user.party){w.unlock(*retained);retained=nullptr;}}
    auto* current=user.party;
    if(current) {
        w.lock(*current);auto& p=static_cast<PartyRegistryParty&>(*current);
        const bool leader=p.count>0 && raid_membership_slot(p,p.leader)==&user;
        w.unlock(p);if(retained)w.unlock(*retained);return leader?2:1;
    }
    if(retained)w.unlock(*retained);
    return 0;
}
void send_raid_membership(PartyRegistryParty& p,std::span<const std::uint8_t> payload,PartyMembershipSendWorld& w) {
    auto* buffer=allocate_party_membership_send_buffer(payload,w);if(!buffer)return;
    w.lock(p);
    if(p.experience.kind<=1)for(std::int32_t i=0;i<p.count;++i) {
        auto* u=raid_membership_slot(p,i);
        if(u && u->send_socket!=-1){buffer->references.fetch_add(1,std::memory_order_seq_cst);w.enqueue_send(*u,*buffer);}
    }
    w.unlock(p);release_party_membership_buffer(*buffer,w);
}
void PartyRaidMembershipWorld::send_party(PartyNormalMembership& p,const net::Packet& b) {send_raid_membership(static_cast<PartyRegistryParty&>(p),b,*this);}
std::uint8_t PartyRaidMembershipWorld::party_type(PartyMembershipUser& u){return raid_membership_type(u,*this);}
bool PartyRaidRequestWorld::enter_raid(PartyRegistryParty& p,const PartyRequestUser& u){return enter_raid_party(p,u.member,membership_world());}
void PartyRaidRequestWorld::leave_raid(PartyRegistryParty& p,const PartyRequestUser& u){leave_raid_party(p,u.member,membership_world());}
}
