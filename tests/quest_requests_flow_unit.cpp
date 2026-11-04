#include "eternity/game/quest_requests.hpp"
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
    std::vector<std::array<GroundItem*,24>> owned;
    std::vector<std::uint8_t> events;
    std::vector<std::uint64_t> released;
    const QuestDefinition* def=nullptr;
    RestoredQuest quest;
    bool active=false,found=false,completed=false;
    QuestRequestNpc npc;
    void number(std::uint32_t n) {for(unsigned shift=0;shift<32;shift+=8) events.push_back(static_cast<std::uint8_t>(n>>shift));}
    void load(std::span<const std::uint8_t> bytes,unsigned bags) {
        projected.resize(bags);owned.resize(bags);
        check(bytes.size()==bags*24*5,"invalid inventory fixture");
        for(std::size_t i=0;i<bytes.size()/5;++i) if(bytes[5*i]) {
            auto& p=projected[i/24][i%24];p={true,bytes[5*i+1],bytes[5*i+2],bytes[5*i+3],bytes[5*i+4]};
            auto* item=item_pool.allocate();check(item!=nullptr,"pool allocation failed");owned[i/24][i%24]=item;
            item->item.uid=1000+i;item->item.bag=static_cast<std::uint8_t>(i/24+1);item->item.slot=static_cast<std::uint16_t>(i%24);
            item->item.type=p.type;item->item.type_id=p.id;item->item.count=p.count;
            check(replica.load(item->item),"replica load failed");
        }
        PlayerItem equipment;equipment.uid=999;equipment.bag=0;equipment.slot=0;equipment.type=1;equipment.type_id=1;equipment.count=13;
        check(replica.load(equipment),"equipment load failed");
        if(bags<5) {auto locked=equipment;locked.uid=998;locked.bag=static_cast<std::uint8_t>(bags+1);locked.count=17;check(replica.load(locked),"locked bag load failed");}
    }
    const std::array<std::uint8_t,464>* definition(std::uint16_t) override {return &def->record();}
    RestoredQuest* find_quest(std::uint16_t id) override {events.push_back('A');number(id);return active?&quest:nullptr;}
    std::optional<QuestRequestNpc> find_npc(std::uint32_t id) override {events.push_back('N');number(id);return found?std::optional{npc}:std::nullopt;}
    QuestRequestInventory inventory() override {return {0x12345678,projected};}
    void send_database(const eternity::net::Packet& packet) override {
        check(packet.size()==9 && packet[0]==2 && packet[1]==7,"unexpected database command");
        const auto b=static_cast<std::size_t>(packet[6]-1),s=static_cast<std::size_t>(packet[7]);
        check(b<owned.size() && s<24 && owned[b][s],"missing live item during DB send");
        // The live owner sees partial changes before either network callback;
        // whole-stack removal retains its old count until ownership release.
        owned[b][s]->item.count=projected[b][s].count;
        check(replica.apply(packet)==InventoryReplicaResult::applied,"DB replica rejected quest decrement");
        events.push_back('B');events.insert(events.end(),packet.begin(),packet.end());
    }
    void send_self(const eternity::net::Packet& packet) override {
        check(packet.size()==7 && packet[0]==6 && packet[1]==2,"unexpected client command");
        const auto cached=replica.inventory();
        const auto found_item=std::find_if(cached.begin(),cached.end(),[&](const auto& p){return p.item.bag==packet[2] && p.item.slot==packet[3];});
        check(packet[6] ? found_item!=cached.end() && found_item->item.count==packet[6] : found_item==cached.end(),"client notice preceded DB mutation");
        events.push_back('C');events.insert(events.end(),packet.begin(),packet.end());
    }
    void release_item(std::uint8_t bag,std::uint8_t slot) override {
        auto*& item=owned.at(bag-1).at(slot);check(item!=nullptr,"duplicate release");
        events.insert(events.end(),{'F',bag,slot,item->item.count});released.push_back(item->item.uid);
        item_pool.release(item);item=nullptr;
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
            const auto& p=projected[b][s];const auto* live=owned[b][s];
            const auto saved=std::find_if(cached.begin(),cached.end(),[&](const auto& v){return v.item.bag==b+1 && v.item.slot==s;});
            check(p.present==(live!=nullptr) && p.present==(saved!=cached.end()),"ownership/replica presence mismatch");
            if(p.present) {++expected_count;check(live->item.count==p.count && saved->item.count==p.count,"ownership/replica count mismatch");check(saved->item.uid==live->item.uid,"UID changed during consumption");}
        }
        check(cached.size()==expected_count,"unrelated inventory changed");
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
        std::cout<<rows<<" original request/item flows; "<<completed<<" completions; "<<releases<<" real pool releases; DB replica/journals matched\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
