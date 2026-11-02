#include "eternity/game/guild_membership.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint32_t value,unsigned int count=4) {for(unsigned int i=0;i<count;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet presence(std::uint8_t mode,std::uint32_t id) {net::Packet p{0x0c,0x0d,mode};append(p,id);return p;}
GuildMembership::Record take(GuildMembership::Members& rows,std::uint32_t id) {auto node=rows.extract(id);return node.empty()?nullptr:std::move(node.mapped());}
void name(net::Packet& p,const std::string& raw) {const auto n=raw.find('\0')==std::string::npos?raw.size():raw.find('\0');if(n>20) throw std::invalid_argument("Guild member name exceeds original wire field");p.insert(p.end(),raw.begin(),raw.begin()+static_cast<std::ptrdiff_t>(n));p.insert(p.end(),21-n,0);}
}
GuildMembership::GuildMembership(std::uint32_t id,GuildMemberPorts port):id_(id),ports_(std::move(port)) {
    if(!ports_.allocate || !ports_.release || !ports_.send) throw std::invalid_argument("Incomplete guild membership transport");
}
void GuildMembership::broadcast(net::Packet p) const {
    if(!ports_.allocate(p)) return;
    for(const auto& [key,record]:online) {static_cast<void>(key);if(!record) break;if(record->user) ports_.send(*record->user,p);}
    ports_.release();
}
bool GuildMembership::connect(GuildUser& user) {
    const auto application=applications.find(user.character_id);
    if(application!=applications.end() && application->second) {user.request_id=id_;return true;}
    auto record=take(offline,user.character_id);if(!record) return false;
    user.guild_id=id_;user.rank=record->rank;record->user=&user;user.guild=this;user.ranking_entry=record->ranking_entry;
    if(record->rank<3) for(auto& leader:leaders) if(!leader) {leader=record;break;}
    broadcast(presence(0x68,user.character_id));online.emplace(record->id,record);return true;
}
bool GuildMembership::disconnect(GuildUser& user) {
    auto record=take(online,user.character_id);if(!record) return false;
    if(user.rank<3) for(auto& leader:leaders) if(leader==record) {leader.reset();break;}
    record->ranking_entry=user.ranking_entry;record->user=nullptr;offline.emplace(record->id,record);broadcast(presence(0x69,user.character_id));return true;
}
void GuildMembership::send_users(GuildUser& user) const {
    for(unsigned int phase=0;phase<3;++phase) {
        const auto op=phase==0?0x12:phase==1?0x13:0x19;net::Packet p{static_cast<std::uint8_t>(op),0x0d,0};
        const auto flush=[&]{if(p[2]) ports_.send(user,std::move(p));p={static_cast<std::uint8_t>(op),0x0d,0};};
        for(const auto& [key,record]:(phase==0?online:offline)) {
            static_cast<void>(key);if(!record) break;if(phase!=0 && (record->billing_id!=0)!=(phase==2)) continue;
            append(p,record->id);p.push_back(static_cast<std::uint8_t>(record->rank));append(p,static_cast<std::uint32_t>(record->level),2);p.push_back(static_cast<std::uint8_t>(record->job));name(p,record->name);if(++p[2]==40) flush();
        }
        flush();
    }
}
void GuildMembership::send_applications(GuildUser& user) const {
    net::Packet p{0x16,0x0d,0};const auto flush=[&]{if(p[2]) ports_.send(user,std::move(p));p={0x16,0x0d,0};};
    for(const auto& [key,record]:applications) {static_cast<void>(key);if(!record) break;append(p,record->id);append(p,static_cast<std::uint32_t>(record->level),2);p.push_back(static_cast<std::uint8_t>(record->job));name(p,record->name);if(++p[2]==40) flush();}flush();
}
void GuildDirectory::add(std::uint32_t id,GuildMembership* guild) {by_character_.emplace(id,guild);}
bool GuildDirectory::connect(GuildUser& user) {const auto row=by_character_.find(user.character_id);return row!=by_character_.end() && row->second && row->second->connect(user);}
void GuildDirectory::initialize(GuildUser& user) {user.request_id=0;user.guild_id=0;user.rank=0;user.guild=nullptr;if(connect(user)) send(user);}
void GuildDirectory::release(GuildUser& user) {if(user.guild) user.guild->disconnect(user);}
void GuildDirectory::send(GuildUser& user) {if(user.guild) {user.guild->send_users(user);if(user.rank<3) user.guild->send_applications(user);}}
}
