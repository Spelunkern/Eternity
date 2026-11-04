#include "eternity/game/party_runtime_membership.hpp"
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t n,unsigned bytes=4) {
    for(unsigned i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(n>>(i*8)));
}
void valid(const PartyNormalMembership& p) {
    if(p.experience.kind!=0 || p.count<0 || p.count>7) throw std::invalid_argument("Invalid normal party projection");
    for(std::int32_t i=0;i<p.count;++i) if(!p.slots[static_cast<std::size_t>(i)]) throw std::invalid_argument("Absent normal party member");
}
void refresh(PartyNormalMembership& p) {
    std::array<std::optional<std::uint16_t>,7> levels{};
    for(std::int32_t i=0;i<p.count;++i) levels[static_cast<std::size_t>(i)]=p.slots[static_cast<std::size_t>(i)]->level;
    p.experience.check_experience_level(std::span(levels).first(static_cast<std::size_t>(p.count)));
}
void member(net::Packet& packet,PartyMembershipUser& user,std::uint32_t now,PartyMembershipWorld& world) {
    if(user.name.size()>20 || user.name.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid party member name");
    append(packet,user.id);
    // Original strcpy leaves trailing name bytes unspecified. Initialize them
    // deterministically rather than publishing native stack contents.
    const auto start=packet.size(); packet.resize(start+21,0);
    for(std::size_t i=0;i<user.name.size();++i) packet[start+i]=static_cast<std::uint8_t>(user.name[i]);
    append(packet,user.level,2); append(packet,user.job,1);
    for(auto value:{user.hp,user.maximum_hp,user.mp,user.maximum_mp,user.sp,user.maximum_sp,user.instance}) append(packet,value,2);
    for(auto value:user.position) append(packet,std::bit_cast<std::uint32_t>(value));
    const auto count_at=packet.size(); append(packet,0,1);
    world.lock_effects(user);
    user.effect_cursor=0;
    unsigned included=0;
    while(user.effect_cursor<user.effects.size()) {
        const auto& effect=user.effects[user.effect_cursor];
        if(effect.visibility!=1) {
            append(packet,effect.skill,2); append(packet,effect.level,1);
            const auto delta=std::bit_cast<std::int32_t>(effect.deadline-now)/1000;
            append(packet,delta<0?0U:static_cast<std::uint32_t>(delta));
            if(++included==40) break;
        }
        ++user.effect_cursor;
    }
    world.unlock_effects(user);
    packet[count_at]=static_cast<std::uint8_t>(included);
}
void scene(PartyMembershipUser& user,PartyMembershipWorld& world) {
    net::Packet packet; append(packet,0x520,2); append(packet,user.id);
    append(packet,world.party_type(user),1);
    if(user.in_zone) world.send_scene(user,packet);
}
void log(PartyNormalMembership& p,PartyMembershipUser& user,std::uint16_t opcode,PartyMembershipWorld& world) {
    net::Packet packet(78,0); world.fill_log(user,std::span(packet).first(74));
    packet[0]=static_cast<std::uint8_t>(opcode);packet[1]=static_cast<std::uint8_t>(opcode>>8);
    for(unsigned i=0;i<4;++i) packet[74+i]=static_cast<std::uint8_t>(p.id>>(i*8));
    world.send_log(packet);
}
}
bool enter_normal_party(PartyNormalMembership& p,PartyMembershipUser& user,PartyMembershipWorld& world) {
    if(p.count>=7) return false; // Original guard precedes all member reads.
    valid(p);
    const auto now=world.tick(); world.lock(p);
    p.slots[static_cast<std::size_t>(p.count)]=&user; user.party=&p;
    if(p.count>0) {net::Packet packet;append(packet,0xb04,2);member(packet,user,now,world);world.send_party(p,packet);}
    net::Packet roster;append(roster,0xb01,2);append(roster,static_cast<std::uint32_t>(p.leader),1);append(roster,static_cast<std::uint32_t>(p.count),1);
    for(std::int32_t i=0;i<p.count;++i) member(roster,*p.slots[static_cast<std::size_t>(i)],now,world);
    world.send_user(user,roster);
    ++p.count;user.pending_request=0;refresh(p);world.unlock(p);
    scene(user,world);log(p,user,0xf01,world);return true;
}
bool leave_normal_party(PartyNormalMembership& p,PartyMembershipUser& user,PartyMembershipWorld& world) {
    valid(p);world.lock(p);
    std::int32_t index=0;
    while(index<p.count && p.slots[static_cast<std::size_t>(index)]!=&user) ++index;
    if(index==p.count) {world.unlock(p);return false;}
    user.party=nullptr;
    net::Packet packet;append(packet,0xb05,2);append(packet,user.id);world.send_party(p,packet);
    for(auto i=index;i+1<p.count;++i) p.slots[static_cast<std::size_t>(i)]=p.slots[static_cast<std::size_t>(i+1)];
    if(index+1<p.item_cursor) --p.item_cursor;
    --p.count;bool leader_changed=false;
    if(p.count==0) world.unregister_party(p);
    else if(p.count==1) leave_normal_party(p,*p.slots[0],world);
    else if(index==p.leader) {p.leader=0;leader_changed=true;}
    else if(index<p.leader) --p.leader;
    if(user.guild_creation) world.cleanup_guild_creation(user);
    refresh(p);
    PartyMembershipUser* leader=nullptr;
    if(leader_changed && p.slots[static_cast<std::size_t>(p.leader)]) {
        packet.clear();append(packet,0xb07,2);append(packet,p.slots[static_cast<std::size_t>(p.leader)]->id);world.send_party(p,packet);
        leader=p.slots[static_cast<std::size_t>(p.leader)];
    }
    world.unlock(p);scene(user,world);if(leader) scene(*leader,world);log(p,user,0xf02,world);return true;
}
}
