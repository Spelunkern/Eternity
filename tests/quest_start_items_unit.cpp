#include "eternity/game/quest_start_items.hpp"
#include "eternity/game/quest_start_log.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/inventory_replica.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
net::Packet decode(const std::string& s) {net::Packet p;for(std::size_t i=0;i<s.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
void append(net::Packet& p,std::uint64_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
struct World final:QuestStartItemsWorld {
    ItemDefinition definition;GroundItem initial;GroundItem* created=nullptr;unsigned found=0,allocation=0;std::int32_t bags=0,hole=0;std::uint32_t billing=0;std::uint64_t uid=0;net::Packet events;bool stored=false;
    GroundItemPool pool{1,[this]()->std::unique_ptr<GroundItem> {if(!allocation) return {};return std::make_unique<GroundItem>(initial);}};
    QuestStartItemsUser user{billing,bags};
    World() {initial.item.uid=0xface;initial.craft_storage.fill(0xa5);initial.craft_storage[0]=0;}
    void event(char c,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(c));for(auto v:values) append(events,v);}
    const ItemDefinition* find_definition(std::int32_t t,std::int32_t id) override {event('D',{static_cast<std::uint32_t>(t),static_cast<std::uint32_t>(id)});return found && t>=1 && t<=255 && id>=1 && id<=255?&definition:nullptr;}
    bool slot_empty(std::int32_t bag,std::int32_t slot) override {event('K',{static_cast<std::uint32_t>(bag),static_cast<std::uint32_t>(slot)});return (bag-1)*24+slot==hole;}
    GroundItem* create_item() override {event('O');created=pool.allocate();return created;}
    std::uint32_t local_make_time() override {check(created && created->definition==&definition && created->item.count==0,"Calendar must follow binding and precede payload setup");event('M');return quest_start_log_time(2026,11,3,15,30,40);}
    std::uint64_t next_uid() override {event('U');return uid;}
    void store(std::int32_t bag,std::int32_t slot,GroundItem& item) override {check(&item==created && item.item.craft.empty() && !stored,"Raw slot publication must precede craft default");stored=true;event('S',{static_cast<std::uint32_t>(bag),static_cast<std::uint32_t>(slot)});}
    void send_database(const net::Packet& p) override {
        check(stored,"DB send precedes ownership");event('B');events.insert(events.end(),p.begin(),p.end());
        InventoryReplicaPool cache_pool;InventoryReplica cache(billing,0,cache_pool);check(cache.apply(p)==InventoryReplicaResult::applied,"Canonical DB inventory cache rejected grant");
        const auto items=cache.inventory();check(items.size()==1 && items[0].item.uid==created->item.uid && items[0].item.count==created->item.count && items[0].item.craft==created->item.craft,"DB cache payload mismatch");
    }
    void send_self(const net::Packet& p) override {event('C');events.insert(events.end(),p.begin(),p.end());}
    net::Packet snapshot() const {
        const auto& i=created?*created:initial;net::Packet p;append(p,i.item.uid,8);append(p,i.item.type,1);append(p,i.item.type_id,1);append(p,i.item.count,1);append(p,i.item.quality,2);
        for(auto c:i.item.gems) append(p,c,1);for(auto c:i.craft_storage) append(p,c,1);append(p,i.item.made_at);append(p,i.item.make_type.empty()?0:static_cast<unsigned char>(i.item.make_type[0]),1);for(auto c:i.craft_additions) append(p,c,2);append(p,i.definition==&definition?1:0);return p;
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original grants fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0,successes=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;World w;unsigned id,mode,type_actual,id_actual,quality,server,wisdom,result,bag,slot,info;std::int32_t type,item_id,count;std::uint32_t made;std::string state,events;
        std::istringstream row(line);row>>id>>mode>>type>>item_id>>count>>w.found>>w.bags>>w.hole>>w.allocation>>type_actual>>id_actual>>quality>>server>>wisdom>>w.definition.id>>w.billing>>w.uid>>made>>result>>bag>>slot>>info>>state>>events;check(!row.fail(),"Malformed item fixture");
        w.definition.type=static_cast<std::uint8_t>(type_actual);w.definition.type_id=static_cast<std::uint8_t>(id_actual);w.definition.quality[0]=static_cast<std::uint16_t>(quality);w.definition.server=static_cast<std::uint8_t>(server);w.definition.required_wisdom=static_cast<std::uint16_t>(wisdom);
        QuestStartItemsResult output{17,19,nullptr};const bool actual=mode?quest_start_items_grant(type,item_id,count,w.user,w):quest_start_items_add(type,item_id,count,output,w.user,w);
        check(actual==(result!=0),"Grant result mismatch "+std::to_string(id));
        if(!mode) check(static_cast<std::uint32_t>(output.bag)==bag && static_cast<std::uint32_t>(output.slot)==slot && (output.definition!=nullptr)==(info!=0),"Output pointer contract mismatch "+std::to_string(id));
        check(w.snapshot()==decode(state),"Complete CItem payload mismatch "+std::to_string(id));check(w.events==decode(events),"Complete grant/packet trace mismatch "+std::to_string(id));if(actual) {check(w.created->item.made_at==made,"Make time mismatch");++successes;}++cases;
    }
    check(cases==384 && successes>70,"Insufficient complete original grant coverage");std::cout<<cases<<" complete original QuestAddItem cases; "<<successes<<" grants/real inventory cache packets matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
