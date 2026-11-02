#include "eternity/game/character_replica.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& bytes) {constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes) {const auto b=static_cast<std::uint8_t>(byte);result+=digits[b>>4];result+=digits[b&15];}return result;}
net::Packet unhex(const std::string& text) {net::Packet result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
std::string item(const CachedInventoryItem& cached) {const auto& v=cached.item;std::ostringstream out;out<<std::bit_cast<std::uint32_t>(cached.save_type)<<' '<<cached.saved_type<<' '<<v.uid<<' '<<v.id<<' '<<+v.type<<' '<<+v.type_id<<' '<<+v.count<<' '<<v.quality<<' '<<hex(v.gems)<<' '<<hex(cached.craft_storage)<<' '<<v.made_at<<' '<<(v.make_type.empty()?0:static_cast<unsigned char>(v.make_type[0]));return out.str();}
std::string snapshot(const CharacterReplica& cache,const InventoryReplicaPool& pool) {
    std::ostringstream out;out<<"STATE";for(const auto& row:cache.purchases().history()) out<<' '<<row.save_type<<' '<<hex(row.code)<<' '<<row.made_at<<' '<<row.points;
    const auto pending=cache.purchases().pending();out<<' '<<pending.size();for(const auto& purchase:pending) {out<<' '<<+purchase.log_slot<<' '<<purchase.items.size();for(const auto& value:purchase.items) out<<' '<<item(value);}
    const auto inventory=cache.inventory().inventory();out<<' '<<inventory.size();for(const auto& value:inventory) out<<' '<<+value.item.bag*24+value.item.slot<<' '<<item(value);
    const auto s=pool.statistics();out<<' '<<s.attempts<<' '<<s.available<<' '<<s.stranded;return out.str();
}
std::string encode(const PointSaveOperation& op) {auto result=op.procedure;std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});for(const auto& p:op.parameters) result+=' '+(std::holds_alternative<std::int64_t>(p)?"i"+std::to_string(std::get<std::int64_t>(p)):"s"+hex(std::get<std::string>(p)));return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original purchase oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open purchase oracle");std::unique_ptr<InventoryReplicaPool> pool;std::unique_ptr<CharacterReplica> cache;int limit=-1,fresh=0;std::uint32_t character=0;std::size_t states=0,ops=0,saves=0,acks=0,queries=0,faults=0,event_cursor=0;std::vector<std::string> events;PurchaseSaveResult result;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {std::uint32_t account=0,cash=0;row>>account>>character>>cash;cache.reset();pool=std::make_unique<InventoryReplicaPool>([&]()->std::unique_ptr<CachedInventoryItem>{return limit<0 || fresh++<limit?std::make_unique<CachedInventoryItem>():nullptr;});PlayerCore core;core.user_id=account;core.character.id=character;cache=std::make_unique<CharacterReplica>(core,*pool);cache->purchases().load_history({},cash);}
            else if(tag=="OP") {bool in_game=false;std::string packet;unsigned int expected=0;std::size_t n=0;row>>in_game>>limit>>packet>>expected>>n;fresh=0;cache->purchases().set_in_game(in_game);cache->purchases().clear_actions();const auto actual=cache->apply(unhex(packet));require(static_cast<unsigned int>(actual)==expected,"Purchase dispatch result differs at operation "+std::to_string(ops));require(cache->purchases().actions().size()==n,"Purchase dispatch action count differs");for(auto action:cache->purchases().actions()) {unsigned int value=0;row>>value;require(static_cast<unsigned int>(action)==value,"Original purchase action differs");}++ops;}
            else if(tag=="ACK") {cache->inventory().inventory_save_success();cache->points().point_save_success();++acks;}
            else if(tag=="SAVE") {
                std::size_t n=0;row>>n;std::vector<int> modes(n);for(auto& mode:modes) row>>mode;events.clear();event_cursor=0;std::size_t group=0;int mode=0,query_i=0;std::array<bool,2> timeout{};const auto name=[](PurchaseConnection c){return c==PurchaseConnection::items?"I":"P";};PurchaseSavePort port;
                port.acquire=[&](PurchaseConnection c){events.push_back("ALLOC "+std::string(name(c)));if(c==PurchaseConnection::items) {require(group<modes.size(),"Native allocated an extra purchase DB group");mode=modes[group++];query_i=0;timeout={mode==18,mode==19};return mode!=1;}return mode!=5;};
                port.begin=[&](auto c){events.push_back("BEGIN "+std::string(name(c)));return mode!=2;};port.commit=[&](auto c){events.push_back("COMMIT "+std::string(name(c)));return mode!=12;};port.rollback=[&](auto c){events.push_back("ROLLBACK "+std::string(name(c)));return mode!=13 && mode!=15;};port.release=[&](auto c){events.push_back("FREE "+std::string(name(c)));};port.timeout=[&](auto c){return timeout[c==PurchaseConnection::items?0:1];};port.diagnostic=[&](std::string_view){events.push_back("LOG");};
                port.query=[&](PurchaseConnection c,const PointSaveOperation& op)->std::int64_t {
                    if(c==PurchaseConnection::items) {events.push_back("QUERY I "+encode(op));++query_i;if(query_i==1 && (mode==3 || mode==4 || mode==13 || mode==15)) {timeout[0]=mode==4 || mode==15;throw std::runtime_error("Original item query error");}return mode==14?-1:1;}
                    events.push_back("BIND P");if(mode==6 || mode==16) {timeout[1]=mode==16;throw std::runtime_error("Original charge bind error");}events.push_back("QUERY P "+encode(op));if(mode==7 || mode==8) {timeout[1]=mode==8;throw std::runtime_error("Original charge query error");}events.push_back("MORE P");if(mode==9 || mode==17) {timeout[1]=mode==17;throw std::runtime_error("Original charge result error");}return mode==10 || mode==18 || mode==19?-1:mode==11?0:1;
                };
                result=execute_purchase_save(cache->purchases(),character,port);++saves;
            }
            else if(tag=="RETURN") {bool expected=false;row>>expected;require(result.legacy_success==expected,"Original purchase save return differs at save "+std::to_string(saves));if(!expected) ++faults;}
            else if(tag=="EVENT") {require(event_cursor<events.size() && events[event_cursor]==line.substr(6),"Purchase DB/log order differs at save "+std::to_string(saves)+" event "+std::to_string(event_cursor)+"\nactual: "+(event_cursor<events.size()?events[event_cursor]:"<none>")+"\nsource: "+line.substr(6));++event_cursor;if(line.starts_with("EVENT QUERY")) ++queries;}
            else if(tag=="STATE") {const auto actual=snapshot(*cache,*pool);require(actual==line,"Purchase cache/queue/pool differs at state "+std::to_string(states)+"\nactual: "+actual+"\nsource: "+line);++states;}
            else if(tag=="PACKET") {std::string expected;row>>expected;require(hex(cache->purchases().packet())==expected,"Original purchase history packet differs");}
            else if(tag=="END") require(event_cursor==events.size(),"Native produced extra purchase DB/log events");
            else throw std::runtime_error("Unknown purchase oracle row");
        }
        require(states==1536 && ops==1248 && saves==192 && acks==96 && queries==809 && faults==94,"Purchase oracle coverage changed");
        InventoryReplicaPool boundary_pool;PlayerCore core;core.user_id=12345;core.character.id=17;CharacterReplica boundary(core,boundary_pool);const auto before=snapshot(boundary,boundary_pool);net::Packet p(37);p[0]=2;p[1]=14;p[2]=0x39;p[3]=0x30;
        for(int variation=0;variation<4;++variation) {auto bad=p;if(variation==0) bad.pop_back();if(variation==1) bad[2]^=1;if(variation==2) bad[36]=1;if(variation==3) {bad.resize(83);bad[36]=1;bad[49]=6;}require(boundary.apply(bad)==InventoryReplicaResult::rejected,"Malformed purchase was accepted");}require(snapshot(boundary,boundary_pool)==before && boundary.purchases().actions().empty(),"Rejected purchase changed state/actions");
        PlayerSnapshot loaded;loaded.core=core;loaded.cash_points=100;loaded.catalogs["PointLog"].rows={storage::Row{storage::Cell{"history"},storage::Cell{"2024-02-29 23:59:58.000"},storage::Cell{"-1"}}};require(boundary.purchases().load(loaded) && boundary.purchases().history()[0].points==UINT32_MAX && boundary.purchases().history()[0].save_type==0 && boundary.purchases().packet()[10]==1,"Typed point history load changed widths/calendar/flags");loaded.core.user_id=12346;require(!boundary.purchases().load(loaded),"Cross-owner point history loaded");
        std::cout<<states<<" original purchase states, "<<saves<<" saves and "<<queries<<" SQL attempts passed, with DB/log order, partial failures, pending history and pool reuse\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
