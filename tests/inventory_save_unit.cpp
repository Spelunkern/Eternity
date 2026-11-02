#include "eternity/game/inventory_save.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(std::string_view bytes) {constexpr char d[]="0123456789abcdef";std::string result;for(unsigned char c:bytes) {result+=d[c>>4];result+=d[c&15];}return result;}
std::string lower(std::string value) {for(auto& c:value) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');return value;}
std::string encode(const InventorySaveOperation& op) {std::string result=lower(op.procedure);for(const auto& p:op.parameters) {result+=' ';if(const auto* i=std::get_if<std::int64_t>(&p)) result+='i'+std::to_string(*i);else if(const auto* s=std::get_if<std::string>(&p)) result+='s'+hex(*s);else throw std::runtime_error("Unexpected save parameter");}return result;}
net::Packet packet(std::uint16_t op,std::uint32_t user,std::initializer_list<std::uint8_t> rest) {net::Packet p{static_cast<std::uint8_t>(op),static_cast<std::uint8_t>(op>>8)};for(unsigned int i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(user>>(8*i)));p.insert(p.end(),rest);return p;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected inventory save oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open save oracle");
        InventoryReplicaPool pool;std::unique_ptr<InventoryReplica> replica;std::uint32_t account=0,character=0;unsigned int server=0,cases=0,queries=0;std::size_t next=0;std::vector<InventorySaveOperation> plan;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>server>>account>>character;replica=std::make_unique<InventoryReplica>(account,123,pool);plan.clear();next=0;}
            else if(tag=="DEL") {bool bank=false;PlayerItem item;row>>bank>>item.uid;item.count=1;require(replica->load(item,bank),"Cannot seed deleted item");
                std::uint8_t bag=0,slot=0;if(bank) {const auto live=replica->inventory();for(unsigned int index=0;index<144;++index) {bag=static_cast<std::uint8_t>(index/24);slot=static_cast<std::uint8_t>(index%24);if(std::none_of(live.begin(),live.end(),[&](const auto& cached){return cached.item.bag==bag && cached.item.slot==slot;})) break;}require(replica->apply(packet(0x705,account,{0,0,bag,slot,0}))==InventoryReplicaResult::applied,"Cannot journal bank deletion");}
                require(replica->apply(packet(0x702,account,{bag,slot,1}))==InventoryReplicaResult::applied,"Cannot journal inventory deletion");}
            else if(tag=="ITEM") {bool bank=false;unsigned int index=0,type=0,tid=0,count=0,quality=0,make=0;int save=0;std::string gems,craft;PlayerItem item;
                row>>bank>>index>>item.uid>>item.id>>type>>tid>>count>>quality>>gems>>craft>>item.made_at>>make>>save;
                item.bag=bank?0:static_cast<std::uint8_t>(index/24);item.slot=static_cast<std::uint16_t>(bank?index:index%24);item.type=static_cast<std::uint8_t>(type);item.type_id=static_cast<std::uint8_t>(tid);item.count=static_cast<std::uint8_t>(count);item.quality=static_cast<std::uint16_t>(quality);item.make_type=std::string(1,static_cast<char>(make));item.craft=craft=="-"?"":craft;
                for(std::size_t i=0;i<6;++i) item.gems[i]=static_cast<std::uint8_t>(std::stoul(gems.substr(i*2,2),nullptr,16));require(replica->load(item,bank,save),"Cannot seed save item");}
            else if(tag=="QUERY") {if(plan.empty()) plan=inventory_save_plan(*replica,static_cast<std::uint8_t>(server),character);require(next<plan.size(),"Original emitted extra SQL query");require(encode(plan[next])==line.substr(6),"SQL plan differs at case "+std::to_string(cases)+" query "+std::to_string(next)+"\nactual: "+encode(plan[next])+"\noriginal: "+line.substr(6));
                const auto& op=plan[next++];if(op.procedure.ends_with("Add_E")) {const bool bank=op.procedure.find("Stored")!=std::string::npos;require(op.retry_delete_procedure.ends_with("Del_E") && op.retry_delete_parameters.size()==(bank?4U:3U) && std::get<std::int64_t>(op.retry_delete_parameters.back())==0,"Original insert fallback contract differs");require(std::get<std::string>(op.retry_delete_parameters[bank?2:1])==std::to_string(std::get<std::int64_t>(op.parameters[bank?2:1])),"Fallback UID differs");}else require(op.retry_delete_parameters.empty(),"Non-insert has fallback");++queries;}
            else if(tag=="END") {if(plan.empty()) plan=inventory_save_plan(*replica,static_cast<std::uint8_t>(server),character);require(next==plan.size(),"Native emitted extra SQL query");const auto count=replica->inventory().size()+replica->bank().size();replica->inventory_save_success();require(inventory_save_plan(*replica,static_cast<std::uint8_t>(server),character).empty(),"Committed cache still has pending changes");require(replica->money()==123 && replica->inventory().size()+replica->bank().size()==count,"Acknowledgment changed item ownership or money");++cases;}
            else throw std::runtime_error("Unknown save fixture row");
        }
        require(cases==128 && queries>1800,"Save oracle coverage changed");std::cout<<cases<<" original inventory/bank save plans ("<<queries<<" SQL calls) passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
