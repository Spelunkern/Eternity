#include "eternity/game/purchases.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdio>

namespace eternity::game {
namespace {
void copy_string(std::array<std::uint8_t,21>& target,std::span<const std::uint8_t> source) {target.fill(0);std::copy(source.begin(),std::find(source.begin(),source.end(),0),target.begin());}
std::string text(const std::array<std::uint8_t,21>& value) {return {value.begin(),std::find(value.begin(),value.end(),0)};}
std::string timestamp(std::uint32_t value) {char buffer[32];std::snprintf(buffer,sizeof(buffer),"%04u-%02u-%02u %02u:%02u:%02u",2000+(value>>26),(value>>22)&15,(value>>17)&31,(value>>12)&31,(value>>6)&63,value&63);return buffer;}
std::uint32_t point_date(const storage::Cell& cell) {
    if(!cell || cell->size()<19 || (*cell)[4]!='-' || (*cell)[7]!='-' || (*cell)[10]!=' ' || (*cell)[13]!=':' || (*cell)[16]!=':') throw std::runtime_error("Invalid point history calendar");
    const auto number=[&](std::size_t start,std::size_t size){int value=0;const auto first=cell->data()+start,last=first+size;const auto result=std::from_chars(first,last,value);if(result.ec!=std::errc{} || result.ptr!=last) throw std::runtime_error("Invalid point history calendar field");return value;};
    const auto date=std::chrono::year{number(0,4)}/static_cast<unsigned int>(number(5,2))/static_cast<unsigned int>(number(8,2));const auto hour=number(11,2),minute=number(14,2),second=number(17,2);
    if(!date.ok() || hour<0 || minute<0 || second<0 || hour>23 || minute>59 || second>59) throw std::runtime_error("Invalid point history calendar range");
    return pack_world_time(std::chrono::sys_days{date}+std::chrono::hours{hour}+std::chrono::minutes{minute}+std::chrono::seconds{second});
}
}
PurchaseReplica::~PurchaseReplica() {for(auto& purchase:pending_) for(auto& item:purchase.items) inventory_.pool_.release(std::move(item));}
void PurchaseReplica::release(OwnedPurchase& purchase) {for(auto& item:purchase.items) inventory_.pool_.release(std::move(item));}
void PurchaseReplica::load_history(std::span<const PurchaseLog> rows,std::uint32_t cash) {if(rows.size()>history_.size()) throw std::invalid_argument("Point history exceeds ten slots");std::copy(rows.begin(),rows.end(),history_.begin());for(auto& row:history_) row.save_type=0;cash_points_=cash;}
bool PurchaseReplica::load(const PlayerSnapshot& player) {
    if(player.core.user_id!=account_id()) return false;
    std::vector<PurchaseLog> rows;for(const auto& row:player.catalogs.at("PointLog").rows) {
        if(row.size()!=3 || !row[0] || !row[2]) throw std::runtime_error("Original point history row changed");
        PurchaseLog value;const auto code=row[0]->substr(0,20);copy_string(value.code,std::span{reinterpret_cast<const std::uint8_t*>(code.data()),code.size()});value.made_at=point_date(row[1]);std::int64_t points=0;const auto& amount=*row[2];const auto result=std::from_chars(amount.data(),amount.data()+amount.size(),points);if(result.ec!=std::errc{} || result.ptr!=amount.data()+amount.size() || points<INT32_MIN || points>UINT32_MAX) throw std::runtime_error("Invalid point history amount");value.points=static_cast<std::uint32_t>(points);rows.push_back(value);
    }
    load_history(rows,player.cash_points);return true;
}
InventoryReplicaResult PurchaseReplica::apply(std::span<const std::uint8_t> p) {
    // ps_dbAgent.exe CUser::PacketUserPoint 00406cd9..00407075, status 0e02.
    const auto get=[&](std::size_t offset,unsigned int bytes){std::uint64_t value=0;for(unsigned int i=0;i<bytes;++i) value|=static_cast<std::uint64_t>(p[offset+i])<<(8*i);return value;};
    if(p.size()<6 || get(2,4)!=account_id()) return InventoryReplicaResult::rejected;
    if(get(0,2)!=0xe02) return InventoryReplicaResult::unsupported;
    if(p.size()<37 || p.size()!=37+46U*p[36]) return InventoryReplicaResult::rejected;
    for(std::size_t at=37;at<p.size();at+=46) if(p[at+12]>=6 || p[at+13]>=24) return InventoryReplicaResult::rejected;
    std::size_t slot=0;while(slot<history_.size() && history_[slot].save_type==1) ++slot;
    if(slot<history_.size()) {auto& log=history_[slot];copy_string(log.code,p.subspan(6,21));log.points=static_cast<std::uint32_t>(get(27,4));log.made_at=static_cast<std::uint32_t>(get(31,4));log.save_type=1;}
    OwnedPurchase purchase;purchase.log_slot=static_cast<std::uint8_t>(slot);
    const auto fail=[&](){for(auto& item:purchase.items) inventory_.pool_.strand(std::move(item));actions_.push_back(in_game_?PurchaseAction::queue_exit:PurchaseAction::exit);return InventoryReplicaResult::unsupported;};
    if(slot==history_.size()) return fail();
    const auto fill=[&](CachedInventoryItem& item,std::size_t at){auto& value=item.item;value.uid=get(at,8);value.id=static_cast<std::uint32_t>(get(at+8,4));value.bag=p[at+12];value.slot=p[at+13];value.type=p[at+14];value.type_id=p[at+15];value.count=p[at+16];value.quality=static_cast<std::uint16_t>(get(at+17,2));std::copy_n(p.begin()+static_cast<std::ptrdiff_t>(at+19),6,value.gems.begin());copy_string(item.craft_storage,p.subspan(at+25,21));value.craft=text(item.craft_storage);value.made_at=static_cast<std::uint32_t>(get(31,4));value.make_type=p[35]?std::string(1,static_cast<char>(p[35])):std::string{};};
    for(std::size_t at=37;at<p.size();at+=46) {
        auto& target=inventory_.inventory_[p[at+12]*24+p[at+13]];if(target) return fail();
        auto item=inventory_.pool_.allocate();if(!item) return fail();fill(*item,at);item->save_type=0;target=std::move(item);
        auto frozen=inventory_.pool_.allocate();if(!frozen) return fail();fill(*frozen,at);frozen->save_type=static_cast<std::int32_t>(p[at+12]|(static_cast<unsigned int>(p[at+13])<<16));purchase.items.push_back(std::move(frozen));
    }
    pending_.push_back(std::move(purchase));actions_.push_back(PurchaseAction::save);return InventoryReplicaResult::applied;
}
std::vector<PendingPurchase> PurchaseReplica::pending() const {std::vector<PendingPurchase> result;for(const auto& purchase:pending_) {PendingPurchase row;row.log_slot=purchase.log_slot;for(const auto& item:purchase.items) row.items.push_back(*item);result.push_back(std::move(row));}return result;}
net::Packet PurchaseReplica::packet() const {
    // SendCharacterPointLog 0040ab50: raw 21-byte code, packed date and amount.
    net::Packet p{1,14};const auto put=[&](std::uint32_t value){for(unsigned int i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};put(account_id());put(cash_points_);p.push_back(0);for(const auto& row:history_) if(row.code[0]) {p.insert(p.end(),row.code.begin(),row.code.end());put(row.made_at);put(row.points);++p[10];}return p;
}
std::vector<PointSaveOperation> purchase_save_plan(const PurchaseReplica& replica,const PendingPurchase& purchase,std::uint32_t character) {
    if(!character || character>INT32_MAX || !replica.account_id() || replica.account_id()>INT32_MAX || purchase.log_slot>=10) throw std::invalid_argument("Invalid purchase save owner");
    const auto& log=replica.history()[purchase.log_slot];std::vector<PointSaveOperation> result;
    for(const auto& cached:purchase.items) {const auto& item=cached.item;const auto position=std::bit_cast<std::uint32_t>(cached.save_type);std::vector<storage::Parameter> p{std::int64_t{character},std::bit_cast<std::int64_t>(item.uid),std::int64_t{position&65535},std::int64_t{position>>16},std::int64_t{std::bit_cast<std::int32_t>(item.id)},std::int64_t{item.type},std::int64_t{item.type_id},std::int64_t{item.quality}};for(auto gem:item.gems) p.emplace_back(std::int64_t{gem});p.emplace_back(item.craft);p.emplace_back(std::int64_t{item.count});p.emplace_back(timestamp(log.made_at));p.emplace_back(item.make_type);result.push_back({"usp_Save_Char_Item_Add_E",std::move(p)});}
    result.push_back({"usp_Save_User_BuyPointItems_NCash",{std::int64_t{replica.account_id()},std::int64_t{character},std::int64_t{std::bit_cast<std::int32_t>(log.points)},text(log.code),timestamp(log.made_at)}});return result;
}
PurchaseSaveResult execute_purchase_save(PurchaseReplica& replica,std::uint32_t character,const PurchaseSavePort& port) {
    if(!port.acquire || !port.begin || !port.commit || !port.rollback || !port.timeout || !port.release || !port.query) throw std::invalid_argument("Incomplete purchase save ports");
    const auto log=[&](std::string_view message){if(port.diagnostic) port.diagnostic(message);};PurchaseSaveResult result;std::size_t cursor=0;
    while(cursor<replica.pending_.size()) {
        const auto all=replica.pending();const auto plan=purchase_save_plan(replica,all[cursor],character);
        if(!port.acquire(PurchaseConnection::items)) {log("Purchase item DB allocation failed");result.legacy_success=false;break;}
        bool points=false;
        try {
            if(!port.begin(PurchaseConnection::items)) log("Purchase item transaction begin failed");
            for(std::size_t i=0;i+1<plan.size();++i) (void)port.query(PurchaseConnection::items,plan[i]); // No procedure-return binding or retry.
            points=port.acquire(PurchaseConnection::points);
            if(!points) {log("Purchase point DB allocation failed");throw std::runtime_error("Purchase point DB allocation failed");}
            if(port.query(PurchaseConnection::points,plan.back())<0) throw std::runtime_error("Purchase charge procedure failed");
        }catch(const std::runtime_error& error) {
            log(error.what());const bool retry=port.timeout(PurchaseConnection::items) || (points && port.timeout(PurchaseConnection::points));if(points) port.release(PurchaseConnection::points);
            if(!port.rollback(PurchaseConnection::items)) log("Purchase item transaction rollback failed");
            port.release(PurchaseConnection::items);
            if(retry) {++result.retained;++cursor;continue;}
            replica.release(replica.pending_[cursor]);replica.pending_.erase(replica.pending_.begin()+static_cast<std::ptrdiff_t>(cursor));result.legacy_success=false;++result.discarded;continue;
        }
        port.release(PurchaseConnection::points);if(!port.commit(PurchaseConnection::items)) log("Purchase item transaction commit failed");
        auto& purchase=replica.pending_[cursor];replica.history_[purchase.log_slot].save_type=0;port.release(PurchaseConnection::items);replica.release(purchase);replica.pending_.erase(replica.pending_.begin()+static_cast<std::ptrdiff_t>(cursor));++result.saved;
    }
    return result;
}
PurchaseStore::PurchaseStore(storage::SqlServer& items,storage::SqlServer& points,std::string catalog):items_(items),points_(points) {
    if(&items==&points) throw std::invalid_argument("Purchase saving requires two distinct DB connections");
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');})) throw std::invalid_argument("Invalid purchase SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
PurchaseSaveResult PurchaseStore::save(PurchaseReplica& replica,std::uint32_t character,const std::function<void(std::string_view)>& diagnostic) {
    if(items_.character_write_context_active() || points_.character_write_context_active()) throw std::logic_error("Purchase saving must precede the complete character transaction");
    const auto db=[&](PurchaseConnection connection)->storage::SqlServer&{return connection==PurchaseConnection::items?items_:points_;};
    const auto transaction=[&](PurchaseConnection connection,auto operation){if(operation!=&storage::SqlServer::begin && !db(connection).transaction_active()) return false;try{(db(connection).*operation)();return true;}catch(const std::runtime_error&){return false;}};
    PurchaseSavePort port;port.acquire=[](PurchaseConnection){return true;};port.begin=[&](auto connection){return transaction(connection,&storage::SqlServer::begin);};port.commit=[&](auto connection){return transaction(connection,&storage::SqlServer::commit);};port.rollback=[&](auto connection){return transaction(connection,&storage::SqlServer::rollback);};port.timeout=[&](auto connection){return db(connection).last_query_sqlstate()=="HYT00";};
    port.release=[&](auto connection){if(db(connection).transaction_active()) (void)transaction(connection,&storage::SqlServer::rollback);};port.diagnostic=diagnostic;
    port.query=[&](PurchaseConnection connection,const PointSaveOperation& op)->std::int64_t {
        const bool charge=connection==PurchaseConnection::points;std::string query=charge?"DECLARE @Saved int; EXEC @Saved=":"EXEC ";query+=prefix_+op.procedure+" ";for(std::size_t i=0;i<op.parameters.size();++i) query+=(i?",?":"?");
        if(!charge) {const auto& make=std::get<std::string>(op.parameters.back());if(make.empty() || make=="'") {db(connection).observe_query_error("[42000] Original purchase item SQL literal is truncated or unclosed");throw std::runtime_error("[42000] Original purchase item SQL literal is truncated or unclosed");}}
        try {const auto sets=db(connection).query(query+(charge?"; SELECT @Saved;":";"),op.parameters);if(!charge) return 0;
            if(sets.size()!=1 || sets[0].rows.size()!=1 || sets[0].rows[0].size()!=1 || !sets[0].rows[0][0]) throw std::runtime_error("Original purchase charge result changed");
            const auto& cell=*sets[0].rows[0][0];std::size_t consumed=0;const auto value=std::stoll(cell,&consumed);if(consumed!=cell.size()) throw std::runtime_error("Invalid purchase charge result");return value;
        }catch(const std::runtime_error& error) {db(connection).observe_query_error(error.what());throw;}
    };
    return execute_purchase_save(replica,character,port);
}
}
