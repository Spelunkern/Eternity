#include "eternity/game/obelisk_runtime_calendar.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
using Key=std::array<std::uint16_t,6>;
Key key(const ObeliskRuntimeCalendar& c) {return {c.year,c.month,c.day,c.hour,c.minute,c.second};}
struct Platform final:ObeliskRuntimeCalendarPlatform {
    ObeliskRuntimeCalendar local;
    std::map<Key,std::pair<bool,std::uint64_t>> variants;
    unsigned local_calls=0;
    ObeliskRuntimeCalendar local_calendar() override {++local_calls;return local;}
    bool variant_day(const ObeliskRuntimeCalendar& calendar,double& output) override {
        const auto found=variants.find(key(calendar));
        if(found==variants.end()) throw std::runtime_error("Missing captured OLE response");
        output=std::bit_cast<double>(found->second.second);return found->second.first;
    }
};
ObeliskRuntimeCalendar read_calendar(std::istringstream& row) {
    ObeliskRuntimeCalendar value;
    row>>value.year>>value.month>>value.weekday>>value.day>>value.hour>>value.minute>>value.second>>value.millisecond;
    return value;
}
void check(bool condition,const std::string& context) {if(!condition) throw std::runtime_error(context);}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected calendar oracle fixture");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing calendar fixture");
        Platform platform;ObeliskRuntimeCalendarClock clock(platform);
        std::string line;unsigned variants=0,packs=0,subtractions=0,restores=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#') continue;
            std::istringstream row(line);char kind;row>>kind;
            if(kind=='V') {
                Key fields;for(auto& value:fields) row>>value;
                unsigned success;std::uint64_t bits;row>>success>>bits;
                platform.variants.emplace(fields,std::pair{success!=0,bits});++variants;
            } else if(kind=='P') {
                platform.local=read_calendar(row);std::uint32_t expected;row>>expected;platform.local_calls=0;
                check(clock.packed_now()==expected,"Original calendar packing mismatch at "+std::to_string(packs));
                check(platform.local_calls==1,"Packing must acquire local calendar once");++packs;
            } else if(kind=='S') {
                std::uint32_t now,change,expected;row>>now>>change>>expected;
                const auto actual=clock.seconds_between(now,change);
                check(actual==expected,"Original second difference mismatch at "+std::to_string(subtractions)+": "+std::to_string(actual)+" != "+std::to_string(expected));++subtractions;
            } else if(kind=='R') {
                platform.local=read_calendar(row);platform.local_calls=0;
                std::uint32_t change,packed,country,last,remaining,saved,calls;row>>change>>packed>>country>>last>>remaining>>saved>>calls;
                ObeliskRuntime o;o.base_id=123;std::array<std::span<ObeliskRuntime>,1> maps{std::span<ObeliskRuntime>(&o,1)};
                restore_obelisk_ownership(maps,{},123,packed,change,clock);
                check(static_cast<std::uint32_t>(o.country)==country && static_cast<std::uint32_t>(o.last_destroy_country)==last &&
                      o.reset_time==remaining && o.change_time==saved && platform.local_calls==calls,
                      "Original restoration with real calendar mismatch at "+std::to_string(restores));++restores;
            } else throw std::runtime_error("Unknown calendar fixture row");
            check(!row.fail(),"Malformed calendar fixture row");
        }
        check(variants==2365 && packs==264 && subtractions==1229 && restores==512,"Incomplete calendar fixture");
        std::cout<<packs<<" original local-calendar packs, "<<subtractions<<" original differences, "<<restores
                 <<" original full ownership restores; "<<variants<<" Wine OLE responses\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
