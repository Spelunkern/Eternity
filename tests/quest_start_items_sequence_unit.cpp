#include "eternity/game/quest_start_items.hpp"
#include "eternity/game/quest_start.hpp"
#include "eternity/game/quest_start_log.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/inventory_replica.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
net::Packet decode(const std::string& s) {net::Packet p;if(s=="-") return p;for(std::size_t i=0;i<s.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
std::vector<std::uint64_t> numbers(const std::string& s) {std::vector<std::uint64_t> r;if(s=="-") return r;std::istringstream row(s);for(std::string v;std::getline(row,v,',');) r.push_back(std::stoull(v));return r;}
void append(net::Packet& p,std::uint64_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
struct World final:QuestStartWorld,QuestStartItemsWorld {
    std::uint32_t billing=0;std::int32_t bags=0,new_bags=-99;unsigned mask=0,lookup=0,attempts=0,phase=0,db_count=0;bool allow=false;
    GroundItem blocker;std::array<GroundItem*,48> grid{};std::array<ItemDefinition,3> definitions{};std::vector<GroundItem*> stored;std::vector<std::uint64_t> uids;std::size_t uid_cursor=0;net::Packet events;
    GroundItemPool pool{1,[this]()->std::unique_ptr<GroundItem> {if(!allow) return {};auto item=std::make_unique<GroundItem>();item->item.uid=0xface;item->craft_storage.fill(0xa5);return item;}};
    InventoryReplicaPool cache_pool;std::unique_ptr<InventoryReplica> cache;QuestStartItemsUser user{billing,bags};
    void event(char c,std::initializer_list<std::uint32_t> v={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto x:v) append(events,x);}
    const ItemDefinition* find_definition(std::int32_t type,std::int32_t id) override {event('D',{static_cast<std::uint32_t>(type),static_cast<std::uint32_t>(id)});return type==5 && id>=1 && id<=3 && (lookup&(1u<<(id-1)))?&definitions[static_cast<std::size_t>(id-1)]:nullptr;}
    bool slot_empty(std::int32_t bag,std::int32_t slot) override {event('K',{static_cast<std::uint32_t>(bag),static_cast<std::uint32_t>(slot)});return grid.at(static_cast<std::size_t>((bag-1)*24+slot))==nullptr;}
    GroundItem* create_item() override {event('O');allow=(mask&(1u<<attempts++))!=0;return pool.allocate();}
    std::uint32_t local_make_time() override {event('M');return quest_start_log_time(2026,11,3,15,30,40);}
    std::uint64_t next_uid() override {event('U');check(uid_cursor<uids.size(),"Unexpected UID request");return uids[uid_cursor++];}
    void store(std::int32_t bag,std::int32_t slot,GroundItem& item) override {auto& cell=grid.at(static_cast<std::size_t>((bag-1)*24+slot));check(!cell,"Occupied slot overwritten");check(item.item.craft.empty(),"Premature default craft");cell=&item;stored.push_back(&item);event('S',{static_cast<std::uint32_t>(bag),static_cast<std::uint32_t>(slot)});}
    void send_database(const net::Packet& p) override {
        event('B');events.insert(events.end(),p.begin(),p.end());check(cache->apply(p)==InventoryReplicaResult::applied,"Persistent cache rejected grant");check(cache->inventory().size()==stored.size(),"Persistent cache lost earlier grant");
        if(++db_count==1 && new_bags!=-99) bags=new_bags;
    }
    void send_self(const net::Packet& p) override {event('C');events.insert(events.end(),p.begin(),p.end());}
    // Existing quest_start runs the real start-item loop; its already-verified
    // prelude is asserted separately and excluded from this item-subflow trace.
    void prepare_start_log(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {check(phase++==0,"Prelude capture order");}
    void send_self(const std::array<std::uint8_t,8>&) override {check(phase++==1,"Prelude client order");}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {check(phase++==2,"Prelude log order");}
    bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {check(phase==3,"Grant before prelude");return quest_start_items_grant(type,id,count,user,*this);}
    std::optional<QuestStartNpc> find_npc(std::uint32_t) override {throw std::runtime_error("Unexpected lookup");}
    bool active(std::uint16_t) override {throw std::runtime_error("Unexpected active check");}
    bool finished(std::uint16_t) override {throw std::runtime_error("Unexpected finished check");}
    bool find_item(std::uint8_t,std::uint8_t,bool) override {throw std::runtime_error("Unexpected requirement");}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected quest clock");}
    bool add(const QuestStartRecord&) override {throw std::runtime_error("Unexpected quest allocation");}
    net::Packet snapshot() const {
        net::Packet p;append(p,stored.size());for(const auto* i:stored) {append(p,i->item.bag,1);append(p,i->item.slot,1);append(p,i->item.uid,8);append(p,i->item.type,1);append(p,i->item.type_id,1);append(p,i->item.count,1);append(p,i->item.quality,2);for(auto c:i->item.gems) append(p,c,1);for(auto c:i->craft_storage) append(p,c,1);append(p,i->item.made_at);append(p,i->item.make_type.empty()?0:static_cast<unsigned char>(i->item.make_type[0]),1);for(auto c:i->craft_additions) append(p,c,2);append(p,i->definition->id);}return p;
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected sequence fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0,grants=0,recovered_failures=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;World w;unsigned id;std::uint32_t final_bags;std::string record,holes,uids,grid,state,events;
        std::istringstream row(line);row>>id>>record>>w.bags>>holes>>w.mask>>w.lookup>>w.new_bags>>w.billing>>uids>>final_bags>>grid>>state>>events;check(!row.fail(),"Malformed sequence fixture");
        const auto bytes=decode(record);check(bytes.size()==464,"Invalid quest projection");QuestStartRecord q;std::copy(bytes.begin(),bytes.end(),q.begin());w.uids=numbers(uids);w.grid.fill(&w.blocker);for(auto cell:numbers(holes)) w.grid.at(static_cast<std::size_t>(cell))=nullptr;
        for(unsigned i=0;i<3;++i) {auto& d=w.definitions[i];d.id=5001+i;d.type=5;d.type_id=static_cast<std::uint8_t>(i+1);d.quality[0]=static_cast<std::uint16_t>(257+i);d.server=1;d.required_wisdom=99;}
        w.cache=std::make_unique<InventoryReplica>(w.billing,0,w.cache_pool);quest_start(q,{},w);
        net::Packet occupancy;for(auto* cell:w.grid) occupancy.push_back(cell==nullptr?0:cell==&w.blocker?1:2);
        check(occupancy==decode(grid),"Persistent slot occupancy mismatch "+std::to_string(id));check(w.snapshot()==decode(state),"Persistent item payload mismatch "+std::to_string(id));check(w.events==decode(events),"Original sequence trace mismatch "+std::to_string(id));check(static_cast<std::uint32_t>(w.bags)==final_bags,"Live bag count mismatch");check(w.uid_cursor==w.uids.size(),"Unused UID response");check(w.phase==3,"Missing prelude");
        if(id%10==4 && w.stored.size()==2) ++recovered_failures;grants+=static_cast<unsigned>(w.stored.size());++cases;
    }
    check(cases==240 && grants>200 && recovered_failures>0,"Insufficient persistent sequence coverage");std::cout<<cases<<" original three-grant sequences; "<<grants<<" persistent items; "<<recovered_failures<<" middle allocation failures recovered\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
