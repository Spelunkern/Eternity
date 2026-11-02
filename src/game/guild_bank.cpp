#include "eternity/game/guild_bank.hpp"
#include <algorithm>

namespace eternity::game {
namespace {
void append(net::Packet& p,std::uint64_t value,unsigned int n=4) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
std::uint64_t get(std::span<const std::uint8_t> p,std::size_t at,unsigned int n=4) {std::uint64_t value=0;for(unsigned int i=0;i<n;++i) value|=std::uint64_t{p[at+i]}<<(8*i);return value;}
std::string craft(std::span<const std::uint8_t> p) {const auto end=std::find(p.begin(),p.end(),0);if(end==p.end()) throw std::invalid_argument("Unterminated guild bank craft");return {p.begin(),end};}
void fixed(net::Packet& p,const std::string& value) {if(value.size()>20) throw std::invalid_argument("Guild bank craft exceeds original field");p.insert(p.end(),value.begin(),value.end());p.insert(p.end(),21-value.size(),0);}
void client_row(net::Packet& p,const PlayerItem& item,std::uint8_t slot) {p.insert(p.end(),{slot,item.type,item.type_id});append(p,item.quality,2);p.insert(p.end(),item.gems.begin(),item.gems.end());p.push_back(item.count);fixed(p,item.craft);}
}
GuildBankReplica::~GuildBankReplica() {for(auto& item:slots_) pool_.release(std::move(item));}
void GuildBankReplica::begin_load() {if(loading_) throw std::logic_error("Guild bank load already active");pending_.clear();loading_=true;}
void GuildBankReplica::load(PlayerItem value,std::int32_t slot) {
    if(!loading_) throw std::logic_error("Guild bank load is not active");
    if(slot<0) throw std::invalid_argument("Negative guild bank slot");if(slot>=240) return;
    if(value.craft.size()>20 || value.craft.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid guild bank craft");
    auto item=pool_.allocate();if(!item) return;const bool duplicate=bool(slots_[static_cast<std::size_t>(slot)]);
    if(!duplicate) item->save_type=0;else if(item->save_type==0) item->save_type=2;
    value.slot=static_cast<std::uint16_t>(slot);value.bag=0;
    value.craft.erase(std::remove(value.craft.begin(),value.craft.end(),' '),value.craft.end());item->item=std::move(value);
    std::copy(item->item.craft.begin(),item->item.craft.end(),item->craft_storage.begin());item->craft_storage[item->item.craft.size()]=0;item->craft_storage[20]=0;
    if(duplicate) pending_.push_back(std::move(item));else slots_[static_cast<std::size_t>(slot)]=std::move(item);
}
void GuildBankReplica::finish_load(bool success) {
    if(!loading_) throw std::logic_error("Guild bank load is not active");loading_=false;
    if(success) {std::size_t position=0;for(auto& item:pending_) {while(position<240 && slots_[position]) ++position;if(position<240) slots_[position++]=std::move(item);else pool_.release(std::move(item));}}
    // The original error path directly deletes temporary nodes, bypassing its pool.
    pending_.clear();
}
std::vector<CachedInventoryItem> GuildBankReplica::snapshot() const {std::vector<CachedInventoryItem> out;for(std::size_t i=0;i<240;++i) if(slots_[i]) {out.push_back(*slots_[i]);out.back().item.slot=static_cast<std::uint16_t>(i);}return out;}
std::vector<net::Packet> GuildBankReplica::packets(std::uint32_t guild) const {
    std::vector<net::Packet> out;net::Packet p{0x0d,0x0b};append(p,guild);p.push_back(0);
    const auto flush=[&]{if(p[6]) out.push_back(std::move(p));p={0x0d,0x0b};append(p,guild);p.push_back(0);};
    for(std::size_t i=0;i<240;++i) if(slots_[i]) {const auto& v=slots_[i]->item;append(p,v.uid,8);p.insert(p.end(),{static_cast<std::uint8_t>(i),v.type,v.type_id});append(p,v.quality,2);p.insert(p.end(),v.gems.begin(),v.gems.end());p.push_back(v.count);append(p,v.made_at);p.push_back(v.make_type.empty()?0:static_cast<std::uint8_t>(v.make_type[0]));fixed(p,v.craft);if(++p[6]==40) flush();}flush();return out;
}
GuildBank::GuildBank(GuildBankServices& services):services_(services) {if(!services.send_db || !services.fatal_definition || !services.corrected_count) throw std::invalid_argument("Incomplete game guild bank services");}
GuildBank::~GuildBank() {for(auto* item:slots_) services_.pool.release(item);}
const GroundItem* GuildBank::find(std::uint8_t slot) const noexcept {return slot<240?slots_[slot]:nullptr;}
GuildBankUpdate GuildBank::apply(std::span<const std::uint8_t> p) {
    if(p.size()<7 || get(p,0,2)!=0xb0d || p.size()<7+std::size_t{p[6]}*46) return GuildBankUpdate::rejected;
    for(std::size_t i=0;i<p[6];++i) {
        const auto row=p.subspan(7+i*46,46);const auto slot=row[8];if(slot>=240) return GuildBankUpdate::rejected;
        const auto* definition=services_.definitions.find(row[9],row[10]);if(!definition) {services_.fatal_definition(row[9],row[10]);return GuildBankUpdate::fatal;}
        const auto name=craft(row.subspan(25,21));auto* item=services_.pool.allocate();if(!item) continue;
        item->definition=definition;auto& value=item->item;value.uid=get(row,0,8);value.id=definition->id;value.bag=0;value.slot=slot;value.type=row[9];value.type_id=row[10];value.quality=static_cast<std::uint16_t>(get(row,11,2));value.count=row[19];
        if(definition->stack_count<value.count && definition->real_type>0 && definition->real_type<26) {
            services_.corrected_count(slot,value.count,definition->stack_count);value.count=definition->stack_count;net::Packet correction{0x14,0x0b};append(correction,get(p,2));correction.push_back(slot);correction.push_back(value.count);services_.send_db(std::move(correction));
        }
        std::copy_n(row.begin()+13,6,value.gems.begin());value.made_at=static_cast<std::uint32_t>(get(row,20));if(row[24]) value.make_type.assign(1,static_cast<char>(row[24]));
        const auto expanded=decode_craft(*definition,name,services_.enchant);set_ground_item_craft(*item,expanded.name);item->craft_additions=expanded.additions;
        slots_[slot]=item;
    }
    return GuildBankUpdate::applied;
}
std::vector<net::Packet> GuildBank::client_packets() const {
    std::vector<net::Packet> out;net::Packet p{0x29,0x0d,0};const auto flush=[&]{if(p[2]) out.push_back(std::move(p));p={0x29,0x0d,0};};
    for(std::size_t i=0;i<240;++i) if(slots_[i]) {client_row(p,slots_[i]->item,static_cast<std::uint8_t>(i));if(++p[2]==50) flush();}flush();return out;
}
}
