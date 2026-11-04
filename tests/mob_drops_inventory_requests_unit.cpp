#include "eternity/game/quest_requests.hpp"
#include "mob_drops_inventory_support.hpp"
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
std::vector<std::uint8_t> unhex(const std::string& s) {
    std::vector<std::uint8_t> result;
    if(s=="-") return result;
    for(std::size_t i=0;i<s.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return result;
}
struct World final:QuestRequestWorld {
    InventoryReplicaPool replica_pool;
    InventoryReplica replica{0x12345678,0,replica_pool};
    GroundItemPool item_pool;
    std::vector<std::array<QuestRequestItem,24>> projected;
    inventory_test::Scene scene;ItemDefinitions definitions;EnchantValues enchant;MobAttackTemplates mobs;
    ItemDefinition item_definition;
    Actor actor{9001,{}, {},{},scene,{definitions,enchant,mobs}};
    std::vector<std::uint8_t> events;
    std::vector<std::uint64_t> released;
    const QuestDefinition* def=nullptr;
    RestoredQuest quest;
    bool active=false,found=false,completed=false;
    QuestRequestNpc npc;
    void number(std::uint32_t n) {for(unsigned shift=0;shift<32;shift+=8) events.push_back(static_cast<std::uint8_t>(n>>shift));}
    void load(std::span<const std::uint8_t> bytes,unsigned bags) {
        projected.resize(bags);actor.account_id()=0x12345678;
        check(bytes.size()==bags*24*5,"invalid inventory fixture");
        for(std::size_t i=0;i<bytes.size()/5;++i) if(bytes[5*i]) {
            auto& p=projected[i/24][i%24];p={true,bytes[5*i+1],bytes[5*i+2],bytes[5*i+3],bytes[5*i+4]};
            auto* item=item_pool.allocate();check(item!=nullptr,"pool allocation failed");item->definition=&item_definition;
            item->item.uid=1000+i;item->item.bag=static_cast<std::uint8_t>(i/24+1);item->item.slot=static_cast<std::uint16_t>(i%24);
            item->item.type=p.type;item->item.type_id=p.id;item->item.count=p.count;
            check(actor.claim_inventory_item(item->item.bag,item->item.slot,*item,item_pool),"Actor raw claim failed");
            check(replica.load(item->item),"replica load failed");
        }
        PlayerItem equipment;equipment.uid=999;equipment.bag=0;equipment.slot=0;equipment.type=1;equipment.type_id=1;equipment.count=13;
        auto* equipped=item_pool.allocate();equipped->definition=&item_definition;equipped->item=equipment;check(actor.claim_inventory_item(0,0,*equipped,item_pool),"Equipment sentinel claim failed");
        check(replica.load(equipment),"equipment load failed");
        if(bags<5) {auto locked=equipment;locked.uid=998;locked.bag=static_cast<std::uint8_t>(bags+1);locked.count=17;auto* held=item_pool.allocate();held->definition=&item_definition;held->item=locked;check(actor.claim_inventory_item(locked.bag,locked.slot,*held,item_pool),"Locked sentinel claim failed");check(replica.load(locked),"locked bag load failed");}
    }
    const std::array<std::uint8_t,464>* definition(std::uint16_t) override {return &def->record();}
    RestoredQuest* find_quest(std::uint16_t id) override {events.push_back('A');number(id);return active?&quest:nullptr;}
    std::optional<QuestRequestNpc> find_npc(std::uint32_t id) override {events.push_back('N');number(id);return found?std::optional{npc}:std::nullopt;}
    QuestRequestInventory inventory() override {return {0x12345678,projected};}
    void send_database(const eternity::net::Packet& packet) override {
        check(packet.size()==9 && packet[0]==2 && packet[1]==7,"unexpected database command");
        const auto b=static_cast<std::size_t>(packet[6]-1),s=static_cast<std::size_t>(packet[7]);
        const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(b+1),static_cast<std::uint16_t>(s));
        check(b<projected.size() && s<24 && held && held->pool==&item_pool,"missing Actor live item during DB send");
        // The live owner sees partial changes before either network callback;
        // whole-stack removal retains its old count until ownership release.
        check(actor.set_inventory_item_count(static_cast<std::uint8_t>(b+1),static_cast<std::uint16_t>(s),projected[b][s].count,held->item),"Actor count sync failed");
        const auto mirror=std::find_if(actor.inventory().begin(),actor.inventory().end(),[&](const auto& value){return value.bag==b+1 && value.slot==s;});
        check(mirror!=actor.inventory().end() && mirror->count==held->item->item.count,"Mirror was not synchronized before DB callback");
        check(replica.apply(packet)==InventoryReplicaResult::applied,"DB replica rejected quest decrement");
        events.push_back('B');events.insert(events.end(),packet.begin(),packet.end());
    }
    void send_self(const eternity::net::Packet& packet) override {
        check(packet.size()==7 && packet[0]==6 && packet[1]==2,"unexpected client command");
        const auto cached=replica.inventory();
        const auto found_item=std::find_if(cached.begin(),cached.end(),[&](const auto& p){return p.item.bag==packet[2] && p.item.slot==packet[3];});
        check(packet[6] ? found_item!=cached.end() && found_item->item.count==packet[6] : found_item==cached.end(),"client notice preceded DB mutation");
        actor.send_self(packet);events.push_back('C');events.insert(events.end(),packet.begin(),packet.end());
    }
    void release_item(std::uint8_t bag,std::uint8_t slot) override {
        const auto held=actor.retained_inventory_item(bag,slot);check(held.has_value(),"duplicate Actor release");auto* item=held->item;
        events.insert(events.end(),{'F',bag,slot,item->item.count});released.push_back(item->item.uid);
        check(actor.release_inventory_item(bag,slot,item),"Actor direct release failed");
        check(actor.inventory_slot_empty(bag,slot),"Actor retained a released slot");
    }
    void complete_quest(RestoredQuest& q,std::uint32_t request_npc,bool success) override {
        check(&q==&quest && success,"invalid completion provider arguments");completed=true;
        verify_live();
        events.push_back('E');number(request_npc);number(success?1:0);
        events.insert(events.end(),q.packet.counters.begin(),q.packet.counters.end());
    }
    void verify_live() {
        const auto cached=replica.inventory();unsigned expected_count=projected.size()<5?2U:1U;
        for(std::size_t b=0;b<projected.size();++b) for(std::size_t s=0;s<24;++s) {
            const auto& p=projected[b][s];const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(b+1),static_cast<std::uint16_t>(s));const auto* live=held?held->item:nullptr;
            const auto saved=std::find_if(cached.begin(),cached.end(),[&](const auto& v){return v.item.bag==b+1 && v.item.slot==s;});
            check(p.present==(live!=nullptr) && p.present==(saved!=cached.end()),"ownership/replica presence mismatch");
            if(p.present) {++expected_count;check(live->item.count==p.count && saved->item.count==p.count,"ownership/replica count mismatch");check(saved->item.uid==live->item.uid,"UID changed during consumption");}
        }
        check(cached.size()==expected_count && actor.inventory().size()==expected_count,"unrelated Actor inventory changed");
        for(const auto& mirror:actor.inventory()) {const auto held=actor.retained_inventory_item(mirror.bag,mirror.slot);check(held && held->item->item.uid==mirror.uid && held->item->item.count==mirror.count,"Actor pointer/mirror mismatch");}
        for(const auto& value:cached) if(value.item.uid==999 || value.item.uid==998) check(value.item.count==(value.item.uid==999?13:17),"equipment/locked bag changed");
        check(replica.deleted_inventory()==released,"deletion journal/release order mismatch");
        check(item_pool.statistics().available==released.size(),"pool release count mismatch");
    }
};
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"fixture required");std::ifstream input(argv[1]);check(input.good(),"cannot read fixture");
        unsigned rows=0,completed=0,releases=0,mutations=0;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned scenario,id,requested,active,found,nt,ni,bags;std::string block,initial,counters,final,final_counters,events;
            row>>scenario>>id>>requested>>active>>found>>nt>>ni>>bags>>block>>initial>>counters>>final>>final_counters>>events;check(!row.fail(),"malformed fixture");
            const auto definitions=QuestDefinitions::decode(unhex(block));World world;world.def=definitions.find(1);check(world.def!=nullptr,"missing quest definition");
            world.quest.definition=world.def;world.active=active!=0;world.found=found!=0;world.npc={static_cast<std::int32_t>(nt),static_cast<std::int32_t>(ni)};
            const auto bytes=unhex(counters);std::copy(bytes.begin(),bytes.end(),world.quest.packet.counters.begin());world.load(unhex(initial),bags);
            quest_end_request(static_cast<std::uint16_t>(id),requested,1,world);
            std::vector<std::uint8_t> actual;
            for(const auto& bag:world.projected) for(const auto& item:bag) actual.insert(actual.end(),{static_cast<std::uint8_t>(item.present),item.type,item.id,item.count,static_cast<std::uint8_t>(item.effect)});
            if(actual!=unhex(final) || std::vector<std::uint8_t>(world.quest.packet.counters.begin(),world.quest.packet.counters.end())!=unhex(final_counters) || world.events!=unhex(events)) {
                std::cerr<<"flow scenario "<<scenario<<" inventory "<<(actual==unhex(final))<<" counters "<<(std::vector<std::uint8_t>(world.quest.packet.counters.begin(),world.quest.packet.counters.end())==unhex(final_counters))<<" events "<<(world.events==unhex(events))<<'\n';throw std::runtime_error("original flow mismatch");
            }
            world.verify_live();completed+=world.completed?1:0;releases+=static_cast<unsigned>(world.released.size());mutations+=world.replica.deleted_inventory().empty()?0:1;++rows;
        }
        check(rows==512 && completed>50 && releases>100 && mutations>20,"insufficient flow coverage");
        std::cout<<rows<<" original request/item flows; "<<completed<<" completions; "<<releases<<" actual Actor direct releases; pool/mirror/DB replica/journals matched\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
