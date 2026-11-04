#include "mob_drops_inventory_support.hpp"
#include <iostream>
#include <bit>
#include "eternity/storage/sql_server.hpp"
using namespace eternity::game;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char** argv) {try {
    check(argc==1 || (argc==2 && std::string(argv[1])=="--sql"),"Expected optional --sql");PlayerItem persisted;
    GroundItemPool pool;ItemDefinitions definitions;EnchantValues enchant;MobAttackTemplates mobs;ItemDefinition definition;definition.id=1001;
    inventory_test::Scene scene;auto* first=pool.allocate();auto* other=pool.allocate();check(first && other,"Allocation failed");first->definition=other->definition=&definition;
    first->item.uid=0xfedcba9876543210ULL;first->item.count=255;first->craft_storage.fill(0xa5);first->craft_storage[0]=0;
    PlayerItem legacy;legacy.bag=0;legacy.slot=0;legacy.count=17;
    {
        Actor actor(42,{}, {},{},scene,{definitions,enchant,mobs},{legacy});
        check(!actor.inventory_slot_empty(0,0) && !actor.retained_inventory_item(0,0),"Value-only compatibility row lost");
        check(!actor.set_inventory_item_count(0,0,1) && !actor.sync_inventory_item(0,0) && !actor.release_inventory_item(0,0) && actor.inventory()[0].count==17,"Unretained row fabricated pooled ownership");
        const auto before=pool.statistics();check(actor.claim_inventory_item(1,0,*first,pool),"Raw claim failed");
        check(pool.statistics().fresh_attempts==before.fresh_attempts && pool.statistics().available==before.available,"Claim reallocated or released");
        const auto borrowed=actor.retained_inventory_item(1,0);check(borrowed && borrowed->item==first && borrowed->pool==&pool,"Borrowed identity/pool changed");
        check(!actor.claim_inventory_item(1,0,*other,pool) && !actor.claim_inventory_item(1,1,*first,pool),"Occupied slot or duplicate pointer accepted");
        check(!actor.set_inventory_item_count(1,0,1,other) && !actor.release_inventory_item(1,0,other),"Stale pointer changed another allocation");
        // A nested serialized callback can append ownership and reallocate the
        // host vectors without invalidating the borrowed pooled object.
        const auto callback=[&] {
            check(actor.claim_inventory_item(1,1,*other,pool),"Nested callback claim failed");
            check(borrowed->item==actor.retained_inventory_item(1,0)->item,"Borrow referenced movable vector storage");
            check(actor.release_inventory_item(1,1,other),"Nested callback direct release failed");
        };callback();
        actor.exchange().mode=2;actor.exchange().items[0]={1,0,3};
        check(actor.set_inventory_item_count(1,0,3,first) && first->item.count==3 && actor.inventory().back().count==3,"Count mirror diverged");
        set_ground_item_craft(*first,"00000000000000000000");first->item.quality=65535;first->item.gems[0]=7;
        check(actor.inventory().back().craft.empty(),"Pointer publication applied future craft");
        check(actor.sync_inventory_item(1,0,first) && actor.inventory().back().craft==first->item.craft && actor.inventory().back().quality==65535 && actor.inventory().back().gems[0]==7,"DB-boundary payload mirror incomplete");
        persisted=actor.inventory().back();
        check(actor.release_inventory_item(1,0,first) && first->item.count==3 && actor.inventory_slot_empty(1,0) && !actor.retained_inventory_item(1,0),"Direct release copied mirror or kept ownership");
        check(!actor.release_inventory_item(1,0),"Duplicate direct release accepted");
        check(actor.exchange().mode==2 && actor.exchange().items[0].bag==1 && actor.exchange().items[0].slot==0 && actor.exchange().items[0].count==3,"Direct release performed ItemRemove exchange cleanup");
        auto* reused=pool.allocate();check(reused==first && reused->item.uid==0xfedcba9876543210ULL && reused->item.count==0,"Actual LIFO/reset contract changed");
        check(actor.claim_inventory_item(2,23,*reused,pool),"Reused item claim failed");
        check(pool.allocate()==other,"Nested release did not retain actual pool LIFO");bool invalid=false;try {actor.claim_inventory_item(10,0,*other,pool);}catch(const std::out_of_range&) {invalid=true;}check(invalid,"Invalid position accepted");
        other->cell=CellCoordinates{0,0};invalid=false;try {actor.claim_inventory_item(1,1,*other,pool);}catch(const std::invalid_argument&) {invalid=true;}other->cell.reset();check(invalid,"Linked allocation accepted");
        check(scene.packets.empty(),"Ownership primitive emitted a packet");
    }
    check(pool.statistics().available==1,"Actor teardown did not release exactly its claimed allocation");pool.release(other);check(pool.statistics().available==2,"Unclaimed allocation ownership was stolen");
    if(argc==2) {
        eternity::storage::SqlServer database(eternity::storage::SqlServerConfig::from_environment());
        const auto uid=std::bit_cast<std::int64_t>(persisted.uid);
        const std::array<eternity::storage::Parameter,4> parameters{uid,static_cast<std::int64_t>(persisted.count),static_cast<std::int64_t>(std::bit_cast<std::int16_t>(persisted.quality)),persisted.craft};
        const auto rows=database.query("SELECT CAST(? AS bigint),CAST(? AS tinyint),CAST(? AS smallint),CAST(? AS varchar(20));",parameters);
        check(rows.size()==1 && rows[0].rows.size()==1 && rows[0].rows[0]==eternity::storage::Row{std::to_string(uid),"3","-1",std::string(20,'0')},"Retained mirror SQL UID/count/quality/craft projection differs");
        std::cout<<"Read-only isolated SQL retained-mirror signed UID/WORD quality/count/craft round trip passed\n";
    }
    std::cout<<"Actual Actor claim/borrow/count/full-mirror/direct-release guards, reuse and teardown passed without transport effects\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
