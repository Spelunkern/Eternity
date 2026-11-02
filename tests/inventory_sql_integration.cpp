#include "eternity/game/inventory_save.hpp"
#include <iostream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
net::Packet packet(std::uint16_t op,std::initializer_list<std::uint8_t> rest) {net::Packet p{static_cast<std::uint8_t>(op),static_cast<std::uint8_t>(op>>8),0x29,0x23,0,0};p.insert(p.end(),rest);return p;}
PlayerItem item(std::uint64_t uid,std::uint8_t bag,std::uint16_t slot) {PlayerItem result;result.uid=uid;result.id=5001;result.type=5;result.type_id=1;result.count=10;result.bag=bag;result.slot=slot;result.quality=100;result.craft="01020304050607080900";result.gems={1,2,3,4,5,6};result.made_at=pack_world_time(std::chrono::sys_days{std::chrono::year{2024}/2/29}+std::chrono::hours{23}+std::chrono::minutes{59}+std::chrono::seconds{58});result.make_type="U";return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Inventory SQL test requires disposable catalog");
        const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";storage::SqlServer db(storage::SqlServerConfig::from_environment());InventoryStore store(db,1,catalog);
        InventoryReplicaPool pool;InventoryReplica cache(9001,77,pool);
        bool rejected=false;try{store.save_in_transaction(cache,810001);}catch(const std::logic_error&){rejected=true;}require(rejected,"Inventory phase ran outside character transaction");
        for(std::uint16_t i=0;i<81;++i) require(cache.load(item(0x7777000000000000ULL+i,static_cast<std::uint8_t>(1+i/24),i%24),false,1),"Cannot seed new inventory");
        auto bank=item((UINT64_MAX-1),0,239);bank.craft="12'34";require(cache.load(bank,true,1),"Cannot seed new bank item");
        auto high=item(0x7777000000000100ULL,5,23);high.quality=65535;require(cache.load(high,false,1),"Cannot seed normalization row");
        db.begin();store.save_in_transaction(cache,810001);require(cache.inventory()[0].save_type==1,"SQL phase acknowledged before character commit");db.commit();cache.inventory_save_success();
        auto rows=db.query("SELECT COUNT(*),MIN(CONVERT(varchar(19),MakeTime,120)) FROM "+prefix+"CharItems WHERE CharID=810001; SELECT Quality FROM "+prefix+"CharItems WHERE CharID=810001 AND Bag=5 AND Slot=23; SELECT ItemUID,Slot,Craftname,[Count] FROM "+prefix+"UserStoredItems WHERE ServerID=1 AND UserUID=9001;");
        require(rows.size()==3 && rows[0].rows[0]==storage::Row{storage::Cell{"82"},storage::Cell{"2024-02-29 23:59:58"}} && rows[1].rows[0][0]==storage::Cell{"0"},"Original insert/count/time/quality normalization changed");
        require(rows[2].rows[0]==storage::Row{storage::Cell{"-2"},storage::Cell{"239"},storage::Cell{"12'34"},storage::Cell{"10"}},"Bank UID bits/last slot/bound craft did not persist");
        // Existing rows update by UID, with bank Count before Craftname in the original signature.
        require(cache.apply(packet(0x617,{1,0,7}))==InventoryReplicaResult::applied,"Cannot mutate inventory count");
        require(cache.apply(packet(0x706,{239,6,238,4}))==InventoryReplicaResult::applied,"Cannot move bank row");
        db.begin();store.save_in_transaction(cache,810001);db.commit();cache.inventory_save_success();
        rows=db.query("SELECT [Count],Craftname FROM "+prefix+"CharItems WHERE CharID=810001 AND Bag=1 AND Slot=0; SELECT Slot,[Count],Craftname FROM "+prefix+"UserStoredItems WHERE UserUID=9001;");
        require(rows[0].rows[0]==storage::Row{storage::Cell{"7"},storage::Cell{"01020304050607080900"}} && rows[1].rows[0]==storage::Row{storage::Cell{"238"},storage::Cell{"10"},storage::Cell{"12'34"}},"Original dirty modification parameter order changed");
        // Bank-to-inventory forces insert and journals the old bank UID.
        require(cache.apply(packet(0x705,{238,0,0,0,0}))==InventoryReplicaResult::applied,"Cannot transfer bank to inventory");
        for(std::uint16_t i=0;i<81;++i) require(cache.apply(packet(0x702,{static_cast<std::uint8_t>(1+i/24),static_cast<std::uint8_t>(i%24),255}))==InventoryReplicaResult::applied,"Cannot journal deletion batch");
        require(cache.deleted_inventory().size()==81 && cache.deleted_bank().size()==1,"Delete journals changed");
        // A later failure must roll back earlier deletes and unrelated character phases.
        auto bad=item(0x7777000000000200ULL,5,22);bad.id=2147483000;require(cache.load(bad,false,1),"Cannot seed rejected item");
        db.begin();db.query("INSERT "+prefix+"UserStoredMoney(ServerID,UserUID,Money) VALUES(1,9001,123);");
        bool failed=false;try{store.save_in_transaction(cache,810001);}catch(const std::runtime_error&){failed=true;}require(failed && db.transaction_active(),"Failed phase lost caller-owned transaction");db.rollback();
        require(cache.deleted_inventory().size()==81 && cache.deleted_bank().size()==1 && cache.inventory()[0].save_type==1,"Failed phase cleared pending cache state");
        rows=db.query("SELECT COUNT(*) FROM "+prefix+"CharItems WHERE CharID=810001; SELECT COUNT(*) FROM "+prefix+"UserStoredItems WHERE UserUID=9001; SELECT COUNT(*) FROM "+prefix+"UserStoredMoney WHERE UserUID=9001;");
        require(rows[0].rows[0][0]==storage::Cell{"82"} && rows[1].rows[0][0]==storage::Cell{"1"} && rows[2].rows[0][0]==storage::Cell{"0"},"Character rollback retained partial inventory/bank/unrelated writes");
        require(cache.apply(packet(0x702,{5,22,255}))==InventoryReplicaResult::applied,"Cannot remove failed new item");
        db.begin();store.save_in_transaction(cache,810001);db.commit();cache.inventory_save_success();
        rows=db.query("SELECT ItemUID,Bag,Slot,[Count] FROM "+prefix+"CharItems WHERE CharID=810001 ORDER BY Bag,Slot; SELECT COUNT(*) FROM "+prefix+"UserStoredItems WHERE UserUID=9001;");
        require(rows[0].rows.size()==2 && rows[0].rows[0]==storage::Row{storage::Cell{"-2"},storage::Cell{"0"},storage::Cell{"0"},storage::Cell{"10"}} && rows[1].rows[0][0]==storage::Cell{"0"},"Retried batches/transfer did not commit together");
        // Failed add deletes the UID globally (ChkID=0), even under a different owner.
        InventoryReplica fallback(9001,77,pool);auto duplicate=item((UINT64_MAX-1),1,0);duplicate.count=19;require(fallback.load(duplicate,false,1),"Cannot seed duplicate inventory UID");
        auto duplicate_bank=item(123456789,0,239);duplicate_bank.count=23;require(fallback.load(duplicate_bank,true,1),"Cannot seed duplicate bank UID");
        db.query("UPDATE "+prefix+"CharItems SET CharID=810002 WHERE CharID=810001 AND ItemUID=-2; INSERT "+prefix+"UserStoredItems(ServerID,UserUID,ItemUID,Slot,ItemID,Type,TypeID,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Craftname,[Count],MakeTime,MakeType) VALUES(2,9002,123456789,0,5001,5,1,0,0,0,0,0,0,0,'',1,'20240101','U');");
        db.begin();store.save_in_transaction(fallback,810001);db.commit();fallback.inventory_save_success();
        rows=db.query("SELECT CharID,[Count] FROM "+prefix+"CharItems WHERE ItemUID=-2; SELECT ServerID,UserUID,Slot,[Count] FROM "+prefix+"UserStoredItems WHERE ItemUID=123456789;");
        require(rows[0].rows==std::vector<storage::Row>{{storage::Cell{"810001"},storage::Cell{"19"}}} && rows[1].rows==std::vector<storage::Row>{{storage::Cell{"1"},storage::Cell{"9001"},storage::Cell{"239"},storage::Cell{"23"}}},"Original global UID fallback/retry failed");
        require(fallback.apply(packet(0x618,{239,0}))==InventoryReplicaResult::applied,"Cannot set bank quantity to zero");
        db.begin();store.save_in_transaction(fallback,810001);db.commit();fallback.inventory_save_success();
        rows=db.query("SELECT [Count] FROM "+prefix+"UserStoredItems WHERE ItemUID=123456789;");
        require(rows[0].rows==std::vector<storage::Row>{{storage::Cell{"0"}}},"Zero-count bank status deleted its row");
        require(fallback.apply(packet(0x619,{239}))==InventoryReplicaResult::applied,"Cannot delete bank row by status");
        db.begin();store.save_in_transaction(fallback,810001);db.rollback();
        rows=db.query("SELECT COUNT(*) FROM "+prefix+"UserStoredItems WHERE ItemUID=123456789;");
        require(rows[0].rows[0][0]==storage::Cell{"1"} && fallback.deleted_bank()==std::vector<std::uint64_t>{123456789},"Rollback cleared bank status deletion journal");
        db.begin();store.save_in_transaction(fallback,810001);db.commit();fallback.inventory_save_success();
        rows=db.query("SELECT COUNT(*) FROM "+prefix+"UserStoredItems WHERE ItemUID=123456789;");
        require(rows[0].rows[0][0]==storage::Cell{"0"} && fallback.deleted_bank().empty(),"Retried bank status deletion did not persist");
        std::cout<<"Original inventory/bank SQL commit, dirty updates, batches, rollback and insert retry passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
