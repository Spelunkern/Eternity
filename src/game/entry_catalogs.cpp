#include "eternity/game/entry_catalogs.hpp"
#include <algorithm>
#include <bit>

namespace eternity::game {
namespace {
std::uint32_t get(std::span<const std::uint8_t> p,std::size_t at,unsigned int n=4) {std::uint32_t v=0;for(unsigned int i=0;i<n;++i) v|=std::uint32_t{p[at+i]}<<(8*i);return v;}
void append(net::Packet& p,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
std::string wire_string(std::span<const std::uint8_t> p) {const auto end=std::find(p.begin(),p.end(),0);if(end==p.end()) throw std::invalid_argument("Unterminated entry catalog string");return {p.begin(),end};}
std::string bounded(std::string value,std::size_t maximum) {value.resize(value.find('\0')==std::string::npos?value.size():value.find('\0'));if(value.size()>maximum) throw std::invalid_argument("Entry catalog string exceeds original wire storage");return value;}
void fixed(net::Packet& p,const std::string& value,std::size_t width) {const auto s=bounded(value,width-1);p.insert(p.end(),s.begin(),s.end());p.resize(p.size()+width-s.size(),0);}
void count_batch(std::vector<net::Packet>& out,net::Packet& p,std::uint16_t opcode) {if(p[2]) out.push_back(std::move(p));p={static_cast<std::uint8_t>(opcode),static_cast<std::uint8_t>(opcode>>8),0};}
}
void SocialCatalog::clear() {friends_.clear();blocked_.clear();}
void SocialCatalog::add_friend(FriendEntry value) {if(friends_.size()==100) return;value.name=bounded(std::move(value.name),20);value.memo=bounded(std::move(value.memo),50);friends_.push_back(std::move(value));}
void SocialCatalog::add_block(BlockEntry value) {if(blocked_.size()==100) return;value.name=bounded(std::move(value.name),20);value.memo=bounded(std::move(value.memo),50);blocked_.push_back(std::move(value));}
const FriendEntry* SocialCatalog::find(std::uint32_t id) const {const auto it=std::find_if(friends_.begin(),friends_.end(),[&](const auto& value){return value.id==id;});return it==friends_.end()?nullptr:&*it;}
EntryCatalogUpdate SocialCatalog::apply_db(std::span<const std::uint8_t> p) {
    if(p.size()<6 || get(p,2)!=account_) return EntryCatalogUpdate::rejected;
    const auto op=get(p,0,2);if(op!=0xc01 && op!=0xc05) return EntryCatalogUpdate::unsupported;
    const auto width=op==0xc01?79U:76U;if(p.size()<7 || p.size()!=7+width*p[6]) return EntryCatalogUpdate::rejected;
    try {for(std::size_t at=7;at<p.size();at+=width) {
        if(op==0xc01) {if(friends_.size()<100) add_friend({get(p,at),p[at+4],p[at+5],p[at+6],wire_string(p.subspan(at+7,21)),wire_string(p.subspan(at+28,51))});}
        else if(blocked_.size()<100) add_block({get(p,at),wire_string(p.subspan(at+4,21)),wire_string(p.subspan(at+25,51))});
    }}catch(const std::invalid_argument&){return EntryCatalogUpdate::rejected;}return EntryCatalogUpdate::applied;
}
void SocialCatalog::notify_online(const Lookup& lookup,const std::function<void(std::uint32_t,net::Packet)>& send) const {
    if(!lookup || !send) throw std::invalid_argument("Missing social world lookup/transport");
    for(const auto& value:friends_) if(value.id && lookup(value.id)) {net::Packet p{7,0x22};append(p,character_);p.push_back(1);send(value.id,std::move(p));}
}
std::vector<net::Packet> SocialCatalog::packets(const Lookup& lookup) const {
    std::vector<net::Packet> out;send_packets(lookup,[&](net::Packet p){out.push_back(std::move(p));});return out;
}
void SocialCatalog::send_packets(const Lookup& lookup,const std::function<void(net::Packet)>& send) const {
    if(!lookup || !send) throw std::invalid_argument("Missing social world lookup/transport");
    net::Packet p{1,0x22,0};const auto flush=[&](std::uint16_t op){if(p[2]) send(std::move(p));p={static_cast<std::uint8_t>(op),static_cast<std::uint8_t>(op>>8),0};};
    for(const auto& value:friends_) {const auto* peer=lookup(value.id);append(p,value.id);p.push_back(value.job);p.push_back(peer && peer->find(character_)?1:0);fixed(p,value.name,21);p.push_back(static_cast<std::uint8_t>(value.memo.size()));p.insert(p.end(),value.memo.begin(),value.memo.end());if(++p[2]==25) flush(0x2201);}
    flush(0x2208);
    for(const auto& value:blocked_) {append(p,value.id);fixed(p,value.name,21);p.push_back(static_cast<std::uint8_t>(value.memo.size()));p.insert(p.end(),value.memo.begin(),value.memo.end());if(++p[2]==25) flush(0x2208);}
    if(p[2]) send(std::move(p));
}
EntryCatalogUpdate EntryCatalogs::apply_db(std::span<const std::uint8_t> p) {
    if(p.size()<6 || get(p,2)!=account_) return EntryCatalogUpdate::rejected;
    const auto op=get(p,0,2);if(op!=0xd01 && op!=0xe01 && op!=0xe04) return EntryCatalogUpdate::unsupported;
    const auto header=op==0xe01?11U:7U,width=op==0xe01?29U:op==0xd01?15U:4U;
    if(p.size()<header || p.size()!=header+width*p[header-1] || (op==0xe01 && p[10]>10)) return EntryCatalogUpdate::rejected;
    if(op==0xe01) cash_=get(p,6);
    for(std::size_t at=header,index=0;at<p.size();at+=width,++index) {
        if(op==0xd01) {const auto slot=p[at];if(slot<4) points_[slot]={get(p,at+1,2),std::bit_cast<float>(get(p,at+3)),std::bit_cast<float>(get(p,at+7)),std::bit_cast<float>(get(p,at+11))};}
        else if(op==0xe04) {const auto slot=p[at];if(slot<240) stored_[slot]={p[at+1],p[at+2],p[at+3]};}
        else {auto& row=history_[index];row.code.fill(0);const auto text=p.subspan(at,21);std::copy(text.begin(),std::find(text.begin(),text.end(),0),row.code.begin());row.made_at=get(p,at+21);row.points=get(p,at+25);}
    }
    return EntryCatalogUpdate::applied;
}
std::optional<net::Packet> character_save_points(std::span<const EntrySavePoint,4> points) {
    net::Packet p{0x25,2,0};for(std::size_t i=0;i<4;++i) if(points[i].map!=65535) {const auto& row=points[i];p.push_back(static_cast<std::uint8_t>(i+1));append(p,row.map,2);for(auto value:{row.x,row.y,row.z}) append(p,std::bit_cast<std::uint32_t>(value));++p[2];}if(!p[2]) return std::nullopt;return p;
}
std::optional<net::Packet> EntryCatalogs::save_point_packet() const {return character_save_points(points_);}
net::Packet EntryCatalogs::history_packet() const {
    net::Packet p{1,0x26};append(p,cash_);p.push_back(0);for(const auto& row:history_) if(row.code[0]) {const auto end=std::find(row.code.begin(),row.code.end(),0);p.insert(p.end(),row.code.begin(),end);p.resize(p.size()+static_cast<std::size_t>(row.code.end()-end),0);append(p,row.made_at);append(p,row.points);++p[6];}return p;
}
std::optional<net::Packet> EntryCatalogs::stored_packet() const {net::Packet p{6,0x26,0};for(std::size_t i=0;i<240;++i) if(stored_[i].count) {const auto& row=stored_[i];p.insert(p.end(),{static_cast<std::uint8_t>(i),row.type,row.type_id,row.count});++p[2];}if(!p[2]) return std::nullopt;return p;}
net::Packet character_honor(std::uint16_t value) {net::Packet p{0x30,2};append(p,value,2);return p;}
net::Packet character_kill_count(std::uint8_t country,std::uint32_t points,std::uint32_t expires,std::uint32_t tick) {net::Packet p{0x11,2,country};append(p,points);const auto delta=std::bit_cast<std::int32_t>(expires-tick);append(p,expires && delta>0?static_cast<std::uint32_t>(delta):0);return p;}
std::vector<net::Packet> guild_listings(const std::map<std::uint32_t,GuildListing>& rows,std::uint8_t country) {
    std::vector<net::Packet> out{{0x0d,0x0d}};net::Packet p{0x2f,0x0d,0};for(const auto& [key,value]:rows) {static_cast<void>(key);if(value.country!=country) continue;append(p,value.id);fixed(p,value.name,25);fixed(p,value.master,21);fixed(p,value.notice,65);p.push_back(value.rank);append(p,value.points);if(++p[2]==14) count_batch(out,p,0xd2f);}if(p[2]) out.push_back(std::move(p));out.push_back({0x0e,0x0d});return out;
}
}
