#include "eternity/game/mob_drops_reload.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace eternity;
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
struct Diagnostics:game::MobDropReloadDiagnostics {
    unsigned errors=0,rejections=0;void rejected(const game::MobDropRow&) override {++rejections;}void failed(game::MobDropReloadFailure) override {++errors;}
};
std::uint64_t hash(const game::MobDropDefinitions& definitions,std::int32_t changes) {
    std::uint64_t result=14695981039346656037ULL;const auto add=[&](std::uint32_t value,unsigned count) {for(unsigned i=0;i<count;++i) {result^=(value>>(i*8))&255U;result*=1099511628211ULL;}};
    for(const auto& mob:definitions.mobs) for(const auto& slot:mob) {add(slot.grade,2);add(slot.rate,4);}add(std::bit_cast<std::uint32_t>(changes),4);return result;
}
int main(int argc,char** argv) {try {
    check(argc==2 || argc==3,"Expected ignored selective SQL projection and optional original reference");
    storage::SqlServer database(storage::SqlServerConfig::from_environment());const auto maximum=database.query("EXEC dbo.usp_Read_MobMaxID_R;");
    check(maximum.size()==1 && maximum[0].rows.size()==1 && maximum[0].rows[0][0],"Original mob maximum contract changed");const auto count=std::stoul(*maximum[0].rows[0][0])+1;check(count>100 && count<=65536,"Unexpected restored mob count");
    const auto full=database.query("EXEC dbo.usp_Read_MobItems_R;");check(full.size()==1,"Original MobItems result shape changed");auto definitions=game::MobDropDefinitions::load_sql(count,full[0]);
    struct Reference {std::uint32_t counter=0;std::uint64_t hash=0;std::size_t rows=0;};std::map<unsigned,Reference> expected;
    if(argc==3) {std::ifstream input(argv[2]);check(input.good(),"Cannot read original selective SQL reference");for(std::string line;std::getline(input,line);) if(!line.empty() && line[0]=='A') {std::istringstream row(line);char tag;unsigned request;Reference value;row>>tag>>request>>value.counter>>value.hash>>value.rows;check(!row.fail(),"Malformed original selective SQL reference");expected.emplace(request,value);}}
    std::ofstream output(argv[1]);check(output.good(),"Cannot write ignored selective SQL projection");output<<'[';bool first_case=true;std::int32_t changes=17;std::size_t total_rows=0;Diagnostics diagnostics;const auto* binding=definitions.mobs.data();
    const std::array<std::uint16_t,5> requests{1,2,17,100,static_cast<std::uint16_t>(count-1)};
    for(auto request:requests) {
        const std::array<storage::Parameter,1> parameters{static_cast<std::int64_t>(request)};const auto result=database.query("EXEC dbo.usp_Read_MobItems_R2 ?;",parameters);check(result.size()==1 && result[0].columns.size()==4,"Original selective MobItems result shape changed");
        for(auto& slot:definitions.mobs[request]) {slot.grade^=0x1234;slot.rate^=1;}
        if(!first_case) output<<',';first_case=false;output<<"{\"request\":"<<request<<",\"count\":"<<count<<",\"counter\":"<<std::bit_cast<std::uint32_t>(changes)<<",\"initial\":[";bool first_slot=true;
        for(const auto& mob:definitions.mobs) for(const auto& slot:mob) {if(!first_slot) output<<',';first_slot=false;output<<slot.grade<<','<<slot.rate;}output<<"],\"rows\":[";bool first_row=true;
        for(const auto& row:result[0].rows) {
            check(row.size()==4 && row[0] && row[1] && row[2] && row[3],"Invalid original selective MobItems row");if(!first_row) output<<',';first_row=false;
            output<<'['<<static_cast<std::uint16_t>(std::stoll(*row[0]))<<','<<static_cast<std::uint8_t>(std::stoll(*row[1]))+0U<<','<<static_cast<std::uint16_t>(std::stoll(*row[2]))<<','<<static_cast<std::uint32_t>(std::stoll(*row[3]))<<']';
        }
        output<<"]}";check(game::reload_mob_drop_sql(definitions,request,changes,database,diagnostics),"Selective MobItems reload failed");check(definitions.mobs.data()==binding && diagnostics.errors==0 && diagnostics.rejections==0,"Selective reload changed storage or reported error");
        const auto before=changes;const auto actual_hash=hash(definitions,changes);
        if(argc==3) {const auto found=expected.find(request);check(found!=expected.end() && found->second.counter==std::bit_cast<std::uint32_t>(changes) && found->second.hash==actual_hash && found->second.rows==result[0].rows.size(),"Original selective SQL reload differs");}
        check(game::reload_mob_drop_sql(definitions,request,changes,database,diagnostics) && changes==before,"Idempotent SQL reload incremented changes");total_rows+=result[0].rows.size();
    }
    output<<']';check(output.good(),"Cannot finish ignored selective SQL snapshot");
    std::cout<<requests.size()<<" selective original SQL reloads; "<<total_rows<<" rows; cumulative counter "<<std::bit_cast<std::uint32_t>(changes)<<"; stable "<<count<<" definitions"<<(argc==3?" matched original execution":" native projection")<<" without database writes\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
