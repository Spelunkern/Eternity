#include "eternity/game/vertical_persistence.hpp"
#include "eternity/game/vertical_entry_catalog_restore.hpp"
#include <stdexcept>
using namespace eternity::game;
static void require(bool v) {if(!v) throw std::runtime_error("vertical persistence check failed");}
int main() {
    PlayerSnapshot s;s.core.user_id=42;s.core.character.id=100;s.core.money=50;s.core.skill_points=8;
    s.catalogs["PointLog"]={};s.skills.push_back({7,3,1,0,0});s.quests.push_back({5,10,{1,2,3}});s.finished_quests.push_back({6,1});s.quick_slots.push_back({1,2,3,4});s.concern_market.push_back(99);
    PlayerItem item;item.uid=123;item.id=1000;item.bag=1;item.slot=2;s.items.push_back(item);
    InventoryReplicaPool pool;auto cache=vertical_persistence_restore(s,3,pool);
    require(cache->core().money==50 && cache->inventory().inventory().size()==1 && cache->skills().learned().size()==1 && cache->quests().active().size()==1 && cache->quests().finished().size()==1 && cache->growth().maximum()==3 && cache->market().ids().size()==1);
    require(cache->interface().load_point(2,20,1,2,3));cache->purchases().load_history({},123);const std::array<PlayerPointItem,1> point_rows{{{4,25001,2}}};cache->points().load_stored(point_rows);
    const auto entry_catalog=vertical_restore_entry_catalogs(*cache);require(entry_catalog.account_id()==42 && entry_catalog.save_points()[2].map==20 && entry_catalog.save_points()[2].x==1 && entry_catalog.cash_points()==123 && entry_catalog.stored()[4].type==25 && entry_catalog.stored()[4].type_id==1 && entry_catalog.stored()[4].count==2);
    eternity::net::Packet money{3,6,42,0,0,0,77,0,0,0};require(cache->apply(money)==InventoryReplicaResult::applied && cache->core().money==77);
    s.items[0].relocated=true;auto relocated=vertical_persistence_restore(s,3,pool);require(relocated->inventory().inventory()[0].save_type==2);
    PlayerSnapshot ordered=s;ordered.items.clear();auto moved=item;moved.uid=200;moved.slot=0;moved.relocated=true;moved.source_order=1;auto direct=item;direct.uid=201;direct.slot=3;direct.source_order=0;ordered.items={moved,direct};
    unsigned allocations=0;InventoryReplicaPool reused([&]{auto value=std::make_unique<CachedInventoryItem>();value->save_type=++allocations==2?1:0;value->saved_type=77;value->craft_storage.fill(0x7a);return value;});
    auto restored=vertical_persistence_restore(ordered,3,reused);auto rows=restored->inventory().inventory();require(rows.size()==2 && rows[0].item.uid==200 && rows[0].save_type==1 && rows[0].saved_type==77 && rows[0].craft_storage[1]==0x7a && rows[0].craft_storage[20]==0 && rows[1].item.uid==201 && rows[1].save_type==0 && rows[1].saved_type==77);
    PlayerSnapshot bank_snapshot;bank_snapshot.core=s.core;bank_snapshot.catalogs["PointLog"]={};auto bank_item=item;bank_item.slot=5;bank_item.relocated=true;bank_snapshot.bank.push_back(bank_item);
    InventoryReplicaPool bank_pool;auto bank_cache=vertical_persistence_restore(bank_snapshot,3,bank_pool);require(bank_cache->inventory().bank().size()==1 && bank_cache->inventory().bank()[0].save_type==2);
    PlayerSnapshot full;full.core=s.core;full.catalogs["PointLog"]={};auto overflow=item;overflow.source_order=0;full.overflow_items.push_back(overflow);auto later=item;later.source_order=1;full.items.push_back(later);
    unsigned count=0;InventoryReplicaPool overflow_pool([&]{auto value=std::make_unique<CachedInventoryItem>();value->save_type=++count==1?1:0;return value;});
    auto full_cache=vertical_persistence_restore(full,3,overflow_pool);require(count==2 && full_cache->inventory().inventory().size()==1 && overflow_pool.statistics().available==1);
    auto recycled=overflow_pool.allocate();require(recycled && recycled->save_type==1 && recycled->item.uid==overflow.uid);overflow_pool.release(std::move(recycled));
    PlayerSnapshot raw;raw.core=s.core;raw.catalogs["PointLog"]={};raw.catalogs["Bank"]={};
    const auto raw_item=[](int uid){return eternity::storage::Row{"5001","5","1",std::to_string(uid),"1","0","100","0","0","0","0","0","0","","1","N","2024","1","1","0","0","0"};};
    raw.catalogs["Items"].rows={raw_item(301),raw_item(302),raw_item(303)};raw.catalogs["Skills"]={};decode_player_inventory(raw);
    unsigned attempt=0;InventoryReplicaPool failures([&]()->std::unique_ptr<CachedInventoryItem>{if(++attempt==1)return {};return std::make_unique<CachedInventoryItem>();});
    auto consistent=vertical_persistence_restore(raw,3,failures);const auto actual=consistent->inventory().inventory();
    require(attempt==3 && actual.size()==2 && actual[0].item.uid==302 && actual[0].item.bag==1 && actual[0].item.slot==0 && actual[0].save_type==0 && actual[1].item.uid==303 && actual[1].item.slot==1 && actual[1].save_type==2);
    require(raw.items.size()==2 && raw.items[0].uid==302 && raw.items[1].uid==303 && raw.items[1].slot==1 && raw.overflow_items.empty());
    PlayerSnapshot failed_duplicate;failed_duplicate.core=s.core;failed_duplicate.catalogs["PointLog"]={};failed_duplicate.catalogs["Items"].rows={raw_item(501),raw_item(502)};failed_duplicate.catalogs["Bank"]={};failed_duplicate.catalogs["Skills"]={};decode_player_inventory(failed_duplicate);
    unsigned attempt2=0;InventoryReplicaPool failure2([&]()->std::unique_ptr<CachedInventoryItem>{if(++attempt2==2)return {};return std::make_unique<CachedInventoryItem>();});
    auto skipped=vertical_persistence_restore(failed_duplicate,3,failure2);require(skipped->inventory().inventory().size()==1 && skipped->inventory().inventory()[0].item.uid==501 && failed_duplicate.items.size()==1 && failed_duplicate.items[0].uid==501);
    PlayerSnapshot raw_bank;raw_bank.core=s.core;raw_bank.catalogs["PointLog"]={};raw_bank.catalogs["Items"]={};raw_bank.catalogs["Skills"]={};
    for(int uid:{401,402,403}) {auto row=raw_item(uid);row.erase(row.begin()+4);row[4]="3";raw_bank.catalogs["Bank"].rows.push_back(row);}decode_player_inventory(raw_bank);
    unsigned bank_attempt=0;InventoryReplicaPool bank_failure([&]()->std::unique_ptr<CachedInventoryItem>{if(++bank_attempt==1)return {};return std::make_unique<CachedInventoryItem>();});
    auto bank_consistent=vertical_persistence_restore(raw_bank,3,bank_failure);auto bank_rows=bank_consistent->inventory().bank();
    require(bank_rows.size()==2 && bank_rows[0].item.uid==403 && bank_rows[0].item.slot==0 && bank_rows[0].save_type==2 && bank_rows[1].item.uid==402 && bank_rows[1].item.slot==3 && bank_rows[1].save_type==0 && raw_bank.bank.size()==2 && raw_bank.bank[0].uid==403);

}
