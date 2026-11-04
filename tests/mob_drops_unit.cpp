#include "eternity/game/mob_drops.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
struct Provider:MobDropProvider {
    std::string events;
    void event(const std::string& e) {if(!events.empty()) events+=',';events+=e;}
    std::uint32_t select_item(std::uint16_t grade) override {event("S"+std::to_string(grade));return grade%3==0?0:grade+10000;}
    void deliver_item(std::uint32_t item,std::uint8_t count,bool sequence) override {check(count==1 && !sequence,"Wrong delivery arguments");event("D"+std::to_string(item));}
};
struct Clock:MobDropCatalogWorld {std::uint32_t now=0;std::string events;std::uint32_t tick() override {events+='T';return now;}void lock() override {events+='L';}void unlock() noexcept override {events+='U';}};
struct Random:RandomSource {MicrosoftRandom generator;unsigned calls=0;explicit Random(std::uint32_t seed):generator(seed) {} std::int32_t next() override {++calls;return generator.next();}};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected fixture path");std::ifstream input(argv[1]);check(input.good(),"Cannot open fixture");unsigned slots=0,limits=0,sequences=0,loads=0,positions=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);char tag;unsigned scenario;std::uint32_t seed;row>>tag>>scenario>>seed;Random random(seed);
        if(tag=='S') {
            MobDropDefinition definition;for(auto& slot:definition) row>>slot.grade>>slot.rate;
            std::uint32_t final;unsigned calls;std::string expected;row>>final>>calls>>expected;
            ServerRandom server(seed);Provider provider;mob_drop_slots(definition,random,server,provider);
            check((provider.events.empty()?"-":provider.events)==expected,"Original slot events differ: "+std::to_string(scenario));
            check(random.calls==calls && random.generator.state()==final,"Original slot random consumption differs");++slots;
        } else if(tag=='L') {
            MobDropLimit limit;std::uint32_t tick,expected_count,next,final;unsigned success,calls;
            row>>limit.maximum>>limit.count>>tick>>limit.next_tick>>limit.delay>>limit.random_delay>>success>>expected_count>>next>>final>>calls;
            MobDropCatalogEntry entry;entry.limit=limit;MobDropCatalog catalog;catalog.grades[1].push_back(&entry);Clock clock;clock.now=tick;
            const bool actual=catalog.select(1,random,clock)!=nullptr;limit=entry.limit;check(clock.events=="TLU","Catalog clock/lock order differs");
            check(actual==(success!=0) && std::bit_cast<std::uint32_t>(limit.count)==expected_count && limit.next_tick==next,"Original catalog limit differs: "+std::to_string(scenario));
            check(random.calls==calls && random.generator.state()==final,"Original catalog random consumption differs");++limits;
        } else if(tag=='Q') {
            std::uint16_t level,grade;std::uint32_t object,final;unsigned success,calls;row>>level>>object>>grade>>success>>final>>calls;
            MobDropCatalogEntry entry;MobDropCatalog catalog;catalog.grades[1].push_back(&entry);MobDropSequence first,second;first.divisor=2;first.grades.fill(grade);second.divisor=3;second.grades.fill(1);catalog.sequences={first,second};Clock clock;clock.now=100;
            check((catalog.sequence(level,object,random,clock)!=nullptr)==(success!=0),"Original sequence selection differs: "+std::to_string(scenario));
            check(random.calls==calls && random.generator.state()==final,"Original sequence random consumption differs");++sequences;
        } else if(tag=='R') {
            unsigned mob,order,grade;std::uint32_t rate;std::string expected;row>>mob>>order>>grade>>rate>>expected;
            MobDropRow record{static_cast<std::uint16_t>(mob),static_cast<std::uint8_t>(order),static_cast<std::uint16_t>(grade),rate};
            const auto result=MobDropDefinitions::load(seed,std::span(&record,1));std::string actual;const char* hex="0123456789abcdef";
            const auto byte=[&](std::uint32_t value){actual+=hex[(value>>4)&15];actual+=hex[value&15];};
            for(const auto& definition:result.mobs) for(const auto& slot:definition) {byte(slot.grade);byte(slot.grade>>8);byte(0);byte(0);for(unsigned j=0;j<4;++j) byte(slot.rate>>(j*8));}
            check(actual==expected,"Original MobItems assignment differs: "+std::to_string(scenario));
            check(result.rejected_rows.empty()==(mob<seed && order<10),"Original MobItems rejected row differs");++loads;
        } else if(tag=='P') {
            std::uint32_t x,z,ex,ey,ez,final;row>>x>>z>>ex>>ey>>ez>>final;
            const auto pos=mob_drop_position(std::bit_cast<float>(x),std::bit_cast<float>(z),random);
            check(std::bit_cast<std::uint32_t>(pos.x)==ex && std::bit_cast<std::uint32_t>(pos.y)==ey && std::bit_cast<std::uint32_t>(pos.z)==ez && random.generator.state()==final,"Original drop position differs: "+std::to_string(scenario));++positions;
        } else throw std::runtime_error("Unknown fixture tag");
        check(!row.fail(),"Malformed fixture");
    }
    const std::array<MobDropRow,5> rows{{{1,0,9,100},{1,9,7,999999},{1,9,8,1},{2,0,1,1},{0,10,1,1}}};
    const auto definitions=MobDropDefinitions::load(2,rows);
    check(definitions.mobs.size()==2 && definitions.mobs[1][0].grade==9 && definitions.mobs[1][9].grade==8 && definitions.rejected_rows==std::vector<std::size_t>{3,4},"MobItems load bounds/overwrite differs");
    eternity::storage::ResultSet sql;sql.columns={"MobID","ItemOrder","Grade","DropRate"};sql.rows={{{"1"},{"9"},{"-1"},{"-1"}},{{"1"},{"9"},{"2"},{"100"}}};
    auto loaded=MobDropDefinitions::load_sql(2,sql);check(loaded.mobs[1][9].grade==2 && loaded.mobs[1][9].rate==100,"SQL positional MobItems projection differs");
    sql.rows.resize(1);loaded=MobDropDefinitions::load_sql(2,sql);check(loaded.mobs[1][9].grade==65535 && loaded.mobs[1][9].rate==UINT32_MAX,"SQL signed MobItems bit patterns lost");
    check(slots==512 && limits==256 && sequences==128 && loads==128 && positions==128,"Incomplete original coverage");std::cout<<slots<<" original slot loops; "<<limits<<" original catalog limits; "<<sequences<<" original sequences; "<<loads<<" original definition assignments; "<<positions<<" original positions matched\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
