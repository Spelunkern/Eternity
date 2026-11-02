#include "eternity/game/point_items.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <new>

namespace eternity::game {
std::unique_ptr<CachedPointItem> PointItemPool::allocate() {
    std::lock_guard lock(mutex_);if(!available_.empty()) {auto item=std::move(available_.back());available_.pop_back();return item;}
    ++attempts_;try{return allocator_?allocator_():std::make_unique<CachedPointItem>();}catch(const std::bad_alloc&){return nullptr;}
}
void PointItemPool::release(std::unique_ptr<CachedPointItem> item) {if(item) {std::lock_guard lock(mutex_);available_.push_back(std::move(item));}}
InventoryReplicaPoolStatistics PointItemPool::statistics() const {std::lock_guard lock(mutex_);return {attempts_,available_.size(),0};}
PointItemReplica::~PointItemReplica() {for(auto& item:stored_) pool_.release(std::move(item));}
void PointItemReplica::load_products(std::span<const PlayerPointItem> rows) {
    // LoadBillingItem 0041ead0: nonzero count owns the slot; later zero-count rows
    // can be overwritten. Only the summary and save flag are written on load.
    for(const auto& row:rows) if(row.slot<240 && !products_[row.slot].count) {
        auto& item=products_[row.slot];item.type=static_cast<std::uint8_t>(row.id/1000);item.type_id=static_cast<std::uint8_t>(row.id%1000);item.count=row.count;item.save_type=0;
    }
}
void PointItemReplica::load_stored(std::span<const PlayerPointItem> rows) {
    // LoadCharacter 004229c3..00422a74: duplicate pointers win, including count zero;
    // failed allocation skips that row. Reused receipt payload is retained.
    for(const auto& row:rows) if(row.slot<240 && !stored_[row.slot]) {
        auto item=pool_.allocate();if(!item) continue;item->save_type=0;
        item->receipt.type=static_cast<std::uint8_t>(row.id/1000);item->receipt.type_id=static_cast<std::uint8_t>(row.id%1000);item->receipt.count=row.count;stored_[row.slot]=std::move(item);
    }
}
bool PointItemReplica::load(const PlayerSnapshot& player) {if(player.core.user_id!=account_id()) return false;load_stored(player.point_items);return true;}
InventoryReplicaResult PointItemReplica::apply(std::span<const std::uint8_t> p) {
    // PacketUserItem 00408f55..00409081 / PacketUserPoint 004070d5..00407254.
    const auto get=[&](std::size_t offset,unsigned int bytes){std::uint64_t value=0;for(unsigned int i=0;i<bytes;++i) value|=static_cast<std::uint64_t>(p[offset+i])<<(8*i);return value;};
    if(p.size()<6 || get(2,4)!=account_id()) return InventoryReplicaResult::rejected;
    const auto op=get(0,2);if(op!=0x710 && op!=0xe05) return InventoryReplicaResult::unsupported;
    if(p.size()!=58 || p[6]>=240 || p[7]>=6 || p[8]>=24) return InventoryReplicaResult::rejected;
    // Original dereferences this pointer after allocating. Guard invalid ownership
    // before mutation; valid original inputs preserve its exact allocation order.
    auto& target=inventory_.inventory_[p[7]*24+p[8]];if(target) return InventoryReplicaResult::disconnect;
    if(op==0xe05 && !stored_[p[6]]) return InventoryReplicaResult::rejected;
    auto item=inventory_.pool_.allocate();if(!item) return InventoryReplicaResult::applied;
    auto& value=item->item;value.uid=get(9,8);value.id=static_cast<std::uint32_t>(get(17,4));value.type=p[21];value.type_id=p[22];value.count=p[23];value.quality=static_cast<std::uint16_t>(get(24,2));std::copy_n(p.begin()+26,6,value.gems.begin());
    std::copy_n(p.begin()+32,21,item->craft_storage.begin());item->craft_storage[20]=0;value.craft.assign(item->craft_storage.begin(),std::find(item->craft_storage.begin(),item->craft_storage.end(),0));value.made_at=static_cast<std::uint32_t>(get(53,4));value.make_type=p[57]?std::string(1,static_cast<char>(p[57])):std::string{};item->save_type=0;
    auto receipt=value;receipt.bag=p[7];receipt.slot=p[8];receipt.gems.fill(0);receipt.craft.clear();
    if(op==0x710) {auto& product=products_[p[6]];product.receipt=std::move(receipt);product.count=0;if(product.save_type!=1) product.save_type=3;}
    else {auto& stored=*stored_[p[6]];stored.receipt=std::move(receipt);if(stored.save_type!=1) stored.save_type=3;}
    target=std::move(item);return InventoryReplicaResult::applied;
}
std::vector<std::pair<std::uint8_t,CachedPointItem>> PointItemReplica::stored() const {
    std::vector<std::pair<std::uint8_t,CachedPointItem>> result;for(std::size_t i=0;i<stored_.size();++i) if(stored_[i]) result.emplace_back(static_cast<std::uint8_t>(i),*stored_[i]);return result;
}
namespace {
net::Packet packet(std::uint16_t op,std::uint32_t account) {net::Packet result{static_cast<std::uint8_t>(op),static_cast<std::uint8_t>(op>>8)};for(unsigned int i=0;i<4;++i) result.push_back(static_cast<std::uint8_t>(account>>(8*i)));result.push_back(0);return result;}
std::optional<net::Packet> finish(net::Packet p) {if(p.size()==7) return std::nullopt;p[6]=static_cast<std::uint8_t>((p.size()-7)/4);return p;}
std::string timestamp(std::uint32_t value) {char buffer[32];std::snprintf(buffer,sizeof(buffer),"%04u-%02u-%02u %02u:%02u:%02u",2000+(value>>26),(value>>22)&15,(value>>17)&31,(value>>12)&31,(value>>6)&63,value&63);return buffer;}
}
std::optional<net::Packet> PointItemReplica::product_packet() const {auto result=packet(0x40c,account_id());for(std::size_t i=0;i<products_.size();++i) {const auto& item=products_[i];if(item.count) result.insert(result.end(),{static_cast<std::uint8_t>(i),item.type,item.type_id,item.count});}return finish(std::move(result));}
std::optional<net::Packet> PointItemReplica::stored_packet() const {auto result=packet(0xe04,account_id());for(std::size_t i=0;i<stored_.size();++i) if(stored_[i]) {const auto& item=stored_[i]->receipt;result.insert(result.end(),{static_cast<std::uint8_t>(i),item.type,item.type_id,item.count});}return finish(std::move(result));}
void PointItemReplica::point_save_success() {for(auto& product:products_) product.save_type=0;for(auto& item:stored_) if(item && item->save_type==3) pool_.release(std::move(item));}
std::vector<PointSaveOperation> point_save_plan(const PointItemReplica& replica,std::uint8_t server,std::uint32_t character) {
    if(!character || character>INT32_MAX || !replica.account_id() || replica.account_id()>INT32_MAX) throw std::invalid_argument("Invalid point item save owner");
    std::vector<PointSaveOperation> result;
    const auto add=[&](bool product,std::uint8_t slot,const PlayerItem& item) {
        std::vector<storage::Parameter> p;if(product) p.emplace_back(std::int64_t{server});
        p.emplace_back(std::int64_t{replica.account_id()});p.emplace_back(std::int64_t{slot});p.emplace_back(std::int64_t{character});p.emplace_back(std::bit_cast<std::int64_t>(item.uid));p.emplace_back(std::int64_t{item.bag});p.emplace_back(std::int64_t{item.slot});p.emplace_back(std::int64_t{std::bit_cast<std::int32_t>(item.id)});p.emplace_back(std::int64_t{item.type});p.emplace_back(std::int64_t{item.type_id});p.emplace_back(std::int64_t{item.quality});
        for(unsigned int i=0;i<6;++i) p.emplace_back(std::int64_t{0});
        p.emplace_back(std::string{});p.emplace_back(std::int64_t{item.count});p.emplace_back(timestamp(item.made_at));p.emplace_back(item.make_type);
        result.push_back({product?"usp_Save_Char_Product_Item_E":"usp_Save_User_StoredPointItems",std::move(p)});
    };
    for(std::size_t i=0;i<replica.products().size();++i) {const auto& product=replica.products()[i];if(product.save_type==3) add(true,static_cast<std::uint8_t>(i),product.receipt);}
    for(const auto& [slot,item]:replica.stored()) if(item.save_type==3) add(false,slot,item.receipt);
    return result;
}
PointItemStore::PointItemStore(storage::SqlServer& database,std::uint8_t server,std::string catalog):database_(database),server_(server) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid point item SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
void PointItemStore::load_products(PointItemReplica& replica) {
    const std::array<storage::Parameter,1> owner{std::int64_t{replica.account_id()}};const auto sets=database_.query("EXEC "+prefix_+"usp_Read_Char_Product_Item_E ?;",owner);
    if(sets.size()!=1) throw std::runtime_error("Original product loader result changed");
    replica.load_products(decode_point_items(sets[0]));
}
void execute_point_save(std::span<const PointSaveOperation> plan,const std::function<std::int64_t(const PointSaveOperation&)>& execute) {
    for(const auto& operation:plan) if(execute(operation)<0) throw std::runtime_error("Original point item save procedure failed");
}
void PointItemStore::save_in_transaction(const PointItemReplica& replica,std::uint32_t character) {
    if(!database_.transaction_active()) throw std::logic_error("Point item save requires the complete character transaction");
    execute_point_save(point_save_plan(replica,server_,character),[&](const auto& op)->std::int64_t {
        // Original sprintf puts MakeType inside a quoted %c literal. Zero ends
        // the SQL string early; a quote leaves the literal unclosed. Bound values
        // must not turn those original query errors into successful deliveries.
        const auto& make=std::get<std::string>(op.parameters.back());
        if(make.empty() || make=="'") throw std::runtime_error("Original point item SQL literal is truncated or unclosed");
        std::string query="DECLARE @Saved int; EXEC @Saved="+prefix_+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) query+=(i?",?":"?");
        const auto sets=database_.query(query+"; SELECT @Saved;",op.parameters);
        if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original point item save result changed");
        const auto& cell=*sets[0].rows[0][0];std::size_t consumed=0;const auto value=std::stoll(cell,&consumed);if(consumed!=cell.size()) throw std::runtime_error("Invalid point item procedure result");return value;
    });
}
}
