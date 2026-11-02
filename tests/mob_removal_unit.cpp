#include "eternity/game/zone_mobs.hpp"
#include "eternity/game/mob_pool.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <bit>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Forced mob removal invariant failed");}
struct Services final:ZoneMobServices,MobPoolServices,MobRemovalOwners {
    ZoneMobs* zone=nullptr;MobRuntime* subject=nullptr;unsigned phase=0,branch=0,ticks=0;bool removing=false;std::uint32_t now=0xfffffff0U;std::string events;
    std::unique_ptr<MobRuntime> construct() override {return std::make_unique<MobRuntime>();}
    std::uint32_t unix_time() override {return 1;}
    std::uint32_t tick_ms() override {check(phase==1);++ticks;events+='T';return now;}
    void send_cell_view(CellCoordinates cell,const eternity::net::Packet& packet) override {
        if(!removing) return;
        check(phase==0 && packet==mob_removed(subject->id));check(subject->zone==nullptr && subject->cell && subject->cell->x==cell.x && subject->cell->z==cell.z);
        check(zone->find(subject->id)==nullptr && zone->in_cell(cell).empty() && zone->size()==0);phase=1;events+='V';
    }
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected broadcast allocation");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua init");}
    void owner(unsigned expected) {check(phase==1 && branch==expected && !subject->cell && !subject->zone);phase=2;events+=" OoBb"[expected];}
    void destroy_obelisk(MobRuntime&,std::int32_t type) override {check(type==17);owner(1);}
    void remove_obelisk_member(MobRuntime&,void*) override {owner(2);}
    void destroy_linked_group(std::int32_t type) override {check(type==17);owner(3);}
    void remove_linked_member(MobRuntime&,void*) override {owner(4);}
    void release_compiled_ai(MobRuntime&) override {check(phase==(branch?2U:1U));phase=3;events+='P';}
    void release_stack_ai(MobRuntime&) override {throw std::runtime_error("Unexpected stack cleanup");}
    void close_lua(void*) override {throw std::runtime_error("Unexpected Lua cleanup");}
};
int main(int argc,char** argv) {try {
    check(argc==3);
    for(unsigned branch=0;branch<5;++branch) for(bool full:{false,true}) {
        Services services;std::uint32_t counter=0;MobPool pool(counter,services);MapDefinition map;map.size=160;ZoneMobs zone(map.geometry(),services);services.zone=&zone;services.branch=branch;
        MobRuntimeTemplate definition;definition.respawn_time=32;MobRespawn respawn;respawn.type=17;respawn.count=3;respawn.current=full?3:0;respawn.next=123;
        // All lower-priority flags are present to verify dispatch precedence.
        respawn.obelisk=branch==1;respawn.obelisk_source=branch==1 || branch==2?&respawn:nullptr;respawn.both=branch>=1 && branch<=3;respawn.both_source=branch?&respawn:nullptr;
        auto* mob=pool.allocate();services.subject=mob;mob->definition=&definition;mob->respawn=&respawn;mob->position={41,0,41};check(zone.enter(*mob));services.removing=true;
        check(!zone.remove_by_id(999,pool,services) && services.phase==0);
        check(zone.remove_by_id(mob->id,pool,services));check(services.phase==3 && pool.available()==1 && respawn.current==(full?2:-1));
        check(respawn.next==(full?16U:123U) && services.ticks==(full?1U:0U));check(!zone.remove_by_id(mob->id,pool,services));
    }
    {
        Services services;std::uint32_t counter=0;MobPool pool(counter,services);MapDefinition map;map.size=160;ZoneMobs zone(map.geometry(),services);services.zone=&zone;
        auto* mob=pool.allocate();services.subject=mob;mob->position={41,0,41};check(zone.enter(*mob));services.removing=true;
        check(zone.remove_by_id(mob->id,pool,services) && services.phase==3 && services.ticks==0 && pool.available()==1);
    }
    std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t current,count,old,delay,now,expected_current,expected_next,calls;
        row>>current>>count>>old>>delay>>now>>expected_current>>expected_next>>calls;check(static_cast<bool>(row));
        Services services;std::uint32_t counter=0;MobPool pool(counter,services);MapDefinition map;map.size=160;ZoneMobs zone(map.geometry(),services);services.zone=&zone;services.now=now;
        MobRuntimeTemplate definition;definition.respawn_time=delay;MobRespawn respawn;respawn.current=std::bit_cast<std::int32_t>(current);respawn.count=std::bit_cast<std::int32_t>(count);respawn.next=old;
        auto* mob=pool.allocate();services.subject=mob;mob->definition=&definition;mob->respawn=&respawn;mob->position={41,0,41};check(zone.enter(*mob));services.removing=true;
        check(zone.remove_by_id(mob->id,pool,services));check(static_cast<std::uint32_t>(respawn.current)==expected_current && respawn.next==expected_next && services.ticks==calls && services.phase==3 && pool.available()==1);++cases;
    }
    check(cases==360);
    std::ifstream full_input(argv[2]);check(full_input.good());unsigned complete_cases=0;
    while(std::getline(full_input,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);unsigned branch,found,full,returned;std::string events;std::uint32_t current,next;
        row>>branch>>found>>full>>returned>>events>>current>>next;check(static_cast<bool>(row));
        Services services;services.branch=branch>=1 && branch<=4?branch:0;std::uint32_t counter=76;MobPool pool(counter,services);MapDefinition map;map.size=160;ZoneMobs zone(map.geometry(),services);services.zone=&zone;
        MobRuntimeTemplate definition;definition.respawn_time=32;MobRespawn respawn;respawn.type=17;respawn.count=3;respawn.current=full?3:0;respawn.next=123;
        respawn.obelisk=branch==1;respawn.obelisk_source=branch==1 || branch==2?&respawn:nullptr;respawn.both=branch>=1 && branch<=3;respawn.both_source=branch>=1 && branch<=4?&respawn:nullptr;
        auto* mob=pool.allocate();services.subject=mob;mob->definition=&definition;mob->respawn=branch?&respawn:nullptr;mob->position={41,0,81};check(zone.enter(*mob));services.removing=true;
        check(zone.remove_by_id(found?77:999,pool,services)==(returned!=0));
        // Source FindMob/global/cell unlink are ports. Compare externally visible
        // send/clock/owner/recycle order; send observers assert actual native unlink.
        std::string visible;for(char event:events) if(event!='F' && event!='G' && event!='C') visible+=event;
        check(services.events==visible && static_cast<std::uint32_t>(respawn.current)==current && respawn.next==next);
        check(pool.available()==(found?1U:0U) && zone.size()==(found?0U:1U));++complete_cases;
    }
    check(complete_cases==24);
    std::cout<<"11 removal ordering/dispatch cases and 360 original accounting cases and 24 original coordinator cases passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
