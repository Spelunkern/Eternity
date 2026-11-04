#include "eternity/game/mob_drops_creation.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
struct Random:game::RandomSource {
    game::MicrosoftRandom generator;unsigned calls=0;explicit Random(std::uint32_t seed):generator(seed) {}
    std::int32_t next() override {++calls;return generator.next();}
};
int main(int argc,char** argv) {try {
    check(argc==2 || argc==3,"Expected ignored SQL projection path and optional original reference");
    storage::SqlServer database(storage::SqlServerConfig::from_environment());const auto tables=database.query("EXEC dbo.usp_Read_Items_R;");check(tables.size()==1,"Original Items result count changed");
    const auto& table=tables[0];const auto definitions=game::ItemDefinitions::load(table);
    const auto column=[&](const char* name) {const auto at=std::find(table.columns.begin(),table.columns.end(),name);check(at!=table.columns.end(),"Missing original item key");return static_cast<std::size_t>(at-table.columns.begin());};
    const auto type_column=column("Type"),id_column=column("TypeID");std::ofstream snapshot(argv[1]);check(snapshot.good(),"Cannot write ignored SQL projection");snapshot<<'[';
    std::uint64_t hash=14695981039346656037ULL,draws=0;std::size_t count=0,expanded=0;
    const auto byte=[&](std::uint8_t value) {hash^=value;hash*=1099511628211ULL;};const auto word=[&](std::uint32_t value,unsigned size) {for(unsigned i=0;i<size;++i) byte(static_cast<std::uint8_t>(value>>(i*8)));};
    for(const auto& row:table.rows) {
        check(row.size()>std::max(type_column,id_column) && row[type_column] && row[id_column],"Invalid SQL item identifier");
        const auto* definition=definitions.find(static_cast<std::uint8_t>(std::stoul(*row[type_column])),static_cast<std::uint8_t>(std::stoul(*row[id_column])));check(definition,"Missing restored item definition");
        if(count) snapshot<<',';snapshot<<'['<<unsigned(definition->type)<<','<<unsigned(definition->type_id)<<','<<unsigned(definition->server)<<','<<definition->required_wisdom<<','<<unsigned(definition->real_type)<<']';
        game::GroundItem item;item.definition=definition;game::set_ground_item_craft(item,"abcdefghijklmnopqrst");for(unsigned j=0;j<11;++j) item.craft_additions[j]=static_cast<std::uint16_t>(100+j);
        Random random(static_cast<std::uint32_t>(count)*7919U);game::create_mob_drop_craft(item,random);
        for(auto value:item.craft_storage) byte(value);for(auto value:item.craft_additions) word(value,2);word(random.generator.state(),4);word(random.calls,4);
        draws+=random.calls;if(definition->server && definition->required_wisdom>0 && definition->required_wisdom<100) ++expanded;++count;
    }
    snapshot<<']';check(snapshot.good(),"Cannot finish SQL item projection");
    if(argc==3) {
        std::ifstream reference(argv[2]);check(reference.good(),"Cannot open original SQL craft reference");bool compared=false;
        for(std::string line;std::getline(reference,line);) if(!line.empty() && line[0]=='A') {std::istringstream input(line);char tag;std::uint64_t original_hash,original_draws;std::size_t original_count,original_expanded;input>>tag>>original_count>>original_hash>>original_draws>>original_expanded;
            check(!input.fail() && count==original_count && hash==original_hash && draws==original_draws && expanded==original_expanded,"Complete original SQL craft expansion differs");compared=true;}
        check(compared,"Missing original SQL craft evidence");
    }
    check(count>0 && expanded>0 && draws>0,"Empty SQL craft coverage");std::cout<<count<<" SQL definitions; "<<expanded<<" craft-eligible; "<<draws<<" CRT draws; hash "<<hash<<(argc==3?" matched original execution":" native projection")<<" without database writes\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
