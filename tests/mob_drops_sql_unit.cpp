#include "eternity/game/mob_drops.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace eternity;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
// Read-only isolated SQL check; run by the coordinator inside its serialized SQL
// integration environment. Never registers as an ordinary credential-free CTest.
int main() {try {
    storage::SqlServer database(storage::SqlServerConfig::from_environment());
    const auto maximum=database.query("EXEC dbo.usp_Read_MobMaxID_R;");
    check(maximum.size()==1 && maximum[0].rows.size()==1 && maximum[0].rows[0].size()==1 && maximum[0].rows[0][0],"Original MobMaxID contract changed");
    const auto count=static_cast<std::size_t>(std::stoul(*maximum[0].rows[0][0]))+1;
    const auto tables=database.query("EXEC dbo.usp_Read_MobItems_R;");check(tables.size()==1,"Original MobItems result count changed");
    const auto loaded=game::MobDropDefinitions::load_sql(count,tables[0]);
    std::map<std::pair<std::size_t,std::size_t>,std::pair<std::uint16_t,std::uint32_t>> expected;std::size_t rejected=0;
    for(const auto& row:tables[0].rows) {
        check(row.size()==4 && row[0] && row[1] && row[2] && row[3],"Original MobItems NULL/column contract changed");
        const auto mob=static_cast<std::uint16_t>(std::stoll(*row[0]));const auto order=static_cast<std::uint8_t>(std::stoll(*row[1]));
        if(mob>=count || order>=10) {++rejected;continue;}
        expected[{mob,order}]={static_cast<std::uint16_t>(std::stoll(*row[2])),static_cast<std::uint32_t>(std::stoll(*row[3]))};
    }
    check(!expected.empty() && loaded.mobs.size()==count && loaded.rejected_rows.size()==rejected,"Original MobItems catalog is empty or differs");
    for(std::size_t mob=0;mob<count;++mob) for(std::size_t slot=0;slot<10;++slot) {
        const auto value=expected[{mob,slot}];check(loaded.mobs[mob][slot].grade==value.first && loaded.mobs[mob][slot].rate==value.second,"Original MobItems SQL projection differs");
    }
    std::cout<<tables[0].rows.size()<<" original SQL MobItems rows; "<<count<<" mob definitions; "<<rejected<<" rejected rows matched without writes\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
