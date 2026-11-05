#include "eternity/game/guild_sql.hpp"
#include "eternity/game/guild_member_update.hpp"
#include "eternity/game/guild_member_delete.hpp"
#include "eternity/game/guild_member_create.hpp"
#include "eternity/game/guild_delete.hpp"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const std::string& value) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char b:value) out<<std::setw(2)<<unsigned(b);return out.str();}
std::string decode(const std::string& value) {if(value=="-") return {};std::string out;for(std::size_t i=0;i<value.size();i+=2) out.push_back(static_cast<char>(std::stoul(value.substr(i,2),nullptr,16)));return out;}
GuildSaveResult result(unsigned int mode,unsigned int attempt) {
    if(mode==2) return {false,"HYT00",1222};
    if(mode==3 && attempt<2) return {false,"HYT00",1222};
    if(mode==4 || mode==5) return {false,"42000",mode==4?105:2627};
    if(mode==6) return {false,"HYT01",105};
    if(mode==7 || mode==8) return attempt==0?GuildSaveResult{false,"HYT00",1222}:GuildSaveResult{false,"42000",mode==7?105:2627};
    if(mode==9) return attempt<2?GuildSaveResult{false,"HYT00",1222}:GuildSaveResult{false,"HYT01",999};return {};
}
void member_create_cases() {
    const net::Packet p{7,11,0x78,0x56,0x34,0x12,0x98,0xba,0xdc,0xfe,255};
    const GuildSaveResult ok{},timeout{false,"HYT00",1},error{false,"42000",2};
    const auto run=[&](std::vector<GuildSaveResult> results,std::vector<std::size_t> diagnosed,bool success){
        std::size_t next=0;std::vector<std::size_t> got;
        auto execute=[&](std::uint32_t guild,std::uint32_t character,std::uint8_t rank){require(guild==0x12345678 && character==0xfedcba98 && rank==255 && next<results.size(),"Guild member create SQL parameters or retry count differs");return results[next++];};
        auto diagnostic=[&](const GuildSaveResult&){got.push_back(next-1);};bool threw=false;
        try{require(guild_member_create(p,execute,diagnostic)==InventoryReplicaResult::applied,"Guild member create did not apply");}catch(const std::runtime_error&){threw=true;}
        require(threw!=success && next==results.size() && got==diagnosed,"Guild member create failure/retry ordering differs");
    };
    run({ok},{},true);run({timeout,ok},{},true);run({timeout,timeout,ok},{1},true);run({timeout,timeout,timeout},{1,2},false);
    run({error,ok},{0},true);run({error,timeout},{0},false);run({timeout,error,ok},{1},true);run({error,error},{0,1},false);
    bool missing=false;try{(void)guild_member_create(p,{},{});}catch(const std::invalid_argument&){missing=true;}require(missing,"Missing guild member create SQL service accepted");
    require(guild_member_create(net::Packet{7,11},{},{})==InventoryReplicaResult::rejected && guild_member_create(net::Packet{8,11},{},{})==InventoryReplicaResult::unsupported,"Guild member create envelope differs");
}
void guild_delete_cases() {
    const net::Packet p{5,11,0x78,0x56,0x34,0x12};const GuildSaveResult ok{},timeout{false,"HYT00",1},error{false,"42000",2};
    const auto run=[&](std::vector<GuildSaveResult> results,std::vector<std::size_t> diagnosed,bool success){
        std::size_t next=0,retired=0;std::vector<std::size_t> got;std::map<std::uint32_t,int> replica{{0x12345678,1}};
        auto execute=[&](std::uint32_t guild){require(guild==0x12345678 && next<results.size() && replica.contains(guild),"Guild delete query or ordering differs");return results[next++];};
        auto diagnostic=[&](const GuildSaveResult&){got.push_back(next-1);};auto retire=[&](std::uint32_t guild){++retired;replica.erase(guild);};bool threw=false;
        try{require(guild_delete(p,execute,diagnostic,retire)==InventoryReplicaResult::applied,"Guild delete did not apply");}catch(const std::runtime_error&){threw=true;}
        require(threw!=success && next==results.size() && got==diagnosed && retired==unsigned(success) && replica.contains(0x12345678)!=success,"Guild delete retry, diagnostics or retirement differs");
    };
    run({ok},{},true);run({timeout,ok},{},true);run({timeout,timeout,ok},{1},true);run({timeout,timeout,timeout},{1,2},false);run({error,ok},{0},true);run({error,timeout},{0},false);run({timeout,error,ok},{1},true);run({error,error},{0,1},false);
    bool missing=false;try{(void)guild_delete(p,{}, {},{});}catch(const std::invalid_argument&){missing=true;}require(missing,"Missing guild delete services accepted");
    require(guild_delete(net::Packet{5,11},{},{},{})==InventoryReplicaResult::rejected && guild_delete(net::Packet{6,11},{},{},{})==InventoryReplicaResult::unsupported,"Guild delete envelope differs");
}
void member_delete_cases() {
    const net::Packet p{8,11,0x78,0x56,0x34,0x12,0x98,0xba,0xdc,0xfe};
    const GuildSaveResult ok{},timeout{false,"HYT00",1},error{false,"42000",2};
    const auto run=[&](std::vector<GuildSaveResult> results,std::vector<std::size_t> diagnosed,bool success){
        std::size_t next=0;std::vector<std::size_t> got;
        auto execute=[&](std::uint32_t guild,std::uint32_t character){require(guild==0x12345678 && character==0xfedcba98 && next<results.size(),"Guild member delete SQL parameters or retry count differs");return results[next++];};
        auto diagnostic=[&](const GuildSaveResult&){got.push_back(next-1);};bool threw=false;
        try{require(guild_member_delete(p,execute,diagnostic)==InventoryReplicaResult::applied,"Guild member delete did not apply");}catch(const std::runtime_error&){threw=true;}
        require(threw!=success && next==results.size() && got==diagnosed,"Guild member delete failure/retry ordering differs");
    };
    run({ok},{},true);run({timeout,ok},{},true);run({timeout,timeout,ok},{1},true);run({timeout,timeout,timeout},{1,2},false);
    run({error,ok},{0},true);run({error,timeout},{0},false);run({timeout,error,ok},{1},true);run({error,error},{0,1},false);
    bool missing=false;try{(void)guild_member_delete(p,{},{});}catch(const std::invalid_argument&){missing=true;}require(missing,"Missing guild member delete SQL service accepted");
    require(guild_member_delete(net::Packet{8,11},{},{})==InventoryReplicaResult::rejected && guild_member_delete(net::Packet{9,11},{},{})==InventoryReplicaResult::unsupported,"Guild member delete envelope differs");
}
void member_update_cases() {
    const net::Packet p{9,11,0x78,0x56,0x34,0x12,0x98,0xba,0xdc,0xfe,7};
    const GuildSaveResult ok{},timeout{false,"HYT00",1},error{false,"42000",2};
    const auto run=[&](std::vector<GuildSaveResult> results,std::vector<std::size_t> diagnosed,bool success){
        std::size_t next=0;std::vector<std::size_t> got;
        auto execute=[&](std::uint32_t character,std::uint8_t rank){require(character==0xfedcba98 && rank==7 && next<results.size(),"Guild member SQL parameters or retry count differs");return results[next++];};
        auto diagnostic=[&](const GuildSaveResult&){got.push_back(next-1);};bool threw=false;
        try {require(guild_member_update(p,execute,diagnostic)==InventoryReplicaResult::applied,"Guild member update did not apply");}catch(const std::runtime_error&){threw=true;}
        require(threw!=success && next==results.size() && got==diagnosed,"Guild member update failure/retry ordering differs");
    };
    run({ok},{},true);run({timeout,ok},{},true);run({timeout,timeout,ok},{1},true);run({timeout,timeout,timeout},{1,2},false);
    run({error,ok},{0},true);run({error,timeout},{0},false);run({timeout,error,ok},{1},true);run({error,error},{0,1},false);
    bool missing=false;try{(void)guild_member_update(p,{},{});}catch(const std::invalid_argument&){missing=true;}require(missing,"Missing guild member SQL service accepted");
    require(guild_member_update(net::Packet{9,11},{},{})==InventoryReplicaResult::rejected && guild_member_update(net::Packet{10,11},{},{})==InventoryReplicaResult::unsupported,"Guild member update envelope differs");
}
int main(int argc,char** argv) {
    try {
        member_create_cases();guild_delete_cases();member_delete_cases();member_update_cases();
        require(argc==2,"Expected original guild save coordinator oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild save oracle");
        std::map<std::uint32_t,GuildDbMetadata> guilds;std::map<std::uint32_t,unsigned int> indexes,queries;std::map<std::pair<std::string,std::uint32_t>,unsigned int> children;unsigned int scenario=0,mode=0;std::size_t cases=0,next=0;std::vector<std::string> trace;bool returned=false;std::string line;
        const auto child=[&](const std::string& kind,GuildDbMetadata& guild)->std::int32_t {const auto id=guild.metadata.listing.id;auto& attempt=children[{kind,id}];const auto profile=(scenario+indexes.at(id)+(kind=="NPC"))%3;const auto value=profile==0 || (profile==1 && attempt>0)?0:profile;++attempt;trace.push_back(kind+' '+std::to_string(id)+' '+std::to_string(value));return static_cast<std::int32_t>(value);};
        GuildSavePorts ports{[&]{const bool ok=mode!=1;trace.push_back("ACQUIRE "+std::to_string(ok));return ok;},[&]{trace.push_back("RELEASE");},[&]{trace.push_back("PREPARE");},[&](const GuildSaveOperation& operation){
            const auto id=static_cast<std::uint32_t>(std::get<std::int64_t>(operation.parameters[0]));const auto value=result(mode,queries[id]++);std::string procedure=operation.procedure;for(auto& c:procedure) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');std::string text="EXEC "+procedure;
            for(const auto& p:operation.parameters) {if(const auto* n=std::get_if<std::int64_t>(&p)) text+=" i"+std::to_string(*n);else text+=" s"+hex(std::get<std::string>(p));}
            trace.push_back(text+" | "+std::to_string(value.success)+' '+(value.sqlstate.empty()?"-":value.sqlstate)+' '+std::to_string(value.native_error));return value;
        },[&](GuildDbMetadata& guild){return child("BANK",guild);},[&](GuildDbMetadata& guild){return child("NPC",guild);},[&](const GuildSaveResult&){trace.push_back("ERROR");}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>scenario>>mode;guilds.clear();indexes.clear();queries.clear();children.clear();trace.clear();next=0;++cases;}
            else if(tag=="GUILD") {GuildDbMetadata guild;auto& m=guild.metadata;auto& g=m.listing;unsigned int buy=0,rank=0,use=0;std::string notice;row>>g.id>>g.points>>m.etin>>m.return_count>>m.keep_etin>>buy>>rank>>use>>notice>>guild.save_type;m.buy_house=static_cast<std::uint8_t>(buy);g.rank=static_cast<std::uint8_t>(rank);m.use_house=static_cast<std::uint8_t>(use);g.notice=decode(notice);indexes.emplace(g.id,static_cast<unsigned int>(indexes.size()));guilds.emplace(g.id,std::move(guild));}
            else if(tag=="RUN") {std::vector<GuildDbMetadata*> ordered;for(auto& [id,guild]:guilds) ordered.push_back(&guild);returned=save_guilds(ordered,ports);}
            else if(tag=="TRACE") {require(next<trace.size() && trace[next]==line.substr(6),"Original guild coordinator trace differs at case "+std::to_string(cases)+": "+(next<trace.size()?trace[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="RETURN") {bool expected=false;row>>expected;require(returned==expected,"Original guild save return semantics changed");}
            else if(tag=="STATE") {std::uint32_t id=0,marker=0;std::string notice;row>>id>>marker>>notice;const auto& guild=guilds.at(id);require(static_cast<std::uint32_t>(guild.save_type)==marker && guild.metadata.listing.notice==decode(notice),"Original guild save dirty marker/notice changed");}
            else if(tag=="END") require(next==trace.size(),"Extra native guild coordinator effects");else throw std::runtime_error("Unknown guild coordinator fixture row");
        }
        require(cases==160,"Original guild save coordinator coverage changed");std::cout<<cases<<" original guild save coordinations matched metadata SQL, timeouts, child retries, order, diagnostics and return state\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
