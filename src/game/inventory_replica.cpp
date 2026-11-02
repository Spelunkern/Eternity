#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/money.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace eternity::game {
std::unique_ptr<CachedInventoryItem> InventoryReplicaPool::allocate() {
    std::lock_guard lock(mutex_);if(!available_.empty()) {auto result=std::move(available_.back());available_.pop_back();return result;}
    ++attempts_;try{return allocator_?allocator_():std::make_unique<CachedInventoryItem>();}catch(const std::bad_alloc&){return nullptr;}
}
void InventoryReplicaPool::release(std::unique_ptr<CachedInventoryItem> item) {if(item){std::lock_guard lock(mutex_);available_.push_back(std::move(item));}}
void InventoryReplicaPool::strand(std::unique_ptr<CachedInventoryItem> item) {if(item){std::lock_guard lock(mutex_);stranded_.push_back(std::move(item));}}
InventoryReplicaPoolStatistics InventoryReplicaPool::statistics() const {std::lock_guard lock(mutex_);return {attempts_,available_.size(),stranded_.size()};}
InventoryReplica::~InventoryReplica() {for(auto& item:inventory_) pool_.release(std::move(item));for(auto& item:bank_) pool_.release(std::move(item));}
void InventoryReplica::inventory_save_success() noexcept {
    deleted_inventory_.clear();deleted_bank_.clear();
    for(auto& item:inventory_) if(item) item->save_type=0;
    for(auto& item:bank_) if(item) item->save_type=0;
}
namespace {
std::uint64_t get(std::span<const std::uint8_t> p,std::size_t offset,unsigned int bytes) {std::uint64_t value=0;for(unsigned int i=0;i<bytes;++i) value|=static_cast<std::uint64_t>(p[offset+i])<<(8*i);return value;}
void dirty(CachedInventoryItem& item) {if(item.save_type==0) item.save_type=2;}
void craft(CachedInventoryItem& item,std::span<const std::uint8_t> value) {std::copy_n(value.begin(),21,item.craft_storage.begin());item.craft_storage[20]=0;const auto end=std::find(item.craft_storage.begin(),item.craft_storage.end(),0);item.item.craft.assign(item.craft_storage.begin(),end);}
}
bool InventoryReplica::load(PlayerItem value,bool bank,std::int32_t save_type) {
    if(value.slot>=(bank?240:24) || (!bank && value.bag>=6) || value.craft.size()>20 || value.craft.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid resolved DB-agent item row");
    auto& slot=bank?bank_[value.slot]:inventory_[value.bag*24+value.slot];if(slot) throw std::invalid_argument("Duplicate DB-agent item row");
    auto item=pool_.allocate();if(!item) return false;item->item=std::move(value);item->save_type=save_type;item->saved_type=0;item->craft_storage.fill(0);std::copy(item->item.craft.begin(),item->item.craft.end(),item->craft_storage.begin());slot=std::move(item);return true;
}
void InventoryReplica::journal(const Item& item,bool bank) {if(item->save_type!=1) (bank?deleted_bank_:deleted_inventory_).push_back(item->item.uid);}
std::vector<CachedInventoryItem> InventoryReplica::inventory() const {std::vector<CachedInventoryItem> result;for(std::size_t i=0;i<inventory_.size();++i) if(inventory_[i]) {result.push_back(*inventory_[i]);result.back().item.bag=static_cast<std::uint8_t>(i/24);result.back().item.slot=static_cast<std::uint16_t>(i%24);}return result;}
std::vector<CachedInventoryItem> InventoryReplica::bank() const {std::vector<CachedInventoryItem> result;for(std::size_t i=0;i<bank_.size();++i) if(bank_[i]) {result.push_back(*bank_[i]);result.back().item.bag=0;result.back().item.slot=static_cast<std::uint16_t>(i);}return result;}
bool InventoryReplica::apply_money(std::span<const std::uint8_t> packet) {
    // PacketUserSetStatus 00405b00 case 0603 writes the same CUser+78 DWORD
    // as gem/craft/repair commands in PacketUserItem.
    MoneyReplica status(account_id_,money_);if(!status.apply(packet)) return false;money_=status.money();return true;
}
InventoryReplicaResult InventoryReplica::apply(std::span<const std::uint8_t> p) {
    // ps_dbAgent.exe PacketUserItem 00408480; status 0617..0619 at
    // PacketUserSetStatus 00405f4e..00405fea; journals 00405200/00405270.
    // Validate the owning dispatcher envelope and memory domain before mutation.
    if(p.size()<6 || get(p,2,4)!=account_id_) return InventoryReplicaResult::rejected;
    const auto op=get(p,0,2);std::size_t size=0;
    switch(op) {case 0x617:size=9;break;case 0x618:size=8;break;case 0x619:size=7;break;case 0x701:size=57;break;case 0x702:case 0x707:case 0x70c:size=9;break;case 0x703:size=12;break;case 0x704:case 0x705:size=11;break;case 0x706:size=10;break;case 0x708:case 0x709:case 0x70a:size=14;break;case 0x70b:if(p.size()<11) return InventoryReplicaResult::rejected;size=11+3U*p[10];break;case 0x70d:if(p.size()<7) return InventoryReplicaResult::rejected;size=7+3U*p[6];break;case 0x70e:case 0x70f:size=6;break;case 0x711:size=18;break;case 0x715:size=15;break;case 0x716:size=33;break;case 0x717:size=29;break;default:return InventoryReplicaResult::unsupported;}
    if(p.size()!=size) return InventoryReplicaResult::rejected;
    const auto inv_valid=[&](std::size_t offset){return p[offset]<6 && p[offset+1]<24;};const auto bank_valid=[&](std::size_t offset){return p[offset]<240;};
    switch(op) {
        case 0x618:case 0x619:if(!bank_valid(6)) return InventoryReplicaResult::rejected;break;
        case 0x701:if(!inv_valid(18)) return InventoryReplicaResult::rejected;break;
        case 0x703:if(!inv_valid(6) || !inv_valid(9)) return InventoryReplicaResult::rejected;break;
        case 0x704:if(!inv_valid(6) || !bank_valid(9)) return InventoryReplicaResult::rejected;break;
        case 0x705:if(!bank_valid(6) || !inv_valid(8)) return InventoryReplicaResult::rejected;break;
        case 0x706:if(!bank_valid(6) || !bank_valid(8)) return InventoryReplicaResult::rejected;break;
        case 0x70b:case 0x70d:{const auto start=op==0x70b?11U:7U;for(std::size_t i=start;i<p.size();i+=3) if(p[i]>=24) return InventoryReplicaResult::rejected;break;}
        case 0x70c:if(p[6]>=24) return InventoryReplicaResult::rejected;break;
        case 0x70e:case 0x70f:{const auto begin=op==0x70e?24U:0U,end=op==0x70e?144U:24U;for(auto i=begin;i<end;++i) if(!inventory_[i]) return InventoryReplicaResult::rejected;break;}
        default:if(!inv_valid(6)) return InventoryReplicaResult::rejected;
    }
    if((op==0x708 || op==0x715) && p[8]>=6) return InventoryReplicaResult::rejected;
    const auto inv=[&](std::size_t offset)->Item&{return inventory_[p[offset]*24+p[offset+1]];};
    const auto move=[&](Item& source,Item& target,std::uint8_t source_count,std::uint8_t target_count,bool bank) {
        if(!source) return;
        if(!target) {target=std::move(source);dirty(*target);return;}
        source->item.count=source_count;target->item.count=target_count;source.swap(target);dirty(*target);dirty(*source);
        if(source->item.count==0) {journal(source,bank);pool_.release(std::move(source));}
    };
    switch(op) {
        case 0x617:case 0x618:{auto& item=op==0x617?inv(6):bank_[p[6]];if(!item) break;dirty(*item);item->item.count=p[op==0x617?8:7];break;}
        case 0x619:{auto& item=bank_[p[6]];if(!item) break;journal(item,true);pool_.release(std::move(item));break;}
        case 0x701:{auto& target=inv(18);if(target) {if(target->item.type!=p[20] || target->item.type_id!=p[21]) return InventoryReplicaResult::disconnect;target->item.count=static_cast<std::uint8_t>(target->item.count+p[22]);dirty(*target);break;}
            auto item=pool_.allocate();if(!item) break;auto& value=item->item;value.uid=get(p,6,8);value.id=static_cast<std::uint32_t>(get(p,14,4));value.type=p[20];value.type_id=p[21];value.count=p[22];value.quality=static_cast<std::uint16_t>(get(p,23,2));std::copy_n(p.begin()+25,6,value.gems.begin());craft(*item,p.subspan(31,21));value.made_at=static_cast<std::uint32_t>(get(p,52,4));value.make_type=p[56]?std::string(1,static_cast<char>(p[56])):std::string{};item->save_type=1;target=std::move(item);break;}
        case 0x702:{auto& item=inv(6);if(!item) break;if(p[8]>=item->item.count) {journal(item,false);pool_.release(std::move(item));}else {dirty(*item);item->item.count=static_cast<std::uint8_t>(item->item.count-p[8]);}break;}
        case 0x703:move(inv(6),inv(9),p[8],p[11],false);break;
        case 0x704:case 0x705:{auto& owned=inv(op==0x704?6:8);auto& stored=bank_[p[op==0x704?9:6]];auto& source=op==0x704?owned:stored;auto& target=op==0x704?stored:owned;if(!source) break;
            if(!target) {journal(source,op==0x705);target=std::move(source);target->save_type=1;break;}
            journal(owned,false);journal(stored,true);source->item.count=p[op==0x704?8:7];target->item.count=p[10];owned.swap(stored);owned->save_type=1;stored->save_type=1;break;}
        case 0x706:move(bank_[p[6]],bank_[p[8]],p[7],p[9],true);break;
        case 0x70b:case 0x70d:{const auto start=op==0x70b?11U:7U;for(std::size_t i=start;i<p.size();i+=3) if(auto& item=inventory_[p[i]]) {item->item.quality=static_cast<std::uint16_t>(get(p,i+1,2));dirty(*item);}if(op==0x70b) money_=static_cast<std::uint32_t>(get(p,6,4));break;}
        case 0x70c:if(auto& item=inventory_[p[6]]) {item->item.quality=static_cast<std::uint16_t>(get(p,7,2));dirty(*item);}break;
        case 0x70e:case 0x70f:{const auto begin=op==0x70e?24U:0U,end=op==0x70e?144U:24U;for(auto i=begin;i<end;++i) {journal(inventory_[i],false);pool_.strand(std::move(inventory_[i]));}break;}
        default:{auto& item=inv(6);if(!item) break;auto& value=item->item;
            switch(op) {
                case 0x707:value.count=p[8];break;
                case 0x708:value.gems[p[8]]=p[9];money_=static_cast<std::uint32_t>(get(p,10,4));break;
                case 0x709:value.gems.fill(0);value.quality=static_cast<std::uint16_t>(get(p,8,2));money_=static_cast<std::uint32_t>(get(p,10,4));break;
                case 0x70a:value.quality=static_cast<std::uint16_t>(get(p,8,2));money_=static_cast<std::uint32_t>(get(p,10,4));break;
                case 0x711:std::copy_n(p.begin()+8,6,value.gems.begin());money_=static_cast<std::uint32_t>(get(p,14,4));break;
                case 0x715:value.gems[p[8]]=0;value.quality=static_cast<std::uint16_t>(get(p,9,2));money_=static_cast<std::uint32_t>(get(p,11,4));break;
                case 0x716:craft(*item,p.subspan(12,21));money_=static_cast<std::uint32_t>(get(p,8,4));break;
                case 0x717:craft(*item,p.subspan(8,21));break;
            }
            dirty(*item);break;}
    }
    return InventoryReplicaResult::applied;
}
}
