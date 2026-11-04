#include "eternity/game/mob_drops_catalog.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace eternity;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
// Read-only isolated SQL check; run by the coordinator inside its serialized SQL
// integration environment. Never registers as an ordinary credential-free CTest.
int main(int argc,char** argv) {try {
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
    const auto items=database.query("EXEC dbo.usp_Read_Items_R;");check(items.size()==1,"Original Items result count changed");
    auto catalog_definitions=game::MobDropCatalogDefinitions::load_sql(items[0]);
    // Capture pre-initialization rows for executing the original catalog over the
    // exact restored definitions. The optional path must be ignored local work/.
    if(argc>=2) {
        std::ofstream snapshot(argv[1]);check(snapshot.good(),"Cannot write ignored item projection");snapshot<<'[';bool first=true;
        for(const auto& record:catalog_definitions.records) {
            if(!first) snapshot<<',';first=false;const auto& entry=*record.entry;
            snapshot<<'['<<unsigned(record.table_type)<<','<<unsigned(record.table_id)<<','<<entry.item.id<<','<<unsigned(entry.item.type)<<','<<unsigned(entry.item.type_id)<<','<<entry.item.real_type<<','<<record.grade<<','<<unsigned(record.server)<<','<<entry.limit.maximum<<','<<entry.limit.count<<','<<entry.limit.next_tick<<','<<entry.limit.delay<<','<<entry.limit.random_delay<<']';
        }
        snapshot<<']';check(snapshot.good(),"Cannot finish ignored item projection");
    }
    game::MobDropCatalog catalog;game::initialize_mob_drop_catalog(catalog,catalog_definitions.records);
    std::uint64_t hash=14695981039346656037ULL;const auto add=[&](std::uint32_t value) {for(unsigned i=0;i<4;++i) {hash^=(value>>(i*8))&255U;hash*=1099511628211ULL;}};
    std::size_t selected=0;
    for(std::size_t grade=0;grade<catalog.grades.size();++grade) {
        add(static_cast<std::uint32_t>(catalog.grades[grade].size()));add(static_cast<std::uint32_t>(catalog.unrestricted_counts[grade]));
        for(const auto* entry:catalog.grades[grade]) {add(entry->item.id);++selected;}
    }
    for(const auto& entry:catalog_definitions.entries) {add(static_cast<std::uint32_t>(entry.limit.count));add(entry.limit.next_tick);add(entry.limit.delay);add(entry.limit.random_delay);}
    if(argc>=3) {
        std::ifstream reference(argv[2]);check(reference.good(),"Cannot open original SQL catalog reference");bool compared=false;
        for(std::string line;std::getline(reference,line);) if(!line.empty() && line[0]=='A') {
            char tag;unsigned scenario;std::size_t rows;std::uint64_t expected;std::istringstream input(line);input>>tag>>scenario>>rows>>expected;
            check(!input.fail() && rows==catalog_definitions.records.size() && hash==expected,"Complete original SQL item catalog differs");compared=true;
        }
        check(compared,"Missing original SQL catalog evidence");
    }
    std::cout<<catalog_definitions.records.size()<<" original SQL item definitions; "<<selected<<" catalog members; hash "<<hash<<" matched"<<(argc>=3?" original execution":" native projection")<<"\n";
    std::cout<<tables[0].rows.size()<<" original SQL MobItems rows; "<<count<<" mob definitions; "<<rejected<<" rejected rows matched without writes\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
