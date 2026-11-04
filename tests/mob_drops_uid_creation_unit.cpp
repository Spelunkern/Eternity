#include "eternity/game/character_commands.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
namespace {
struct Harness {
    unsigned failure=0;std::vector<std::vector<storage::Parameter>> saved;
    std::vector<storage::Parameter> created;unsigned commits=0,rollbacks=0;
} *active=nullptr;
std::vector<storage::ResultSet> table(unsigned columns,std::vector<storage::Row> rows) {
    return {{std::vector<std::string>(columns,"column"),std::move(rows)}};
}
std::int64_t number(const storage::Parameter& value) {return std::get<std::int64_t>(value);}
std::chrono::sys_seconds date(unsigned second) {return std::chrono::sys_days{std::chrono::year{2032}/1/1}+std::chrono::seconds{second};}
struct Clock:game::ItemIdClock {
    unsigned scenario=0;int index=-1;std::string events;
    std::uint32_t ticks_ms() override {
        constexpr std::array<std::array<std::uint32_t,4>,4> ticks{{{{0,1,1999,2000}},{{0xfffffff0U,0xffffffffU,0,1999}},{{10000,11999,12000,13999}},{{0,0,0,0}}}};
        events+='T';return ticks[scenario%4][static_cast<unsigned>(++index)%4];
    }
    std::uint32_t packed_local_calendar() override {events+='G';return game::pack_item_id_time(date(index<0?0:10+static_cast<unsigned>(index)%20));}
};
std::shared_ptr<game::CreationRules> rules(unsigned count) {
    game::Definitions defs;defs.catalogs["StatusDefs"].columns={"Level","Job","HP","SP","MP"};
    for(unsigned job=0;job<6;++job) defs.catalogs["StatusDefs"].rows.push_back({"1",std::to_string(job),"205","125","50"});
    defs.catalogs["Items"].columns={"Type","TypeID","Quality"};
    defs.catalogs["BaseItemDefs"].columns={"Family","Job","ItemID","Type","TypeID","Count","Slot","Quality"};
    for(unsigned i=0;i<count;++i) {
        defs.catalogs["Items"].rows.push_back({"25",std::to_string(i+1),std::to_string(1200+i)});
        defs.catalogs["BaseItemDefs"].rows.push_back({"0","0",std::to_string(25001+i),"25",std::to_string(i+1),std::to_string(i+1),"0","0"});
    }
    return std::make_shared<game::CreationRules>(game::CreationRules::load(defs));
}
std::string persisted(const std::vector<storage::Parameter>& row) {
    check(row.size()==11 && std::get<std::string>(row[8])=="기본아이템","Starter save literal/parameter shape differs");
    std::vector<std::uint8_t> bytes;
    const auto put=[&](std::uint64_t value,unsigned count) {for(unsigned b=0;b<count;++b) bytes.push_back(static_cast<std::uint8_t>(value>>(8*b)));};
    put(static_cast<std::uint64_t>(number(row[2])),1);put(static_cast<std::uint64_t>(number(row[3])),1);
    put(std::bit_cast<std::uint64_t>(number(row[1])),8);put(static_cast<std::uint64_t>(number(row[4])),4);
    put(static_cast<std::uint64_t>(number(row[5])),1);put(static_cast<std::uint64_t>(number(row[6])),1);
    put(static_cast<std::uint64_t>(number(row[7])),2);put(static_cast<std::uint64_t>(number(row[9])),1);
    const auto& timestamp=std::get<std::string>(row[10]);
    check(timestamp.starts_with("2032-01-01 00:00:") && timestamp.size()==19,"Separate starter make time differs");
    put(game::pack_item_id_time(date(static_cast<unsigned>(std::stoul(timestamp.substr(17))))),4);
    std::string hex;for(auto byte:bytes) {hex+="0123456789abcdef"[byte>>4];hex+="0123456789abcdef"[byte&15];}return hex;
}
}
// Test-only SQL boundary: actual CharacterCommands, CreationRules, Characters
// transaction/parameter construction, roster reload and wire reply code execute.
// No ODBC connection, database/schema lifecycle or stored procedure runs here.
namespace eternity::storage {
SqlServer::SqlServer(const SqlServerConfig&) {check(active!=nullptr,"Unexpected SQL owner");}
SqlServer::~SqlServer()=default;
void SqlServer::begin() {transaction_active_=true;}
void SqlServer::commit() {transaction_active_=false;++active->commits;}
void SqlServer::rollback() {transaction_active_=false;++active->rollbacks;}
std::vector<ResultSet> SqlServer::query(std::string_view sql,std::span<const Parameter> parameters) {
    if(sql.find("sp_getapplock")!=sql.npos) return table(1,{{"0"}});
    if(sql.find("UPDLOCK,HOLDLOCK")!=sql.npos) return table(1,{});
    if(sql.find("usp_Create_Char_R")!=sql.npos) {
        active->created.assign(parameters.begin(),parameters.end());return table(1,{{active->failure==1?"0":"42"}});
    }
    if(sql.find("usp_Save_Char_Item_Add_E")!=sql.npos) {
        active->saved.emplace_back(parameters.begin(),parameters.end());
        const bool fail=(active->failure==2 && active->saved.size()==1)||(active->failure==3 && active->saved.size()==2);
        return table(1,{{fail?"-1":"0"}});
    }
    if(sql.find("usp_Read_User_MaxGrow_R")!=sql.npos) return table(2,{{"1","3"}});
    if(sql.find("usp_Read_Char_Items_Simple_R")!=sql.npos) return table(9,{});
    if(sql.find("usp_Read_Chars_R")!=sql.npos) {
        const auto& p=active->created;Row row(29,Cell{"0"});row[0]="42";row[1]=std::get<std::string>(p[3]);
        constexpr std::array<unsigned,19> indices{4,5,6,7,8,9,10,11,12,15,16,17,18,19,20,21,22,23,24};
        for(unsigned i=0;i<indices.size();++i) row[i+2]=std::to_string(number(p[indices[i]]));
        row[23]="2024";row[24]="1";row[25]="1";return table(29,{row});
    }
    throw std::runtime_error("Unexpected scripted SQL query");
}
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original creation-loop fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot open original fixture");
    unsigned cases=0,total=0,successful=0,persisted_rows=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,count,server;std::uint32_t baseline;std::uint64_t before,after;std::string expected_events,payload;
        row>>scenario>>count>>server>>baseline>>before>>after>>expected_events>>payload;check(!row.fail(),"Malformed creation-loop fixture");
        Clock clock;clock.scenario=scenario;auto ids=std::make_shared<game::ItemIds>(static_cast<std::uint8_t>(server),clock);ids->set_start_id(baseline);clock.events.clear();
        check(ids->next()==before,"Shared producer before creation differs");
        Harness harness;harness.failure=scenario%4;active=&harness;unsigned samples=0;
        game::CharacterCommands commands({},game::CharacterCommandOptions{1,std::chrono::sys_days{std::chrono::year{2024}/1/1},"Eternity_Test_Creation_Port"},rules(count),ids,[&] {
            if(samples++==0) return game::CreationTime{0xffffffffffffffffULL,date(0)};
            clock.events+='M';return game::CreationTime{0xffffffffffffffffULL,date(29+samples-1)};
        },[](std::size_t){return 0;});
        game::CharacterRoster roster;roster.country=0;roster.max_grow=2;roster.new_account=true;
        net::Packet request(31,0);request[0]=2;request[1]=1;const std::string name="UIDHero";std::copy(name.begin(),name.end(),request.begin()+10);
        const auto reply=commands.execute(5001,"owner",roster,request);
        check(ids->next()==after,"Shared producer after creation differs");
        std::erase_if(expected_events,[](char event){return event=='L'||event=='U'||event=='P';});
        check(clock.events==expected_events && samples==count+1,"Live UID / separate make-time sampling order differs");
        const auto expected_saved=harness.failure==1?0U:harness.failure==2?1U:harness.failure==3?2U:count;
        check(harness.saved.size()==expected_saved,"Starter save failure did not stop at the failed row");
        persisted_rows+=static_cast<unsigned>(harness.saved.size());
        for(unsigned i=0;i<harness.saved.size();++i) check(persisted(harness.saved[i])==payload.substr(i*46,46),"Original 23-byte starter payload / SQL persistence parameters differ");
        check(!reply.close && !reply.packets.empty() && reply.packets[0]==net::Packet({2,1,static_cast<std::uint8_t>(harness.failure?1:0)}),"Creation result packet differs");
        if(!harness.failure) {
            check(harness.saved.size()==count && harness.commits==1 && harness.rollbacks==0,"Successful item persistence transaction differs");
            check(reply.packets.size()==6 && roster.country==0 && roster.max_grow==2 && roster.new_account,"Cached roster flags changed during reload");
            for(unsigned i=1;i<5;++i) check(reply.packets[i]==net::Packet({1,1,static_cast<std::uint8_t>(i),0,0,0,0}),"Empty character-list packet ordering differs");
            const auto& packet=reply.packets.back();check(packet.size()==83 && packet[0]==1 && packet[1]==1 && packet[2]==0 && packet[3]==42 && std::string(packet.begin()+56,packet.begin()+63)==name,"Created character-list wire record differs");++successful;
        } else check(reply.packets.size()==1 && harness.commits==0 && harness.rollbacks==1,"Failed creation transaction/reply differs");
        check(harness.created.size()==36 && std::get<std::string>(harness.created[3])==name,"Actual creation procedure parameter projection differs");
        // Invalid admission must not sample either clock or open a SQL statement.
        const auto events=clock.events;const auto sample_count=samples;const auto saved_count=harness.saved.size();
        auto invalid=request;invalid[2]=5;check(commands.execute(5001,"owner",roster,invalid).close,"Invalid slot was admitted");
        check(clock.events==events && samples==sample_count && harness.saved.size()==saved_count,"Rejected create consumed services");
        total+=count;++cases;active=nullptr;
    }
    check(cases==64 && successful==16,"Incomplete creation command coverage");
    std::cout<<cases<<" actual CharacterCommands flows / "<<total<<" original generated starter payloads; "<<persisted_rows<<" SQL parameter records compared; "<<successful<<" successes and 48 scripted SQL failures; same shared UID owner before/after matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
