#include "mob_damage_fixture.hpp"
#include "eternity/game/linked_mobs.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
void check(bool value) {if(!value) throw std::runtime_error("Zone corpse cleanup invariant failed");}
struct Services final:ZoneMobServices,MobPoolServices,RandomSource {
    MobDamageFixture damage_fixture;MobDamagePool& damage_pool() override {return damage_fixture.pool;}

    ZoneMobs* zone=nullptr;bool cleanup=false;std::vector<std::uint32_t> sent,released;
    std::unique_ptr<MobRuntime> construct() override {return std::make_unique<MobRuntime>();}
    std::uint32_t unix_time() override {return 1;}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected cleanup clock");}
    std::int32_t next() override {throw std::runtime_error("Unexpected cleanup random");}
    void send_cell_view(CellCoordinates cell,const eternity::net::Packet& packet) override {
        if(!cleanup) return;check(packet.size()==6 && packet[0]==2 && packet[1]==6);std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=static_cast<std::uint32_t>(packet[2+i])<<(8*i);
        auto* mob=zone->find(id);check(mob && mob->zone==zone && mob->cell && mob->cell->x==cell.x && mob->cell->z==cell.z);
        const auto members=zone->in_cell(cell);check(std::find(members.begin(),members.end(),mob)!=members.end());sent.push_back(id);
    }
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected cleanup broadcast allocation");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected cleanup Lua init");}
    void release_compiled_ai(MobRuntime& mob) override {check(!mob.zone && !mob.cell && zone->find(mob.id)==nullptr);check(sent.back()==mob.id);released.push_back(mob.id);}
    void release_stack_ai(MobRuntime&) override {}
    void close_lua(void*) override {throw std::runtime_error("Unexpected Lua close");}
};
int main() {try {
    Services services;std::uint32_t counter=0;MobPool pool(counter,services);MapDefinition map;map.size=160;ZoneMobs zone(map.geometry(),services);services.zone=&zone;
    LinkedMobGroup group;group.current_index=1;LinkedMobPopulation linked(std::span<const LinkedMobGroup>(&group,1));MobRuntimeTemplate definition;definition.respawn_time=180000;definition.respawn_selector=3;
    MobRespawn regular;regular.count=regular.current=2;MobRespawn obelisk;obelisk.obelisk=true;obelisk.current=1;obelisk.next=UINT32_MAX;MobRespawn principal;principal.both=true;principal.current=0;principal.next=UINT32_MAX;
    std::vector<MobRuntime*> actors;
    for(unsigned i=0;i<8;++i) {auto* mob=pool.allocate();mob->definition=&definition;mob->position={41,0,81};mob->status=2;mob->death_time=100;check(zone.enter(*mob));actors.push_back(mob);}
    actors[0]->respawn=&regular;actors[1]->respawn=&regular;actors[2]->respawn=&obelisk;actors[3]->respawn=&principal;
    actors[4]->status=1;actors[5]->state_stack=std::vector<std::int32_t>{};actors[6]->state_stack=std::vector<std::int32_t>{1,2};actors[7]->death_time=200;
    services.cleanup=true;zone.update_deaths(pool,services,100,linked);check(zone.size()==8 && services.sent.empty());
    zone.update_deaths(pool,services,101,linked);check(services.sent==std::vector<std::uint32_t>{1,2,3,4,7} && services.released==services.sent && zone.size()==3 && pool.available()==5);
    check(regular.current==0 && regular.next==180101 && obelisk.current==1 && obelisk.next==180101 && principal.current==0 && principal.next==180101);
    zone.update_deaths(pool,services,200,linked);check(zone.size()==3);zone.update_deaths(pool,services,201,linked);check(zone.size()==2 && services.sent.back()==8 && pool.available()==6);
    check(zone.find(5)==actors[4] && zone.find(6)==actors[5]);
    std::cout<<"Zone corpse eligibility, FIFO traversal, pre-unlink delivery, timer dispatch and recycling passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
