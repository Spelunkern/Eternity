#include "eternity/game/vertical_persistence.hpp"
#include <stdexcept>
using namespace eternity::game;
static void require(bool v) {if(!v) throw std::runtime_error("vertical persistence check failed");}
int main() {
    PlayerSnapshot s;s.core.user_id=42;s.core.character.id=100;s.core.money=50;s.core.skill_points=8;
    s.catalogs["PointLog"]={};s.skills.push_back({7,3,1,0,0});s.quests.push_back({5,10,{1,2,3}});s.finished_quests.push_back({6,1});s.quick_slots.push_back({1,2,3,4});s.concern_market.push_back(99);
    PlayerItem item;item.uid=123;item.id=1000;item.bag=1;item.slot=2;s.items.push_back(item);
    InventoryReplicaPool pool;auto cache=vertical_persistence_restore(s,3,pool);
    require(cache->core().money==50 && cache->inventory().inventory().size()==1 && cache->skills().learned().size()==1 && cache->quests().active().size()==1 && cache->quests().finished().size()==1 && cache->growth().maximum()==3 && cache->market().ids().size()==1);
    eternity::net::Packet money{3,6,42,0,0,0,77,0,0,0};require(cache->apply(money)==InventoryReplicaResult::applied && cache->core().money==77);
    bool phases=false,ack=false,timeout=false;CharacterSavePort port;port.growth=[](){return 0;};port.acquire=port.begin=port.commit=port.rollback=[](){return true;};port.timeout=[](){return false;};port.lock=port.unlock=port.recovery=port.release=[](){};port.acknowledge=[&](){ack=true;};port.phases=[&](bool quick){require(quick && cache->core().money==77);phases=true;};require(execute_character_save(true,true,false,timeout,port) && phases && ack);
    s.items[0].relocated=true;bool rejected=false;try {(void)vertical_persistence_restore(s,3,pool);} catch(const std::runtime_error&) {rejected=true;}require(rejected);
}
