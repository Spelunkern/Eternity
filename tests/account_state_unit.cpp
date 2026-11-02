#include "eternity/game/character_replica.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Invalid account fixture hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
std::string encode(const AccountSaveOperation& op) {auto result=op.procedure;for(auto& c:result) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');for(const auto& p:op.parameters) result+=' '+std::to_string(std::get<std::int64_t>(p));return result;}
void compare(std::istream& row,const MarketReplica& replica) {int saved=0;std::size_t count=0;row>>saved>>count;require(saved==replica.save_type() && count==replica.ids().size(),"Market count/pending state differs");for(auto id:replica.ids()) {std::uint32_t expected=0;row>>expected;require(id==expected,"Market ID/order differs");}require(static_cast<bool>(row),"Truncated market state");std::string extra;require(!(row>>extra),"Trailing market state");}
void market(const char* file) {
    std::ifstream input(file);require(input.good(),"Cannot read market oracle");InventoryReplicaPool pool;std::unique_ptr<CharacterReplica> character;std::vector<AccountSaveOperation> plan;std::vector<std::string> attempted;std::size_t next=0;int failure=-1;unsigned int cases=0,episodes=0,acks=0,planned=0,attempts=0,failures=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
        if(tag=="INIT") {PlayerCore core;std::size_t count=0;row>>core.user_id>>count;core.character.id=500001;character=std::make_unique<CharacterReplica>(core,pool);std::vector<std::uint32_t> ids(count);for(auto& id:ids) row>>id;require(character->market().load(ids),"Cannot seed market");++episodes;}
        else if(tag=="OP") {std::string p;row>>p;require(character->apply(unhex(p))==InventoryReplicaResult::applied,"Market packet acceptance differs");}
        else if(tag=="ACK") {character->core_save_success();character->inventory().inventory_save_success();character->skills().save_success();character->quests().save_success();character->interface().save_success();++acks;}
        else if(tag=="HOLD") {}
        else if(tag=="STATE") {compare(row,character->market());++cases;}
        else if(tag=="PACKET") {std::string p;row>>p;const auto actual=character->market().packet();require(p=="-"?!actual:actual && *actual==unhex(p),"Market initial packet/empty omission differs");}
        else if(tag=="SAVE") {row>>failure;plan=market_save_plan(character->market(),500001);next=0;attempted.clear();}
        else if(tag=="PLAN") {require(next<plan.size(),"Original emitted extra market SQL");const auto actual=encode(plan[next++]);require(actual==line.substr(5),"Market SQL differs");++planned;}
        else if(tag=="ATTEMPT") {attempted.push_back(line.substr(8));++attempts;}
        else if(tag=="RESULT") {bool failed=false;row>>failed;std::vector<std::string> actual;unsigned int diagnostics=0;const auto result=execute_market_save(character->market(),500001,[&](const auto& op){const auto index=actual.size();actual.push_back(encode(op));if(static_cast<int>(index)==failure) throw std::runtime_error("fixture");},[&](std::string_view){++diagnostics;});require(result.failure.has_value()==failed && diagnostics==static_cast<unsigned int>(failed) && actual==attempted,"Market failure/continuation differs");compare(row,character->market());failures+=failed;}
        else if(tag=="END") require(next==plan.size(),"Native emitted extra market SQL");else throw std::runtime_error("Unknown market fixture row");
    }
    require(cases==1024 && episodes==16 && acks==64 && planned==4920 && attempts==3888 && failures>100,"Market coverage changed");std::cout<<cases<<" original market states and "<<attempts<<" attempted SQL calls passed\n";
    MarketReplica validation(12345);for(const auto& p:std::vector<net::Packet>{{0x1a,6},{0x1a,6,0x39,0x30,0,0,11},{0x1a,6,0x38,0x30,0,0,0},{0x1a,6,0x39,0x30,0,0,1,1,0,0}}) require(validation.apply(p)==InventoryReplicaResult::rejected,"Malformed market packet accepted");std::vector<std::uint32_t> overflow(11);require(!validation.load(overflow) && validation.ids().empty(),"Market storage overflow accepted");PlayerSnapshot loaded;loaded.core.user_id=12345;loaded.concern_market={0,UINT32_MAX,7,7};require(validation.load(loaded) && validation.ids().size()==4 && validation.save_type()==0,"Typed market load lost signed bits/order/duplicates");loaded.core.user_id=12346;require(!validation.load(loaded),"Cross-owner market load accepted");
}
void growth(const char* file) {
    std::ifstream input(file);require(input.good(),"Cannot read growth oracle");std::unique_ptr<GrowthReplica> replica;unsigned int server=0,grow=0,level=0,maximum=0,cases=0,tries=0,queries=0;std::uint32_t account=0;bool null=false;std::vector<std::string> actual;std::size_t next=0,expected_queries=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
        if(tag=="CASE") {require(next==expected_queries,"Growth query count differs");row>>server>>account>>grow>>level>>maximum>>null;replica=std::make_unique<GrowthReplica>(static_cast<std::uint8_t>(maximum));++cases;}
        else if(tag=="TRY") {require(next==expected_queries,"Growth query count differs");unsigned int mode=0,after=0,logs=0,alloc=0;int code=0;row>>mode>>after>>code>>logs>>alloc>>expected_queries;unsigned int allocations=0,diagnostics=0;actual.clear();next=0;
            const auto result=execute_growth_save(null?nullptr:replica.get(),static_cast<std::uint8_t>(server),account,static_cast<std::uint8_t>(grow),static_cast<std::uint16_t>(level),[&](const auto& operation)->std::optional<std::int64_t>{++allocations;if(mode==1) return std::nullopt;if(mode==2) throw std::runtime_error("bind");actual.push_back(encode(operation));if(mode==3 || mode==4) throw std::runtime_error("query/results");return mode==5?-1:mode==6?0:1;},[&](std::string_view){++diagnostics;});
            require(replica->maximum()==after && result.legacy_code==code && result.advanced==static_cast<bool>(alloc) && diagnostics==logs && allocations==alloc && result.failure.has_value()==static_cast<bool>(logs) && actual.size()==expected_queries,"Original growth state/error outcome differs at "+std::to_string(tries));++tries;}
        else if(tag=="QUERY") {require(next<actual.size() && actual[next++]==line.substr(6),"Original growth SQL arguments differ");++queries;}else throw std::runtime_error("Unknown growth fixture row");
    }
    require(cases==830 && tries==1660 && queries==62 && next==expected_queries,"Growth coverage changed");std::cout<<tries<<" original growth saves and "<<queries<<" SQL calls passed\n";
}
}
int main(int argc,char** argv) {try{require(argc==3,"Expected market/growth fixtures");market(argv[1]);growth(argv[2]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
