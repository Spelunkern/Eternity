#include "eternity/game/mob_drops_catalog.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
struct Hash {
    std::uint64_t value=14695981039346656037ULL;
    void add(std::uint32_t word) {for(unsigned i=0;i<4;++i) {value^=(word>>(i*8))&255U;value*=1099511628211ULL;}}
};
std::uint64_t catalog_hash(const MobDropCatalog& catalog,std::span<const MobDropCatalogEntry> entries) {
    Hash hash;
    for(std::size_t grade=0;grade<catalog.grades.size();++grade) {
        hash.add(static_cast<std::uint32_t>(catalog.grades[grade].size()));hash.add(static_cast<std::uint32_t>(catalog.unrestricted_counts[grade]));
        for(const auto* entry:catalog.grades[grade]) hash.add(entry->item.id);
    }
    for(const auto& entry:entries) {hash.add(std::bit_cast<std::uint32_t>(entry.limit.count));hash.add(entry.limit.next_tick);hash.add(entry.limit.delay);hash.add(entry.limit.random_delay);}
    return hash.value;
}
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original catalog fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read catalog fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream header(line);char tag;unsigned scenario,count;std::uint64_t expected;header>>tag>>scenario>>count>>expected;check(!header.fail(),"Malformed catalog scenario");
        if(tag=='A') {check(count>0 && expected!=0,"Invalid restored catalog reference");continue;}
        check(tag=='S',"Unknown catalog scenario");
        std::vector<MobDropCatalogEntry> entries(count);std::vector<MobDropCatalogRecord> records;records.reserve(count);
        for(unsigned i=0;i<count;++i) {
            check(static_cast<bool>(std::getline(input,line)),"Truncated catalog scenario");std::istringstream row(line);
            unsigned table_type,table_id,type,id,grade,server,drop;auto& entry=entries[i];
            row>>tag>>table_type>>table_id>>entry.item.id>>type>>id>>entry.item.real_type>>grade>>server>>drop>>entry.limit.count>>entry.limit.next_tick>>entry.limit.delay>>entry.limit.random_delay;
            check(tag=='R' && !row.fail(),"Malformed catalog row");entry.item.type=static_cast<std::uint8_t>(type);entry.item.type_id=static_cast<std::uint8_t>(id);entry.limit.maximum=static_cast<std::uint16_t>(drop);
            records.push_back({static_cast<std::uint8_t>(table_type),static_cast<std::uint8_t>(table_id),&entry,static_cast<std::uint16_t>(grade),static_cast<std::uint8_t>(server)});
        }
        MobDropCatalog catalog;catalog.grades[5].push_back(&entries[0]);catalog.unrestricted_counts[5]=17;catalog.sequences.push_back({7,{}});
        initialize_mob_drop_catalog(catalog,records);
        check(catalog_hash(catalog,entries)==expected,"Original catalog initialization differs: "+std::to_string(scenario));
        check(catalog.sequences.size()==1 && catalog.sequences[0].divisor==7,"Catalog initialization changed external sequence configuration");
        // The runtime may initialize again: grade vectors must replace contents,
        // and limited entries must reset while unlimited state remains untouched.
        for(const auto& record:records) {
            auto& entry=*record.entry;
            if(record.table_type && record.table_id && entry.limit.maximum && entry.item.type && entry.item.type_id) {entry.limit.count=17;entry.limit.next_tick=99;}
        }
        initialize_mob_drop_catalog(catalog,records);check(catalog_hash(catalog,entries)==expected,"Catalog reinitialization retains stale state");++cases;
    }
    MobDropCatalog catalog;MobDropCatalogEntry first,second;first.item={1,1,1};second.item={2,1,1};
    const std::array<MobDropCatalogRecord,2> duplicate{{{1,1,&first,3,0},{1,1,&second,4,0}}};
    initialize_mob_drop_catalog(catalog,duplicate);check(catalog.grades[3].empty() && catalog.grades[4]==std::vector<MobDropCatalogEntry*>{&second},"Duplicate table slot did not use last binding");
    second.limit.maximum=7;bool rejected=false;try {initialize_mob_drop_catalog(catalog,duplicate);} catch(const std::invalid_argument&) {rejected=true;}check(rejected,"Undefined original interval selector was guessed");
    check(cases==32,"Incomplete original catalog coverage");std::cout<<cases<<" complete original catalog initializations, stable partitions and limit resets matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
