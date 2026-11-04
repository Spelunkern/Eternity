#include "eternity/game/obelisk_runtime_config.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
struct Owner final:ObeliskRuntimeDefinitionOwner {
    std::array<std::uint32_t,12> marked{};
    void mark_obelisk(const MobRuntimeTemplate& d) override {if(d.id<marked.size()) marked[d.id]=1;}
};
MobRuntimeTemplates catalog() {
    eternity::storage::ResultSet rows;rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};
    for(unsigned i=1;i<4096;++i) rows.rows.push_back({std::to_string(i),i%7==0?"0":"1","0","1","0","0","0","0","0","0","0","0","0","0","0"});
    return MobRuntimeTemplates::load(rows);
}
void run(int scenario,const std::vector<std::uint32_t>& expected,const MobRuntimeTemplates& definitions) {
    const std::array<std::int32_t,8> ids{0,1,7,-1,4096,2,3,4};std::array<ObeliskRuntimeZoneInput,3> zones;
    for(std::size_t mi=0;mi<3;++mi) {
        auto& zone=zones[mi];zone.map=static_cast<int>(mi);
        for(int row=0;row<(scenario/3+static_cast<int>(mi))%3;++row) {
            ObeliskRuntimeInput o;o.base_id=static_cast<std::uint32_t>(mi*65536)+static_cast<std::uint32_t>(row+1);o.country=(scenario+static_cast<int>(mi)+row)%5-1;
            o.center={std::array<float,5>{-1,0,39.75,40,2}[static_cast<std::size_t>((scenario+row)%5)],-2.5,scenario%11==0?-1.F:1.F};
            for(std::size_t k=0;k<3;++k) o.mobs[k]=ids[(static_cast<std::size_t>(scenario+row)+k)%8];
            for(int j=0;j<(scenario/5+row)%3;++j) {
                ObeliskRuntimeSpawnInput s;s.center={static_cast<float>(j)+0.25F,4,5};s.radius=std::array<float,4>{-2.75,0.75,2.75,1073741824}[static_cast<std::size_t>((scenario+j)%4)];s.count=(scenario+j)%4;
                for(std::size_t k=0;k<3;++k) s.mobs[k]=ids[(static_cast<std::size_t>(scenario+j+1)+k)%8];
                o.spawns.push_back(s);
            }
            zone.records.push_back(o);
        }
    }
    Owner owner;ObeliskRuntimeConfigurationState state;state.next_id=scenario%2?0xfffffffeU:0;state.total=scenario/9%3;state.kill_count_add={9,10};state.maximum_kill_count={1000,-45};
    auto configuration=ObeliskRuntimeConfiguration::bind(zones,std::array<std::int32_t,3>{40,40,40},definitions,owner,state);
    // A moved owner must preserve every spawn address already stored in respawns.
    auto moved=std::move(configuration);
    std::vector<std::uint32_t> actual{state.next_id,static_cast<std::uint32_t>(state.total),static_cast<std::uint32_t>(state.kill_count_add[0]),static_cast<std::uint32_t>(state.kill_count_add[1])};
    const auto word=[&](auto value){actual.push_back(static_cast<std::uint32_t>(value));};
    const auto position=[&](UserPosition p){word(std::bit_cast<std::uint32_t>(p.x));word(std::bit_cast<std::uint32_t>(p.y));word(std::bit_cast<std::uint32_t>(p.z));};
    const auto definition=[&](const MobRuntimeTemplate* d){word(d?d->id:0);};
    for(std::size_t mi=0;mi<3;++mi) {
        auto records=moved.for_map(mi);word(records.size());
        for(auto& o:records) {
            word(o.id);word(o.country);word(o.last_destroy_country);word(o.reset_time);word(o.change_time);word(o.base_id);word(o.base_country);position(o.center);
            for(auto* d:o.definitions) definition(d);
            word(o.respawn.type);word(o.respawn.count);word(o.respawn.current);word(o.respawn.next);word(o.respawn.obelisk);word(o.spawns.size());word(o.principal!=nullptr);
            for(auto& s:o.spawns) {
                position(s.center);word(std::bit_cast<std::uint32_t>(s.radius));word(s.radius_twice);word(s.count);
                for(auto* d:s.definitions) definition(d);
                word(s.respawn.type);word(s.respawn.count);word(s.respawn.current);word(s.respawn.next);word(s.respawn.obelisk);word(s.respawn.obelisk_source==&s);
                for(auto* member:s.members) word(member!=nullptr);
            }
        }
    }
    for(auto flag:owner.marked) word(flag);
    if(actual!=expected) {
        std::size_t i=0;while(i<actual.size()&&i<expected.size()&&actual[i]==expected[i]) ++i;
        std::cerr<<"Scenario "<<scenario<<" word "<<i<<" native="<<(i<actual.size()?std::to_string(actual[i]):"end")<<" original="<<(i<expected.size()?std::to_string(expected[i]):"end")<<'\n';
        throw std::runtime_error("Obelisk configuration materialization mismatch");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected config fixture");std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Missing config fixture");
        auto definitions=catalog();std::string line;unsigned count=0;
        while(std::getline(input,line)) {if(line.empty()||line[0]=='#') continue;std::istringstream row(line);int scenario;row>>scenario;std::vector<std::uint32_t> expected;std::uint32_t n;while(row>>n) expected.push_back(n);run(scenario,expected,definitions);++count;}
        if(count!=64) throw std::runtime_error("Incomplete config fixture");std::cout<<count<<" original obelisk materialization scenarios passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
