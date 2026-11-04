#include "eternity/game/item_ids.hpp"
#include "eternity/storage/sql_server.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
std::uint64_t hash_byte(std::uint64_t hash,unsigned char byte) {return (hash^byte)*1099511628211ULL;}
struct Clock:ItemIdClock {
    std::uint32_t tick=0,packed=0;std::string events;
    std::uint32_t ticks_ms() override {events+='T';return tick;}
    std::uint32_t packed_local_calendar() override {events+='C';return packed;}
};
int main(int argc,char** argv) {try {
    check(argc==2 || (argc==3 && std::string(argv[2])=="--sql"),"Expected original UID fixture [--sql]");
    std::unique_ptr<eternity::storage::SqlServer> database;
    if(argc==3) database=std::make_unique<eternity::storage::SqlServer>(eternity::storage::SqlServerConfig::from_environment());
    unsigned sql_values=0;std::ifstream input(argv[1]);check(input.good(),"Cannot read fixture");
    Clock clock;std::unique_ptr<ItemIds> ids;unsigned sessions=0,operations=0;std::uint64_t draws=0;unsigned server=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='B') {std::uint32_t baseline;row>>baseline;ids->set_start_id(baseline);continue;}
        int y;unsigned m,d,h,minute,second;
        if(tag=='I') {
            unsigned case_id;std::uint32_t expected;row>>case_id>>server>>y>>m>>d>>h>>minute>>second>>expected;
            clock.packed=expected;clock.events.clear();ids=std::make_unique<ItemIds>(static_cast<std::uint8_t>(server),clock);
            check(clock.events=="C","Constructor must sample calendar only");++sessions;
        } else {
            char kind;unsigned repetitions;std::uint32_t packed,counter,baseline,deadline;std::uint64_t expected,last,event_hash;
            row>>kind>>repetitions>>clock.tick>>y>>m>>d>>h>>minute>>second>>packed>>expected>>last>>event_hash>>counter>>baseline>>deadline;
            if(m==0) clock.packed=0;
            else clock.packed=pack_item_id_time(std::chrono::sys_days{std::chrono::year{y}/m/d}+std::chrono::hours{h}+std::chrono::minutes{minute}+std::chrono::seconds{second});
            std::uint64_t hash=14695981039346656037ULL,events=14695981039346656037ULL,actual=0;
            for(unsigned i=0;i<repetitions;++i) {
                clock.events.clear();actual=kind=='R'?ids->raw_next():ids->next();
                for(unsigned b=0;b<8;++b) hash=hash_byte(hash,static_cast<unsigned char>(actual>>(b*8)));
                // Original OS trace is lock -> tick -> optional calendar -> unlock.
                // Lock placement is native internal ownership, not an injected port.
                for(auto byte:std::string("L")+clock.events+"U") events=hash_byte(events,static_cast<unsigned char>(byte));
            }
            check(actual==last && hash==expected,"Original UID stream differs at operation "+std::to_string(operations));
            check(events==event_hash,"Original tick/calendar cadence differs");++operations;draws+=repetitions;
            if(database && operations<=64) {
                const auto signed_id=std::bit_cast<std::int64_t>(actual);
                const std::array<eternity::storage::Parameter,1> parameters{signed_id};
                const auto result=database->query("SELECT CONVERT(bigint, ?) AS UID;",parameters);
                check(result.size()==1 && result[0].rows.size()==1 && result[0].rows[0].size()==1 && result[0].rows[0][0]==std::to_string(signed_id),"Original UID signed bigint round trip differs");
                ++sql_values;
            }
        }
        check(!row.fail(),"Malformed UID fixture");
        if(m!=0) {
            const auto date=std::chrono::sys_days{std::chrono::year{y}/m/d}+std::chrono::hours{h}+std::chrono::minutes{minute}+std::chrono::seconds{second};
            if(tag=='I') check(pack_item_id_time(date)==clock.packed,"Original constructor calendar packing differs");
        }
    }
    check(sessions==128 && operations==2304 && draws>65536,"Incomplete shared UID coverage");
    std::cout<<sessions<<" original constructors; "<<operations<<" interleaved operations; "<<draws<<" original generated IDs matched; "<<sql_values<<" read-only SQL bigint round trips\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
