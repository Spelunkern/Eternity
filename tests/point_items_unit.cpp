#include "eternity/game/character_replica.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& bytes) {constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes) {const auto b=static_cast<std::uint8_t>(byte);result+=digits[b>>4];result+=digits[b&15];}return result;}
net::Packet unhex(const std::string& value) {net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
std::string receipt(const PlayerItem& item) {std::ostringstream out;out<<+item.bag<<' '<<item.slot<<' '<<item.uid<<' '<<item.id<<' '<<+item.type<<' '<<+item.type_id<<' '<<+item.count<<' '<<item.quality<<' '<<item.made_at<<' '<<(item.make_type.empty()?0:static_cast<unsigned char>(item.make_type[0]));return out.str();}
std::string snapshot(const CharacterReplica& cache,const InventoryReplicaPool& inventory,const PointItemPool& points) {
    std::ostringstream product;std::size_t count=0;
    for(std::size_t i=0;i<cache.points().products().size();++i) {const auto& item=cache.points().products()[i];const auto payload=receipt(item.receipt);if(!item.save_type && !item.type && !item.type_id && !item.count && payload=="0 0 0 0 0 0 0 0 0 0") continue;product<<' '<<i<<' '<<item.save_type<<' '<<+item.type<<' '<<+item.type_id<<' '<<+item.count<<' '<<payload;++count;}
    std::ostringstream out;out<<"STATE "<<count<<product.str();const auto stored=cache.points().stored();out<<' '<<stored.size();for(const auto& [slot,item]:stored) out<<' '<<+slot<<' '<<item.save_type<<' '<<receipt(item.receipt);
    const auto rows=cache.inventory().inventory();out<<' '<<rows.size();for(const auto& cached:rows) {const auto& item=cached.item;out<<' '<<+item.bag*24+item.slot<<' '<<cached.save_type<<' '<<cached.saved_type<<' '<<item.uid<<' '<<item.id<<' '<<+item.type<<' '<<+item.type_id<<' '<<+item.count<<' '<<item.quality<<' '<<hex(item.gems)<<' '<<hex(cached.craft_storage)<<' '<<item.made_at<<' '<<(item.make_type.empty()?0:static_cast<unsigned char>(item.make_type[0]));}
    const auto a=inventory.statistics(),b=points.statistics();out<<' '<<a.attempts<<' '<<a.available<<' '<<b.attempts<<' '<<b.available;return out.str();
}
std::string encode(const PointSaveOperation& op) {std::string result=op.procedure;std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});for(const auto& value:op.parameters) result+=' '+(std::holds_alternative<std::int64_t>(value)?"i"+std::to_string(std::get<std::int64_t>(value)):"s"+hex(std::get<std::string>(value)));return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original point item oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open point item oracle");
        bool allocate=true;std::unique_ptr<InventoryReplicaPool> inventory;std::unique_ptr<PointItemPool> points;std::unique_ptr<CharacterReplica> cache;std::string line;std::uint8_t server=0;std::uint32_t character=0;std::vector<PointSaveOperation> plan;std::vector<std::string> attempts;std::size_t planned=0,attempted=0,packet=0,states=0,queries=0,ops=0,acks=0,failures=0,loads=0;bool failed=false;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {std::uint32_t account=0;unsigned int realm=0;row>>account>>realm>>character;server=static_cast<std::uint8_t>(realm);cache.reset();inventory=std::make_unique<InventoryReplicaPool>([&]()->std::unique_ptr<CachedInventoryItem>{return allocate?std::make_unique<CachedInventoryItem>():nullptr;});points=std::make_unique<PointItemPool>([&]()->std::unique_ptr<CachedPointItem>{return allocate?std::make_unique<CachedPointItem>():nullptr;});PlayerCore core;core.user_id=account;core.character.id=character;cache=std::make_unique<CharacterReplica>(core,*inventory,nullptr,nullptr,points.get());}
            else if(tag=="PRODUCTS" || tag=="POINTS") {std::size_t n=0;row>>allocate>>n;std::vector<PlayerPointItem> items;for(std::size_t i=0;i<n;++i) {unsigned int slot=0,count=0;std::uint32_t id=0;row>>slot>>id>>count;items.push_back({static_cast<std::uint8_t>(slot),id,static_cast<std::uint8_t>(count)});}if(tag=="PRODUCTS") cache->points().load_products(items);else cache->points().load_stored(items);++loads;}
            else if(tag=="OP") {std::string packet;unsigned int expected=0;row>>allocate>>packet>>expected;require(static_cast<unsigned int>(cache->apply(unhex(packet)))==expected,"Original receipt dispatch differs at operation "+std::to_string(ops));++ops;}
            else if(tag=="ACK") {cache->inventory().inventory_save_success();cache->points().point_save_success();++acks;}
            else if(tag=="STATE") {const auto actual=snapshot(*cache,*inventory,*points);require(actual==line,"Original product/point/inventory state differs at state "+std::to_string(states)+"\nactual: "+actual+"\nsource: "+line);++states;packet=0;}
            else if(tag=="PACKET") {std::string expected;row>>expected;const auto actual=packet++==0?cache->points().product_packet():cache->points().stored_packet();require((actual?hex(*actual):"-")==expected,"Original product/point packet differs");}
            else if(tag=="SAVE") {int failure=-1;bool negative=false;row>>failure>>negative;plan=point_save_plan(cache->points(),server,character);planned=attempted=0;attempts.clear();failed=false;try{execute_point_save(plan,[&](const auto& op)->std::int64_t{const auto index=static_cast<int>(attempts.size());attempts.push_back(encode(op));if(index==failure) {if(negative) return -1;throw std::runtime_error("Original SQL API fault");}return 1;});}catch(const std::runtime_error&){failed=true;}if(failed) ++failures;}
            else if(tag=="PLAN") {require(planned<plan.size() && encode(plan[planned++])==line.substr(5),"Original point SQL plan differs at state "+std::to_string(states));++queries;}
            else if(tag=="ATTEMPT") {require(attempted<attempts.size() && attempts[attempted++]==line.substr(8),"Original point SQL attempt differs");}
            else if(tag=="RESULT") {bool expected=false;row>>expected;require(failed==expected,"Original point SQL failure boundary differs");}
            else if(tag=="END") {require(planned==plan.size() && attempted==attempts.size() && packet==2,"Point oracle coverage is truncated");}
            else throw std::runtime_error("Unknown point oracle row");
        }
        require(states==1024 && ops==880 && acks==64 && loads==112 && failures==495 && queries==2912,"Point oracle coverage changed");
        InventoryReplicaPool pool;PlayerCore core;core.user_id=12345;core.character.id=17;CharacterReplica boundary(core,pool);
        for(auto op:{0x710,0xe05}) {net::Packet p(58);p[0]=static_cast<std::uint8_t>(op);p[1]=static_cast<std::uint8_t>(op>>8);p[2]=0x39;p[3]=0x30;p[57]='U';for(auto offset:{6,7,8}) {auto bad=p;bad[offset]=255;require(boundary.apply(bad)==InventoryReplicaResult::rejected,"Out-of-domain receipt accepted");}auto bad=p;bad.pop_back();require(boundary.apply(bad)==InventoryReplicaResult::rejected,"Truncated receipt accepted");bad=p;bad[2]^=1;require(boundary.apply(bad)==InventoryReplicaResult::rejected,"Cross-owner receipt accepted");}
        require(boundary.inventory().inventory().empty(),"Rejected receipt mutated inventory");net::Packet missing(58);missing[0]=5;missing[1]=14;missing[2]=0x39;missing[3]=0x30;require(boundary.apply(missing)==InventoryReplicaResult::rejected,"Missing stored point source was accepted");PlayerItem occupied;occupied.uid=1;require(boundary.inventory().load(occupied),"Cannot seed occupied target");require(boundary.apply(missing)==InventoryReplicaResult::disconnect,"Occupied point target did not retain original disconnect precedence");PlayerSnapshot other;other.core.user_id=12346;require(!boundary.points().load(other),"Cross-owner stored-point load accepted");
        std::cout<<states<<" original point states, "<<queries<<" SQL plans, "<<failures<<" fatal save boundaries and "<<acks<<" full acknowledgments passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
